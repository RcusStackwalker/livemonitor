#include "cdbgengine.h"

#include <QtEndian>

CdbgEngine::CdbgEngine(QObject *parent) :
    CommandCanEngine(parent)
{
    m_sid = 0x630;
}

void CdbgEngine::serveCommand()
{
    if (m_commandQueue.isEmpty()) {
        Logger::error("Unexpected empty commandQueue");
        return;
    }

    EngineCommand ec = m_commandQueue.dequeue();
    Logger::message(Logger::EngineState, QString("Serving command %1").arg(qPrintable(ec.toString())));
    Logger::message(metaObject()->className(), QString::fromLatin1("> %1").arg(bytesToString(ec.txbuf, ec.txcount)));

    CanMessage msg(m_sid, ec.txbuf, 8);
    CanInterface::instance()->sendMessage(msg);
    /*
    if (ret < 0) {
        Logger::error("Port write failure");
        setState(PortError);
        return;
    }
    */

    setState(WaitingReply);
    m_currentCommand = ec;
    /*somehow register ec.callback to call in case of timeout/reply*/
}

void CdbgEngine::deinit()
{
    /*clear command queue, stop timers, switch to Terminating state, delete sensors*/
    executeAndClearCurrentCommand(CommandCallback::ResultCanceled);
    while (!m_commandQueue.isEmpty()) {
        EngineCommand ec = m_commandQueue.dequeue();
        ec.executeCallback(CommandCallback::ResultCanceled);
    }
    setState(Terminating);

}



void CdbgEngine::slotOnNewCanMessage(const QVector<CanMessage> &messages)
{
    foreach (const CanMessage &msg, messages) {
        if (msg.id() != (m_sid + 1))
            continue;
        switch (state()) {
        case WaitingReply:
            memcpy(m_currentCommand.rxbuf, msg.data(), msg.dlc());
            m_currentCommand.rxrealcount = msg.dlc();
            executeAndClearCurrentCommand(CommandCallback::ResultOk);
            setState(ReplyCooldown);
            break;
        default:
            ;
            //log active
            //qDebug("CdbgEngine got unexpected message");
        }

    }
}







QVector<EngineCommand> CdbgEngine::getLogFrameInitCommands(unsigned instance, unsigned idx, const QVector<CdbgLogItemDescription> &items)
{
    QVector<EngineCommand> cmds;
    unsigned totalSize = 0;
    Q_ASSERT(items.count() <= 7);
    for (int i = 0; i < items.count(); ++i) {
        const CdbgLogItemDescription &desc = items[i];
        Q_ASSERT((desc.size == 1) || (desc.size == 2) || (desc.size == 4));
        totalSize += desc.size;
        EngineCommand cmd;
        cmd.txcount = 8;
        cmd.txbuf[0] = 21;
        cmd.txbuf[1] = 0;
        cmd.txbuf[2] = instance;
        cmd.txbuf[3] = idx;
        cmd.txbuf[4] = i;
        cmds.append(cmd);
        EngineCommand cmd2;
        cmd2.txcount = 8;
        cmd2.txbuf[0] = 22;
        cmd2.txbuf[1] = 0;
        cmd2.txbuf[2] = items[i].size;
        cmd2.txbuf[3] = 0;
        qToBigEndian<uint32_t>(items[i].pointer, cmd2.txbuf + 4);
        cmds.append(cmd2);
    }
    Q_ASSERT(totalSize <= 7);
    return cmds;
}

EngineCommand CdbgEngine::getLogInstanceResetCommand(unsigned instance)
{
    uint8_t data[] = { 20, 0, (uint8_t)instance, 0, 0, 0, 0x06, 0x31 };
    EngineCommand ec;
    ec.txcount = sizeof(data);
    memcpy(ec.txbuf, data, sizeof(data));
    return ec;
}

