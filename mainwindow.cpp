#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QInputDialog>

#include "obdsessionwidget.h"
#include "logitemsmodel.h"
#include "cdbgengine.h"

#include <stdint.h>

#include <QtEndian>

#include "messageemulatorwindow.h"
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_logItemsModel = new LogItemsModel(this);
    ui->logItemsView->setModel(m_logItemsModel);
    //m_logItemsModel->loadSchema(QLatin1String(":/default_schema.xml"));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_scheme_triggered()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Schema file"), QString(), tr("*.xml"));
    if (f.isEmpty())
        return;
    m_logItemsModel->loadSchema(f);
}

void MainWindow::on_actionOBD_Session_triggered()
{
    QStringList items;
    for (unsigned i = 0x7e0; i < 0x7e8; ++i) {
        items << QString::fromLatin1("0x%1").arg(i, 3, 16, QLatin1Char('0'));
    }
    bool ok = false;
    QString item = QInputDialog::getItem(this, tr("ECU"), tr("Id"), items, 0, false, &ok);
    if (!ok || !items.contains(item))
        return;
    QDockWidget *dw = new ObdSessionWidget(0x7e0 + items.indexOf(item), ObdEngine::SessionBasic);
    connect(dw->toggleViewAction(), SIGNAL(triggered()), dw, SLOT(deleteLater()));
}

/*
void MainWindow::on_toolButton_clicked()
{
    EngineCommand ec;
    memset(ec.txbuf, 0, sizeof(ec.txbuf));
    ec.txbuf[0] = 1;
    ec.txbuf[1] = 1;
    ec.txcount = 8;
    ec.callback = &m_cdbgInitCallback;
    m_cdbg->request(ec);
}

void MainWindow::cdbgInitCallback(CommandCallback::Result ret, const EngineCommand &cmd)
{
    if (ret != CommandCallback::ResultOk) {
        qDebug("Lost command");
        return;
    }
    EngineCommand ec;
    memset(ec.txbuf, 0, sizeof(ec.txbuf));
    ec.txbuf[0] = 18;
    ec.txbuf[1] = 0;
    ec.txbuf[2] = 2;//log access
    ec.txcount = 8;
    ec.callback = &m_cdbgSecuritySeedCallback;
    m_cdbg->request(ec);

}



void MainWindow::cdbgSecurityAccessCallback(CommandCallback::Result ret, const EngineCommand &cmd)
{
    if (ret != CommandCallback::ResultOk) {
        qDebug("Lost command");
        return;
    }
    uint8_t security_flags = cmd.rxbuf[3];
    if (!security_flags) {
        qDebug("Security failed");
        return;
    }
    qDebug("New security access %02x", security_flags);

    EngineCommandSequence *seq = new EngineCommandSequence(m_cdbg, &m_cdbgReadMemoryCallback);

    seq->append(m_cdbg->getLogInstanceResetCommand(0));
    for (int i = 0; i < 1; ++i) {
        QVector<CdbgLogItemDescription> frame;
        for (int j = 0; j < 8; ++j) {

        }
        seq->append(m_cdbg->getLogFrameInitCommands(0, 0, ));
    }
    EngineCommand ec;
    uint8_t data[][8] = {
        { 20, 0, 0, 0, 0, 0, 0x06, 0x31 },
        { 21, 0, 0, 0, 0, 0, 0, 0 },
        { 22, 0, 2, 0, 0x00, 0x80, 0x4d, 0x4c },
        { 21, 0, 0, 0, 1, 0, 0, 0 },
        { 22, 0, 1, 0, 0x00, 0x80, 0x58, 0x93 },
        {  6, 0, 1, 0, 1, 0, 0, 10 },
    };
    for (int i = 0; i < 6; ++i) {
        EngineCommand cmd;
        cmd.txcount = 8;
        memcpy(cmd.txbuf, data[i], 8);
        seq->append(cmd);
    }
    seq->execute();
}
*/
void MainWindow::on_pushButton_clicked()
{
    bool ok;
    unsigned sid = QInputDialog::getInt(this, tr("Message CAN ID"), tr("SID"), 0x418, 0, 0x7ff, 1, &ok);
    if (!ok) return;
    MessageEmulatorWindow *w = new MessageEmulatorWindow(sid, 0);
    w->show();
}

void MainWindow::on_obdSessionButton_clicked()
{
    bool ok;
    unsigned sid = QInputDialog::getInt(this, tr("Message CAN ID"), tr("SID"), 0x7e0, 0, 0x7ff, 1, &ok);
    if (!ok) return;
    QString sessionText = QInputDialog::getItem(this, tr("SessionId"), tr("ID"), QStringList() << QLatin1String("Boot") << QLatin1String("User"), 1, false, &ok);
    if (!ok) return;
    ObdEngine::SessionId sessionId = sessionText == QLatin1String("Boot") ? ObdEngine::SessionBootload : ObdEngine::SessionBasic;
    ObdSessionWidget *w = new ObdSessionWidget(sid, sessionId, 0);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}
