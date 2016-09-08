#include "logitemsmodel.h"

#include <QFile>
#include <QDateTime>
#include <QDomDocument>
#include <QTextStream>
#include <QRegularExpression>

#include <QtEndian>

#include "cdbgengine.h"

#define MASK_FROM_LENGTH(L) (L == 0 ? 0 : (L == 32 ? 0xffffffff : ((1 << (L))- 1)))

qint64 logStartTime;

LogItemsModel::LogItemsModel(QObject *parent) :
    QAbstractTableModel(parent)
  , m_sessionInitCallback(this, &LogItemsModel::sessionInitCallback)
{
    logStartTime = QDateTime::currentMSecsSinceEpoch();
    m_cdbg = new CdbgEngine(this);
    //m_cdbg->setSid(0x700);
    m_timer.setInterval(10);
    connect(&m_timer, SIGNAL(timeout()), SLOT(slotTimerTimeout()));
    connect(CanInterface::instance(), SIGNAL(receivedMessages(QVector<CanMessage>)), SLOT(slotReceivedMessages(QVector<CanMessage>)));
}

int LogItemsModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return SectionCount;
}

int LogItemsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_logItems.count();
}

QVariant LogItemsModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (0 > index.row() || index.row() >= m_logItems.count())
        return QVariant();


    const LogItem &item = m_logItems.at(index.row());

    switch (index.column()) {
    case Name:
        return item.name;
    case Source:
        return item.source;
    case Value:
        return item.value;
    case Suffix:
        return item.suffix;
    default:
        return QVariant();
    }
}

QVariant LogItemsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case Name:
        return tr("Name");
    case Source:
        return tr("Source");
    case Value:
        return tr("Value");
    case Suffix:
        return tr("Units");
    default:
        return tr("TODO");
    }
}

void LogItemsModel::loadSchema(const QString &fname)
{
    QDomDocument doc;
    QFile f(fname);
    f.open(QFile::ReadOnly | QFile::Text);
    QString errMsg;
    int errLine, errColumn;
    doc.setContent(&f, &errMsg, &errLine, &errColumn);
    if (!errMsg.isEmpty()) {
        qFatal("XML Fault: %s [l:%d,c:%d]", qPrintable(errMsg), errLine, errColumn);
    }

    Q_ASSERT(!doc.isNull());
    QDomElement docNode = doc.documentElement();
    Q_ASSERT(!docNode.isNull());

    beginResetModel();

    m_logItems.clear();
    unsigned frameIndex = 0;
    unsigned byteIndex = 1;
    QVector<CdbgLogItemDescription> dbgLogItems;
    m_dbgInitSequence = new CdbgCommandSequence(m_cdbg, CdbgCommandSequence::destroyingCallback());
    m_dbgInitSequence->append(m_cdbg->getLogInstanceResetCommand(0));
    for (QDomElement itemNode = docNode.firstChildElement(QLatin1String("item")); !itemNode.isNull(); itemNode = itemNode.nextSiblingElement(QLatin1String("item"))) {
        LogItem item;
        item.name = itemNode.firstChildElement(QLatin1String("name")).text();
        item.suffix = itemNode.firstChildElement(QLatin1String("suffix")).text();
        item.startbit = itemNode.attributeNode(QLatin1String("startbit")).value().toInt();
        item.startbyte = itemNode.attributeNode(QLatin1String("startbyte")).value().toInt();
        item.bitlength = itemNode.attributeNode(QLatin1String("bitlength")).value().toInt();
        item.adder = itemNode.attributeNode(QLatin1String("adder")).value().toFloat();
        item.littleEndian = itemNode.attributeNode(QLatin1String("endian")).value() == QLatin1String("little");
        item.type = itemNode.attributeNode(QLatin1String("type")).value() == QLatin1String("signed") ? LogItem::TypeSigned : LogItem::TypeUnsigned;
        QDomAttr multAttr = itemNode.attributeNode(QLatin1String("multiplier"));
        item.multiplier = multAttr.isNull() ? 1 : multAttr.value().toFloat();
        QDomAttr sidAttribute = itemNode.attributeNode(QLatin1String("sid"));
        QDomAttr ptrAttribute = itemNode.attributeNode(QLatin1String("ptr"));
        if (!sidAttribute.isNull()) {
            bool ok;
            item.sid = sidAttribute.value().toUInt(&ok, 16);
            if (!ok)
                continue;
            //if (!item.bitlength)
            //    continue;//skip whole message logging
            if (item.sid > 0x7ff)
                continue;
            item.data0 = itemNode.attributeNode(QLatin1String("data0")).value().toInt(&ok);
            if (!ok)
                item.data0 = -1;
            item.source = QString::fromLatin1("CAN SID=0x%1.%2.%3:%4").arg(item.sid, 3, 16)
                    .arg(item.startbyte)
                    .arg(item.startbit)
                    .arg(item.bitlength);
        } else if (!ptrAttribute.isNull() && (frameIndex <= 7)) {
            bool ok;
            uint32_t ptr = ptrAttribute.value().toUInt(&ok, 16);
            if (!ok)
                continue;
            CdbgLogItemDescription desc;
            desc.pointer = ptr;
            desc.size = item.bitlength / 8;
            if (byteIndex + desc.size > 8) {
                foreach (const EngineCommand &cmd, m_cdbg->getLogFrameInitCommands(0, frameIndex, dbgLogItems)) {
                    m_dbgInitSequence->append(cmd);
                }
                dbgLogItems.clear();
                ++frameIndex;
                byteIndex = 1;
            }

            item.sid = 0x631;
            item.data0 = frameIndex;
            item.startbyte = byteIndex;
            item.source = QString::fromLatin1("CAN SID=0x%1.%2.%3:%4").arg(item.sid, 3, 16)
                    .arg(item.startbyte)
                    .arg(item.startbit)
                    .arg(item.bitlength);
            dbgLogItems.append(desc);
            byteIndex += desc.size;
        } else {
            continue;
        }
        item.logid = item.name.isEmpty() ? item.source : item.name;
        item.logid.replace(QRegularExpression(QLatin1String("[.:, ]")), QLatin1String("_"));
        m_logItems.append(item);
    }
    if (!dbgLogItems.isEmpty() && (frameIndex <= 7)) {
        foreach (const EngineCommand &cmd, m_cdbg->getLogFrameInitCommands(0, frameIndex, dbgLogItems)) {
            m_dbgInitSequence->append(cmd);
        }
    }
    qDebug("Total debug frames %u, byteIndex in last %u", frameIndex, byteIndex);
    m_dbgInitSequence->append(m_cdbg->getLogStartCommand(0, frameIndex + 1, 10));
    //m_dbgInitSequence->execute();//executed after session init
#define LOG_MODE
#ifdef LOG_MODE
    m_cdbg->getSessionInitCommandSequence(&m_sessionInitCallback)->execute();
#endif

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFile.setFileName(tr("c:\\users\\main\\Documents\\log%1.csv").arg(QDateTime::currentDateTime().toString(QLatin1String("yy-MM-ddThh-mm-ss"))));
    m_logFile.open(QFile::WriteOnly | QFile::Text);
    m_logFile.write("Time");
    foreach (const LogItem &item, m_logItems) {
        if (item.name.isEmpty())
            continue;
        m_logFile.write(",");
        m_logFile.write(qPrintable(item.logid));
    }
    m_logFile.write("\n");

    endResetModel();
    m_timer.start();
}


