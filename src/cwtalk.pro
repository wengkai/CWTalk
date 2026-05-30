QT += core gui widgets serialport

CONFIG += c++17

win32:RC_ICONS = resources/cwtalk.ico

RESOURCES += resources/cwtalk.qrc

TARGET = CWTalk
TEMPLATE = app

INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    config.cpp \
    adiftime.cpp \
    settingsdialog.cpp \
    catreaderfactory.cpp \
    yaesucatreader.cpp \
    icomcatreader.cpp \
    pckeyer.cpp \
    qsolog.cpp \
    qsorecordformat.cpp \
    sessionlogwindow.cpp \
    callsignprefixdb.cpp \
    adif/record.cpp \
    adif/file.cpp

HEADERS += \
    mainwindow.h \
    config.h \
    adiftime.h \
    settingsdialog.h \
    icatreader.h \
    catreaderfactory.h \
    yaesucatreader.h \
    icomcatreader.h \
    ikeyer.h \
    pckeyer.h \
    qsolog.h \
    qsorecordformat.h \
    sessionlogwindow.h \
    callsignprefixdb.h \
    adif/record.h \
    adif/file.h