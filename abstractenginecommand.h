#ifndef ABSTRACTENGINECOMMAND_H
#define ABSTRACTENGINECOMMAND_H

#include <QString>

#include "commandcallback.h"

#include <stdint.h>

template <unsigned TxLength, unsigned RxLength> struct AbstractEngineCommand
{
    uint8_t txbuf[TxLength];
    uint8_t rxbuf[RxLength];
    unsigned txcount;
    unsigned rxcount;
    unsigned rxrealcount;
    bool silent;

    CommandCallback *callback;
    void *callbackArg;
    void executeCallback(CommandCallback::Result ret) {
        if (!callback)
            return;

        callback->callback(ret, this);
    }
    QString toString() const {
        QString ret;
        ret.append('[');
        for (unsigned i = 0; i < txcount; ++i) {
            if (i > 0)
                ret.append(' ');
            ret.append(QString("%1").arg(txbuf[i], 2, 16, QLatin1Char('0')));
        }
        ret.append(']');

        ret.append(QString(" rxcount=%1").arg(rxcount));
        for (unsigned i = 0; i < rxcount; ++i) {
            ret.append(QString(" %1").arg(rxbuf[i], 2, 16, QLatin1Char('0')));
        }
        return ret;
    }
    void clear() {
        txcount = 0;
        rxcount = 0;
        callback = 0;
        callbackArg = 0;
    }

    AbstractEngineCommand() : txcount(0), rxcount(0), rxrealcount(0), callback(0), callbackArg(0) {
        silent = false;
        memset(txbuf, 0, TxLength);
        memset(rxbuf, 0, RxLength);
    }
};

#endif // ABSTRACTENGINECOMMAND_H