QString dataToString(const uint8_t *data, unsigned count)
{
    QString ret;
    static QString fmt = QLatin1String("%1");
    for (unsigned i = 0; i < count; ++i) {
        if (i)
            ret.append(QLatin1Char(' '));
        ret.append(fmt.arg(data[i], 2, 16, QLatin1Char('0')));
    }
    return ret;
}

void LogItemsModel::updateLogItems(unsigned id, const uint8_t *data, unsigned count)
{
    bool processed = false;
    for (int i = 0; i < m_logItems.count(); ++i) {
        LogItem &item = m_logItems[i];
        if (item.sid != id)
            continue;
        if ((item.data0 != -1) && (item.data0 != data[0]))
            continue;
        if (!item.bitlength) {
            item.value = dataToString(data, count);
        } else {
            unsigned value = 0;
            unsigned byteLength = (item.startbit + item.bitlength + 7) / 8;
            if (item.startbyte >= 8)
                continue;
            if (item.startbyte + byteLength > 8)
                continue;
            if (byteLength == 1) {
                value = (data[item.startbyte] >> item.startbit) & MASK_FROM_LENGTH(item.bitlength);
            } else if (item.startbit) {
                //value = 0;
            } else if (item.littleEndian) {
                for (unsigned i = 0; i < byteLength; ++i) {
                    unsigned offset = i * 8;
                    unsigned mask = i == byteLength - 1 ? MASK_FROM_LENGTH(item.bitlength - i * 8) : 0xff;
                    value |= (data[i + item.startbyte] & mask) << offset;
                }
            } else for (unsigned i = 0; i < byteLength; ++i) {
                unsigned offset = (byteLength - i - 1) * 8;
                unsigned mask = i == 0 ? MASK_FROM_LENGTH(item.bitlength - (byteLength-1) * 8) : 0xff;
                value |= (data[i + item.startbyte] & mask) << offset;
            }
            switch (item.type) {
            case LogItem::TypeUnsigned:
                item.value = QString::number(value * item.multiplier - item.adder);
                break;
            case LogItem::TypeSigned: {
                signed int svalue = value & (1 << (item.bitlength - 1)) ? value | (MASK_FROM_LENGTH(32 - item.bitlength) << item.bitlength) : value;
                item.value = QString::number(svalue * item.multiplier - item.adder);
            } break;
            default:
                Q_ASSERT(false);
            }

        }
        processed = true;
    }
    if (!processed) {
        if ((id & 0x7f0) == 0x7e0)
            return;
#if 0
        //reporting unlogged messages
        qDebug("SID %03x not processed", id);
#endif
    }
}

void LogItemsModel::slotReceivedMessages(const QVector<CanMessage> &messages)
{
    //qDebug("Log received message");
    foreach (const CanMessage &msg, messages) {
        updateLogItems(msg.id(), msg.data(), msg.dlc());
    }
    dataChanged(index(0, Value), index(rowCount() - 1, Value));
}

void LogItemsModel::sessionInitCallback(CommandCallback::Result ret, void *cmd)
{
    Q_UNUSED(cmd);
    switch (ret) {
    case CommandCallback::ResultOk:
        m_dbgInitSequence->execute();
        qDebug("Debug sequence start");
        break;
    default:
        if (m_cdbg->sid() < 0x781) {
            m_cdbg->setSid(m_cdbg->sid() + 1);
            m_cdbg->getSessionInitCommandSequence(&m_sessionInitCallback)->execute();
        }
        qDebug("Debug session init sequence fault %u", ret);
    }
}

void LogItemsModel::slotTimerTimeout()
{
    //m_logFile.write(QDateTime::currentDateTime().toString(Qt::ISODate).toLocal8Bit());
    m_logFile.write(QByteArray::number((QDateTime::currentMSecsSinceEpoch() - logStartTime)/1000.0, 'f', 3));
    foreach (const LogItem &item, m_logItems) {
        if (item.name.isEmpty())
            continue;
        m_logFile.write(",");
        m_logFile.write(item.value.toString().toLocal8Bit());
    }
    m_logFile.write("\n");
}
