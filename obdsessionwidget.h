#ifndef OBDSESSIONWIDGET_H
#define OBDSESSIONWIDGET_H

#include <QDockWidget>

#include <QQueue>

#include "obdengine.h"


namespace Ui {
class ObdSessionWidget;
}

class ObdSessionWidget : public QDockWidget
{
    Q_OBJECT
    
public:
    explicit ObdSessionWidget(unsigned id, ObdEngine::SessionId sessionId, QWidget *parent = 0);
    ~ObdSessionWidget();
    enum State {
        WaitingForConnectResponse,
        WaitingForCommandResponse,
        Idle,
        Error
    };

private slots:
    void slotStateTimerTimeout();
    void on_pushButton_clicked();

    void on_sendCommandButton_clicked();

    void on_pushButton_2_clicked();

    void on_writeFlashButton_clicked();
    void slotUpdateReflashProgress();
    void on_readMemoryButton_clicked();

private:
    void setState(State state);

    void testerPresentCallback(CommandCallback::Result ret, void *cmd);
    void sentCommandCallback(CommandCallback::Result ret, void *cmd);
    void flashReadCallback(CommandCallback::Result ret, void *cmd);
    void sessionInitCallback(CommandCallback::Result ret, void *cmd);
    void erasePageUploadedCallback(CommandCallback::Result ret, void *cmd);
    void writePageUploadedCallback(CommandCallback::Result ret, void *cmd);
    void flashErasedCallback(CommandCallback::Result ret, void *cmd);
    void flashUploadedCallback(CommandCallback::Result ret, void *cmd);
    void requestReadFlashBlock();

    Ui::ObdSessionWidget *ui;
    ObdEngine *m_engine;
    State m_state;
    QTimer *m_stateTimer;
    QTimer *m_reflashProgressTimer;
    unsigned m_currentFlashReadPointer;
    unsigned m_flashReadSize;
    QVector<uint8_t> m_readFlashData;

    ObdCommandSequence *m_erasePageUploadSequence;
    ObdCommandSequence *m_writePageUploadSequence;
    ObdCommandSequence *m_flashUploadSequence;


    MemFunCommandCallback<ObdSessionWidget> m_sentCommandCallback;
    MemFunCommandCallback<ObdSessionWidget> m_testerPresentCallback;
    MemFunCommandCallback<ObdSessionWidget> m_flashReadCallback;
    MemFunCommandCallback<ObdSessionWidget> m_sessionInitCallback;

    MemFunCommandCallback<ObdSessionWidget> m_erasePageUploadedCallback;
    MemFunCommandCallback<ObdSessionWidget> m_writePageUploadedCallback;
    MemFunCommandCallback<ObdSessionWidget> m_flashErasedCallback;
    MemFunCommandCallback<ObdSessionWidget> m_flashUploadedCallback;
};

#endif // OBDSESSIONWIDGET_H
