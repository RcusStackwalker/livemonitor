#include "obdenginecommand.h"

ObdEngineCommand::ObdEngineCommand(const uint8_t *data, unsigned length)
{
    txcount = length;
    memcpy(txbuf, data, length);
}
