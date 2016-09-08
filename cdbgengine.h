#ifndef CDBGENGINE_H
#define CDBGENGINE_H

#include <QObject>
#include <QQueue>
#include <QTimer>

#include "canengine.h"
#include "enginecommand.h"
#include "caninterface.h"
#include "enginecommandsequence.h"

struct CdbgLogItemDescription {
    uint32_t pointer;
    uint8_t size;
};

typedef EngineCommandSequence<EngineCommand> CdbgCommandSequence;

class CdbgEngine : public CommandCanEngine<EngineCommand>
{
    Q_OBJECT
public:
    explicit CdbgEngine(QObject *parent = 0);

    void deinit();
    void resetStateMachine();

    QVector<EngineCommand> getLogFrameInitCommands(unsigned instance, unsigned idx, const QVector<CdbgLogItemDescription> &items);
    EngineCommand getLogInstanceResetCommand(unsigned instance);
    EngineCommand getLogStartCommand(unsigned instance, unsigned frameCount, unsigned interval);
    CdbgCommandSequence *getSessionInitCommandSequence(CommandCallback *callback);
protected:
    virtual void slotOnNewCanMessage(const QVector<CanMessage> &messages);
protected:
    virtual void serveCommand();
};

#endif // CDBGENGINE_H
