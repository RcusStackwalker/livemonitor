#ifndef OBDENGINECOMMAND_H
#define OBDENGINECOMMAND_H

#include "abstractenginecommand.h"

struct ObdEngineCommand : public AbstractEngineCommand<257, 257>
{
    ObdEngineCommand() {}
    ObdEngineCommand(const uint8_t *data, unsigned length);
};

#endif // OBDENGINECOMMAND_H
