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

QT += core gui widgets opengl openglwidgets

# Name of library
TARGET = seamly3d

# We want create a library
TEMPLATE = lib

CONFIG += staticlib

include(seamly3d.pri)

# This is static library so no need in "make install"

# directory for executable file
DESTDIR = bin

# files created moc
MOC_DIR = moc

# objecs files
OBJECTS_DIR = obj

include(warnings.pri)
