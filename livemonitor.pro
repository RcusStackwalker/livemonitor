#-------------------------------------------------
#
# Project created by QtCreator 2013-06-07T17:36:31
#
#-------------------------------------------------

QT       += core gui xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = livemonitor
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    dmaprotocol.cpp \
    logitemsmodel.cpp \
    j2534.cpp \
    caninterface.cpp \
    obdsessionwidget.cpp \
    cdbgengine.cpp \
    commandcallback.cpp \
    enginecommand.cpp \
    logger.cpp \
    logdisplaycontroller.cpp \
    messageemulatorwindow.cpp \
    obdenginecommand.cpp \
    abstractenginecommand.cpp \
    obdengine.cpp \
    canengine.cpp \
    enginecommandsequence.cpp

HEADERS  += mainwindow.h \
    dmaprotocol.h \
    logitemsmodel.h \
    caninterface.h \
    obdsessionwidget.h \
    cdbgengine.h \
    logger.h \
    logdisplaycontroller.h \
    messageemulatorwindow.h \
    obdenginecommand.h \
    abstractenginecommand.h \
    obdengine.h \
    canengine.h \
    enginecommandsequence.h

FORMS    += mainwindow.ui \
    obdsessionwidget.ui \
    messageemulatorwindow.ui

RESOURCES += \
    resources.qrc

OTHER_FILES += \
    colt_flasher.xml
