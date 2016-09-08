#include "serialengine.h"
#include "serialengine.moc"

#include <QTimer>
#include <QApplication>
#include <QElapsedTimer>
#include <QWidget>

#include <errno.h>

#include "serialport.h"
#include "serialsensor.h"
#include "serialportmanager.h"
#include "logger.h"

#define SERIAL_ENGINE_DEBUG

#ifdef SERIAL_ENGINE_DEBUG
#define se_debug(fmt, args...) Logger::debug("SerialEngine", fmt, ##args)
#else
#define se_debug(fmt, args...) do {} while (0)
#endif

SerialEngine::SerialEngine(unsigned _features, QObject *parent)
    : BaseEngine(_features, parent)
    , m_echoEnabled(false)
    , m_detectionTurn(true)
    , m_state(Synchronizing)
    , m_detectionState(SerialEngine::DetectionStateNew)
    , m_port(0)
    , m_currentSensor(-1)
    , m_stateCheckTimer(new QTimer(this))
{
    //Q_ASSERT(qobject_cast<SerialPortManager*>(parent));
    m_stateCheckTimer->setSingleShot(true);
    connect(m_stateCheckTimer, SIGNAL(timeout()), SLOT(slotOnStateCheckTimeout()));
}

SerialEngine::~SerialEngine()
{}

void SerialEngine::request(const EngineCommand &cb)
{
    Q_ASSERT(cb.rxcount <= sizeof(cb.rxbuf));
    Q_ASSERT(cb.txcount <= sizeof(cb.txbuf));
    //se_debug("Outstanding request: %s", qPrintable(cb.toString()));
    m_commandQueue.enqueue(cb);

    if (m_state == Idle) {
        serveCommand();
    }
}

void SerialEngine::serveCommand()
{
    logAndClearStrayData();
    if (m_commandQueue.isEmpty()) {
        Logger::error("Unexpected empty commandQueue");
        return;
    }

    EngineCommand ec = m_commandQueue.dequeue();
    Logger::message(Logger::EngineState, QString("Serving command %1").arg(qPrintable(ec.toString())));

    int ret = m_port->write((const char*)ec.txbuf, ec.txcount);
    if (ret < 0) {
        Logger::error("Port write failure");
        setState(PortError);
        /*callback failure*/
        return;
    }

    if (m_echoEnabled) {
        setState(WaitingEcho);
    } else {
        setState(WaitingReply);
    }
    m_currentCommand = ec;
    /*somehow register ec.callback to call in case of timeout/reply*/
}

SerialSensor *SerialEngine::currentSensor()
{
    if (m_currentSensor >= 0 && m_currentSensor < m_sensors.count())
        return m_sensors.at(m_currentSensor);

    return 0;
}

int SerialEngine::help_nextSensor()
{
    if (!m_sensors.count()) {
        m_currentSensor = -1;
    } else if (m_currentSensor >= m_sensors.count()) {
        m_currentSensor = 0;
    } else {
        m_currentSensor = (m_currentSensor + 1) % m_sensors.count();
    }
    return m_currentSensor;
}

int SerialEngine::nextSensor()
{
    int passed = 0;
    while (help_nextSensor() != -1) {
        if (readyToPoll(currentSensor())) {
            return m_currentSensor;
        }
        ++passed;
        if (passed > m_sensors.count()) {
            return -1;
        }
    }

    return -1;
}

void SerialEngine::deinit()
{
    /*clear command queue, stop timers, switch to Terminating state, delete sensors*/
    executeAndClearCurrentCommand(CommandCallback::ResultCanceled);
    while (!m_commandQueue.isEmpty()) {
        EngineCommand ec = m_commandQueue.dequeue();
        ec.executeCallback(CommandCallback::ResultCanceled);
    }
    m_currentSensor = -1;
    setState(Terminating);

/*
    foreach (SerialSensor *s, m_sensors) {
        s->deinit();
    }
*/
    foreach (SerialSensor *s, m_sensors) {
        s->deleteLater();
    }

    m_sensors.clear();
}

void SerialEngine::setPort(SerialPort *port)
{
    /*close old port, delete it?*/
    m_port = port;
    setEnginePortLocks();

    connect(m_port, SIGNAL(readyRead()), SLOT(slotOnReadyRead()));
}

