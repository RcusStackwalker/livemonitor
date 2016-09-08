#ifndef DMAPROTOCOL_H
#define DMAPROTOCOL_H

#include <QObject>

class DMARequestCallback
{
public:
    enum Status {
        Ok,
        Timeout,
        InvalidArgument,
        GenericError
    };

    virtual ~DMARequestCallback();
    virtual void callback(Status status) = 0;
};

class DMAReadRequestCallback : public DMARequestCallback
{
public:
    QByteArray data() const { return m_data; }
protected:
    QByteArray m_data;
};

class DMAProtocol : public QObject
{
    Q_OBJECT
public:
    enum State {
        Idle,
        Initializing,
        Ok,
        Error
    };
    enum Mode {
        Mode_U16_X24,
        Mode_U8_X24,
        Mode_U16_X5,
        Mode_FreeLog_Main,
        Mode_FreeLog_Fast,
        Mode_FreeLog_FastX2,
        Mode_FreeLog_Shaft,
        Mode_2FU16_2FU8_2VU8,
        Mode_2FU16_3VU16,
        Mode_
    };

    explicit DMAProtocol(QObject *parent = 0);
    
    void open(Mode mode);
    void close();
    State state() const { return m_state; }

    ///transfers ownership of request callback to DMAProtocol object
    bool writeRequest(unsigned address, void *data, unsigned size, DMARequestCallback *cb);
    bool readRequest(unsigned address, unsigned size, DMAReadRequestCallback *cb);
signals:
    void connected();
    void disconnected();
public slots:
protected:
    State m_state;
};

#endif // DMAPROTOCOL_H
