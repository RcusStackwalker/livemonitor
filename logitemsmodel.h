#ifndef LOGITEMSMODEL_H
#define LOGITEMSMODEL_H

#include <QAbstractTableModel>
#include <QFile>
#include <QTimer>
#include <QTextStream>

#include <stdint.h>

#include "cdbgengine.h"
#include "caninterface.h"

struct LogItem
{
    QString logid;
    QString name;
    QString source;
    QVariant value;
    QString suffix;
    uint8_t startbit;
    uint8_t startbyte;
    uint8_t bitlength;
    uint16_t sid;
    int data0;
    uint32_t pointer;
    float adder;
    float multiplier;
    enum Type {
        TypeUnsigned,
        TypeSigned
    };
    Type type;
    bool littleEndian;
};

class LogItemsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit LogItemsModel(QObject *parent = 0);
    enum Sections {
        Name,
        Value,
        Suffix,
        Source,
        SectionCount
    };

    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

    void loadSchema(const QString &fname);
    void j2534Init();

    void updateLogItems(unsigned id, const uint8_t *data, unsigned count);
signals:
    
public slots:
    void slotTimerTimeout();
    void slotReceivedMessages(const QVector<CanMessage> &messages);
protected:
    QVector<LogItem> m_logItems;
    QFile m_logFile;
    QTimer m_timer;

    void sessionInitCallback(CommandCallback::Result ret, void *cmd);
    MemFunCommandCallback<LogItemsModel> m_sessionInitCallback;

    CdbgCommandSequence *m_dbgInitSequence;
    CdbgEngine *m_cdbg;
};

#endif // LOGITEMSMODEL_H
