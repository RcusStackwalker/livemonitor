#ifndef CANINTERFACE_H
#define CANINTERFACE_H

#include <QObject>
#include <QVector>

#include <stdint.h>

class QTimer;

class CanMessage
{
public:
    CanMessage();
    CanMessage(unsigned id, const uint8_t *data, uint8_t dlc);
    unsigned id() const { return m_id; }
    const uint8_t *data() const { return m_data; }
    uint8_t dlc() const { return m_dlc; }
protected:
    unsigned m_id;
    uint8_t m_dlc;
    uint8_t m_data[8];
};

class CanInterface : public QObject
{
    Q_OBJECT
public:
    explicit CanInterface(QObject *parent = 0);
    
    static CanInterface *instance() { return g_instance; }

    void sendMessage(const CanMessage &msg);
signals:
    void receivedMessages(const QVector<CanMessage> &messages);
protected slots:
    void slotTimerTick();
protected:
    void j2534Init();

    static CanInterface *g_instance;
    QTimer *m_timer;
};

#endif // CANINTERFACE_H
