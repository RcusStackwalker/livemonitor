#include "caninterface.h"

#include <QTimer>
#include <QtEndian>

CanInterface *CanInterface::g_instance;

#include "j2534.h"
J2534 j2534;
unsigned long devID;
unsigned long chanID;

CanInterface::CanInterface(QObject *parent) :
    QObject(parent)
{
    Q_ASSERT(!g_instance);
    g_instance = this;

    m_timer = new QTimer(this);
    m_timer->setInterval(2);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotTimerTick()));

    j2534Init();

    m_timer->start();
}

void CanInterface::sendMessage(const CanMessage &msg)
{
    PASSTHRU_MSG raw;
    memset(&raw, 0, sizeof(raw));

    qToBigEndian<quint32>(msg.id(), raw.Data);
    memcpy(raw.Data + 4, msg.data(), msg.dlc());
    raw.ExtraDataIndex = 0;
    raw.DataSize = msg.dlc() + 4;
    raw.ProtocolID = CAN;
    raw.TxFlags = 0;

    unsigned long txMsgCount = 1;
    long ret = j2534.PassThruWriteMsgs(chanID, &raw, &txMsgCount, 0);
    if (ret) {
        qDebug("CAN Error");
    }
}


void CanInterface::slotTimerTick()
{

    do {
        QVector<CanMessage> messages;
        PASSTHRU_MSG buf[256];
        unsigned long count = 256;
        long ret = j2534.PassThruReadMsgs(chanID, buf, &count, 2);

        if ((ret == ERR_TIMEOUT) && (count < 256))
            ret = 0;

        if (ret) {
            return;
        }

        if (!count)
            return;

        messages.reserve(count);
        for (unsigned idx = 0; idx < count; ++idx) {
            const PASSTHRU_MSG &raw = buf[idx];
            if (raw.DataSize < 4)
                continue;
            if (raw.DataSize > 12)
                continue;
            if (raw.ProtocolID != CAN)
                continue;

            //TODO: somehow detect standard id messages

            CanMessage msg(qFromBigEndian<quint32>(raw.Data), raw.Data + 4, raw.DataSize - 4);
            messages.append(msg);
#if 0
            if ((msg.id() == 0x7e0) || (msg.id() == 0x7e8)) {
                QString str;
                for (int i = 0; i < msg.dlc(); ++i) {
                    str.append(' ');
                    str.append(QString::number(msg.data()[i], 16));
                }
                qDebug("%03x %s", msg.id(), qPrintable(str));
            }
#endif
        }
        qDebug("Received %d CAN messages", messages.count());


        emit receivedMessages(messages);
        if (count < 256)
            return;
    } while (1);
}

void CanInterface::j2534Init()
{
    if (!j2534.init())
        qFatal("can't connect to J2534 DLL.\n");
    if (j2534.PassThruOpen(NULL,&devID))
    {
        qFatal("Can't open passthru");
    }

    // use SNIFF_MODE to listen to packets without any acknowledgement or flow control
    // use CAN_ID_BOTH to pick up both 11 and 29 bit CAN messages
    if (j2534.PassThruConnect(devID,CAN,SNIFF_MODE|CAN_ID_BOTH,500000,&chanID))
    {
        qFatal("Can't connect passthru");
    }

    // now setup the filter(s)
    PASSTHRU_MSG txmsg;
    PASSTHRU_MSG msgMask,msgPattern;
    unsigned long msgId;

    // in the CAN case, we simply create a "pass all" filter so that we can see
    // everything unfiltered in the raw stream

    txmsg.ProtocolID = CAN;
    txmsg.RxStatus = 0;
    txmsg.TxFlags = ISO15765_FRAME_PAD;
    txmsg.Timestamp = 0;
    txmsg.DataSize = 4;
    txmsg.ExtraDataIndex = 0;
    msgMask = msgPattern  = txmsg;
    memset(msgMask.Data,0,4); // mask the first 4 byte to 0
    memset(msgPattern.Data,0,4);// match it with 0 (i.e. pass everything)
    if (j2534.PassThruStartMsgFilter(chanID,PASS_FILTER,&msgMask,&msgPattern,NULL,&msgId))
    {
        qDebug("Can't set filter");
    }
}


CanMessage::CanMessage(unsigned id, const uint8_t *data, uint8_t dlc)
    : m_id(id), m_dlc(dlc)
{
    memcpy(m_data, data, 8);
}

CanMessage::CanMessage()
{

}
