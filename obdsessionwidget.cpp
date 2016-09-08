#include "obdsessionwidget.h"
#include "ui_obdsessionwidget.h"

#include <QMetaEnum>
#include <QFileDialog>
#include <QInputDialog>
#include <QDomDocument>

#define OBD_DIAGNOSTIC_SESSION 0x10
#define OBD_TESTER_PRESENT     0x3e
#define OBD_COMMUNICATION_STOP  0x28
#define OBD_COMMUNICATION_RESUME  0x29

#define FLASH_BLOCK_SIZE 192

static uint8_t session_id = ObdEngine::SessionBasic;
//#define OBD_SESSION_ID (ObdEngine::SessionId)(session_id++)
#define OBD_SESSION_ID ObdEngine::SessionBasic
//#define OBD_SESSION_ID ObdEngine::SessionBootload
//sid 1922 (0x782, dash) supports 0x81, 0x92 - rejects 0x23 with 17
//sid 1924 (0x784, esp) supports 0x81, 0x85, 0x92 - rejects 0x23 with 17
//sid 1942 (0x796) supports 0x81, 0x92 - rejects 0x23 with 17
//sid 1944 (0x798, multi-display) supports 0x81, 0x92 - rejects 0x23 with 17
//sid 1952 (0x7a0, etacs? disables lights) supports 0x81, 0x85, 0x92 - doesn't support MA
//sid 1954 (0x7a2, a/c) supports 0x81 - rejects 0x23 with code 51, 0x90 - succumbs
//sid 1956 (0x7a4, srs) supports (0x81, 0x92) - rejects 0x23 with code 51

//782->783
//784->785
//796->797
//798->799
//7a0->7a1
//7a2->7a3
//7a4->7a5
ObdSessionWidget::ObdSessionWidget(unsigned id, ObdEngine::SessionId sessionId, QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::ObdSessionWidget)
  , m_engine(new ObdEngine(id, this))
  , m_sentCommandCallback(this, &ObdSessionWidget::sentCommandCallback)
  , m_testerPresentCallback(this, &ObdSessionWidget::testerPresentCallback)
  , m_flashReadCallback(this, &ObdSessionWidget::flashReadCallback)
  , m_sessionInitCallback(this, &ObdSessionWidget::sessionInitCallback)
  , m_erasePageUploadedCallback(this, &ObdSessionWidget::erasePageUploadedCallback)
  , m_writePageUploadedCallback(this, &ObdSessionWidget::writePageUploadedCallback)
  , m_flashErasedCallback(this, &ObdSessionWidget::flashErasedCallback)
  , m_flashUploadedCallback(this, &ObdSessionWidget::flashUploadedCallback)
{
    m_reflashProgressTimer = new QTimer(this);
    m_reflashProgressTimer->setInterval(1000);
    connect(m_reflashProgressTimer, SIGNAL(timeout()), SLOT(slotUpdateReflashProgress()));
    ui->setupUi(this);
    m_engine->idleCallback = &m_testerPresentCallback;

    //ObdCommandSequence *seq = m_engine->getSessionInitCommandSequence(ObdEngine::SessionBootload, &m_sessionInitCallback);
    ObdCommandSequence *seq = m_engine->getSessionInitCommandSequence(sessionId, &m_sessionInitCallback);
    //ObdCommandSequence *seq = m_engine->getSessionInitCommandSequence((ObdEngine::SessionId)(session_id++), &m_sessionInitCallback);
    seq->execute();
    setState(WaitingForConnectResponse);

    QMetaEnum e = m_engine->metaObject()->enumerator(m_engine->metaObject()->indexOfEnumerator("ServiceId"));
    for (int i = 0; i < e.keyCount(); ++i) {
        ui->serviceComboBox->addItem(QString::fromLatin1(e.key(i)), e.value(i));
    }
}

ObdSessionWidget::~ObdSessionWidget()
{
    delete ui;
}

