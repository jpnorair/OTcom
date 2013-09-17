TARGET = otcom
DEPENDPATH += . ./bintex/
INCLUDEPATH += . ./bintex/
unix:DESTDIR = ../bin
QT += qt3support network
linux-g++:LIBS += -lusb
windows:CONFIG += console

# Version make rules
ver.target = otc_version.h
ver.commands = ./otc_version.sh
ver.depends = 
QMAKE_EXTRA_TARGETS += ver
QMAKE_CLEAN = otc_version.h
#PRE_TARGETDEPS += ver

# Input
HEADERS += *.h \
    bintex.h
SOURCES += *.cpp \ 
    bintex.c
#RESOURCES += otcom.qrc

# Icon (mainly for mac)
ICON = icons/otcom.icns
