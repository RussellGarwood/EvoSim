# -------------------------------------------------
# Project created by QtCreator 2010-02-10T13:30:03
# -------------------------------------------------
QT += widgets concurrent
TARGET = EvolutionSim
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    simmanager.cpp \
    critter.cpp \
    populationscene.cpp \
    environmentscene.cpp \
    sortablegenome.cpp \
    analyser.cpp \
    genomecomparison.cpp \
    fossilrecord.cpp \
    fossrecwidget.cpp \
    resizecatcher.cpp \
    analysistools.cpp \
    reseed.cpp \
    logspecies.cpp \
    logspeciesdataitem.cpp \
    about.cpp
HEADERS += mainwindow.h \
    simmanager.h \
    critter.h \
    populationscene.h \
    environmentscene.h \
    sortablegenome.h \
    analyser.h \
    genomecomparison.h \
    fossilrecord.h \
    fossrecwidget.h \
    resizecatcher.h \
    analysistools.h \
    version.h \
    reseed.h \
    logspecies.h \
    logspeciesdataitem.h \
    about.h
FORMS += mainwindow.ui \
    fossrecwidget.ui \
    genomecomparison.ui \
    reseed.ui \
    about.ui

OTHER_FILES += \
    README.TXT

RESOURCES += \
    resources.qrc

#Needed to use C++ lamda functions
CONFIG += c++11

DISTFILES += \
    LICENSE.md