void ObdSessionWidget::slotStateTimerTimeout()
{
    switch (m_state) {
    case WaitingForConnectResponse:
        /*retry connection*/
        break;
    default:
        Q_ASSERT(false);
    }
}

void ObdSessionWidget::setState(ObdSessionWidget::State state)
{
    Q_UNUSED(state);
    //TODO
}

void ObdSessionWidget::testerPresentCallback(CommandCallback::Result ret, void *cmd)
{
    Q_UNUSED(cmd);
    switch (ret) {
    case CommandCallback::ResultOk: ui->progressBar->setValue((ui->progressBar->value() + 1) % ui->progressBar->maximum());
        break;
    default: ui->progressBar->setValue(0);
    }
}

void ObdSessionWidget::sentCommandCallback(CommandCallback::Result ret, void *_cmd)
{
    if (ret != CommandCallback::ResultOk) {
        ui->plainTextEdit->appendPlainText(QString::fromLatin1("< error %1").arg(ret));
        return;
    }
    static QString fmt = QString::fromLatin1("< %1\n");
    ObdEngineCommand *cmd = reinterpret_cast<ObdEngineCommand*>(_cmd);
    ui->plainTextEdit->appendPlainText(fmt.arg(bytesToString(cmd->rxbuf, cmd->rxrealcount)));
}

void ObdSessionWidget::flashReadCallback(CommandCallback::Result ret, void *_cmd)
{
    ObdEngineCommand *cmd = reinterpret_cast<ObdEngineCommand*>(_cmd);
    if ((ret == CommandCallback::ResultOk) && cmd->rxrealcount != FLASH_BLOCK_SIZE + 1) {
        static QString fmt = QString::fromLatin1("< got %2 instead of %3: %1\n");
        ObdEngineCommand *cmd = reinterpret_cast<ObdEngineCommand*>(_cmd);
        ui->plainTextEdit->appendPlainText(fmt.arg(bytesToString(cmd->rxbuf, cmd->rxrealcount)).arg(cmd->rxrealcount).arg(FLASH_BLOCK_SIZE + 1));
        ret = CommandCallback::ResultFailed;
    }
    switch (ret) {
    case CommandCallback::ResultOk:
        for (unsigned i = 1; i < cmd->rxrealcount; ++i) {
            m_readFlashData.append(cmd->rxbuf[i]);
        }
        m_currentFlashReadPointer += FLASH_BLOCK_SIZE;
        if (m_currentFlashReadPointer >= m_flashReadSize) {
            //complete
            QString str = QFileDialog::getSaveFileName(0, tr("Read flash"));
            QFile f(str);
            if (!f.open(QFile::WriteOnly)) {
                //failed
                ;
            }
            f.write((const char*)m_readFlashData.data(), m_readFlashData.size());
        } else {
            requestReadFlashBlock();
        }
        break;
    default:
        ui->plainTextEdit->appendPlainText(QString::fromLatin1("read flash error %1").arg(ret));
    }
}

void ObdSessionWidget::sessionInitCallback(CommandCallback::Result ret, void *_cmd)
{
    ObdCommandSequence *seq = reinterpret_cast<ObdCommandSequence*>(_cmd);
    delete seq;
    if (ret == CommandCallback::ResultOk) {
        setState(Idle);
    } else {
#if 1
        //seq->execute();
        if (m_engine->sid() < 0x7df)
            m_engine->setSid(m_engine->sid() + 1);
        //seq = m_engine->getSessionInitCommandSequence(ObdEngine::SessionBootload, &m_sessionInitCallback);
        seq = m_engine->getSessionInitCommandSequence(OBD_SESSION_ID, &m_sessionInitCallback);

        seq->execute();
#endif
    }
}

