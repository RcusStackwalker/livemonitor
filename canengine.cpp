#include "canengine.h"

#include "logger.h"

CanEngine::CanEngine(QObject *parent) :
    QObject(parent)
  , m_state(Idle)
  , m_stateCheckTimer(new QTimer(this))
{
    m_stateCheckTimer->setSingleShot(true);
    connect(m_stateCheckTimer, SIGNAL(timeout()), SLOT(slotOnStateCheckTimeout()));
    connect(CanInterface::instance(), SIGNAL(receivedMessages(QVector<CanMessage>)), SLOT(slotOnNewCanMessage(QVector<CanMessage>)));

}

void CanEngine::slotOnStateCheckTimeout()
{
    if (m_state == Terminating || m_state == Stopped)
        return;

    Logger::message(Logger::EngineState, QString("State check timeout %1").arg(enumValueToCharStar(this, "State", m_state)));

    switch (m_state) {
    case Error:
        /*check if line was silent for ? ms*/
        setState(Idle);
        break;
    case WaitingForMultipacketTransmissionFlowControl:
    case WaitingReply:
        executeAndClearCurrentCommand(CommandCallback::ResultTimeout);
        setState(Idle);
        break;
    case ReplyCooldown:
        /*serve next command if queue is not empty*/
        setState(Idle);
        break;
    case Idle:
        if (!idleWork())
            setState(Idle);
        /*we were idling cause had nothing to do*/
        break;
    case MultipacketTransmission:
        multipacketTranmission();
        break;
    default:
        qDebug("Timed out when not expected %u", m_state);
        Q_ASSERT(false);
    }
}

void CanEngine::setState(CanEngine::State s)
{
    m_state = s;
    /*timeouts are engine-dependant*/

    Logger::message(Logger::EngineState,QString("Switcing to state %1").arg(enumValueToCharStar(this, "State", s)));

    switch (s) {
    case Stopped:
        clearCommandQueue();
        break;
    case Idle:
        if (!isCommandQueueEmpty()) {
            serveCommand();
        }
        break;
    default:
    case Error:
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



void CanEngine::setStateCheckTimeout(int timeout)
{
    m_stateCheckTimer->stop();
    m_stateCheckTimer->setInterval(timeout);
    m_stateCheckTimer->start();
}

int CanEngine::stateCheckTimeoutInterval(State s)
{
    switch (s) {
    case Error: return 500;
    case PortError: return 500;
    /*echo timeout depends on current message length*/
    /*reply timeout depends on expected reply length*/
    case WaitingForMultipacketTransmissionFlowControl: return 100;
    case WaitingReply: return 250;
    case MultipacketTransmission: return 1;
    case Stopped:
    case Terminating:
        return 0;
    case Idle:
        return 500;
    case ReplyCooldown: return 1;
    default:
        return -1;
    }
}

