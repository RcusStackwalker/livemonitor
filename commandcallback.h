#ifndef COMMANDCALLBACK_H
#define COMMANDCALLBACK_H

class EngineCommand;
class QString;

class CommandCallback
{
public:
    enum Result {
        ResultOk,
        ResultTimeout,
        ResultFailed,
        ResultCrcFail,
        ResultCanceled,
        ResultNack,
        ResultStray,
        ResultNoEcho,
        ResultEchoMismatch
    };
    virtual ~CommandCallback();
    virtual void callback(Result ret, void *cmd) = 0;
};

template <class T> class MemFunCommandCallback : public CommandCallback
{
public:
    typedef void (T:: *CallbackFun)(CommandCallback::Result, void *);
    MemFunCommandCallback(T *sen, CallbackFun fun) : m_o(sen), m_function(fun) {}
    virtual void callback(Result ret, void *cmd) {
        (m_o->*m_function)(ret, cmd);
    }
protected:
    T *m_o;
    CallbackFun m_function;
};

QString callbackResultToString(CommandCallback::Result result);

#endif /*COMMANDCALLBACK_H*/