void ObdSessionWidget::erasePageUploadedCallback(CommandCallback::Result ret, void *cmd)
{
    switch (ret) {
    case CommandCallback::ResultOk:
        Logger::message("Flasher", tr("Erase page uploaded"));
        m_writePageUploadSequence->execute();
        break;
    default:
        Logger::message("Flasher", tr("Erase page upload failed %1").arg(ret));
    }
}

void ObdSessionWidget::writePageUploadedCallback(CommandCallback::Result ret, void *cmd)
{
    switch (ret) {
    case CommandCallback::ResultOk: {
        Logger::message("Flasher", tr("Write page uploaded"));
        //return;//for now
        //m_writePageUploadSequence->execute();
        uint8_t data[12] = { ObdEngine::ServiceRequestReflash, 154, 1, 1,
                             'R', 'c', 'u', 's', '0', '0', 0, 1 }; //caused bootloader lockup
        ObdCommandSequence *seq = new ObdCommandSequence(m_engine, &m_flashErasedCallback);
        seq->append(ObdEngineCommand(data, sizeof(data)));
        uint8_t purge_data[2] = { ObdEngine::ServiceRoutineControl, 224 };//causes reflash
        seq->append(ObdEngineCommand(purge_data, sizeof(purge_data)));
        seq->execute();
    } break;
    default:
        Logger::message("Flasher", tr("Write page upload failed %1").arg(ret));
    }
}

void ObdSessionWidget::flashErasedCallback(CommandCallback::Result ret, void *cmd)
{
    switch (ret) {
    case CommandCallback::ResultOk:
        Logger::message("Flasher", tr("Userspace flash erased"));
        m_flashUploadSequence->execute();
        m_reflashProgressTimer->start();
        break;
    default:
        Logger::message("Flasher", tr("Flash erase failed %1").arg(ret));
    }
}

void ObdSessionWidget::flashUploadedCallback(CommandCallback::Result ret, void *cmd)
{
    switch (ret) {
    case CommandCallback::ResultOk: {
        Logger::message("Flasher", tr("Userspace flash written"));
    } break;
    default:
        Logger::message("Flasher", tr("Flash write %1").arg(ret));
    }
}

void ObdSessionWidget::requestReadFlashBlock()
{
    ui->plainTextEdit->appendPlainText(QString::fromLatin1("Requesting read flash block 0x%1").arg(m_currentFlashReadPointer, 5, 16, QLatin1Char('0')));
    uint8_t data[5];
    data[0] = ObdEngine::ServiceReadMemoryByAddress;
    data[1] = m_currentFlashReadPointer >> 16;
    data[2] = m_currentFlashReadPointer >> 8;
    data[3] = m_currentFlashReadPointer;
    data[4] = FLASH_BLOCK_SIZE;
    ObdEngineCommand ec(data, sizeof(data));
    ec.callback = &m_flashReadCallback;
    m_engine->request(ec);
}

void ObdSessionWidget::on_pushButton_clicked()
{
    //sendCommand(new ObdCommand(OBD_COMMUNICATION_STOP, ));
}

void ObdSessionWidget::on_sendCommandButton_clicked()
{
    QStringList dataList = ui->commandLineEdit->text().split(QLatin1Char(' '), QString::SkipEmptyParts);
    QVector<uint8_t> data;
    data.append(ui->serviceComboBox->itemData(ui->serviceComboBox->currentIndex()).toUInt());
    foreach (const QString &s, dataList) {
        bool ok;
        data.append(s.toUInt(&ok, 16));
        if (!ok)
            return;
    }
    ObdEngineCommand ec(data.data(), data.size());
    QString str = QLatin1String(">");
    for (int i = 0; i < data.count(); ++i) {
        str.append(QString::fromLatin1(" %1").arg(data[i], 2, 16, QLatin1Char('0')));
    }
    str.append(QLatin1Char('\n'));
    ui->plainTextEdit->appendPlainText(str);
    ec.callback = &m_sentCommandCallback;
    m_engine->request(ec);
}

