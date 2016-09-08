#ifndef OBDENGINE_H
#define OBDENGINE_H

#include "canengine.h"
#include "caninterface.h"
#include "obdenginecommand.h"
#include "enginecommandsequence.h"

typedef EngineCommandSequence<ObdEngineCommand> ObdCommandSequence;

class ObdEngine : public CommandCanEngine<ObdEngineCommand>
{
    Q_OBJECT
    Q_ENUMS(ServiceId)
public:
    enum ServiceId {
        ServiceGetCurrentData = 0x01,
        ServiceDiagnosticSession = 0x10,
        ServiceEcuReset = 0x11,
        ServiceClearDTC = 0x14,
        ServiceGetFP52 = 0x18,
        ServiceGetEepromData = 0x1a,
        ServiceReadECUIdentification = 0x21,
        ServiceReadMemoryByAddress = 0x23,
        ServiceSecurityAccess = 0x27,
        ServiceCommunicationStop = 0x28,
        ServiceCommunicationResume = 0x29,
        ServiceRoutineControl = 0x31,
        ServiceRequestDownload = 0x34,
        ServiceRequestUpload = 0x35,
        ServiceTransferData = 0x36,
        ServiceRequestTransferExit = 0x37,
        ServiceRequestReflash = 0x3b,
        ServiceTesterPresent = 0x3e
    };
    enum { CrcTransferAddress = 0x200000, CrcTransferSize = 2 };
    enum { RoutineCheckCrc = 225 };
    enum SessionId {
        SessionBasic = 0x81,
        SessionBootload = 0x85,
        SessionOem = 0x92
    };

    explicit ObdEngine(unsigned id, QObject *parent = 0);

    void deinit();
    void resetStateMachine();

    CommandCallback *idleCallback;

    ObdCommandSequence *getSessionInitCommandSequence(SessionId id, CommandCallback *callback);
    ObdCommandSequence *getFlashWriteCommandSequence(void *ptr, unsigned start, unsigned size, CommandCallback *cb);
protected:
    virtual void slotOnNewCanMessage(const QVector<CanMessage> &messages);
protected:
    unsigned m_multipacketTxIndex;
    unsigned m_multipacketRxIndex;
    unsigned m_multipacketRxSequence;
    unsigned m_multipacketTxSequence;
    virtual void multipacketTranmission();
    virtual void serveCommand();

    virtual bool idleWork();
    void decodeReplyErrorCodeAndCallback();

};

#endif // OBDENGINE_H
