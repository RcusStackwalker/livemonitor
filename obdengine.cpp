#include "obdengine.h"

#include "caninterface.h"

#include "logger.h"

#include <QtEndian>

ObdEngine::ObdEngine(unsigned id, QObject *parent) :
    CommandCanEngine(parent)
{
    idleCallback = 0;
    m_sid = id;
}


void ObdEngine::serveCommand()
{
    if (m_commandQueue.isEmpty()) {
        Logger::error("Unexpected empty commandQueue");
        return;
    }

    ObdEngineCommand ec = m_commandQueue.dequeue();
    m_currentCommand = ec;
    if (!ec.silent) {
        Logger::message(metaObject()->className(), QString::fromLatin1("> %1").arg(bytesToString(ec.txbuf, ec.txcount)));
    }

    if (ec.txcount <= 7) {
        uint8_t data[8];
        data[0] = ec.txcount;
        memcpy(data + 1, ec.txbuf, ec.txcount);
        memset(data + ec.txcount + 1, 0xff, 7 - ec.txcount);
        CanInterface::instance()->sendMessage(CanMessage(m_sid, data, sizeof(data)));
        setState(WaitingReply);
        return;
    }

    uint8_t data[8];
    data[0] = 0x10 | (ec.txcount >> 8);
    data[1] = ec.txcount;
    memcpy(data + 2, ec.txbuf, 6);
    m_multipacketTxIndex = 6;
    m_multipacketTxSequence = 1;
    CanInterface::instance()->sendMessage(CanMessage(m_sid, data, sizeof(data)));
    setState(WaitingForMultipacketTransmissionFlowControl);
}

bool ObdEngine::idleWork()
{
    uint8_t data[2] = { ServiceTesterPresent, 1 };
    ObdEngineCommand ec(data, sizeof(data));
    ec.callback = idleCallback;
    ec.silent = true;
    request(ec);
    return true;
}

void ObdEngine::decodeReplyErrorCodeAndCallback()
{
    if (m_currentCommand.rxbuf[0] == m_currentCommand.txbuf[0] + 0x40) {
        executeAndClearCurrentCommand(CommandCallback::ResultOk);
    } else if ((m_currentCommand.rxbuf[0] == 0x7f)
               && (m_currentCommand.rxbuf[1] == m_currentCommand.txbuf[0])
               && (m_currentCommand.rxbuf[2] == 120)) {
        qDebug("Device has to check something, keep connection open");
        setState(WaitingReply);
        setStateCheckTimeout(10000);
        return;
    } else {
        qDebug("Command %02x Failed with erorr code %u", m_currentCommand.rxbuf[1], m_currentCommand.rxbuf[2]);
        executeAndClearCurrentCommand(CommandCallback::ResultNack);
    }
    setState(ReplyCooldown);
}

void ObdEngine::slotOnNewCanMessage(const QVector<CanMessage> &messages)
{
    foreach (const CanMessage &msg, messages) {
        if (msg.id() != (m_sid + 8))
            continue;
        //Q_ASSERT(false);
        //TODO: reply
        uint8_t d0 = msg.data()[0];
        switch (state()) {
        case WaitingReply:
            switch (d0 & 0xf0) {
            case 0x00: //single frame
                //single frame reply
                m_currentCommand.rxrealcount = d0;
                memcpy(m_currentCommand.rxbuf, msg.data() + 1, d0);
                decodeReplyErrorCodeAndCallback();
                break;
            case 0x10: { //first frame
                /**/
                m_currentCommand.rxrealcount = qFromBigEndian<uint16_t>(msg.data()) & 0x0fff;
                memcpy(m_currentCommand.rxbuf, msg.data() + 2, 6);
                m_multipacketRxIndex = 6;
                m_multipacketRxSequence = 1;
                uint8_t flow_data[8] = { 0x30, 0, 0, 0xff, 0xff, 0xff, 0xff, 0xff };
                CanMessage msg(m_sid, flow_data, 8);
                CanInterface::instance()->sendMessage(msg);
                setState(WaitingReply);
            } break;
            case 0x20: //consecutive frame
                /**/
                //check frame sequence
                if (m_multipacketRxSequence != (d0 & 0x0f)) {
                    setState(Error);
                    continue;
                }
                if (257 - m_multipacketRxIndex < 7) {
                    for (unsigned i = 1; m_multipacketRxIndex < 257; ++i, ++m_multipacketRxIndex)
                        m_currentCommand.rxbuf[m_multipacketRxIndex] = msg.data()[i];
                } else {
                    memcpy(m_currentCommand.rxbuf + m_multipacketRxIndex, msg.data() + 1, 7);
                    m_multipacketRxIndex += 7;
                }
                if (m_multipacketRxIndex >= m_currentCommand.rxrealcount) {
                    decodeReplyErrorCodeAndCallback();
                } else {
                    m_multipacketRxSequence = (m_multipacketRxSequence + 1) % 16;
                    setState(WaitingReply);
                }
                break;
            default:
                executeAndClearCurrentCommand(CommandCallback::ResultStray);
                break;
            }

            break;
        case WaitingForMultipacketTransmissionFlowControl:
            if (d0 == 0x30) {//flow control
                multipacketTranmission();
                break;
            } //else fall through
        default:
            ;
            //log active
            qDebug("CdbgEngine got unexpected message %s", qPrintable(bytesToString(msg.data(), 8)));
        }

    }
}

