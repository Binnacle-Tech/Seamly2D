# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for corect working file translations.pro

SOURCES += \
    $$PWD/mesh.cpp \
    $$PWD/meshslicer.cpp \
    $$PWD/bodymeasurements.cpp \
    $$PWD/parametricbody.cpp \
    $$PWD/objmeshwriter.cpp \
    $$PWD/bodymeasurementspanel.cpp \
    $$PWD/objmeshreader.cpp \
    $$PWD/polygontriangulator.cpp \
    $$PWD/orbitcamera.cpp \
    $$PWD/viewport3d.cpp \
    $$PWD/workspace3dwindow.cpp \
    $$PWD/workspace3dintegration.cpp \
    $$PWD/makehuman/makehumanmodel.cpp \
    $$PWD/makehuman/makehumanfitter.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/mesh.h \
    $$PWD/meshslicer.h \
    $$PWD/bodymeasurements.h \
    $$PWD/parametricbody.h \
    $$PWD/objmeshwriter.h \
    $$PWD/bodymeasurementspanel.h \
    $$PWD/objmeshreader.h \
    $$PWD/polygontriangulator.h \
    $$PWD/orbitcamera.h \
    $$PWD/viewport3d.h \
    $$PWD/workspace3dwindow.h \
    $$PWD/workspace3dintegration.h \
    $$PWD/makehuman/makehumanmodel.h \
    $$PWD/makehuman/makehumanfitter.h \
    $$PWD/stable.h

# MakeHuman body data (CC0 assets), embedded so no install or deploy step is needed.
RESOURCES += $$PWD/makehuman/makehuman.qrc