EngineCommand CdbgEngine::getLogStartCommand(unsigned instance, unsigned frameCount, unsigned interval)
{
    Q_ASSERT(frameCount > 0 && frameCount <= 8);
    Q_ASSERT(interval <= 65535 * 10);
    uint8_t data[] = { 6, 0, 1, (uint8_t)instance, (uint8_t)frameCount, 0, 0, 0 };
    if (interval > 65535) {
        data[5] = 1;
        interval /= 10;
    } else {
        data[5] = 0;
    }
    qToBigEndian<uint16_t>(interval, data + 6);
    EngineCommand ec;
    ec.txcount = 8;
    memcpy(ec.txbuf, data, 8);
    return ec;
}

uint32_t seed_to_key(uint32_t seed)
{
    uint8_t data[4];
    uint8_t data_new[4];
    uint16_t word0, word1;
    int i;
    qToBigEndian<uint32_t>(seed, data);

    for (i = 0; i < 4; ++i) {
        switch (data[i] & 0x03) {
        case 0: data[i] += 145; break;
        case 1: data[i] += 24; break;
        case 2: data[i] += 211; break;
        case 3: data[i] += 2; break;
        }
        data[i] = (data[i] << 3) + (data[i] >> 5);
    }
    switch ((data[0] & 0x01) + (data[1] & 0x01) + (data[2] & 0x01) + (data[3] & 0x01)) {
    case 0:
        data_new[0] = data[1];
        data_new[1] = data[3];
        data_new[2] = data[2];
        data_new[3] = data[0];
        break;
    case 1:
        data_new[0] = data[3];
        data_new[1] = data[2];
        data_new[2] = data[0];
        data_new[3] = data[1];
        break;
    case 2:
        data_new[0] = data[1];
        data_new[1] = data[2];
        data_new[2] = data[3];
        data_new[3] = data[0];
        break;
    case 3:
        data_new[0] = data[1];
        data_new[1] = data[0];
        data_new[2] = data[2];
        data_new[3] = data[3];
        break;
    default:
        data_new[0] = data[2];
        data_new[1] = data[0];
        data_new[2] = data[1];
        data_new[3] = data[3];
        break;
    }

    word0 = ((data_new[0] << 8) + data_new[1]) * 3 + data_new[3]  * 8;
    word1 = ((data_new[2] << 8) + data_new[3]) * 5 + data_new[1] * 8;
    data[0] = word0 >> 8;
    data[1] = word0 & 0xff;
    data[2] = word1 >> 8;
    data[3] = word1 & 0xff;
    return qFromBigEndian<uint32_t>(data);
}

/*
void MainWindow::cdbgSecuritySeedCallback(CommandCallback::Result ret, const EngineCommand &cmd)
{
    if (ret != CommandCallback::ResultOk) {
        qDebug("Lost command");
        return;
    }

}
*/

template void EngineCommandSequence<EngineCommand>::sequenceCallback(CommandCallback::Result ret, void *_cmd);

class SessionInitCommandSequence : public CdbgCommandSequence
{
public:
    SessionInitCommandSequence(CdbgEngine *eng, CommandCallback *cb) : EngineCommandSequence(eng, cb) {
        EngineCommand ec;
        ec.txcount = 8;
        ec.txbuf[0] = 1;
        ec.txbuf[1] = 1;
        append(ec);
        EngineCommand ec2;
        ec2.txbuf[0] = 18;
        ec2.txbuf[1] = 0;
        ec2.txbuf[2] = 2;//log access
        ec2.txcount = 8;
        append(ec2);
    }
protected:
    virtual void messageCallbackCustomAction(unsigned idx, void *_cmd) {
        if (idx != 1)
            return;
        EngineCommand *cmd = reinterpret_cast<EngineCommand*>(_cmd);
        uint32_t seed = qFromBigEndian<uint32_t>(cmd->rxbuf + 4);
        EngineCommand ec;
        memset(ec.txbuf, 0, sizeof(ec.txbuf));
        ec.txbuf[0] = 19;
        ec.txbuf[1] = 0;
        qToBigEndian<uint32_t>(seed_to_key(seed), ec.txbuf + 2);
        ec.txcount = 8;
        append(ec);
    }
};

CdbgCommandSequence *CdbgEngine::getSessionInitCommandSequence(CommandCallback *callback)
{
    return new SessionInitCommandSequence(this, callback);
}
