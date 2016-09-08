#ifndef MESSAGEEMULATORWINDOW_H
#define MESSAGEEMULATORWINDOW_H

#include <QWidget>

#include <stdint.h>

#include <QTimer>

namespace Ui {
class MessageEmulatorWindow;
}

class MessageEmulatorWindow : public QWidget
{
    Q_OBJECT
    
public:
    explicit MessageEmulatorWindow(unsigned sid, QWidget *parent = 0);
    ~MessageEmulatorWindow();
private slots:
    void onSendTimeout();
private:
    Ui::MessageEmulatorWindow *ui;
    unsigned m_sid;
    uint8_t m_data[8];
    QTimer *m_sendTimer;
};

#endif // MESSAGEEMULATORWINDOW_H
