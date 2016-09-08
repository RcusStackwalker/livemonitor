#include "logdisplaycontroller.h"

#include <QPlainTextEdit>
#include <QSettings>
#include <QAction>
#include <QFileDialog>
#include <QFile>

#include "logger.h"

LogDisplayController::LogDisplayController(QObject *parent)
    : Base(parent)
    , m_widget(0)
    , m_file(0)
{
    connect(Logger::instance(), SIGNAL(newMessage(int,QDateTime,QString)),
            SLOT(slotLoggerMessage(int,QDateTime,QString)));
}

LogDisplayController::~LogDisplayController()
{}

QWidget *LogDisplayController::widget()
{
    if (!m_widget) {
        m_widget = createWidget();
    }

    return m_widget;
}

static void add_widget_action(QWidget *w, const QString &str, const char *slotName)
{
    QAction *act = new QAction(str, w);
    QObject::connect(act, SIGNAL(triggered(bool)), w, slotName);
    w->addAction(act);
}

QWidget *LogDisplayController::createWidget()
{
    QPlainTextEdit *te = new QPlainTextEdit();
    te->setReadOnly(true);
    te->setLineWrapMode(QPlainTextEdit::NoWrap);
    /*somehow control line limit?*/
    QSettings settings;
    settings.beginGroup("Log");
    te->document()->setMaximumBlockCount(settings.value("MaxLogLines", 1000).toInt());

    te->setContextMenuPolicy(Qt::ActionsContextMenu);
    add_widget_action(te, tr("Clear"), SLOT(clear()));
    add_widget_action(te, tr("Select All"), SLOT(selectAll()));
    add_widget_action(te, tr("Copy"), SLOT(copy()));
    //save action
    {
        QAction *act = new QAction(tr("Write to.."), te);
        act->setCheckable(true);
        connect(act, SIGNAL(triggered(bool)), SLOT(slotWriteToTriggered(bool)));
        te->addAction(act);
    }
    {
        QAction *act = new QAction(tr("Enable all sections"), te);
        connect(act, SIGNAL(triggered(bool)), SLOT(slotEnableAllSections()));
        te->addAction(act);
    }
    {
        QAction *act = new QAction(tr("Disable all sections"), te);
        connect(act, SIGNAL(triggered(bool)), SLOT(slotDisableAllSections()));
        te->addAction(act);
    }
    {
        QAction *act = new QAction(te);
        act->setSeparator(true);
        te->addAction(act);
    }
    //add section actions
    settings.beginGroup(objectName());
    Logger::SectionList sections = Logger::instance()->sections();
    for (int i = 0; i < sections.size(); ++i) {
        const Logger::Section &sect = sections.at(i);
        QAction *act = new QAction(sect.title, te);
        act->setData(i);
        act->setCheckable(true);
        bool v = settings.value(sect.name, sect.default_on).toBool();
        act->setChecked(v);
        connect(act, SIGNAL(toggled(bool)), SLOT(slotSectionToggled(bool)));

        m_sections.append(v);

        te->addAction(act);
        m_sectionActions.append(act);
    }

    return te;
}

void LogDisplayController::slotLoggerMessage(int section, const QDateTime &dt, const QString &msg)
{
    if (!m_widget)
        return;

    /*if section disabled return*/
    Q_ASSERT(section >= 0 && section < m_sections.count());

    if (!m_sections.at(section))
        return;

    QString line = QString("[%1][%2]%3").arg(Logger::instance()->sectionTitle(section),
            dt.toString(Qt::ISODate), msg);

    if (m_file) {
        m_file->write(line.toUtf8());
        m_file->write("\n", 1);
    }

    static_cast<QPlainTextEdit*>(m_widget.data())->appendPlainText(line);
}

void LogDisplayController::slotSectionToggled(bool v)
{
    QAction *act = static_cast<QAction*>(sender());
    int idx = act->data().toInt();

    m_sections[idx] = v;

    QSettings settings;
    settings.beginGroup("Log");
    settings.beginGroup(objectName());
    settings.setValue(Logger::instance()->sectionName(idx), v);
}

bool LogDisplayController::writeTo(const QString &name)
{
    disableWrite();

    m_file = new QFile(name, this);
    m_file->open(QFile::WriteOnly);
}

bool LogDisplayController::isWriteEnabled() const
{
    return !!m_file;
}

void LogDisplayController::disableWrite()
{
    if (!m_file)
        return;

    m_file->close();
    m_file->deleteLater();
    m_file = 0;
}

void LogDisplayController::slotWriteToTriggered(bool v)
{
    QAction *act = static_cast<QAction*>(sender());

    if (!act->isChecked()) {
        disableWrite();
        return;
    }

    QString str = QFileDialog::getSaveFileName(0, tr("Log file"));
    if (str.isEmpty() || !writeTo(str)) {
        act->setChecked(false);
    }
}

void LogDisplayController::slotEnableAllSections()
{
    foreach (QAction *a, m_sectionActions) {
        if (!a->isChecked()) {
            a->toggle();
        }
    }
}

void LogDisplayController::slotDisableAllSections()
{
    foreach (QAction *a, m_sectionActions) {
        if (a->isChecked()) {
            a->toggle();
        }
    }
}