void SerialEngine::clearCommandQueue()
{
    executeAndClearCurrentCommand(CommandCallback::ResultFailed);
    while (!m_commandQueue.isEmpty()) {
        EngineCommand ec = m_commandQueue.dequeue();
        ec.executeCallback(CommandCallback::ResultCanceled);
    }
}

void SerialEngine::setState(SerialEngine::State s)
{
    m_state = s;
    /*timeouts are engine-dependant*/

    Logger::message(Logger::EngineState,QString("Switcing to state %1").arg(enumValueToCharStar(this, "State", s)));

    switch (s) {
    case Error:
    case Stopped:
        clearCommandQueue();
        break;
    case Idle: {
        if (m_commandQueue.isEmpty()) {
            EngineCommand cmd;
            if (m_detectionTurn) {
                cmd = detectNext();
            } else {
                cmd = pollNextSensorLevel();
                if (!cmd.isValid() && isAutoDetectSensorsEnabled()) {
                    m_detectionTurn = true;
                    cmd = detectNext();
                }
            }
            if (m_detectionTurn && cmd.isValid())
                emit detectionStateChanged(m_detectionState);
            m_detectionTurn = isAutoDetectSensorsEnabled() ? !m_detectionTurn : false;
            switch (cmd.type) {
            case EngineCommand::LevelPoll:
                currentSensor()->invokeCoolDown();
                //fall through
            case EngineCommand::Search:
                logAndClearStrayData();
                request(cmd);
                break;
            default:
                if (m_buffer.size() >= 32) {
                    logAndClearStrayData();
                }
            }
        } else {
            serveCommand();
        }
    } break;
    default:
        /*do nothing but set timeout*/
        break;
    }

    int timeout = stateCheckTimeoutInterval(s);
    switch (timeout) {
    case -1:
        break;
    case 0:
        m_stateCheckTimer->stop();
        break;
    default:
        setStateCheckTimeout(timeout);
    }

    emit stateChanged(m_state);
}

int SerialEngine::stateCheckTimeoutInterval(State s)
{
    switch (s) {
    case Synchronizing: return 200;
    case Error: return 500;
    case PortError: return 500;
    /*echo timeout depends on current message length*/
    case WaitingEcho: return 10;
    /*reply timeout depends on expected reply length*/
    case WaitingReply: return 100;

    case Stopped:
    case Terminating:
        return 0;
    case Idle:
        return 50;
    case ReplyCooldown: return 10;
    default:
        return -1;
    }
}

void SerialEngine::slotOnReadyRead()
{
    char buf[64];
    int rd;

    while ((rd = m_port->read(buf, sizeof(buf))) > 0) {
        m_buffer.append(buf, rd);
    }
    if ((rd == -1) && (m_port->lastError() != E_PORT_TIMEOUT)) {
        setState(PortError);
        return;
    }
    if (!m_buffer.isEmpty()) {
        //add debug call here
        repackInput();
    }
}

void SerialEngine::inputBufferClear()
{
    m_buffer.clear();
}

void SerialEngine::inputBufferRemoveHead(int c)
{
    if (c == -1)
        return;
    m_buffer.remove(0, c);
}

void SerialEngine::inputBufferMoveHead(int size, EngineCommand *cmd)
{
///TODO: remove
    unsigned char rxsize = size;
    if (size >= sizeof(cmd->rxbuf)) {
        rxsize = sizeof(cmd->rxbuf);//saturate on overflow
    }
    if (!cmd->rxcount)
        cmd->rxcount = rxsize;
    cmd->rxrealcount = rxsize;
    memcpy(cmd->rxbuf, m_buffer.constData(), rxsize);
    inputBufferRemoveHead(size);
}


void SerialEngine::setStateCheckTimeout(int timeout)
{
    m_stateCheckTimer->stop();
    m_stateCheckTimer->setInterval(timeout);
    m_stateCheckTimer->start();
}