void ObdEngine::multipacketTranmission()
{
    for (int i = 0; i < 16; ++i) {
        uint8_t data[8];
        data[0] = 0x20 | m_multipacketTxSequence;
        memcpy(data + 1, m_currentCommand.txbuf + m_multipacketTxIndex, 7);
        CanInterface::instance()->sendMessage(CanMessage(m_sid, data, sizeof(data)));

        m_multipacketTxSequence = (m_multipacketTxSequence + 1) % 16;
        m_multipacketTxIndex += 7;
        if (m_multipacketTxIndex >= m_currentCommand.txcount)
            break;
    }
    if (m_multipacketTxIndex < m_currentCommand.txcount) {
        setState(MultipacketTransmission);
    } else {
        setState(WaitingReply);
    }
}

template EngineCommandSequence<ObdEngineCommand>::~EngineCommandSequence();
template void EngineCommandSequence<ObdEngineCommand>::sequenceCallback(CommandCallback::Result ret, void *_cmd);

#include <QThread>
class ObdSessionInitCommandSequence : public ObdCommandSequence
{
public:
    ObdSessionInitCommandSequence(ObdEngine::SessionId id, ObdEngine *eng, CommandCallback *cb) : ObdCommandSequence(eng, cb) {
        ObdEngineCommand ec;
        ec.txcount = 2;
        ec.txbuf[0] = ObdEngine::ServiceDiagnosticSession;
        ec.txbuf[1] = id;
        append(ec);
        if (0 || (id == ObdEngine::SessionBootload)) {
            ObdEngineCommand ec2;
            ec2.txbuf[0] = ObdEngine::ServiceSecurityAccess;
            ec2.txbuf[1] = 5;//for reflash
            //ec2.txbuf[1] = 1;
            ec2.txcount = 2;
            append(ec2);
        }
    }
protected:
    virtual void messageCallbackCustomAction(unsigned idx, void *_cmd) {
        if (idx != 1)
            return;
        ObdEngineCommand *cmd = reinterpret_cast<ObdEngineCommand*>(_cmd);
        uint16_t pk1 = qFromBigEndian<uint16_t>(cmd->rxbuf + 2);
        uint16_t pk2 = qFromBigEndian<uint16_t>(cmd->rxbuf + 4);
        ObdEngineCommand ec;
        memset(ec.txbuf, 0, sizeof(ec.txbuf));
        ec.txbuf[0] = ObdEngine::ServiceSecurityAccess;
        ec.txbuf[1] = 6;//reflash security answer
        uint16_t sk1 = pk1 * 135 + 1542;
        uint16_t sk2 = pk2 * 135 + 1542;
        //qDebug("PK %04x, %04x. SK %04x, %04x", pk1, pk2, sk1, sk2);
        qToBigEndian<uint16_t>(sk1, ec.txbuf + 2);
        qToBigEndian<uint16_t>(sk2, ec.txbuf + 4);
        ec.txcount = 6;
        append(ec);
    }
};

static uint16_t get_crc(const uint8_t *ptr, unsigned size)
{
    uint16_t crc = 0;
    Q_ASSERT(!(size % 2));
    while (size--) {
        crc += *ptr++;
    }
    return crc;
}

class ObdFlashWriteCommandSequence : public ObdCommandSequence
{
public:
    ObdFlashWriteCommandSequence(void *ptr, unsigned start, unsigned size, ObdEngine *eng, CommandCallback *cb) : ObdCommandSequence(eng, cb) {
        Q_ASSERT(ObdEngine::CrcTransferSize == 2);

        append(getRequestDownloadCommand(start, size));
        appendDataTransferCommands(ptr, size);
        append(getRequestDownloadCommand(ObdEngine::CrcTransferAddress, ObdEngine::CrcTransferSize));
        uint16_t crc = qToBigEndian<uint16_t>(get_crc((const uint8_t*)ptr, size));
        appendDataTransferCommands(&crc, sizeof(crc));
        ObdEngineCommand cmd;
        cmd.txcount = 3;
        cmd.txbuf[0] = ObdEngine::ServiceRoutineControl;
        cmd.txbuf[1] = ObdEngine::RoutineCheckCrc;
        cmd.txbuf[2] = start < 0x800000 ? 2 : 1; //flash/memory fun
        append(cmd);
    }
protected:
    void appendDataTransferCommands(void *ptr, unsigned size) {
        unsigned dataPackets = (size + 255) / 256;
        for (unsigned i = 0; i < dataPackets; ++i) {
            ObdEngineCommand dataec;
            unsigned dataSize = (i == dataPackets - 1) && (size % 256) ? size % 256 : 256;
            dataec.txcount = dataSize + 1;
            dataec.txbuf[0] = ObdEngine::ServiceTransferData;
            memcpy(dataec.txbuf + 1, (uint8_t*)ptr + i * 256, dataSize);
            append(dataec);
        }
    }

    ObdEngineCommand getRequestDownloadCommand(unsigned start, unsigned size) {
        ObdEngineCommand ec;
        ec.txcount = 8;
        ec.txbuf[0] = ObdEngine::ServiceRequestDownload;
        ec.txbuf[1] = start >> 16;
        ec.txbuf[2] = start >> 8;
        ec.txbuf[3] = start;
        ec.txbuf[4] = 0;
        ec.txbuf[5] = size >> 16;
        ec.txbuf[6] = size >> 8;
        ec.txbuf[7] = size;
        return ec;
    }
};



ObdCommandSequence *ObdEngine::getSessionInitCommandSequence(ObdEngine::SessionId id, CommandCallback *callback)
{
    return new ObdSessionInitCommandSequence(id, this, callback);
}

ObdCommandSequence *ObdEngine::getFlashWriteCommandSequence(void *ptr, unsigned start, unsigned size, CommandCallback *cb)
{
    return new ObdFlashWriteCommandSequence(ptr, start, size, this, cb);
}
