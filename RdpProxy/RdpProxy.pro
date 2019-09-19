CONFIG += c++11 console
CONFIG -= app_bundle

QT      += core network
SOURCES += main.cpp browserclient.cpp
HEADERS += browserclient.h

win32 {
    INCLUDEPATH += c:/local/boost_1_62_0  c:/local/openssl_1_1
    LIBS += -Lc:/local/boost_1_62_0/lib32-msvc-14.0 -Lc:/local/openssl_1_1/lib32
#            -lboost_filesystem-vc140-mt-1_62 -lboost_system-vc140-mt-1_62
}
