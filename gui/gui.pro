#-------------------------------------------------
#
# Project created by QtCreator 2018-06-19T11:43:25
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = software_wallet
TEMPLATE = app

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


include(./protobuf.pri)

CRYPTO_LIB_PATH = ../crypto_lib
PSBT_PATH = ../libpsbt_for_signer
include(../common/common.pri)


SOURCES += \
        main.cpp \
        walletmain.cpp \
    walletmanager.cpp \
    walletinstance.cpp \
    localtcpserver.cpp

HEADERS += \
        walletmain.h \
    walletmanager.h \
    walletinstance.h \
    localtcpserver.h


LIBS += ../libpsbt_qmakes/libpsbt.a
LIBS += ../crypto_lib_qmakes/main/libcrypto_lib.a
LIBS += ../crypto_lib_qmakes/ed25519/libed25519.a

SOURCES += safe_rand.c \
           crypto_utils.c

HEADERS += crypto_utils.h


FORMS += \
        walletmain.ui

DISTFILES += \
    cryptolib.pri \
    protobuf.pri
