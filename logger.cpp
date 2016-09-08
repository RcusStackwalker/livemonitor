#include "logger.h"

#include <QMetaEnum>
#include <QCoreApplication>
#include <QStringList>

#include <stdio.h>

#include <rzglobal.h>

LogMessage::LogMessage(Logger::Sections sect)
    : m_section(sect)
    , m_dateTime(QDateTime::currentDateTime())
{}

LogMessage::LogMessage(Logger::Sections sect, const QString &msg)
    : m_section(sect)
    , m_dateTime(QDateTime::currentDateTime())
    , m_stringCache(msg)
{
}

QString LogMessage::toString() const
{
    if (m_stringCache.isNull()) {
        m_stringCache = toStringImpl();
    }
    return m_stringCache;
}

QString LogMessage::toStringImpl() const
{
    return QLatin1String("TODO");
}

Logger *g_instance = 0;

static struct { const char *name; bool default_on; bool debug_only; } log_sections[] = {
    { "PortStatus", true, false },
    { "Error", false, false },
    { "Sensor", false, true },
    { "LineData", false, true },
    { "Flasher", true, false },
    { "EngineState", false, true },
    { "ModBusEngine", false, true },
    { "OmniCommEngine", false, true },
    { "OmniCommSensor", false, true },
    { "RBusEngine", false, true },
    { "RBusSensor", false, true },
    { "Destruction", false, true },
    { "Detector", false, true },
    { "ObdEngine", true, false },
    { "CdbgEngine", true, false }
};


Logger::Logger()
{
    g_instance = this;
    Q_ASSERT(itemsof(log_sections) == SectionsCount);
#ifdef QT_DEBUG
    bool debug_on = true;
#else
    bool debug_on = QCoreApplication::arguments().contains(QLatin1String("-g"));
#endif

    for (int i = 0; i < SectionsCount; ++i) {
        if (!debug_on && log_sections[i].debug_only)
            continue;
        registerSection(log_sections[i].name, tr(log_sections[i].name), log_sections[i].default_on);
    }
}

Logger::~Logger()
{
    g_instance = 0;
}

Logger *Logger::instance()
{
    Q_ASSERT(g_instance);
    return g_instance;
}

void Logger::debug(const char *section, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    Logger::vprintf(Debug, section, fmt, ap);
    va_end(ap);
}

void Logger::warning(const char *section, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    Logger::vprintf(Warning, section, fmt, ap);
    va_end(ap);
}

void Logger::critical(const char *section, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    Logger::vprintf(CriticalError, section, fmt, ap);
    va_end(ap);
}

void Logger::vprintf(int level, const char *section, const char *fmt, va_list args)
{
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    message(section, QString::fromLatin1(buffer), level);
}

void Logger::message(const LogMessage &msg)
{
    if (msg.section() >= instance()->m_sectionLookup.count())
        return;
    emit instance()->newMessage(msg);
    emit instance()->newMessage(static_cast<int>(msg.section()), msg.dateTime(), msg.toString());
}

void Logger::message(Sections sect, const QString &msg)
{
    message(LogMessage(sect, msg));
}

void Logger::message(const char *section, const QString &msg, int level)
{
    instance()->messageImpl(section, msg, level);
}

void Logger::messageImpl(const char *section, const QString &msg, int level)
{
    /*lookup section id*/
    int idx = 0;
    SectionHash::const_iterator it = m_sectionLookup.find(section);
    if (it == m_sectionLookup.end()) {
        qWarning("Unregistered section %s: %s", section, qPrintable(msg));
        return;
    } else {
        idx = it.value();
    }
    emit newMessage(idx, QDateTime::currentDateTime(), msg);
    qDebug("[%s][%s] %s", section, qPrintable(QTime::currentTime().toString()), qPrintable(msg));
}

void Logger::registerSection(const QByteArray &name, const QString &title, bool def)
{
    if (m_sectionLookup.contains(name)) {
        qFatal("double register of section %s", name.data());
    }

    Section item;
    item.name = name;
    item.title = title;
    item.useCount = 0;
    item.default_on = def;

    m_registeredSections.append(item);
    m_sectionLookup.insert(name, m_registeredSections.count()-1);

    emit sectionsChanged();
}

QString Logger::sectionName(int sect)
{
    Q_ASSERT(sect >= 0 && sect < m_registeredSections.count());

    return m_registeredSections.at(sect).name;
}

QString Logger::sectionTitle(int sect)
{
    Q_ASSERT(sect >= 0 && sect < m_registeredSections.count());

    return m_registeredSections.at(sect).title;
}

const char *enumValueToCharStar(QObject *obj, const char *enumName, int value)
{
    static const char null_value[] = "(null)";
    const char *ret = null_value;

    if (!obj)
        return ret;

    const QMetaObject *mo = obj->metaObject();
    int idx = mo->indexOfEnumerator(enumName);

    if (idx == -1)
        return ret;

    QMetaEnum en = mo->enumerator(idx);
    return en.valueToKey(value);
}

QString bytesToString(const QByteArray &buf)
{
    return bytesToString((const uint8_t *)buf.constData(), buf.size());
}

QString bytesToString(const uint8_t *data, unsigned size)
{
    QString str;
    appendBytesToString(str, data, size);
    return str;
}

void appendBytesToString(QString &r, const uint8_t *data, unsigned size)
{
    if (!size)
        return;
    static const char numbers[] = "0123456789ABCDEF";
    int rpos = r.size();
    r.resize(rpos + size * 3 - 1);
    for (int i = 0; i < size-1; ++i) {
        uchar c = data[i];
        r[rpos++] = QChar((ushort)numbers[(c >> 4) % 16]);
        r[rpos++] = QChar((ushort)numbers[c % 16]);
        r[rpos++] = QChar((ushort)' ');
    }
    uchar c = data[size-1];
    r[rpos++] = QChar((ushort)numbers[(c >> 4) % 16]);
    r[rpos++] = QChar((ushort)numbers[c % 16]);
}

