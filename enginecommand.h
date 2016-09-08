#ifndef ENGINECOMMAND_H
#define ENGINECOMMAND_H

#include <QString>
#include <QVector>

#include "abstractenginecommand.h"

#include "logger.h"


class CdbgEngine;

struct EngineCommand : public AbstractEngineCommand<8, 8>
{
};



#endif /*ENGINECOMMAND_H*/
