#include "mainwindow.h"
#include <QApplication>

#include "caninterface.h"
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    Logger logger;
    CanInterface caniface;

    MainWindow w;
    w.show();
    
    return a.exec();
}