void ObdSessionWidget::on_pushButton_2_clicked()
{
    m_flashReadSize = QInputDialog::getInt(this, tr("Flash size"), tr("size"), 0x60000, 0);
    m_currentFlashReadPointer = 0;
    requestReadFlashBlock();
}

QByteArray stringToBytes(const QString &_str)
{
    QByteArray ret;
    QString str = _str;
    str.remove(QLatin1Char(' '));
    str.remove(QLatin1Char('\r'));
    str.remove(QLatin1Char('\n'));
    Q_ASSERT(!(str.length() % 2));
    for (int i = 0; i < str.length() / 2; ++i) {
        bool ok;
        QString n = str.mid(i * 2, 2);
        ret.append(n.toUInt(&ok, 16));
        Q_ASSERT(ok);
        if (!ok) break;
    }
    return ret;
}

void ObdSessionWidget::on_writeFlashButton_clicked()
{
    QString xmlName = QFileDialog::getOpenFileName(this, tr("Flasher Description"), QString(), tr("*.xml"));
    if (xmlName.isEmpty())
        return;
    QString binName = QFileDialog::getOpenFileName(this, tr("ROM File"), QString(), tr("*.bin"));
    QFile xmlFile(xmlName);
    xmlFile.open(QFile::ReadOnly | QFile::Text);
    QDomDocument doc;
    doc.setContent(&xmlFile);
    QDomElement root = doc.documentElement();
    {
        QDomElement erasePageElement = root.firstChildElement(QLatin1String("erasepage"));
        bool ok;
        uint32_t target = erasePageElement.attribute(QLatin1String("address")).toUInt(&ok, 16);
        if (!ok) return;
        QByteArray bytes = stringToBytes(erasePageElement.text());
        m_erasePageUploadSequence = m_engine->getFlashWriteCommandSequence(bytes.data(), target, bytes.size(), &m_erasePageUploadedCallback);
    }
    {
        QDomElement writePageElement = root.firstChildElement(QLatin1String("writepage"));
        bool ok;
        uint32_t target = writePageElement.attribute(QLatin1String("address")).toUInt(&ok, 16);
        if (!ok) return;
        QByteArray bytes = stringToBytes(writePageElement.text());
        m_writePageUploadSequence = m_engine->getFlashWriteCommandSequence(bytes.data(), target, bytes.size(), &m_writePageUploadedCallback);
    }
    {
        QDomElement userSpaceElement = root.firstChildElement(QLatin1String("userspace"));
        QByteArray bytes;
        bool ok;
        uint32_t target = userSpaceElement.attribute(QLatin1String("start")).toUInt(&ok, 16);
        if (!ok)
            return;
        uint32_t size = userSpaceElement.attribute(QLatin1String("end")).toUInt(&ok, 16) - target;
        if (!ok)
            return;
        QFile binFile(binName);
        if (!binFile.open(QFile::ReadOnly))
            return;
        if (!binFile.seek(target))
            return;
        bytes = binFile.read(size);
        if (bytes.size() != size)
            return;
        //load flash from start to end in bytes, recalc crc in process
        m_flashUploadSequence = m_engine->getFlashWriteCommandSequence(bytes.data(), target, size, &m_flashUploadedCallback);
    }
    m_erasePageUploadSequence->execute();
}

void ObdSessionWidget::slotUpdateReflashProgress()
{
    ui->progressBar->setValue(m_flashUploadSequence->currentIndex() * 100 / m_flashUploadSequence->size());
    ui->plainTextEdit->appendPlainText(tr("%1 / %2").arg(m_flashUploadSequence->currentIndex()).arg(m_flashUploadSequence->size()));
}

void ObdSessionWidget::on_readMemoryButton_clicked()
{
    m_currentFlashReadPointer = 0x804000;
    m_flashReadSize = m_currentFlashReadPointer + 0x8000;
    requestReadFlashBlock();
}
