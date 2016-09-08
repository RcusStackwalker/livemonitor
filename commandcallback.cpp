#include "commandcallback.h"

#include <QApplication>

#include "rzglobal.h"

CommandCallback::~CommandCallback()
{
}



QString callbackResultToString(CommandCallback::Result result)
{
    static const char *const result_strings[] = {
        QT_TRANSLATE_NOOP("CommandCallback", "Ok"),
        QT_TRANSLATE_NOOP("CommandCallback", "Timeout"),
        QT_TRANSLATE_NOOP("CommandCallback", "Failed"),
        QT_TRANSLATE_NOOP("CommandCallback", "Crc fail"),
        QT_TRANSLATE_NOOP("CommandCallback", "Canceled"),
        QT_TRANSLATE_NOOP("CommandCallback", "Nack"),
        QT_TRANSLATE_NOOP("CommandCallback", "Stray"),
        QT_TRANSLATE_NOOP("CommandCallback", "No echo"),
        QT_TRANSLATE_NOOP("CommandCallback", "Echo mismatch"),
    };
    Q_ASSERT(result >= 0 && result < itemsof(result_strings));
    if (result < 0 || result >= itemsof(result_strings)) {
        return QApplication::translate("CommandCallback", "Error %1").arg(result);
    }
    return QApplication::translate("CommandCallback", result_strings[result]);
}
