#-------------------------------------------------
#
# Seamly3D - 3D workspace library for Seamly2D.
#
# Self-contained module: avatar/mesh import, 3D viewport and the 3D workspace
# window. Lives adjacent to the 2D code so upstream merges stay conflict free.
#
#-------------------------------------------------

# File with common stuff for whole project
message("Entering seamly3d.pro")
include(../../../common.pri)

# xml and printsupport: the 2D headers patternsource.cpp reads the open pattern through need them.
QT += core gui widgets opengl openglwidgets xml printsupport

# Name of library
TARGET = seamly3d

# We want create a library
TEMPLATE = lib

CONFIG += staticlib

# The MakeHuman body data is a 4.4 MB resource: embed it as an object file instead of a huge generated C++ array.
CONFIG += resources_big

include(seamly3d.pri)

# This is static library so no need in "make install"

# directory for executable file
DESTDIR = bin

# files created moc
MOC_DIR = moc

# objecs files
OBJECTS_DIR = obj

include(warnings.pri)

# Include paths of the Seamly2D libraries (only patternsource.cpp uses them).
include(../libs.pri)
