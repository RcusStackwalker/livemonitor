#ifndef ENGINECOMMANDSEQUENCE_H
#define ENGINECOMMANDSEQUENCE_H

#include "canengine.h"


template <class T> class EngineCommandSequence
{
public:
    EngineCommandSequence(CommandCanEngine<T> *engine, CommandCallback *callback)
        : m_sequenceCallback(this, &EngineCommandSequence<T>::sequenceCallback)
        , m_callback(callback)
        , m_engine(engine)
    {

    }
    virtual ~EngineCommandSequence()
    {

    }

    void append(const T &cmd) {
        m_sequence.append(cmd);
        m_sequence.last().callback = &m_sequenceCallback;
        m_sequence.last().callbackArg = (void*)(m_sequence.count() - 1);
    }

    void execute() {
        m_currentIndex = 0;
        m_engine->request(m_sequence.at(0));
    }
    unsigned size() { return m_sequence.size(); }
    unsigned currentIndex() { return m_currentIndex; }

    static CommandCallback *destroyingCallback();
protected:
    virtual void messageCallbackCustomAction(unsigned idx, void *_cmd) { Q_UNUSED(idx); Q_UNUSED(_cmd); }

    QVector<T> m_sequence;
    void sequenceCallback(CommandCallback::Result ret, void *_cmd);

    MemFunCommandCallback<EngineCommandSequence<T> > m_sequenceCallback;
    CommandCallback *m_callback;
    CommandCanEngine<T> *m_engine;
    unsigned m_currentIndex;
};

template <class T> class SequenceDestroyingCallback : public CommandCallback
{
    void callback(Result ret, void *_cmd) {
        Q_UNUSED(ret);
        T *cmd = reinterpret_cast<T*>(_cmd);
        delete cmd;
    }
};

template <class T>
CommandCallback *EngineCommandSequence<T>::destroyingCallback()
{
    static CommandCallback *cb = 0;
    if (!cb) {
        cb = new SequenceDestroyingCallback<EngineCommandSequence<T> >();
    }
    return cb;
}

template <class T> void EngineCommandSequence<T>::sequenceCallback(CommandCallback::Result ret, void *_cmd)
{
    Q_ASSERT(this);
    T *cmd = reinterpret_cast<T*>(_cmd);
    if (ret != CommandCallback::ResultOk) {
        if (m_callback)
            m_callback->callback(ret, this);
        return;//we may have no this already
    }
    int seqid = (int)cmd->callbackArg;
    //qDebug("%d: replied %u", seqid, cmd->rxbuf[1]);
    messageCallbackCustomAction(seqid, cmd);
    if (seqid == m_sequence.count() - 1) {
        if (m_callback)
            m_callback->callback(ret, this);
    } else {
        m_currentIndex = seqid + 1;
        m_engine->request(m_sequence.at(seqid + 1));
    }
}

#endif // ENGINECOMMANDSEQUENCE_H
