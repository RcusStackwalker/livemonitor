#include "baseengine.h"
#include "baseengine.moc"


BaseEngine::BaseEngine(unsigned _features, QObject *parent)
    : QObject(parent)
    , m_features(_features)
    , m_autoDetectSensorsEnabled(false)
{}

BaseEngine::~BaseEngine()
{}


EngineCommand BaseEngine::createAddressChangeRequest(BaseSensor *sensor, uint16_t newAddress)
{
    Q_ASSERT(false);
    return EngineCommand();
}

EngineCommand BaseEngine::createCalibrationRequest(BaseSensor *sensor, const CalibrationData &data)
{
    Q_ASSERT(false);
    return EngineCommand();
}

void BaseEngine::setAutoDetectSensorsEnabled(bool v)
{
    m_autoDetectSensorsEnabled = v;
}
