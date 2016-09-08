#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QSet>
#include <QList>
#include <QDateTime>

#include <stdint.h>
#include <stdarg.h>

const char *enumValueToCharStar(QObject *obj, const char *enumName, int value);

void appendBytesToString(QString &str, const uint8_t *data, unsigned size);
QString bytesToString(const QByteArray &buf);
QString bytesToString(const uint8_t *data, unsigned size);

class LogMessage;

class Logger : public QObject
{
    Q_OBJECT
    Q_ENUMS(Sections)
public:
    Logger();
    virtual ~Logger();

    enum Level {
        Debug = 0,
        Info,
        Warning,
        CriticalError,
        FatalError,
    };

    enum Sections {
        PortStatus,
        Error,
        Sensor,
        LineData,
        Flasher,
        EngineState,
        ModBusEngine,
        OmniCommEngine,
        OmniCommSensor,
        RBusEngine,
        RBusSensor,
        Destruction,
        Detector,
        ObdEngine,
        CdbgEngine,
        SectionsCount
    };

    struct Section
    {
        QByteArray name;
        QString title;
        int useCount;
        bool default_on;
    };
    typedef QList<Section> SectionList;

    static void message(const LogMessage &msg);
    static void message(Sections sect, const QString &msg);
    static void error(const QString &msg) { message(Error, msg); }
    static void warning(const char *section, const char *fmt, ...);
    static void critical(const char *section, const char *fmt, ...);
    static void fatal(const char *section, const char *fmt, ...);
    static void debug(const char *section, const char *fmt, ...);
    static void info(const char *section, const QString &msg);
    static void message(const char *section, const QString &msg, int level = -1);

    static Logger *instance();

    SectionList sections() const { return m_registeredSections; }
    //bool setSectionEnabled(const QString &sect, bool v);

    void registerSection(const QByteArray &val, const QString &title, bool def);

    QString sectionTitle(int sect);
    QString sectionName(int sect);
signals:
    void sectionsChanged();
    void newMessage(int section, const QDateTime &dt, const QString &msg);
    void newMessage(const LogMessage &msg);
protected:
    SectionList m_registeredSections;
    typedef QHash<QByteArray, int> SectionHash;
    SectionHash m_sectionLookup;

    static void vprintf(int level, const char *section, const char *fmt, va_list args);
    void messageImpl(const char *section, const QString &msg, int level);
};

class LogMessage
{
public:
    LogMessage(Logger::Sections sect);
    LogMessage(Logger::Sections sect, const QString &msg);
    const QDateTime &dateTime() const { return m_dateTime; }
    QString toString() const;
    Logger::Sections section() const { return m_section; }
protected:
    virtual QString toStringImpl() const;

    Logger::Sections m_section;
    QDateTime m_dateTime;
    mutable QString m_stringCache;
};



#endif /*LOGGER_H*/
