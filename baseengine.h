#ifndef BASEENGINE_H
#define BASEENGINE_H

#include <QObject>

#include "enginecommand.h"

class BaseSensor;

struct CalibrationData;

class BaseEngine : public QObject
{
    Q_OBJECT
public:
    virtual ~BaseEngine();

    virtual void request(const EngineCommand &command) = 0;
    virtual EngineCommand createAddressChangeRequest(BaseSensor *sensor, uint16_t newAddress);
    virtual EngineCommand createCalibrationRequest(BaseSensor *sensor, const CalibrationData &data);

    virtual void deinit() = 0;

    bool isAutoDetectSensorsEnabled() const { return m_autoDetectSensorsEnabled; }
    virtual void setAutoDetectSensorsEnabled(bool v);

    unsigned features() const { return m_features; }
signals:
    void sensorDetected(BaseSensor *sen);
protected:
    unsigned m_features;
    BaseEngine(unsigned _features, QObject *parent = 0);

    bool m_autoDetectSensorsEnabled;
};
#endif /*BASEENGINE_H*/
