#-------------------------------------------------
#
# Project created by QtCreator 2013-05-17T14:40:12
#
#-------------------------------------------------

QT       += core
QT       += network

QT       -= gui

TARGET = proxyServer
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    proxyserver.cpp \
    proxyconnection.cpp \
    masterserver.cpp

HEADERS += \
    proxyserver.h \
    proxyconnection.h \
    masterserver.h
