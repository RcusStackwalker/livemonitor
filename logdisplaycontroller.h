#ifndef LOGDISPLAYCONTROLLER_H
#define LOGDISPLAYCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QPointer>

class QDateTime;
class QFile;
class QAction;

class LogDisplayController : public QObject
{
    Q_OBJECT
    typedef QObject Base;
public:
    LogDisplayController(QObject *parent);
    virtual ~LogDisplayController();

    QWidget *widget();

    bool writeTo(const QString &name);
    bool isWriteEnabled() const;
    void disableWrite();
protected slots:
    void slotLoggerMessage(int section, const QDateTime &dt, const QString &msg);
    void slotSectionToggled(bool v);
    void slotWriteToTriggered(bool v);
    void slotEnableAllSections();
    void slotDisableAllSections();
protected:
    QWidget *createWidget();

    QPointer<QWidget> m_widget;

    QVector<bool> m_sections;

    QFile *m_file;

    QList<QAction*> m_sectionActions;
};

#endif /*LOGDISPLAYCONTROLLER_H*/
