QT += core gui widgets serialport

CONFIG += c++17

TARGET = CWTalk
TEMPLATE = app

INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    config.cpp \
    settingsdialog.cpp \
    catreaderfactory.cpp \
    yaesucatreader.cpp \
    icomcatreader.cpp \
    pckeyer.cpp \
    qsolog.cpp \
    callsignprefixdb.cpp \
    adif/record.cpp \
    adif/file.cpp

HEADERS += \
    mainwindow.h \
    config.h \
    settingsdialog.h \
    icatreader.h \
    catreaderfactory.h \
    yaesucatreader.h \
    icomcatreader.h \
    ikeyer.h \
    pckeyer.h \
    qsolog.h \
    callsignprefixdb.h \
    adif/record.h \
    adif/file.h