#ifndef SERIALENGINE_H
#define SERIALENGINE_H

#include "baseengine.h"

#include <QQueue>
#include <QVector>
#include <QTime>

#include "logger.h"

#include <serialport.h>
//class SerialPort;
class SerialSensor;
class QTimer;
class QDialog;

class SerialEngine : public BaseEngine
{
    typedef BaseEngine Base;
    Q_OBJECT
    Q_ENUMS(State)
public:
    virtual ~SerialEngine();
    enum State {
        Synchronizing,
        Idle,
        WaitingEcho,
        WaitingReply,
        ReplyCooldown,
        Error,
        PortError,
        Stopped,
        Terminating,
    };
    enum DetectionState {
        DetectionStateNew,
        DetectionStateNormal,
        DetectionStateOld
    };

    SerialPort *port() { return m_port; }
    virtual void setPort(SerialPort *port);
    virtual void request(const EngineCommand &command);

    virtual void deinit();
    virtual void resetStateMachine();

    virtual QDialog *addSensorDialog() = 0;

    void addSensor(SerialSensor *sen);
    void removeSensor(SerialSensor *sen);

    bool isAddressBusy(uint8_t addr) const;

    virtual void setAutoDetectSensorsEnabled(bool v);

    void idleCheck();
    State state() const { return m_state; }

    DetectionState detectionState() const { return static_cast<DetectionState>(m_detectionState); }

    virtual QString detectionStateString() const = 0;
signals:
    void stateChanged(SerialEngine::State s);
    void detectionStateChanged(int s);
protected slots:
    void slotOnReadyRead();
    void slotOnStateCheckTimeout();
protected:
    SerialEngine(unsigned _features, QObject *parent = 0);
    void advanceDetectionState();
    void resetDetectionState();
    bool m_echoEnabled;
    bool m_detectionTurn;
    State m_state;
    int m_detectionState;

    SerialPort *m_port;

    QQueue<EngineCommand> m_commandQueue;
    EngineCommand m_currentCommand;
    QList<SerialSensor*> m_sensors;

    SerialSensor *sensorByAddress(uint8_t addr);
    int m_currentSensor;
    SerialSensor *currentSensor();
    int nextSensor();
    virtual void repackInput() = 0;

    virtual void setEnginePortLocks() = 0;
    virtual void afterRemovedSensor() {}
    void setState(State s);

    virtual void serveCommand();

    QByteArray m_buffer;
    void inputBufferClear();
    void inputBufferRemoveHead(int c);
    void inputBufferMoveHead(int size, EngineCommand *cmd);

    QTimer *m_stateCheckTimer;

    void setStateCheckTimeout(int timeout);

    void executeAndClearCurrentCommand(CommandCallback::Result res);

    virtual int stateCheckTimeoutInterval(State s);

    EngineCommand pollNextSensorLevel();
    virtual EngineCommand detectNext() = 0;

    void logAndClearStrayData(int size = -1);

    void clearCommandQueue();

    virtual bool readyToPoll(SerialSensor *sen);

    int help_nextSensor();

    friend class SerialPortManager;
};
#endif /*SERIALENGINE_H*/
