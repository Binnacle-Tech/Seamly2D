#-------------------------------------------------
#
# Unit tests for the Seamly3D library (3D workspace).
#
#-------------------------------------------------

include(../../../common.pri)

QT       += core gui testlib widgets opengl openglwidgets xml printsupport

TARGET = Seamly3DTest

CONFIG   += console

# CONFIG += testcase adds a 'make check' target. no_testcase_installs keeps the test out of 'make install'.
CONFIG += testcase no_testcase_installs

# disable app bundle to have stable cross-platform relative paths
CONFIG -= app_bundle

DESTDIR = bin

MOC_DIR = moc

OBJECTS_DIR = obj

HEADERS += \
    stable.h

SOURCES += \
    tst_seamly3d.cpp

# Seamly2D's measurement diagrams, to check that every measurement points at one.
RESOURCES += ../../libs/vmisc/share/resources/diagrams.qrc

*msvc*:SOURCES += stable.cpp

include(warnings.pri)

# Seamly3D static library
unix|win32: LIBS += -L$$OUT_PWD/../../libs/seamly3d/$${DESTDIR}/ -lseamly3d

INCLUDEPATH += $$PWD/../../libs/seamly3d
DEPENDPATH += $$PWD/../../libs/seamly3d

win32:!win32-g++: PRE_TARGETDEPS += $$OUT_PWD/../../libs/seamly3d/$${DESTDIR}/seamly3d.lib
else:unix|win32-g++: PRE_TARGETDEPS += $$OUT_PWD/../../libs/seamly3d/$${DESTDIR}/libseamly3d.a