void SerialEngine::slotOnStateCheckTimeout()
{
    if (m_state == Terminating || m_state == Stopped)
        return;
/*try to read port, maybe we got data but there was lag*/
    char buf[64];
    int rd;
    bool newData = false;

    while ((rd = m_port->read(buf, sizeof(buf))) > 0) {
        m_buffer.append(buf, rd);
        newData = true;
    }
    if ((rd == -1) && (m_port->lastError() != E_PORT_TIMEOUT)) {
        setState(PortError);
        return;
    }
    if (newData) {
        State s = m_state;
        repackInput();
        if (m_state != s) {
#if 0
            Logger::message(Logger::EngineState, QString("Lag switch from %1 to %2")
                            .arg(enumValueToCharStar(this, "State", s))
                            .arg(enumValueToCharStar(this, "State", m_state)));
#endif
            return;
        }
    }

    Logger::message(Logger::EngineState, QString("State check timeout %1").arg(enumValueToCharStar(this, "State", m_state)));

    switch (m_state) {
    case Synchronizing:
        setState(Idle);
        break;
    case Error:
        /*check if line was silent for ? ms*/
        setState(Idle);
        break;
    case WaitingEcho:
        /*echo wait timeout: switch to error state*/
        executeAndClearCurrentCommand(CommandCallback::ResultNoEcho);
        setState(Error);
        break;
    case WaitingReply:
        executeAndClearCurrentCommand(CommandCallback::ResultTimeout);
        setState(Idle);
        break;
    case ReplyCooldown:
        /*serve next command if queue is not empty*/
        setState(Idle);
        break;
    case Idle:
        setState(Idle);
        /*we were idling cause had nothing to do*/
        break;
    default:
        Q_ASSERT(false);
    }
}

void SerialEngine::executeAndClearCurrentCommand(CommandCallback::Result res)
{
    if (m_currentCommand.txcount > 0 && m_currentCommand.rxcount > 0) {
        Logger::message(LineDataMessage(m_currentCommand, res));
    }
    m_currentCommand.executeCallback(res);
    m_currentCommand.clear();
}

SerialSensor *SerialEngine::sensorByAddress(uint8_t addr)
{
    for (int i = 0; i < m_sensors.count(); ++i) {
        SerialSensor *sen = m_sensors.at(i);
        if (sen->address() == addr)
            return sen;
    }
    return 0;
}

void SerialEngine::resetStateMachine()
{
    m_port->setFlowControl(FLOW_OFF);
    setEnginePortLocks();

    setState(Synchronizing);
}

void SerialEngine::addSensor(SerialSensor *sen)
{
    m_sensors.append(sen);
}

bool SerialEngine::isAddressBusy(uint8_t addr) const
{
    bool busy = false;

    for (int i = 0; i < m_sensors.count(); ++i) {
        if (m_sensors.at(i)->address() == addr) {
            busy = true;
            break;
        }
    }

    return busy;
}

void SerialEngine::removeSensor(SerialSensor *sen)
{
    m_sensors.removeAll(sen);
    clearCommandQueue();
    setState(Idle);
    resetDetectionState();
    afterRemovedSensor();
}

EngineCommand SerialEngine::pollNextSensorLevel()
{
    if (nextSensor() == -1)
        return EngineCommand();

    SerialSensor *sen = currentSensor();
    Q_ASSERT(sen);

    return sen->pollLevel();
//    sen->invokeCoolDown();
//    return true;
}

void SerialEngine::logAndClearStrayData(int size)
{
    if (size == -1)
        size = m_buffer.size();

    if (!size)
        return;

    Q_ASSERT(size > 0);

    EngineCommand cmd;
    inputBufferMoveHead(size, &cmd);
    Logger::message(LineDataMessage(cmd, CommandCallback::ResultStray));
}

void SerialEngine::setAutoDetectSensorsEnabled(bool v)
{
    Base::setAutoDetectSensorsEnabled(v);
    if (!isAutoDetectSensorsEnabled()) {
        m_detectionTurn = false;
    }
    if (!m_sensors.count() && v)
        resetStateMachine();
}

void SerialEngine::idleCheck()
{
    if (m_state != SerialEngine::Idle)
        return;

    setState(Idle);
}

bool SerialEngine::readyToPoll(SerialSensor *sen)
{
    return !sen->isInCoolDown();
}

void SerialEngine::advanceDetectionState()
{
    if (m_detectionState == DetectionStateOld)
        return;
    emit detectionStateChanged(++m_detectionState);
}

void SerialEngine::resetDetectionState()
{
    m_detectionState = DetectionStateNew;
    emit detectionStateChanged(m_detectionState);
}
