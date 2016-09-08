#ifndef CANENGINE_H
#define CANENGINE_H

#include <QObject>
#include <QTimer>
#include <QQueue>

#include "logger.h"
#include "commandcallback.h"
#include "caninterface.h"

class CanEngine : public QObject
{
    Q_OBJECT
public:
    explicit CanEngine(QObject *parent = 0);
    enum State {
        Idle,
        WaitingReply,
        WaitingForMultipacketTransmissionFlowControl,
        MultipacketTransmission,
        ReplyCooldown,
        Error,
        PortError,
        Stopped,
        Terminating,
    };

    State state() const { return m_state; }

    unsigned sid() const { return m_sid; }
    void setSid(unsigned s) { m_sid = s; }
signals:
    void stateChanged(CanEngine::State s);

public slots:
protected slots:
    virtual void slotOnNewCanMessage(const QVector<CanMessage> &messages) = 0;
    void slotOnStateCheckTimeout();
protected:
    State m_state;
    QTimer *m_stateCheckTimer;
    unsigned m_sid;

    virtual void multipacketTranmission() {}
    virtual void serveCommand() = 0;
    virtual void executeAndClearCurrentCommand(CommandCallback::Result res) = 0;
    virtual void clearCommandQueue() = 0;
    virtual bool isCommandQueueEmpty() = 0;
    void setState(State s);
    void setStateCheckTimeout(int timeout);
    int stateCheckTimeoutInterval(State s);
    virtual bool idleWork() { return false; }
};

template <class T> class CommandCanEngine : public CanEngine
{
public:
    explicit CommandCanEngine(QObject *parent = 0) : CanEngine(parent) {}

    void request(const T &cb)
    {
        Q_ASSERT(cb.rxcount <= sizeof(cb.rxbuf));
        Q_ASSERT(cb.txcount <= sizeof(cb.txbuf));
        //se_debug("Outstanding request: %s", qPrintable(cb.toString()));
        m_commandQueue.enqueue(cb);

        if (m_state == Idle) {
            serveCommand();
        }
    }
protected:
    QQueue<T> m_commandQueue;
    T m_currentCommand;

    virtual void executeAndClearCurrentCommand(CommandCallback::Result res)
    {
        if (m_currentCommand.silent) {}
        else if (res == CommandCallback::ResultOk) {
            Logger::message(metaObject()->className(), QString::fromLatin1("< %1").arg(bytesToString(m_currentCommand.rxbuf, m_currentCommand.rxrealcount)));
        } else {
            Logger::message(metaObject()->className(), QString::fromLatin1("< error %1 for message %2").arg(res).arg(bytesToString(m_currentCommand.txbuf, m_currentCommand.txcount)));
        }
        if (!m_currentCommand.txcount) {
            qDebug("Weird command huh");
        }
        m_currentCommand.executeCallback(res);
        m_currentCommand.clear();
    }

    void clearCommandQueue()
    {
        executeAndClearCurrentCommand(CommandCallback::ResultFailed);
        while (!m_commandQueue.isEmpty()) {
            T ec = m_commandQueue.dequeue();
            ec.executeCallback(CommandCallback::ResultCanceled);
        }
    }
    virtual bool isCommandQueueEmpty() { return m_commandQueue.isEmpty(); }
};

#endif // CANENGINE_H
