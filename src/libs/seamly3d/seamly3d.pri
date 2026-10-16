# ADD TO EACH PATH $$PWD VARIABLE!!!!!!
# This need for corect working file translations.pro

SOURCES += \
    $$PWD/mesh.cpp \
    $$PWD/meshslicer.cpp \
    $$PWD/bodymeasurements.cpp \
    $$PWD/parametricbody.cpp \
    $$PWD/objmeshwriter.cpp \
    $$PWD/bodymeasurementspanel.cpp \
    $$PWD/measurementguide.cpp \
    $$PWD/piecemesher.cpp \
    $$PWD/piecearrangement.cpp \
    $$PWD/garment.cpp \
    $$PWD/clothsimulator.cpp \
    $$PWD/patternsource.cpp \
    $$PWD/patternpiecespanel.cpp \
    $$PWD/objmeshreader.cpp \
    $$PWD/polygontriangulator.cpp \
    $$PWD/orbitcamera.cpp \
    $$PWD/viewport3d.cpp \
    $$PWD/workspace3dwindow.cpp \
    $$PWD/workspace3dintegration.cpp \
    $$PWD/makehuman/makehumanmodel.cpp \
    $$PWD/makehuman/makehumanfitter.cpp \
    $$PWD/dxfimport/dxfreader.cpp \
    $$PWD/dxfimport/dxfpattern.cpp \
    $$PWD/dxfimport/outlinefitter.cpp \
    $$PWD/dxfimport/dxfpatternconverter.cpp \
    $$PWD/dxfimport/dxfimportwizard.cpp

*msvc*:SOURCES += $$PWD/stable.cpp

HEADERS += \
    $$PWD/mesh.h \
    $$PWD/meshslicer.h \
    $$PWD/bodymeasurements.h \
    $$PWD/parametricbody.h \
    $$PWD/objmeshwriter.h \
    $$PWD/bodymeasurementspanel.h \
    $$PWD/measurementguide.h \
    $$PWD/piecemesher.h \
    $$PWD/piecearrangement.h \
    $$PWD/garment.h \
    $$PWD/clothsimulator.h \
    $$PWD/patternsource.h \
    $$PWD/patternprovider.h \
    $$PWD/patternpiecespanel.h \
    $$PWD/objmeshreader.h \
    $$PWD/polygontriangulator.h \
    $$PWD/orbitcamera.h \
    $$PWD/viewport3d.h \
    $$PWD/workspace3dwindow.h \
    $$PWD/workspace3dintegration.h \
    $$PWD/makehuman/makehumanmodel.h \
    $$PWD/makehuman/makehumanfitter.h \
    $$PWD/dxfimport/dxfreader.h \
    $$PWD/dxfimport/dxfpattern.h \
    $$PWD/dxfimport/outlinefitter.h \
    $$PWD/dxfimport/dxfpatternconverter.h \
    $$PWD/dxfimport/dxfimportwizard.h \
    $$PWD/stable.h

# MakeHuman body data (CC0 assets), embedded so no install or deploy step is needed.
RESOURCES += $$PWD/makehuman/makehuman.qrc
