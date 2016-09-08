#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class LogItemsModel;

#include "commandcallback.h"
#include "enginecommand.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    
private slots:
    void on_actionOpen_scheme_triggered();

    void on_actionOBD_Session_triggered();

    void on_pushButton_clicked();

    void on_obdSessionButton_clicked();

private:
    Ui::MainWindow *ui;
    LogItemsModel *m_logItemsModel;
};

#endif // MAINWINDOW_H
