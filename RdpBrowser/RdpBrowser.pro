#-------------------------------------------------
#
# Project created by QtCreator 2019-06-21T11:49:01
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = RdpBrowser
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        mainwindow.cpp \
    screenwindow.cpp \
    transferwindow.cpp \
    qscrview.cpp

HEADERS += \
        mainwindow.h \
    qscrview.h

FORMS += \
        mainwindow.ui \
    qscrview.ui

RESOURCES += \
    resource.qrc

win32 {
    INCLUDEPATH += c:/local/boost_1_62_0  c:/local/openssl_1_1
    LIBS += -Lc:/local/boost_1_62_0/lib32-msvc-14.0 -Lc:/local/openssl_1_1/lib32
#            -lboost_filesystem-vc140-mt-1_62 -lboost_system-vc140-mt-1_62
}
