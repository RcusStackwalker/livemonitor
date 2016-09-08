#include "messageemulatorwindow.h"
#include "ui_messageemulatorwindow.h"

#include "caninterface.h"
MessageEmulatorWindow::MessageEmulatorWindow(unsigned sid, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessageEmulatorWindow)
  , m_sid(sid)
  , m_sendTimer(new QTimer(this))
{
    ui->setupUi(this);
    memset(m_data, 0, sizeof(m_data));
    m_sendTimer->setInterval(ui->periodSpin->value());
    m_sendTimer->start();
    connect(m_sendTimer, SIGNAL(timeout()), SLOT(onSendTimeout()));
}

MessageEmulatorWindow::~MessageEmulatorWindow()
{
    delete ui;
}

static unsigned count = 0;
void MessageEmulatorWindow::onSendTimeout()
{
    if (ui->messageUpdateCheckBox->isChecked()) {
        ++count;
        if (!(count % 5)) {
            //ui->byte2Spin->setValue((ui->byte2Spin->value() + 1) % 256);
        }
        m_data[0] = ui->byte1Spin->value();
        m_data[1] = ui->byte2Spin->value();
        m_data[2] = ui->byte3Spin->value();
        m_data[3] = ui->byte4Spin->value();
        m_data[4] = ui->byte5Spin->value();
        m_data[5] = ui->byte6Spin->value();
        m_data[6] = ui->byte7Spin->value();
        m_data[7] = ui->byte8Spin->value();
        QString text;
        for (unsigned i = 0; i < 8; ++i) {
            if (i)
                text.append(QLatin1Char(' '));
            text.append(QString::fromLatin1("%1").arg(m_data[i], 2, 16, QLatin1Char('0')));
        }
        ui->messageLine->setText(text);
    }
    if (m_sendTimer->interval() != ui->periodSpin->value())
        m_sendTimer->setInterval(ui->periodSpin->value());

    CanMessage msg(m_sid, m_data, sizeof(m_data));
    CanInterface::instance()->sendMessage(msg);
}
