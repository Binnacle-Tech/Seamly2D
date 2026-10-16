/******************************************************************************
 *   @file   tst_seamly3d.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Unit tests of the Seamly3D geometry, import and camera code.
 **
 **  @copyright
 **  Copyright (C) 2026 Seamly2D project
 **  <https://github.com/fashionfreedom/seamly2d> All Rights Reserved.
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *****************************************************************************/

#include <QBuffer>
#include <QDate>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTreeWidget>
#include <QVector2D>
#include <QDomDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>
#include <memory>

#include "bodymeasurements.h"
#include "bodymeasurementspanel.h"
#include "clothsimulator.h"
#include "fitmap.h"
#include "dxfimport/dxfimportwizard.h"
#include "dxfimport/dxfpattern.h"
#include "dxfimport/dxfpatternconverter.h"
#include "dxfimport/dxfreader.h"
#include "dxfimport/outlinefitter.h"
#include "dxfimport/svgreader.h"
#include "garment.h"
#include "makehuman/makehumanfitter.h"
#include "makehuman/makehumanmodel.h"
#include "mesh.h"
#include "meshslicer.h"
#include "objmeshreader.h"
#include "objmeshwriter.h"
#include "orbitcamera.h"
#include "parametricbody.h"
#include "patternpiecespanel.h"
#include "patternprovider.h"
#include "piecearrangement.h"
#include "piecemesher.h"
#include "polygontriangulator.h"

using namespace Seamly3D;

Q_DECLARE_METATYPE(Seamly3D::BodyMeasurements)

namespace
{
//---------------------------------------------------------------------------------------------------------------------
bool readObj(const QByteArray &data, Mesh &mesh, QString *error = nullptr)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);

    ObjMeshReader reader;
    const bool ok = reader.read(&buffer, mesh);
    if (error != nullptr)
    {
        *error = reader.errorString();
    }
    return ok;
}

//---------------------------------------------------------------------------------------------------------------------
qreal trianglesArea(const QPolygonF &points, const QVector<quint32> &indices)
{
    qreal area = 0;
    for (int i = 0; i + 2 < indices.size(); i += 3)
    {
        QPolygonF triangle;
        triangle << points.at(static_cast<int>(indices.at(i))) << points.at(static_cast<int>(indices.at(i + 1)))
                 << points.at(static_cast<int>(indices.at(i + 2)));
        const qreal signedArea = PolygonTriangulator::signedArea(triangle);
        if (signedArea <= 0)
        {
            return -1; // every triangle must be counter-clockwise
        }
        area += signedArea;
    }
    return area;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief isClosedSurface checks every edge is shared by exactly two triangles, in opposite directions. */
bool isClosedSurface(const Mesh &mesh)
{
    QHash<QPair<quint32, quint32>, int> edges;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            ++edges[qMakePair(mesh.indices.at(i + k), mesh.indices.at(i + (k + 1) % 3))];
        }
    }
    for (auto it = edges.constBegin(); it != edges.constEnd(); ++it)
    {
        if (it.value() != 1 || edges.value(qMakePair(it.key().second, it.key().first)) != 1)
        {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
float signedVolume(const Mesh &mesh)
{
    float volume = 0;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const QVector3D &a = mesh.positions.at(static_cast<int>(mesh.indices.at(i)));
        const QVector3D &b = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 1)));
        const QVector3D &c = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 2)));
        volume += QVector3D::dotProduct(a, QVector3D::crossProduct(b, c));
    }
    return volume / 6.0f;
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements makeBody(std::initializer_list<QPair<BodyMeasurement, double>> values)
{
    BodyMeasurements measurements;
    for (const auto &value : values)
    {
        measurements.setValue(value.first, value.second);
    }
    return measurements;
}

const QByteArray unitCubeObj =
    "# unit cube\n"
    "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
    "v 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
    "f 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n";
} // anonymous namespace

class TST_Seamly3D : public QObject
{
    Q_OBJECT

private slots:
    void boundsOfEmptyMeshAreInvalid();
    void boundsAndCounts();
    void computeNormalsOfFlatTriangle();
    void appendOffsetsIndices();
    void validity();

    void readQuadCube();
    void readFaceFormats_data();
    void readFaceFormats();
    void readNegativeIndices();
    void readKeepsFileNormals();
    void readErrors_data();
    void readErrors();

    void triangulate_data();
    void triangulate();
    void triangulateDegenerate();
    void flatPieceMesh();

    void cameraFramesBounds();
    void cameraClampsPitchAndDistance();
    void cameraLooksAtTarget();

    void sliceCube();
    void sliceOpenStrip();

    void measurementNamesAreUnique();
    void scaleToHeight();
    void sanitizedOrdersHeights();
    void readVit();
    void readVitRejectsOtherFiles();

    void bodyFitsMeasurements_data();
    void bodyFitsMeasurements();
    void bodyIsClosedAndOutward();
    void bodyFollowsMeasurementChanges();

    void objWriteReadRoundTrip();

    void readVitGender();

    void makeHumanDataLoads();
    void makeHumanTargetFactors();
    void makeHumanMacroFactors();
    void makeHumanDefaultBody();
    void makeHumanModifiersChangeMeasures();
    void makeHumanFit_data();
    void makeHumanFit();
    void makeHumanFitDefaultMeasurements();
    void makeHumanFitMaleDefaults();
    void makeHumanTapeMeasuresEverything();
    void readVitBirthDate();
    void sliceByPlane();

    void pieceMesherKeepsShape();
    void pieceMesherEdgeLength();
    void arrangementGuess_data();
    void arrangementGuess();
    void arrangementWrapsAroundBody();
    void arrangementPairs();
    void piecesPanelFromProvider();
    void makeHumanFitUnreachable();
    void makeHumanGenderChangesShape();

    void garmentPanelEdges();
    void seamFinderSewsBodice();
    void seamFinderSewsTrousers();
    void seamFinderSewsSkirt();
    void arrangementWrapsLegs();
    void bodyFrameFindsLegs();
    void clothHoldsSkirtAtWaist();
    void seamEditorSwitchesAndAdds();
    void seamLinesFollowEdges();
    void seamFinderSewsDart();
    void piecesHaveSides();
    void seamFinderSetsInSleeves();
    void bodyColliderInsideOutside();
    void clothDrapesOverSphere();
    void clothSewsTube();
    void clothSewsShirtOnBody();
    void measurementsPanelUnits();

    void dxfReaderParsesDrawing();
    void dxfBulgeArc();
    void dxfSplineIsBezier();
    void dxfAamaPieces();
    void dxfPlainOutlines();
    void outlineFitterCornersAndCurves();
    void dxfConverterWritesValidPattern();
    void dxfConverterRebuildsOutline();
    void dxfImportWizardConverts();
    void svgPathCommands();
    void svgUnitsAndTransforms();
    void svgPatternPieces();
    void dxfPlainMarks();
    void fabricPresets();
    void fitMapsOnSphere();
    void fitMapColors();
};

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::boundsOfEmptyMeshAreInvalid()
{
    const Mesh mesh;
    QVERIFY(mesh.isEmpty());
    QVERIFY(!mesh.bounds().valid);
    QCOMPARE(mesh.bounds().radius(), 0.0f);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::boundsAndCounts()
{
    Mesh mesh;
    QVERIFY(readObj(unitCubeObj, mesh));

    QCOMPARE(mesh.vertexCount(), 8);
    QCOMPARE(mesh.triangleCount(), 12);

    const Bounds3D bounds = mesh.bounds();
    QVERIFY(bounds.valid);
    QCOMPARE(bounds.minimum, QVector3D(0, 0, 0));
    QCOMPARE(bounds.maximum, QVector3D(1, 1, 1));
    QCOMPARE(bounds.center(), QVector3D(0.5f, 0.5f, 0.5f));
    QVERIFY(qFuzzyCompare(bounds.radius(), std::sqrt(3.0f) / 2.0f));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::computeNormalsOfFlatTriangle()
{
    Mesh mesh;
    mesh.positions << QVector3D(0, 0, 0) << QVector3D(1, 0, 0) << QVector3D(0, 1, 0);
    mesh.indices << 0 << 1 << 2;
    mesh.computeNormals();

    QCOMPARE(mesh.normals.size(), 3);
    for (const QVector3D &normal : mesh.normals)
    {
        QCOMPARE(normal, QVector3D(0, 0, 1));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::appendOffsetsIndices()
{
    Mesh first;
    first.positions << QVector3D(0, 0, 0) << QVector3D(1, 0, 0) << QVector3D(0, 1, 0);
    first.indices << 0 << 1 << 2;
    first.computeNormals();

    Mesh second = first;
    second.translate(QVector3D(0, 0, 5));

    first.append(second);
    QCOMPARE(first.vertexCount(), 6);
    QCOMPARE(first.indices, QVector<quint32>({0, 1, 2, 3, 4, 5}));
    QCOMPARE(first.normals.size(), 6);
    QCOMPARE(first.bounds().maximum.z(), 5.0f);
    QVERIFY(first.isValid());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::validity()
{
    Mesh mesh;
    mesh.positions << QVector3D(0, 0, 0) << QVector3D(1, 0, 0) << QVector3D(0, 1, 0);
    mesh.indices << 0 << 1 << 3;
    QVERIFY(!mesh.isValid());

    mesh.indices = {0, 1};
    QVERIFY(!mesh.isValid());

    mesh.indices = {0, 1, 2};
    QVERIFY(mesh.isValid());

    mesh.normals << QVector3D(0, 0, 1);
    QVERIFY(!mesh.isValid());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readQuadCube()
{
    Mesh mesh;
    QVERIFY(readObj(unitCubeObj, mesh));
    QVERIFY(mesh.isValid());
    QCOMPARE(mesh.normals.size(), mesh.positions.size());

    // Smooth normals of a cube corner point away from the center.
    const QVector3D center = mesh.bounds().center();
    for (int i = 0; i < mesh.vertexCount(); ++i)
    {
        QVERIFY(QVector3D::dotProduct(mesh.normals.at(i), mesh.positions.at(i) - center) > 0);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readFaceFormats_data()
{
    QTest::addColumn<QByteArray>("face");

    QTest::newRow("v") << QByteArray("f 1 2 3\n");
    QTest::newRow("v/vt") << QByteArray("f 1/1 2/2 3/3\n");
    QTest::newRow("v//vn") << QByteArray("f 1//1 2//1 3//1\n");
    QTest::newRow("v/vt/vn") << QByteArray("f 1/1/1 2/2/1 3/3/1\n");
    QTest::newRow("tabs and extra spaces") << QByteArray("f\t1   2\t3  \n");
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readFaceFormats()
{
    QFETCH(QByteArray, face);

    const QByteArray data = QByteArray("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\n") + face;
    Mesh mesh;
    QString error;
    QVERIFY2(readObj(data, mesh, &error), qPrintable(error));
    QCOMPARE(mesh.vertexCount(), 3);
    QCOMPARE(mesh.triangleCount(), 1);
    QCOMPARE(mesh.normals.at(0), QVector3D(0, 0, 1));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readNegativeIndices()
{
    Mesh mesh;
    QVERIFY(readObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n", mesh));
    QCOMPARE(mesh.indices, QVector<quint32>({0, 1, 2}));
    QCOMPARE(mesh.positions.at(1), QVector3D(1, 0, 0));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readKeepsFileNormals()
{
    // The same position with two different normals must become two vertices (hard edge).
    const QByteArray data = "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n"
                            "vn 0 0 1\nvn 0 1 0\n"
                            "f 1//1 2//1 3//1\nf 1//2 4//2 2//2\n";
    Mesh mesh;
    QVERIFY(readObj(data, mesh));
    QCOMPARE(mesh.vertexCount(), 6);
    QCOMPARE(mesh.normals.at(0), QVector3D(0, 0, 1));
    QCOMPARE(mesh.normals.at(3), QVector3D(0, 1, 0));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readErrors_data()
{
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("empty") << QByteArray();
    QTest::newRow("no faces") << QByteArray("v 0 0 0\nv 1 0 0\nv 0 1 0\n");
    QTest::newRow("bad vertex") << QByteArray("v 0 zero 0\n");
    QTest::newRow("index out of range") << QByteArray("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n");
    QTest::newRow("zero index") << QByteArray("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n");
    QTest::newRow("two corners") << QByteArray("v 0 0 0\nv 1 0 0\nf 1 2\n");
    QTest::newRow("bad normal index") << QByteArray("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1//1 2//1 3//1\n");
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readErrors()
{
    QFETCH(QByteArray, data);

    Mesh mesh;
    QString error;
    QVERIFY(!readObj(data, mesh, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(mesh.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::triangulate_data()
{
    QTest::addColumn<QPolygonF>("outline");
    QTest::addColumn<int>("triangles");

    QPolygonF square;
    square << QPointF(0, 0) << QPointF(10, 0) << QPointF(10, 10) << QPointF(0, 10);
    QTest::newRow("square ccw") << square << 2;

    QPolygonF clockwise;
    for (int i = square.size() - 1; i >= 0; --i)
    {
        clockwise << square.at(i);
    }
    QTest::newRow("square cw") << clockwise << 2;

    QPolygonF closed = square;
    closed << square.constFirst();
    QTest::newRow("closed outline") << closed << 2;

    // Concave "L" shape like a bodice with a cut out armhole.
    QPolygonF lShape;
    lShape << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, 5) << QPointF(5, 5) << QPointF(5, 20) << QPointF(0, 20);
    QTest::newRow("L shape") << lShape << 4;

    // Star with five reflex corners.
    QPolygonF star;
    for (int i = 0; i < 10; ++i)
    {
        const qreal angle = M_PI / 2 + i * M_PI / 5;
        const qreal radius = (i % 2 == 0) ? 10 : 4;
        star << QPointF(radius * qCos(angle), radius * qSin(angle));
    }
    QTest::newRow("star") << star << 8;

    // A "comb" with collinear points along the base.
    QPolygonF comb;
    comb << QPointF(0, 0) << QPointF(5, 0) << QPointF(10, 0) << QPointF(15, 0) << QPointF(15, 10) << QPointF(12, 10)
         << QPointF(12, 3) << QPointF(9, 3) << QPointF(9, 10) << QPointF(6, 10) << QPointF(6, 3) << QPointF(3, 3)
         << QPointF(3, 10) << QPointF(0, 10);
    QTest::newRow("comb") << comb << 12;
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::triangulate()
{
    QFETCH(QPolygonF, outline);
    QFETCH(int, triangles);

    const QVector<quint32> indices = PolygonTriangulator::triangulate(outline);
    QCOMPARE(indices.size(), triangles * 3);

    // The triangles must tile the outline exactly: same total area, all counter-clockwise.
    QVERIFY(qFuzzyCompare(trianglesArea(outline, indices), qAbs(PolygonTriangulator::signedArea(outline))));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::triangulateDegenerate()
{
    QVERIFY(PolygonTriangulator::triangulate(QPolygonF()).isEmpty());

    QPolygonF line;
    line << QPointF(0, 0) << QPointF(1, 1) << QPointF(2, 2);
    QVERIFY(PolygonTriangulator::triangulate(line).isEmpty());

    QPolygonF bowTie;
    bowTie << QPointF(0, 0) << QPointF(10, 10) << QPointF(10, 0) << QPointF(0, 10) << QPointF(-5, 5);
    const QVector<quint32> indices = PolygonTriangulator::triangulate(bowTie);
    QVERIFY(indices.size() % 3 == 0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::flatPieceMesh()
{
    QPolygonF square;
    square << QPointF(0, 0) << QPointF(0, 10) << QPointF(10, 10) << QPointF(10, 0); // clockwise

    const Mesh mesh = PolygonTriangulator::flatPieceMesh(square, 2.0f);
    QVERIFY(mesh.isValid());
    QCOMPARE(mesh.vertexCount(), 4);
    QCOMPARE(mesh.triangleCount(), 2);
    QCOMPARE(mesh.bounds().minimum, QVector3D(0, 0, 2));
    QCOMPARE(mesh.bounds().maximum, QVector3D(10, 10, 2));

    // Winding must agree with the +Z normal whatever the outline direction.
    Mesh recomputed = mesh;
    recomputed.computeNormals();
    QCOMPARE(recomputed.normals, mesh.normals);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::cameraFramesBounds()
{
    Bounds3D bounds;
    bounds.expand(QVector3D(-1, 0, -1));
    bounds.expand(QVector3D(1, 2, 1));

    OrbitCamera camera;
    camera.frame(bounds);
    QCOMPARE(camera.target(), QVector3D(0, 1, 0));

    // The bounding sphere must be inside the vertical field of view.
    const float halfFov = qDegreesToRadians(camera.fieldOfView() / 2.0f);
    QVERIFY(bounds.radius() / camera.distance() < qSin(halfFov));

    // Invalid bounds leave the camera alone.
    const float distance = camera.distance();
    camera.frame(Bounds3D());
    QCOMPARE(camera.distance(), distance);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::cameraClampsPitchAndDistance()
{
    OrbitCamera camera;
    camera.orbit(0, 1000);
    QCOMPARE(camera.pitch(), OrbitCamera::maxPitch);
    camera.orbit(0, -1000);
    QCOMPARE(camera.pitch(), OrbitCamera::minPitch);

    camera.setDistance(-5);
    QCOMPARE(camera.distance(), OrbitCamera::minDistance);

    camera.setDistance(10);
    camera.zoom(0.5f);
    QCOMPARE(camera.distance(), 5.0f);
    camera.zoom(-1.0f); // ignored
    QCOMPARE(camera.distance(), 5.0f);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::cameraLooksAtTarget()
{
    OrbitCamera camera;
    camera.setTarget(QVector3D(1, 2, 3));
    camera.setDistance(10);
    camera.setAngles(0, 0);
    QCOMPARE(camera.eye(), QVector3D(1, 2, 13));

    // The target projects onto the view axis, in front of the camera.
    const QVector3D inView = camera.viewMatrix().map(camera.target());
    QVERIFY(qFuzzyIsNull(inView.x()));
    QVERIFY(qFuzzyIsNull(inView.y()));
    QVERIFY(qFuzzyCompare(inView.z(), -10.0f));

    // Panning moves the target but keeps the viewing direction.
    const QVector3D before = camera.eye() - camera.target();
    camera.pan(0.1f, 0.0f);
    QVERIFY(camera.target().x() < 1.0f);
    QCOMPARE(camera.eye() - camera.target(), before);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::sliceCube()
{
    Mesh cube;
    QVERIFY(readObj(unitCubeObj, cube));

    const QVector<MeshSlicer::Contour> contours = MeshSlicer::sliceAtHeight(cube, 0.5f);
    QCOMPARE(contours.size(), 1);
    QVERIFY(contours.constFirst().closed);
    QVERIFY(qFuzzyCompare(contours.constFirst().length(), 4.0));
    QVERIFY(qFuzzyCompare(MeshSlicer::girthAtHeight(cube, 0.25f), 4.0));

    // Through the bottom face's vertices and outside the mesh.
    QVERIFY(qFuzzyCompare(MeshSlicer::girthAtHeight(cube, 1.0f), 4.0));
    QCOMPARE(MeshSlicer::girthAtHeight(cube, 2.0f), 0.0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::sliceOpenStrip()
{
    // A vertical wall made of two triangles: the cut is an open line, not a girth.
    Mesh wall;
    wall.positions << QVector3D(0, 0, 0) << QVector3D(2, 0, 0) << QVector3D(2, 1, 0) << QVector3D(0, 1, 0);
    wall.indices << 0 << 1 << 2 << 0 << 2 << 3;

    const QVector<MeshSlicer::Contour> contours = MeshSlicer::sliceAtHeight(wall, 0.5f);
    QCOMPARE(contours.size(), 1);
    QVERIFY(!contours.constFirst().closed);
    QVERIFY(qFuzzyCompare(contours.constFirst().length(), 2.0));
    QCOMPARE(MeshSlicer::girthAtHeight(wall, 0.5f), 0.0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::measurementNamesAreUnique()
{
    QSet<QString> names;
    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        const auto measurement = static_cast<BodyMeasurement>(i);
        QVERIFY(!BodyMeasurements::name(measurement).isEmpty());
        QVERIFY(!BodyMeasurements::displayName(measurement).isEmpty());
        QVERIFY(BodyMeasurements::defaultValue(measurement) > 0);
        names.insert(BodyMeasurements::name(measurement));
    }
    QCOMPARE(names.size(), BodyMeasurements::count());

    // Every measurement says how to take it, with its number and a diagram that exists.
    QSet<QString> codes;
    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        const auto measurement = static_cast<BodyMeasurement>(i);
        codes.insert(BodyMeasurements::code(measurement));
        QVERIFY(!BodyMeasurements::description(measurement).isEmpty());
        QVERIFY2(QFile::exists(BodyMeasurements::diagram(measurement)),
                 qPrintable(BodyMeasurements::diagram(measurement)));
    }
    QCOMPARE(codes.size(), BodyMeasurements::count());

    // The defaults are a consistent body.
    QVERIFY(BodyMeasurements().sanitized() == BodyMeasurements());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::scaleToHeight()
{
    // 81 inches: every measurement grows with the height, so the body keeps its shape.
    const double height = 81 * 2.54;
    const double factor = height / BodyMeasurements::defaultValue(BodyMeasurement::Height);
    BodyMeasurements measurements;
    measurements.scaleToHeight(height);

    QCOMPARE(measurements.value(BodyMeasurement::Height), height);
    for (const BodyMeasurement measurement : {BodyMeasurement::WaistHeight, BodyMeasurement::ArmLength,
                                              BodyMeasurement::WaistCirc, BodyMeasurement::ShoulderWidth})
    {
        QVERIFY(qFuzzyCompare(measurements.value(measurement), BodyMeasurements::defaultValue(measurement) * factor));
    }

    // The realistic body for it stays natural: girths fit closely; at this height MakeHuman's tall proportions put
    // the knee and calf about a centimeter off, which the fitter accepts rather than distort the legs.
    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    QVERIFY(fit.valid);
    for (const MakeHumanFit::Entry &entry : fit.entries)
    {
        const bool height = BodyMeasurements::group(entry.measurement) == BodyMeasurementGroup::Height;
        QVERIFY2(!entry.fitted || qAbs(entry.achieved - entry.target) < (height ? 1.5 : 0.3),
                 qPrintable(BodyMeasurements::name(entry.measurement)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::sanitizedOrdersHeights()
{
    // Waist entered above the bust, a negative knee and a zero girth.
    const BodyMeasurements broken = makeBody({{BodyMeasurement::WaistHeight, 130},
                                              {BodyMeasurement::KneeHeight, -4},
                                              {BodyMeasurement::NeckCirc, 0}});
    const BodyMeasurements fixed = broken.sanitized();

    QVERIFY(fixed.value(BodyMeasurement::WaistHeight) < fixed.value(BodyMeasurement::BustpointHeight));
    QVERIFY(fixed.value(BodyMeasurement::HipHeight) < fixed.value(BodyMeasurement::WaistHeight));
    QVERIFY(fixed.value(BodyMeasurement::KneeHeight) > fixed.value(BodyMeasurement::CalfHeight));
    QVERIFY(fixed.value(BodyMeasurement::AnkleHeight) > 0);
    QCOMPARE(fixed.value(BodyMeasurement::NeckCirc), BodyMeasurements::defaultValue(BodyMeasurement::NeckCirc));

    // A body that can not stand (ankle above the head) falls back to proportions.
    const BodyMeasurements impossible = makeBody({{BodyMeasurement::Height, 150}, {BodyMeasurement::AnkleHeight, 200}});
    const BodyMeasurements rebuilt = impossible.sanitized();
    QVERIFY(rebuilt.value(BodyMeasurement::AnkleHeight) > 0);
    QVERIFY(rebuilt.value(BodyMeasurement::AnkleHeight) < rebuilt.value(BodyMeasurement::CalfHeight));

    // Generating from nonsense still gives a valid body.
    QVERIFY(ParametricBody::generate(impossible).combined().isValid());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readVit()
{
    const QByteArray vit = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<smis>\n"
                           "  <version>0.3.4</version>\n"
                           "  <read-only>false</read-only>\n"
                           "  <unit>inch</unit>\n"
                           "  <body-measurements>\n"
                           "    <m name=\"height\" value=\"70\"/>\n"
                           "    <m name=\"waist_circ\" value=\"30.5\"/>\n"
                           "    <m name=\"hip_circ\" value=\"waist_circ + 8\"/>\n"
                           "    <m name=\"@my_custom\" value=\"12\"/>\n"
                           "  </body-measurements>\n"
                           "</smis>\n";
    QBuffer buffer;
    buffer.setData(vit);
    buffer.open(QIODevice::ReadOnly);

    BodyMeasurements measurements;
    QStringList applied;
    QVERIFY2(measurements.readVit(&buffer, &applied), qPrintable(measurements.errorString()));
    QCOMPARE(applied, QStringList({QStringLiteral("height"), QStringLiteral("waist_circ")}));

    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::Height), 70 * 2.54));
    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::WaistCirc), 30.5 * 2.54));

    // Formulas are skipped, and missing measurements follow the person's height (the default body scaled to it).
    const double scale = 70 * 2.54 / BodyMeasurements::defaultValue(BodyMeasurement::Height);
    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::HipCirc),
                          BodyMeasurements::defaultValue(BodyMeasurement::HipCirc) * scale));
    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::KneeHeight),
                          BodyMeasurements::defaultValue(BodyMeasurement::KneeHeight) * scale));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readVitRejectsOtherFiles()
{
    BodyMeasurements measurements;
    const BodyMeasurements unchanged;

    QBuffer pattern;
    pattern.setData("<?xml version=\"1.0\"?><pattern><version>0.6.0</version></pattern>");
    pattern.open(QIODevice::ReadOnly);
    QVERIFY(!measurements.readVit(&pattern));
    QVERIFY(!measurements.errorString().isEmpty());

    QBuffer broken;
    broken.setData("<smis><unit>cm</unit><body-measurements><m name=\"height\"");
    broken.open(QIODevice::ReadOnly);
    QVERIFY(!measurements.readVit(&broken));

    QVERIFY(measurements == unchanged);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyFitsMeasurements_data()
{
    QTest::addColumn<BodyMeasurements>("measurements");

    QTest::newRow("default") << BodyMeasurements();

    BodyMeasurements tall;
    tall.scaleToHeight(192);
    tall.setValue(BodyMeasurement::BustCirc, 108);
    tall.setValue(BodyMeasurement::UnderbustCirc, 100);
    tall.setValue(BodyMeasurement::WaistCirc, 94);
    tall.setValue(BodyMeasurement::HipCirc, 104);
    tall.setValue(BodyMeasurement::ShoulderWidth, 47);
    QTest::newRow("tall") << tall;

    BodyMeasurements petite;
    petite.scaleToHeight(152);
    petite.setValue(BodyMeasurement::BustCirc, 118);
    petite.setValue(BodyMeasurement::UnderbustCirc, 98);
    petite.setValue(BodyMeasurement::WaistCirc, 104);
    petite.setValue(BodyMeasurement::HipCirc, 126);
    petite.setValue(BodyMeasurement::ThighCirc, 72);
    QTest::newRow("petite plus") << petite;
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyFitsMeasurements()
{
    QFETCH(BodyMeasurements, measurements);

    const BodyModel body = ParametricBody::generate(measurements);
    const BodyMeasurements &m = body.measurements;

    const auto expectGirth = [&m](const Mesh &part, BodyMeasurement girth, float height)
    {
        const double measured = MeshSlicer::girthAtHeight(part, height);
        const double expected = m.value(girth);
        if (qAbs(measured - expected) > expected * 1e-4)
        {
            qWarning("%s: measured %f, expected %f", qPrintable(BodyMeasurements::name(girth)), measured, expected);
            return false;
        }
        return true;
    };
    const auto h = [&m](BodyMeasurement measurement) { return static_cast<float>(m.value(measurement)); };

    QVERIFY(expectGirth(body.torso, BodyMeasurement::BustCirc, h(BodyMeasurement::BustpointHeight)));
    QVERIFY(expectGirth(body.torso, BodyMeasurement::UnderbustCirc, ParametricBody::underbustHeight(m)));
    QVERIFY(expectGirth(body.torso, BodyMeasurement::WaistCirc, h(BodyMeasurement::WaistHeight)));
    QVERIFY(expectGirth(body.torso, BodyMeasurement::HipCirc, h(BodyMeasurement::HipHeight)));
    QVERIFY(expectGirth(body.torso, BodyMeasurement::NeckCirc, static_cast<float>(
                                                                   m.value(BodyMeasurement::NeckBackHeight) * 0.8
                                                                   + m.value(BodyMeasurement::Height) * 0.2)));

    for (const Mesh *leg : {&body.leftLeg, &body.rightLeg})
    {
        QVERIFY(expectGirth(*leg, BodyMeasurement::ThighCirc, ParametricBody::thighHeight(m)));
        QVERIFY(expectGirth(*leg, BodyMeasurement::KneeCirc, h(BodyMeasurement::KneeHeight)));
        QVERIFY(expectGirth(*leg, BodyMeasurement::CalfCirc, h(BodyMeasurement::CalfHeight)));
        QVERIFY(expectGirth(*leg, BodyMeasurement::AnkleCirc, h(BodyMeasurement::AnkleHeight)));
    }

    // Stands on the floor and is as tall as the person.
    const Bounds3D bounds = body.combined().bounds();
    QVERIFY(qAbs(bounds.minimum.y()) < 1e-4f);
    QVERIFY(qFuzzyCompare(bounds.maximum.y(), h(BodyMeasurement::Height)));

    // Symmetric about the YZ plane.
    QVERIFY(qAbs(bounds.minimum.x() + bounds.maximum.x()) < 1e-3f);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyIsClosedAndOutward()
{
    const BodyModel body = ParametricBody::generate(BodyMeasurements(), 24);
    for (const Mesh *part : {&body.torso, &body.leftLeg, &body.rightLeg, &body.leftArm, &body.rightArm})
    {
        QVERIFY(part->isValid());
        QVERIFY(isClosedSurface(*part));
        QVERIFY(signedVolume(*part) > 0);
    }
    QVERIFY(body.combined().isValid());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyFollowsMeasurementChanges()
{
    const BodyModel base = ParametricBody::generate(BodyMeasurements());

    const BodyModel wideHips = ParametricBody::generate(makeBody({{BodyMeasurement::HipCirc, 120}}));
    QVERIFY(wideHips.torso.bounds().size().x() > base.torso.bounds().size().x());

    const BodyModel longArms = ParametricBody::generate(makeBody({{BodyMeasurement::ArmLength, 70}}));
    QVERIFY(longArms.leftArm.bounds().minimum.y() < base.leftArm.bounds().minimum.y());

    // Wider bust pushes the arms out so they do not cut into the body.
    const BodyModel fullBust = ParametricBody::generate(makeBody({{BodyMeasurement::BustCirc, 130}}));
    QVERIFY(fullBust.leftArm.bounds().maximum.x() > base.leftArm.bounds().maximum.x());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::objWriteReadRoundTrip()
{
    const Mesh original = ParametricBody::generate(BodyMeasurements(), 16).combined();

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    ObjMeshWriter writer;
    QVERIFY2(writer.write(&buffer, original), qPrintable(writer.errorString()));
    buffer.close();

    Mesh copy;
    QString error;
    QVERIFY2(readObj(buffer.data(), copy, &error), qPrintable(error));
    QCOMPARE(copy.vertexCount(), original.vertexCount());
    QCOMPARE(copy.triangleCount(), original.triangleCount());

    // The reader numbers vertices in the order faces use them, so compare the triangles corner by corner.
    for (int i = 0; i < original.indices.size(); ++i)
    {
        const QVector3D &expected = original.positions.at(static_cast<int>(original.indices.at(i)));
        const QVector3D &actual = copy.positions.at(static_cast<int>(copy.indices.at(i)));
        QVERIFY((actual - expected).length() < 1e-3f);
    }

    QBuffer empty;
    empty.open(QIODevice::WriteOnly);
    QVERIFY(!writer.write(&empty, Mesh()));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readVitGender()
{
    QBuffer buffer;
    buffer.setData("<smis><unit>cm</unit><personal><gender>male</gender></personal>"
                   "<body-measurements><m name=\"height\" value=\"180\"/></body-measurements></smis>");
    buffer.open(QIODevice::ReadOnly);

    BodyMeasurements measurements;
    QVERIFY(measurements.readVit(&buffer));
    QCOMPARE(measurements.gender(), 1.0);
    QCOMPARE(measurements.muscle(), 0.5);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanDataLoads()
{
    QString error;
    const QSharedPointer<const MakeHumanData> data = MakeHumanData::instance(&error);
    QVERIFY2(!data.isNull(), qPrintable(error));

    QCOMPARE(data->basePositions.size(), 13380);
    QCOMPARE(data->triangles.size(), 13378 * 6);
    QVERIFY(data->targets.size() > 500);

    // Every modifier is built, and every ruler has its measure modifier.
    QCOMPARE(data->modifiers.size(), MakeHumanData::modifierNames().size());
    for (const QString &ruler : MakeHumanBody::rulerNames())
    {
        QVERIFY2(MakeHumanData::modifierNames().contains(ruler), qPrintable(ruler));
    }

    // Macro targets for every age are bundled.
    for (const QString &age : {QStringLiteral("baby"), QStringLiteral("child"), QStringLiteral("young"),
                               QStringLiteral("old")})
    {
        QVERIFY2(data->indexOf(QStringLiteral("macrodetails/universal-female-") + age
                               + QStringLiteral("-averagemuscle-averageweight")) >= 0,
                 qPrintable(age));
    }

    // A damaged pack is rejected, not half loaded.
    MakeHumanData broken;
    QBuffer garbage;
    garbage.setData(qCompress(QByteArray("SMHBxxxx")));
    garbage.open(QIODevice::ReadOnly);
    QVERIFY(!broken.load(&garbage));
    QVERIFY(!broken.errorString().isEmpty());
    QVERIFY(broken.basePositions.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanTargetFactors()
{
    QCOMPARE(MakeHumanData::targetFactors(QStringLiteral("macrodetails/universal-female-young-averagemuscle-maxweight")),
             QStringList({QStringLiteral("female"), QStringLiteral("young"), QStringLiteral("averagemuscle"),
                          QStringLiteral("maxweight")}));
    QCOMPARE(MakeHumanData::targetFactors(QStringLiteral("macrodetails/caucasian-male-young")),
             QStringList({QStringLiteral("caucasian"), QStringLiteral("male"), QStringLiteral("young")}));
    QVERIFY(MakeHumanData::targetFactors(QStringLiteral("measure/measure-waist-circ-incr")).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanMacroFactors()
{
    // Values from human.py for the default human and a few settings.
    MakeHumanMacros macros;
    QCOMPARE(macros.factor(MacroValue::Female), 0.5);
    QCOMPARE(macros.factor(MacroValue::Young), 1.0);
    QCOMPARE(macros.factor(MacroValue::Old), 0.0);
    QCOMPARE(macros.factor(MacroValue::AverageWeight), 1.0);
    QCOMPARE(macros.factor(MacroValue::MaxHeight), 0.0);
    QVERIFY(qFuzzyCompare(macros.factor(MacroValue::Caucasian), 1.0 / 3.0));

    macros.weight = 0.75;
    macros.height = 0.0;
    macros.age = 0.25;
    QCOMPARE(macros.factor(MacroValue::MaxWeight), 0.5);
    QCOMPARE(macros.factor(MacroValue::AverageWeight), 0.5);
    QCOMPARE(macros.factor(MacroValue::MinHeight), 1.0);
    QVERIFY(qAbs(macros.factor(MacroValue::Young) - 0.2) < 1e-9);
    QVERIFY(qAbs(macros.factor(MacroValue::Child) - 0.8) < 1e-9);

    // Age in years (human.py setAgeYears / getAgeYears).
    QCOMPARE(MakeHumanMacros::ageFromYears(25), 0.5);
    QCOMPARE(MakeHumanMacros::ageFromYears(1), 0.0);
    QCOMPARE(MakeHumanMacros::ageFromYears(90), 1.0);
    QVERIFY(qAbs(MakeHumanMacros::ageFromYears(10) - 0.1875) < 1e-9);
    QVERIFY(qAbs(MakeHumanMacros::yearsFromAge(MakeHumanMacros::ageFromYears(47)) - 47) < 1e-9);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanDefaultBody()
{
    const QSharedPointer<const MakeHumanData> data = MakeHumanData::instance();
    QVERIFY(!data.isNull());

    MakeHumanBody body(data);
    const double height = body.heightCm();
    QVERIFY2(height > 150 && height < 185, qPrintable(QString::number(height)));

    const Mesh mesh = body.mesh();
    QVERIFY(mesh.isValid());
    QVERIFY(qAbs(mesh.bounds().minimum.y()) < 1e-3f);
    QVERIFY(qAbs(static_cast<double>(mesh.bounds().size().y()) - height) < 0.01);

    // Tape measures of an average adult are in a sensible range.
    const double waist = body.measure(QStringLiteral("measure-waist-circ"));
    QVERIFY2(waist > 50 && waist < 110, qPrintable(QString::number(waist)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanModifiersChangeMeasures()
{
    MakeHumanBody body(MakeHumanData::instance());
    const QString waist = QStringLiteral("measure-waist-circ");
    const double before = body.measure(waist);

    body.setModifier(waist, 1.0);
    QVERIFY(body.measure(waist) > before);
    body.setModifier(waist, -1.0);
    QVERIFY(body.measure(waist) < before);

    body.setModifier(waist, 5.0);
    QCOMPARE(body.modifier(waist), MakeHumanBody::modifierLimit);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFit_data()
{
    // Measurements taken from MakeHuman bodies are consistent with what a body can be, so the fit must find them.
    // The bodies differ in gender, age, build and proportions.
    QTest::addColumn<double>("gender");
    QTest::addColumn<double>("ageYears");
    QTest::addColumn<double>("weight");
    QTest::addColumn<double>("muscle");
    QTest::addColumn<double>("height");
    QTest::addColumn<QVector<double>>("modifiers");

    const QStringList names = MakeHumanData::modifierNames();
    const auto modifiers = [&names](std::initializer_list<QPair<const char *, double>> values)
    {
        QVector<double> result(names.size(), 0.0);
        for (const auto &value : values)
        {
            result[names.indexOf(QString::fromLatin1(value.first))] = value.second;
        }
        return result;
    };

    QTest::newRow("woman") << 0.0 << 25.0 << 0.5 << 0.5 << 0.5 << modifiers({});
    QTest::newRow("curvy woman")
        << 0.0 << 38.0 << 0.75 << 0.4 << 0.4
        << modifiers({{"measure-hips-circ", 0.6}, {"measure-waist-circ", -0.3}, {"lowerlegs-height", -0.4},
                      {"breast-trans", -0.3}});
    QTest::newRow("tall man")
        << 1.0 << 45.0 << 0.6 << 0.7 << 0.85
        << modifiers({{"measure-shoulder-dist", 0.5}, {"upperlegs-height", 0.3}, {"measure-neck-circ", 0.4},
                      {"shoulders-height", -0.4}});
    QTest::newRow("child") << 0.5 << 8.0 << 0.5 << 0.5 << 0.5 << modifiers({{"head-scale", 0.3}});
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFit()
{
    QFETCH(double, gender);
    QFETCH(double, ageYears);
    QFETCH(double, weight);
    QFETCH(double, muscle);
    QFETCH(double, height);
    QFETCH(QVector<double>, modifiers);

    const QSharedPointer<const MakeHumanData> data = MakeHumanData::instance();
    MakeHumanMacros macros;
    macros.gender = gender;
    macros.age = MakeHumanMacros::ageFromYears(ageYears);
    macros.weight = weight;
    macros.muscle = muscle;
    macros.height = height;
    if (ageYears < 13)
    {
        macros.breastSize = 0.0; // the fitter gives children a flat chest
    }

    MakeHumanBody source(data);
    source.setMacros(macros);
    source.setModifierValues(modifiers);

    MakeHumanTape tape(*data);
    tape.lockLandmarks(source);
    const QVector<double> taken = tape.measure(source, source.heightCm());

    BodyMeasurements measurements;
    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        measurements.setValue(static_cast<BodyMeasurement>(i), taken.at(i));
    }
    measurements.setGender(gender);
    measurements.setAge(ageYears);
    measurements.setMuscle(muscle);

    QElapsedTimer timer;
    timer.start();
    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    qDebug("fit took %lld ms, largest difference %.3f cm", timer.elapsed(), fit.maxError());

    QVERIFY2(fit.valid, qPrintable(fit.errorString));
    QVERIFY(fit.mesh.isValid());
    if (ageYears >= 13 && gender < 0.5)
    {
        QVERIFY(fit.unfitted().isEmpty());
    }
    else
    {
        // Children and men have no bust: lowbust and bustpoint are left out of the fit.
        QCOMPARE(fit.unfitted(),
                 QVector<BodyMeasurement>({BodyMeasurement::BustpointHeight, BodyMeasurement::UnderbustCirc}));
    }
    // Girths count fully in the fit, landmark heights (and the head) half: they may trade a little for a natural body.
    for (const MakeHumanFit::Entry &entry : fit.entries)
    {
        if (!entry.fitted)
        {
            continue;
        }
        const bool height = BodyMeasurements::group(entry.measurement) == BodyMeasurementGroup::Height
                            || entry.measurement == BodyMeasurement::HeadCirc;
        QVERIFY2(qAbs(entry.achieved - entry.target) < (height ? 0.3 : 0.15),
                 qPrintable(QStringLiteral("%1: %2 instead of %3")
                                .arg(BodyMeasurements::name(entry.measurement))
                                .arg(entry.achieved)
                                .arg(entry.target)));
    }
    QVERIFY(qAbs(static_cast<double>(fit.mesh.bounds().size().y()) - measurements.value(BodyMeasurement::Height))
            < 0.05);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFitDefaultMeasurements()
{
    // The default measurements are MakeHuman's average woman, rounded to 0.5 cm: they fit closely.
    const MakeHumanFit fit = MakeHumanFitter::fit(BodyMeasurements());
    QVERIFY(fit.valid);
    QVERIFY2(fit.maxError() < 0.25, qPrintable(QString::number(fit.maxError())));
    for (const double modifier : fit.modifiers)
    {
        QVERIFY(qAbs(modifier) < 0.5); // no reshaping needed
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFitMaleDefaults()
{
    const BodyMeasurements male = BodyMeasurements::defaults(1.0);
    QCOMPARE(male.gender(), 1.0);
    QVERIFY(male.hasDefaultValues());
    QVERIFY(male.value(BodyMeasurement::Height) > BodyMeasurements().value(BodyMeasurement::Height));
    QVERIFY(BodyMeasurements::defaults(0.0) == BodyMeasurements());

    // The default man fits closely without reshaping, and his chest is not pulled in under the pectorals.
    const MakeHumanFit fit = MakeHumanFitter::fit(male);
    QVERIFY(fit.valid);
    QVERIFY2(fit.maxError() < 0.25, qPrintable(QString::number(fit.maxError())));
    const QStringList names = MakeHumanData::modifierNames();
    QCOMPARE(fit.modifiers.at(names.indexOf(QStringLiteral("measure-underbust-circ"))), 0.0);
    QCOMPARE(fit.modifiers.at(names.indexOf(QStringLiteral("breast-trans"))), 0.0);

    // Even with a female lowbust left in the measurements.
    BodyMeasurements mixed = male;
    mixed.setValue(BodyMeasurement::UnderbustCirc, 69.5);
    QCOMPARE(MakeHumanFitter::fit(mixed).modifiers.at(names.indexOf(QStringLiteral("measure-underbust-circ"))),
             0.0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanTapeMeasuresEverything()
{
    const QSharedPointer<const MakeHumanData> data = MakeHumanData::instance();
    MakeHumanBody body(data);
    MakeHumanTape tape(*data);

    QVERIFY(tape.shoulderPath().size() > 5); // over the back, not straight through the body
    const QVector<double> values = tape.measure(body, 170.0);
    QCOMPARE(values.size(), BodyMeasurements::count());
    for (int i = 0; i < values.size(); ++i)
    {
        QVERIFY2(values.at(i) > 1.0, qPrintable(BodyMeasurements::name(static_cast<BodyMeasurement>(i))));
    }
    QCOMPARE(values.at(static_cast<int>(BodyMeasurement::Height)), 170.0);

    // Heights go down from head to ankle, like on a person.
    const BodyMeasurement order[] = {BodyMeasurement::Height,       BodyMeasurement::NeckBackHeight,
                                     BodyMeasurement::ShoulderTipHeight, BodyMeasurement::ArmpitHeight,
                                     BodyMeasurement::WaistHeight,  BodyMeasurement::HipHeight,
                                     BodyMeasurement::CrotchHeight, BodyMeasurement::KneeHeight,
                                     BodyMeasurement::CalfHeight,   BodyMeasurement::AnkleHeight};
    for (size_t i = 1; i < sizeof(order) / sizeof(order[0]); ++i)
    {
        QVERIFY2(values.at(static_cast<int>(order[i])) < values.at(static_cast<int>(order[i - 1])),
                 qPrintable(BodyMeasurements::name(order[i])));
    }

    // Measuring follows the body: a bigger head has a bigger head circumference.
    body.setModifier(QStringLiteral("head-scale"), 1.0);
    QVERIFY(tape.measure(body, 170.0).at(static_cast<int>(BodyMeasurement::HeadCirc))
            > values.at(static_cast<int>(BodyMeasurement::HeadCirc)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::readVitBirthDate()
{
    const int year = QDate::currentDate().year() - 40;
    QBuffer buffer;
    buffer.setData(QStringLiteral("<smis><unit>cm</unit><personal><birth-date>%1-01-01</birth-date></personal>"
                                  "<body-measurements/></smis>").arg(year).toUtf8());
    buffer.open(QIODevice::ReadOnly);
    BodyMeasurements measurements;
    QVERIFY(measurements.readVit(&buffer));
    QVERIFY(qAbs(measurements.age() - 40) < 1.1);

    // SeamlyMe's "unknown" birth date keeps the default age.
    QBuffer unknown;
    unknown.setData("<smis><unit>cm</unit><personal><birth-date>1800-01-01</birth-date></personal>"
                    "<body-measurements/></smis>");
    unknown.open(QIODevice::ReadOnly);
    BodyMeasurements defaults;
    QVERIFY(defaults.readVit(&unknown));
    QCOMPARE(defaults.age(), 25.0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::sliceByPlane()
{
    Mesh cube;
    QVERIFY(readObj(unitCubeObj, cube));

    // A vertical cut through the middle of the cube is a 1 x 1 square.
    QVERIFY(qFuzzyCompare(MeshSlicer::longestClosedLength(
                              MeshSlicer::sliceByPlane(cube, QVector3D(0.5f, 0.5f, 0.5f), QVector3D(1, 0, 0))),
                          4.0));
    // A diagonal cut through two opposite edges is a 1 x sqrt(2) rectangle.
    QVERIFY(qAbs(MeshSlicer::longestClosedLength(
                     MeshSlicer::sliceByPlane(cube, QVector3D(0.5f, 0.5f, 0.5f), QVector3D(1, -1, 0)))
                 - (2 + 2 * std::sqrt(2.0)))
            < 1e-5);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFitUnreachable()
{
    // No human body has this waist; the fit must still give a body and report the miss.
    const MakeHumanFit fit = MakeHumanFitter::fit(makeBody({{BodyMeasurement::WaistCirc, 250}}));
    QVERIFY(fit.valid);
    QVERIFY(fit.mesh.isValid());
    QVERIFY(fit.maxError() > 1.0);

    QVERIFY(!MakeHumanFitter::fit(BodyMeasurements(), QSharedPointer<const MakeHumanData>()).valid);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanGenderChangesShape()
{
    BodyMeasurements female;
    BodyMeasurements male = female;
    male.setGender(1.0);

    const Mesh a = MakeHumanFitter::fit(female).mesh;
    const Mesh b = MakeHumanFitter::fit(male).mesh;
    QCOMPARE(a.vertexCount(), b.vertexCount());

    double difference = 0;
    for (int i = 0; i < a.vertexCount(); ++i)
    {
        difference = qMax(difference, static_cast<double>((a.positions.at(i) - b.positions.at(i)).length()));
    }
    QVERIFY(difference > 1.0); // centimeters
}

//---------------------------------------------------------------------------------------------------------------------
namespace
{
QPolygonF bodicePiece()
{
    // A simple bodice front, 25 x 45 cm, with a curved armhole and neckline, in pattern coordinates (Y down).
    QPolygonF piece;
    piece << QPointF(0, 0) << QPointF(8, 0) << QPointF(20, 4);
    for (int i = 0; i <= 8; ++i)
    {
        const double a = M_PI / 2 * i / 8;
        piece << QPointF(20 + 5 * std::sin(a), 4 + 14 * (1 - std::cos(a)));
    }
    piece << QPointF(25, 45) << QPointF(0, 45);
    return piece;
}

double meshArea(const Mesh &mesh)
{
    double area = 0;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const QVector3D a = mesh.positions.at(static_cast<int>(mesh.indices.at(i)));
        const QVector3D b = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 1)));
        const QVector3D c = mesh.positions.at(static_cast<int>(mesh.indices.at(i + 2)));
        area += static_cast<double>(QVector3D::crossProduct(b - a, c - a).length()) / 2;
    }
    return area;
}

class FakeProvider : public PatternProvider
{
public:
    QVector<PatternPiece> pieces(QString *) override { return m_pieces; }
    QStringList applyMeasurements(BodyMeasurements &) override { return {}; }
    QMetaObject::Connection watch(QObject *, const std::function<void()> &) override { return {}; }

    QVector<PatternPiece> m_pieces;
};
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::pieceMesherKeepsShape()
{
    const QPolygonF outline = bodicePiece();
    const Mesh mesh = PieceMesher::mesh(outline, 2.5);
    QVERIFY(mesh.isValid());
    QVERIFY(mesh.triangleCount() > 100);

    // Same area as the outline, and a closed surface edge-wise (no T-junctions: every inner edge has two sides).
    QVERIFY(qAbs(meshArea(mesh) - qAbs(PolygonTriangulator::signedArea(outline))) < 1e-3);
    QHash<QPair<quint32, quint32>, int> edges;
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            ++edges[qMakePair(mesh.indices.at(i + k), mesh.indices.at(i + (k + 1) % 3))];
        }
    }
    for (auto it = edges.constBegin(); it != edges.constEnd(); ++it)
    {
        QVERIFY(it.value() == 1); // consistent winding
    }
    const Bounds3D bounds = mesh.bounds();
    QVERIFY(qFuzzyIsNull(bounds.minimum.x()) && qFuzzyCompare(bounds.maximum.y(), 45.0f));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::pieceMesherEdgeLength()
{
    const Mesh mesh = PieceMesher::mesh(bodicePiece(), 2.5);
    for (int i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const QVector3D a = mesh.positions.at(static_cast<int>(mesh.indices.at(i + k)));
            const QVector3D b = mesh.positions.at(static_cast<int>(mesh.indices.at(i + (k + 1) % 3)));
            QVERIFY((a - b).length() <= 2.5f + 1e-4f);
        }
    }
    QVERIFY(PieceMesher::mesh(QPolygonF(), 2.5).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::arrangementGuess_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<int>("placement");
    QTest::newRow("front") << "Front bodice" << static_cast<int>(PiecePlacement::Front);
    QTest::newRow("back") << "Back" << static_cast<int>(PiecePlacement::Back);
    QTest::newRow("sleeve") << "Sleeve" << static_cast<int>(PiecePlacement::LeftSleeve);
    QTest::newRow("german back") << "Rückenteil" << static_cast<int>(PiecePlacement::Back);
    QTest::newRow("collar") << "Collar" << static_cast<int>(PiecePlacement::Aside);
    QTest::newRow("trouser front") << "Trouser front" << static_cast<int>(PiecePlacement::LeftLegFront);
    QTest::newRow("pants back") << "Pants back" << static_cast<int>(PiecePlacement::LeftLegBack);
    QTest::newRow("german trousers") << "Hose Vorderteil" << static_cast<int>(PiecePlacement::LeftLegFront);
    QTest::newRow("skirt front") << "Skirt front" << static_cast<int>(PiecePlacement::LowerFront);
    QTest::newRow("skirt back") << "Skirt back" << static_cast<int>(PiecePlacement::LowerBack);
    QTest::newRow("waistband") << "Skirt waistband" << static_cast<int>(PiecePlacement::Aside);
    QTest::newRow("unknown") << "Piece 7" << static_cast<int>(PiecePlacement::Aside);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::arrangementGuess()
{
    QFETCH(QString, name);
    QFETCH(int, placement);
    QCOMPARE(static_cast<int>(PieceArrangement::guess(name)), placement);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::arrangementWrapsAroundBody()
{
    const QPolygonF outline = bodicePiece();
    const Mesh flat = PieceMesher::mesh(outline, 2.5);
    const BodyFrame frame = BodyFrame::fromMeasurements(BodyMeasurements());

    const QVector<Mesh> front = PieceArrangement::place(flat, outline.boundingRect(), PiecePlacement::Front, false,
                                                        false, frame);
    QCOMPARE(front.size(), 1);
    // In front of the body, at the torso radius, top at the neck.
    QVERIFY(front.constFirst().bounds().maximum.z() > frame.torsoRadius * 0.9f);
    QVERIFY(qAbs(front.constFirst().bounds().maximum.y() - frame.neckHeight) < 1e-3f);
    for (const QVector3D &p : front.constFirst().positions)
    {
        QVERIFY(qAbs(QVector2D(p.x(), p.z()).length() - frame.torsoRadius) < 1e-3f);
    }

    // Bending does not stretch: the area stays the piece's area (within the error of the cylinder's chords).
    QVERIFY(qAbs(meshArea(front.constFirst()) - meshArea(flat)) / meshArea(flat) < 0.01);

    const QVector<Mesh> back = PieceArrangement::place(flat, outline.boundingRect(), PiecePlacement::Back, false,
                                                       false, frame);
    QVERIFY(back.constFirst().bounds().maximum.z() < 0.0f);

    QVERIFY(PieceArrangement::place(flat, outline.boundingRect(), PiecePlacement::Hidden, true, false, frame)
                .isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::arrangementPairs()
{
    const QPolygonF outline = bodicePiece();
    const Mesh flat = PieceMesher::mesh(outline, 2.5);
    const BodyFrame frame = BodyFrame::fromMeasurements(BodyMeasurements());

    // A half front on the fold: the two halves meet at the center line and mirror each other.
    const QVector<Mesh> front = PieceArrangement::place(flat, outline.boundingRect(), PiecePlacement::Front, true,
                                                        false, frame);
    QCOMPARE(front.size(), 2);
    QVERIFY(qAbs(front.at(0).bounds().minimum.x()) < 1e-3f);
    QVERIFY(qAbs(front.at(1).bounds().maximum.x()) < 1e-3f);
    QVERIFY(qAbs(front.at(0).bounds().maximum.x() + front.at(1).bounds().minimum.x()) < 1e-3f);

    // Sleeves cut twice: one each side.
    const QVector<Mesh> sleeves = PieceArrangement::place(flat, outline.boundingRect(), PiecePlacement::LeftSleeve,
                                                          true, false, frame);
    QCOMPARE(sleeves.size(), 2);
    QVERIFY(sleeves.at(0).bounds().center().x() > frame.torsoRadius);
    QVERIFY(sleeves.at(1).bounds().center().x() < -frame.torsoRadius);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::piecesPanelFromProvider()
{
    auto provider = std::make_shared<FakeProvider>();
    PatternPiece front;
    front.id = 3;
    front.name = QStringLiteral("Front");
    front.seamLine = bodicePiece();
    front.onFold = true;
    PatternPiece sleeve;
    sleeve.id = 5;
    sleeve.name = QStringLiteral("Sleeve");
    sleeve.seamLine = bodicePiece();
    sleeve.quantity = 2;
    PatternPiece pocket;
    pocket.id = 7;
    pocket.name = QStringLiteral("Pocket");
    pocket.seamLine = bodicePiece();
    provider->m_pieces = {front, sleeve, pocket};

    PatternPiecesPanel panel;
    QSignalSpy changed(&panel, &PatternPiecesPanel::arrangementChanged);
    panel.setProvider(provider);
    QVERIFY(changed.count() >= 1);
    QCOMPARE(panel.pieces().size(), 3);

    // Guessed placements, pairs for pieces cut twice or on the fold.
    QCOMPARE(panel.settings(3).placement, PiecePlacement::Front);
    QVERIFY(panel.settings(3).pair);
    QCOMPARE(panel.settings(5).placement, PiecePlacement::LeftSleeve);
    QVERIFY(panel.settings(5).pair);
    QCOMPARE(panel.settings(7).placement, PiecePlacement::Aside);
    QVERIFY(!panel.settings(7).pair);

    const BodyFrame frame = BodyFrame::fromMeasurements(BodyMeasurements());
    QCOMPARE(panel.overlays(frame).size(), 5); // 2 fronts, 2 sleeves, 1 pocket

    // Settings survive a reload (the pattern changed in 2D), and hidden pieces are not shown.
    PatternPiecesPanel::Settings hidden = panel.settings(7);
    hidden.placement = PiecePlacement::Hidden;
    panel.setSettings(7, hidden);
    panel.reload();
    QCOMPARE(panel.settings(7).placement, PiecePlacement::Hidden);
    QCOMPARE(panel.overlays(frame).size(), 4);
}

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/** @brief A flat rectangular panel, shape = rest, corners at the four corners. */
GarmentPanel rectanglePanel(double width, double height)
{
    QPolygonF outline;
    outline << QPointF(0, 0) << QPointF(width, 0) << QPointF(width, height) << QPointF(0, height);
    GarmentPanel panel;
    QVector<int> outlineVertex;
    panel.rest = PieceMesher::mesh(outline, 2.5, &outlineVertex);
    panel.shape = panel.rest;
    panel.outlineCount = static_cast<int>(PieceMesher::resample(outline, 2.5).size());
    panel.corners = outlineVertex;
    return panel;
}

//---------------------------------------------------------------------------------------------------------------------
Mesh sphereMesh(const QVector3D &center, float radius, int rings = 24, int segments = 48)
{
    Mesh mesh;
    for (int r = 0; r <= rings; ++r)
    {
        const double theta = M_PI * r / rings;
        for (int s = 0; s < segments; ++s)
        {
            const double phi = 2 * M_PI * s / segments;
            mesh.positions.append(center
                                  + radius * QVector3D(static_cast<float>(std::sin(theta) * std::cos(phi)),
                                                       static_cast<float>(std::cos(theta)),
                                                       static_cast<float>(std::sin(theta) * std::sin(phi))));
        }
    }
    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < segments; ++s)
        {
            const auto a = static_cast<quint32>(r * segments + s);
            const auto b = static_cast<quint32>(r * segments + (s + 1) % segments);
            const auto c = static_cast<quint32>((r + 1) * segments + s);
            const auto d = static_cast<quint32>((r + 1) * segments + (s + 1) % segments);
            mesh.indices << a << b << c << b << d << c;
        }
    }
    mesh.computeNormals();
    return mesh;
}


//---------------------------------------------------------------------------------------------------------------------
/** @brief Half of a T-shirt front or back, center line at x = 0, with a concave neckline and armhole. Edges: neck,
 *  shoulder, armhole, side, hem, center. */
PatternPiece tshirtBody(quint32 id, const QString &name, double neckDrop)
{
    PatternPiece piece;
    piece.id = id;
    piece.name = name;
    piece.onFold = true;
    QPolygonF &outline = piece.seamLine;
    for (int i = 0; i < 8; ++i)
    {
        const double a = M_PI / 2 * i / 8;
        outline << QPointF(7 * std::sin(a), neckDrop * std::cos(a));
    }
    piece.corners << 0 << static_cast<int>(outline.size());
    outline << QPointF(7, 0);
    piece.corners << static_cast<int>(outline.size());
    for (int i = 0; i < 10; ++i)
    {
        const double a = M_PI / 2 * i / 10;
        outline << QPointF(19 + 5.5 * (1 - std::cos(a)), 4 + 18 * std::sin(a));
    }
    piece.corners << static_cast<int>(outline.size());
    outline << QPointF(24.5, 22);
    piece.corners << static_cast<int>(outline.size());
    outline << QPointF(24.5, 62);
    piece.corners << static_cast<int>(outline.size());
    outline << QPointF(0, 62);
    return piece;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief A short sleeve: a cap 34 cm wide and 12 cm high, then 20 cm to the hem. Edges: cap, underarm, hem,
 *  underarm. */
PatternPiece tshirtSleeve(quint32 id)
{
    PatternPiece piece;
    piece.id = id;
    piece.name = QStringLiteral("Sleeve");
    piece.quantity = 2;
    QPolygonF &outline = piece.seamLine;
    for (int i = 0; i < 16; ++i)
    {
        const double x = 34.0 * i / 16;
        outline << QPointF(x, 12 - 12 * std::sin(M_PI * x / 34));
    }
    piece.corners << 0 << static_cast<int>(outline.size());
    outline << QPointF(34, 12);
    piece.corners << static_cast<int>(outline.size());
    outline << QPointF(32, 32);
    piece.corners << static_cast<int>(outline.size());
    outline << QPointF(2, 32);
    return piece;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief A trouser piece for the left leg, cut twice: waist at the top (78 cm round with the other pieces), the
 * crease at x = 0, the crotch extension on the left (wider on the back). Corners: center at the waist, side at the
 * waist, side at the hem, inside at the hem, crotch point.
 */
PatternPiece trouserPiece(quint32 id, const QString &name, bool back)
{
    PatternPiece piece;
    piece.id = id;
    piece.name = name;
    piece.quantity = 2;
    QPolygonF &o = piece.seamLine;
    const double center = back ? -9.5 : -9.9;
    const double crotch = back ? -20.5 : -16.1;
    const double side = back ? 16.5 : 16.1;
    piece.corners << static_cast<int>(o.size());
    o << QPointF(back ? -7 : -8.9, back ? -3 : 0);
    piece.corners << static_cast<int>(o.size());
    o << QPointF(back ? 13.0 : 10.1, 0);
    o << QPointF(side, 18) << QPointF(side, 26) << QPointF(back ? 12 : 11, 60);
    piece.corners << static_cast<int>(o.size());
    o << QPointF(back ? 11 : 10, 100);
    piece.corners << static_cast<int>(o.size());
    o << QPointF(back ? -11 : -10, 100) << QPointF(back ? -12 : -11, 60);
    piece.corners << static_cast<int>(o.size());
    o << QPointF(crotch, 26);
    for (int i = 1; i < 8; ++i)
    {
        const double a = M_PI / 2 * i / 8;
        o << QPointF(crotch + (center - crotch) * std::sin(a), 18 + 8 * std::cos(a));
    }
    o << QPointF(center, 18);
    return piece;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Half a straight skirt cut on the fold, @p length long: 78 cm round the waist, 104 round the hips. */
PatternPiece skirtPiece(quint32 id, const QString &name, double length = 60)
{
    PatternPiece piece;
    piece.id = id;
    piece.name = name;
    piece.onFold = true;
    QPolygonF &o = piece.seamLine;
    piece.corners << 0 << 1 << 3 << 4;
    o << QPointF(0, 0) << QPointF(19.5, 0) << QPointF(26, 20) << QPointF(26 + 6 * (length - 20) / 40, length)
      << QPointF(0, length);
    return piece;
}

enum BodiceEdge
{
    Neck,
    Shoulder,
    Armhole,
    Side,
    Hem,
    Center
};
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::garmentPanelEdges()
{
    const GarmentPanel panel = rectanglePanel(20, 40);
    QCOMPARE(panel.edgeCount(), 4);
    QCOMPARE(panel.outlineCount, 48); // 8 + 16 + 8 + 16 points, 2.5 cm apart

    double perimeter = 0;
    for (int e = 0; e < panel.edgeCount(); ++e)
    {
        const QVector<int> vertices = panel.edgeVertices(e);
        QCOMPARE(vertices.constFirst(), panel.corners.at(e));
        QCOMPARE(vertices.constLast(), panel.corners.at((e + 1) % 4));
        perimeter += panel.edgeLength(e);
    }
    QVERIFY(qAbs(panel.edgeLength(0) - 20) < 1e-4);
    QVERIFY(qAbs(panel.edgeLength(1) - 40) < 1e-4);
    QVERIFY(qAbs(perimeter - 120) < 1e-4);

    // The last edge wraps around to the first corner.
    const QVector<int> last = panel.edgeVertices(3);
    QCOMPARE(last.size(), 17);
    QCOMPARE(last.constLast(), 0);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamFinderSewsBodice()
{
    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {tshirtBody(1, QStringLiteral("Front"), 8), tshirtBody(2, QStringLiteral("Back"), 2)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);

    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(BodyMeasurements()));
    QCOMPARE(panels.size(), 4); // both halves of the front and of the back
    for (const GarmentPanel &panel : panels)
    {
        QCOMPARE(panel.edgeCount(), 6);
        QVERIFY(panel.sewable);
    }

    // Folds, side seams and shoulder seams; armholes, necklines and hems stay open.
    const QVector<Seam> seams = SeamFinder::find(panels);
    QCOMPARE(seams.size(), 6);
    int folds = 0;
    int sides = 0;
    int shoulders = 0;
    for (const Seam &seam : seams)
    {
        QCOMPARE(seam.edgeA, seam.edgeB);
        const GarmentPanel &a = panels.at(seam.panelA);
        const GarmentPanel &b = panels.at(seam.panelB);
        switch (seam.edgeA)
        {
            case Center:
                ++folds;
                QCOMPARE(a.pieceId, b.pieceId); // a half sewn to its mirrored copy
                QVERIFY(a.copy != b.copy);
                break;
            case Side:
                ++sides;
                QVERIFY(a.pieceId != b.pieceId); // front to back
                break;
            case Shoulder:
                ++shoulders;
                QVERIFY(a.pieceId != b.pieceId);
                break;
            default:
                QFAIL(qPrintable(QStringLiteral("edge %1 should not be sewn").arg(seam.edgeA)));
        }
    }
    QCOMPARE(folds, 2);
    QCOMPARE(sides, 2);
    QCOMPARE(shoulders, 2);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamEditorSwitchesAndAdds()
{
    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {tshirtBody(1, QStringLiteral("Front"), 8), tshirtBody(2, QStringLiteral("Back"), 2)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(BodyMeasurements()));
    piecesPanel.updateSeams(panels);
    QCOMPARE(piecesPanel.foundSeams().size(), 6);

    QVector<QColor> colors;
    QCOMPARE(piecesPanel.seams(panels, &colors).size(), 6);
    QCOMPARE(colors.size(), 6);

    auto *list = piecesPanel.findChild<QTreeWidget *>(QStringLiteral("seamList"));
    QVERIFY(list != nullptr);
    QCOMPARE(list->topLevelItemCount(), 6);
    QSignalSpy changed(&piecesPanel, &PatternPiecesPanel::seamsChanged);

    // Unchecking a seam leaves it open.
    list->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(piecesPanel.seams(panels).size(), 5);

    // Switched off seams stay off when the pieces move, until the seams are found again.
    piecesPanel.updateSeams(panels);
    QCOMPARE(piecesPanel.seams(panels).size(), 5);
    QCOMPARE(list->topLevelItem(0)->checkState(0), Qt::Unchecked);

    QPushButton *find = nullptr;
    QPushButton *remove = nullptr;
    for (QPushButton *button : piecesPanel.findChildren<QPushButton *>())
    {
        if (button->text() == QStringLiteral("Find Seams"))
        {
            find = button;
        }
        else if (button->text() == QStringLiteral("Remove"))
        {
            remove = button;
        }
    }
    QVERIFY(find != nullptr && remove != nullptr);
    find->click();
    QCOMPARE(piecesPanel.seams(panels).size(), 6);

    // The two necklines, sewn by hand.
    PatternPiecesPanel::SeamSpec neck;
    neck.pieceA = 1;
    neck.edgeA = Neck;
    neck.pieceB = 2;
    neck.edgeB = Neck;
    piecesPanel.addSeam(neck);
    QCOMPARE(list->topLevelItemCount(), 7);
    const QVector<Seam> withNeck = piecesPanel.seams(panels);
    QCOMPARE(withNeck.size(), 7);
    QCOMPARE(withNeck.constLast().edgeA, static_cast<int>(Neck));
    QCOMPARE(panels.at(withNeck.constLast().panelA).pieceId, quint32(1));
    QCOMPARE(panels.at(withNeck.constLast().panelB).pieceId, quint32(2));

    // Selecting a seam highlights both its edges.
    list->setCurrentItem(list->topLevelItem(6));
    QCOMPARE(piecesPanel.highlightedEdges().size(), 2);
    QCOMPARE(piecesPanel.highlightedEdges().constFirst().second, static_cast<int>(Neck));

    // Removing an added seam deletes it; removing a found one switches it off.
    remove->click();
    QCOMPARE(list->topLevelItemCount(), 6);
    QCOMPARE(piecesPanel.seams(panels).size(), 6);
    list->setCurrentItem(list->topLevelItem(2));
    remove->click();
    QCOMPARE(list->topLevelItemCount(), 6);
    QCOMPARE(piecesPanel.seams(panels).size(), 5);

    // An edge that no longer exists is skipped rather than sewn.
    PatternPiecesPanel::SeamSpec gone = neck;
    gone.edgeA = 40;
    piecesPanel.addSeam(gone);
    QCOMPARE(piecesPanel.seams(panels).size(), 5);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamLinesFollowEdges()
{
    GarmentPanel a = rectanglePanel(20, 40);
    GarmentPanel b = rectanglePanel(20, 40);
    for (QVector3D &position : b.shape.positions)
    {
        position += QVector3D(0, 0, 10); // a parallel copy, 10 cm away
    }
    QCOMPARE(a.edgeCount(), 4);

    // A whole edge: from one corner to the next, in order.
    const QVector<int> edge = a.edgeVertices(1);
    const Viewport3D::LineSet line = PatternPiecesPanel::edgeLine(a, 1, Qt::red, 3.0f);
    QCOMPARE(line.color, QColor(Qt::red));
    QCOMPARE(line.segments.size(), 2 * (edge.size() - 1));
    QVERIFY((line.segments.constFirst() - a.shape.positions.at(edge.constFirst())).length() < 1e-4f);
    QVERIFY((line.segments.constLast() - a.shape.positions.at(edge.constLast())).length() < 1e-4f);

    // Part of an edge: the middle half of the 40 cm side.
    const Viewport3D::LineSet part = PatternPiecesPanel::edgeLine(a, 1, Qt::red, 3.0f, 0.25, 0.75);
    float length = 0;
    for (int i = 0; i + 1 < part.segments.size(); i += 2)
    {
        length += (part.segments.at(i + 1) - part.segments.at(i)).length();
    }
    QVERIFY2(qAbs(length - 20.0f) < 0.01f, qPrintable(QString::number(static_cast<double>(length))));
    QVERIFY((part.segments.constFirst() - QVector3D(20, 10, 0)).length() < 1e-3f);

    // A seam is both edges plus ties between the points that meet.
    Seam seam;
    seam.panelA = 0;
    seam.edgeA = 1;
    seam.panelB = 1;
    seam.edgeB = 1;
    const QVector<Viewport3D::LineSet> lines = PatternPiecesPanel::seamLines({a, b}, {seam}, {Qt::green});
    QCOMPARE(lines.size(), 3);
    for (const Viewport3D::LineSet &set : lines)
    {
        QCOMPARE(set.color, QColor(Qt::green));
    }
    const Viewport3D::LineSet &ties = lines.at(2);
    QCOMPARE(ties.segments.size(), 10);
    for (int i = 0; i < ties.segments.size(); i += 2)
    {
        QVERIFY(qAbs((ties.segments.at(i + 1) - ties.segments.at(i)).length() - 10.0f) < 1e-3f);
    }

    // Sewn the other way round, the ties cross.
    seam.reversed = true;
    const QVector<Viewport3D::LineSet> crossed = PatternPiecesPanel::seamLines({a, b}, {seam}, {Qt::green});
    QVERIFY((crossed.at(2).segments.at(0) - crossed.at(2).segments.at(1)).length() > 40.0f);

    // A seam on a missing panel draws nothing.
    seam.panelB = 5;
    QVERIFY(PatternPiecesPanel::seamLines({a, b}, {seam}, {}).isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamFinderSewsTrousers()
{
    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {trouserPiece(1, QStringLiteral("Trouser front"), false),
                          trouserPiece(2, QStringLiteral("Trouser back"), true)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(BodyMeasurements()));
    QCOMPARE(panels.size(), 4); // both legs, front and back
    for (const GarmentPanel &panel : panels)
    {
        QCOMPARE(panel.edgeCount(), 5);
    }

    // Outseams and inseams on each leg, the center front and center back; waists and hems stay open.
    enum TrouserEdge
    {
        Waist,
        Outseam,
        Hem,
        Inseam,
        Crotch
    };
    const QVector<Seam> seams = SeamFinder::find(panels);
    QCOMPARE(seams.size(), 6);
    int outseams = 0;
    int inseams = 0;
    int crotch = 0;
    for (const Seam &seam : seams)
    {
        QCOMPARE(seam.edgeA, seam.edgeB);
        QVERIFY(!seam.reversed);
        const GarmentPanel &a = panels.at(seam.panelA);
        const GarmentPanel &b = panels.at(seam.panelB);
        switch (seam.edgeA)
        {
            case Outseam:
            case Inseam:
                (seam.edgeA == Outseam ? outseams : inseams)++;
                QVERIFY(a.pieceId != b.pieceId); // front to back, on the same leg
                QCOMPARE(a.copy, b.copy);
                break;
            case Crotch:
                ++crotch;
                QCOMPARE(a.pieceId, b.pieceId); // left to right
                QVERIFY(a.copy != b.copy);
                break;
            default:
                QFAIL(qPrintable(QStringLiteral("edge %1 should not be sewn").arg(seam.edgeA)));
        }
    }
    QCOMPARE(outseams, 2);
    QCOMPARE(inseams, 2);
    QCOMPARE(crotch, 2);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamFinderSewsSkirt()
{
    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {skirtPiece(1, QStringLiteral("Skirt front")), skirtPiece(2, QStringLiteral("Skirt back"))};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(BodyMeasurements()));
    QCOMPARE(panels.size(), 4);

    // The folds and the side seams, which both curve out over the hips. The waist edges meet at the sides, but a
    // waistline is never sewn.
    const QVector<Seam> seams = SeamFinder::find(panels);
    QCOMPARE(seams.size(), 4);
    for (const Seam &seam : seams)
    {
        QCOMPARE(seam.edgeA, seam.edgeB);
        QVERIFY2(seam.edgeA == 1 || seam.edgeA == 3, qPrintable(QString::number(seam.edgeA)));
        if (seam.edgeA == 3)
        {
            QCOMPARE(panels.at(seam.panelA).pieceId, panels.at(seam.panelB).pieceId); // the fold
        }
        else
        {
            QVERIFY(panels.at(seam.panelA).pieceId != panels.at(seam.panelB).pieceId); // front to back
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::arrangementWrapsLegs()
{
    const PatternPiece front = trouserPiece(1, QStringLiteral("Trouser front"), false);
    QVector<int> outlineVertex;
    const Mesh flat = PieceMesher::mesh(front.seamLine, 2.5, &outlineVertex);
    const BodyFrame frame = BodyFrame::fromMeasurements(BodyMeasurements());
    const QVector<Mesh> placed =
        PieceArrangement::place(flat, front.seamLine.boundingRect(), PiecePlacement::LeftLegFront, true, false, frame);
    QCOMPARE(placed.size(), 2);

    // Bending only: the hem keeps its length.
    const auto at = [&outlineVertex](const Mesh &mesh, int corner)
    { return mesh.positions.at(outlineVertex.at(corner)); };
    const int hemSide = front.corners.at(2);
    const int hemInside = front.corners.at(3);
    const Mesh &left = placed.at(0);
    const Mesh &right = placed.at(1);

    // The waist at the top, the hem near the floor; the side seam outside the left leg, the crotch near the middle.
    QVERIFY(qAbs(at(left, 0).y() - (frame.waistHeight + 2)) < 0.01f);
    QVERIFY(at(left, hemSide).y() < 10);
    QVERIFY(at(left, 1).x() > frame.hipRadius * 0.7f);
    QVERIFY(at(left, hemSide).x() > frame.leftKnee.x());
    QVERIFY(at(left, hemInside).x() < frame.leftKnee.x());
    const int crotchPoint = front.corners.at(4);
    QVERIFY(at(left, crotchPoint).x() >= 0.3f - 1e-4f && at(left, crotchPoint).x() < 3);
    QVERIFY(at(left, 0).z() > frame.hipRadius * 0.8f); // the center front, in front

    // Every point of the left leg stays left of the center; the mirrored copy is the right leg.
    for (int v = 0; v < left.positions.size(); ++v)
    {
        QVERIFY(left.positions.at(v).x() > 0);
        QVERIFY(qAbs(right.positions.at(v).x() + left.positions.at(v).x()) < 1e-4f);
        QVERIFY(qAbs(right.positions.at(v).z() - left.positions.at(v).z()) < 1e-4f);
    }

    // The back goes behind.
    const QVector<Mesh> back =
        PieceArrangement::place(flat, front.seamLine.boundingRect(), PiecePlacement::LeftLegBack, false, false, frame);
    QCOMPARE(back.size(), 1);
    QVERIFY(at(back.at(0), 0).z() < -frame.hipRadius * 0.8f);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyFrameFindsLegs()
{
    const BodyMeasurements measurements;
    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    QVERIFY(fit.valid);
    const BodyFrame guessed = BodyFrame::fromMeasurements(measurements);
    const BodyFrame frame = guessed.withLegs(fit.mesh);

    // The left leg of the body: its axis left of the center, its girths near the measurements.
    QVERIFY2(frame.leftHip.x() > 4 && frame.leftHip.x() < 16, qPrintable(QString::number(frame.leftHip.x())));
    QVERIFY(frame.leftKnee.x() > 4 && frame.leftKnee.x() < 20);
    QVERIFY(qAbs(frame.leftHip.y() - frame.crotchHeight) < 1e-3f);
    const auto girth = [&measurements](BodyMeasurement m) { return measurements.value(m) / (2 * M_PI); };
    QVERIFY2(qAbs(frame.kneeRadius - girth(BodyMeasurement::KneeCirc)) < 1.5,
             qPrintable(QStringLiteral("%1 %2").arg(frame.kneeRadius).arg(girth(BodyMeasurement::KneeCirc))));
    QVERIFY(frame.thighRadius > frame.kneeRadius);
    QVERIFY((frame.legAxis(frame.kneeHeight) - frame.leftKnee).length() < 1e-3f);
    QVERIFY(frame.legRadius(frame.ankleHeight) < frame.legRadius(frame.kneeHeight));

    // Nothing to cut: the frame stays as it was.
    QCOMPARE(guessed.withLegs(Mesh()).leftHip, guessed.leftHip);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::clothHoldsSkirtAtWaist()
{
    // A skirt hangs from the waist: its waistline, 78 cm round, cannot slip over hips 99.5 cm round. That takes the
    // waistline's tension, which the solver builds slowly: it used to stretch and slide down.
    const BodyMeasurements measurements;
    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    QVERIFY(fit.valid);

    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {skirtPiece(1, QStringLiteral("Skirt front"), 40),
                          skirtPiece(2, QStringLiteral("Skirt back"), 40)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(measurements));
    ClothSimulator simulator;
    simulator.setBody(fit.mesh);
    simulator.setGarment(panels, SeamFinder::find(panels));
    QCOMPARE(simulator.seamCount(), 4);
    for (int frame = 0; frame < 90; ++frame)
    {
        simulator.step();
    }
    QVERIFY(simulator.isSewn());

    float waist = 0;
    float flat = 0;
    float height = 0;
    int count = 0;
    for (const GarmentPanel &panel : simulator.panels())
    {
        const QVector<int> vertices = panel.edgeVertices(0);
        for (int i = 0; i < vertices.size(); ++i)
        {
            height += panel.shape.positions.at(vertices.at(i)).y();
            ++count;
            if (i > 0)
            {
                const auto segment = [&vertices, i](const Mesh &mesh)
                { return (mesh.positions.at(vertices.at(i)) - mesh.positions.at(vertices.at(i - 1))).length(); };
                waist += segment(panel.shape);
                flat += segment(panel.rest);
            }
        }
    }
    height /= static_cast<float>(count);
    const auto hips = static_cast<float>(measurements.value(BodyMeasurement::HipHeight));
    QVERIFY2(height > hips + 8, qPrintable(QStringLiteral("waistline at %1, hips at %2").arg(height).arg(hips)));
    QVERIFY2(waist < flat * 1.08f, qPrintable(QStringLiteral("waistline %1 cm, %2 flat").arg(waist).arg(flat)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamFinderSewsDart()
{
    // A skirt panel with a waist dart: two 10 cm legs meeting at a sharp point.
    QPolygonF outline;
    outline << QPointF(0, 0) << QPointF(10, 0) << QPointF(12, 10) << QPointF(14, 0) << QPointF(24, 0)
            << QPointF(26, 50) << QPointF(-2, 50);
    GarmentPanel panel;
    QVector<int> outlineVertex;
    panel.rest = PieceMesher::mesh(outline, 2.5, &outlineVertex);
    panel.shape = panel.rest;
    panel.outlineCount = static_cast<int>(PieceMesher::resample(outline, 2.5).size());
    panel.corners = outlineVertex;

    const QVector<Seam> seams = SeamFinder::find({panel});
    QCOMPARE(seams.size(), 1);
    QCOMPARE(qMin(seams.constFirst().edgeA, seams.constFirst().edgeB), 1);
    QCOMPARE(qMax(seams.constFirst().edgeA, seams.constFirst().edgeB), 2);
    QVERIFY(seams.constFirst().reversed);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::bodyColliderInsideOutside()
{
    Mesh sphere = sphereMesh(QVector3D(0, 100, 0), 20);
    for (int flip = 0; flip < 2; ++flip)
    {
        if (flip == 1)
        {
            // Normals and winding pointing in: the collider still knows inside from outside.
            for (int i = 0; i + 2 < sphere.indices.size(); i += 3)
            {
                std::swap(sphere.indices[i + 1], sphere.indices[i + 2]);
            }
        }
        BodyCollider collider;
        collider.build(sphere, 3.0f);
        QVector3D surface;
        QVector3D normal;
        const float outside = collider.closest(QVector3D(0, 121, 0), &surface, &normal);
        QVERIFY2(qAbs(outside - 1.0f) < 0.1f, qPrintable(QString::number(outside)));
        QVERIFY(normal.y() > 0.9f);
        const float inside = collider.closest(QVector3D(19.0f, 100, 0), &surface, &normal);
        QVERIFY2(inside < 0 && inside > -2.0f, qPrintable(QString::number(inside)));
        QVERIFY(normal.x() > 0.9f);
        QCOMPARE(collider.closest(QVector3D(0, 130, 0)), collider.reach()); // out of reach
        QCOMPARE(collider.closest(QVector3D(0, 100, 0)), collider.reach()); // deep inside
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::clothDrapesOverSphere()
{
    // A 60 cm square of cloth dropped on a ball: it hangs over it and does not go through.
    GarmentPanel cloth = rectanglePanel(60, 60);
    for (QVector3D &p : cloth.shape.positions)
    {
        p = QVector3D(p.x() - 30, 125, p.y() - 30);
    }
    const int middle = [&cloth]()
    {
        int best = 0;
        for (int v = 0; v < cloth.shape.positions.size(); ++v)
        {
            if (QVector2D(cloth.shape.positions.at(v).x(), cloth.shape.positions.at(v).z()).length()
                < QVector2D(cloth.shape.positions.at(best).x(), cloth.shape.positions.at(best).z()).length())
            {
                best = v;
            }
        }
        return best;
    }();

    ClothSimulator simulator;
    simulator.setBody(sphereMesh(QVector3D(0, 100, 0), 20));
    simulator.setGarment({cloth}, {});
    QCOMPARE(simulator.particleCount(), cloth.shape.vertexCount());
    for (int frame = 0; frame < 75; ++frame)
    {
        simulator.step();
        QVERIFY2(simulator.maxPenetration() < 0.5f, qPrintable(QString::number(simulator.maxPenetration())));
    }

    const GarmentPanel draped = simulator.panels().constFirst();
    const float top = draped.shape.positions.at(middle).y();
    QVERIFY2(qAbs(top - 120.4f) < 1.0f, qPrintable(QString::number(top))); // resting on the top of the ball
    float lowest = top;
    for (int v = 0; v < cloth.outlineCount; ++v)
    {
        lowest = qMin(lowest, draped.shape.positions.at(v).y());
    }
    QVERIFY2(lowest < 105.0f, qPrintable(QString::number(lowest))); // the edges hang down

    // The fabric does not stretch.
    for (int e = 0; e < cloth.edgeCount(); ++e)
    {
        const QVector<int> vertices = cloth.edgeVertices(e);
        double length = 0;
        for (int i = 1; i < vertices.size(); ++i)
        {
            length += static_cast<double>(
                (draped.shape.positions.at(vertices.at(i)) - draped.shape.positions.at(vertices.at(i - 1))).length());
        }
        QVERIFY2(qAbs(length - 60) < 3.0, qPrintable(QString::number(length)));
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::clothSewsTube()
{
    // Two panels facing each other, 10 cm apart, sewn along both long sides into a tube, without gravity.
    GarmentPanel front = rectanglePanel(20, 40);
    GarmentPanel back = front;
    for (QVector3D &p : front.shape.positions)
    {
        p = QVector3D(p.x() - 10, 100 - p.y(), 5);
    }
    for (QVector3D &p : back.shape.positions)
    {
        p = QVector3D(p.x() - 10, 100 - p.y(), -5);
    }

    const QVector<Seam> seams = SeamFinder::find({front, back});
    QCOMPARE(seams.size(), 2); // both sides; the top is a waistline and the bottom the hem

    ClothSettings settings;
    settings.gravity = 0;
    ClothSimulator simulator;
    simulator.setSettings(settings);
    simulator.setGarment({front, back}, seams);
    QCOMPARE(simulator.seamCount(), 2);
    QVERIFY(simulator.maxSeamGap() > 9.0f);
    QVERIFY(!simulator.isSewn());
    for (int frame = 0; frame < 30 && !simulator.isSewn(); ++frame)
    {
        simulator.step();
    }
    QVERIFY(simulator.isSewn());
    QVERIFY2(simulator.maxSeamGap() < 0.5f, qPrintable(QString::number(simulator.maxSeamGap())));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::seamFinderSetsInSleeves()
{
    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {tshirtBody(1, QStringLiteral("Front"), 8), tshirtBody(2, QStringLiteral("Back"), 2),
                          tshirtSleeve(3)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels = piecesPanel.panels(BodyFrame::fromMeasurements(BodyMeasurements()));
    QCOMPARE(panels.size(), 6);

    // Folds, sides, shoulders and the sleeves' underarm seams; then each sleeve cap in two parts, to the front and
    // back armholes on its side. The necklines and hems stay open.
    const QVector<Seam> seams = SeamFinder::find(panels);
    QCOMPARE(seams.size(), 12);
    int capParts = 0;
    for (const Seam &seam : seams)
    {
        const GarmentPanel &a = panels.at(seam.panelA);
        const GarmentPanel &b = panels.at(seam.panelB);
        if (a.pieceId != 3 || seam.edgeA != 0)
        {
            QCOMPARE(seam.startA, 0.0);
            QCOMPARE(seam.endA, 1.0);
            continue;
        }
        ++capParts;
        QVERIFY(b.pieceId == 1 || b.pieceId == 2);
        QCOMPARE(seam.edgeB, static_cast<int>(Armhole));
        // On the same side of the body.
        QVERIFY(a.shape.bounds().center().x() * b.shape.bounds().center().x() > 0);
        // The cap is shared out by the armholes' lengths, the first part from the start of the cap.
        const double share = b.edgeLength(seam.edgeB) / (b.edgeLength(Armhole) * 2);
        QVERIFY(qAbs((seam.endA - seam.startA) - share) < 0.01);
    }
    QCOMPARE(capParts, 4);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::clothSewsShirtOnBody()
{
    // A T-shirt sewn and draped on the default body: the seams close, the shirt hangs from the shoulders and does
    // not go through the body.
    const BodyMeasurements measurements;
    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    QVERIFY(fit.valid);

    auto provider = std::make_shared<FakeProvider>();
    provider->m_pieces = {tshirtBody(1, QStringLiteral("Front"), 8), tshirtBody(2, QStringLiteral("Back"), 2),
                          tshirtSleeve(3)};
    PatternPiecesPanel piecesPanel;
    piecesPanel.setProvider(provider);
    const QVector<GarmentPanel> panels =
        piecesPanel.panels(BodyFrame::fromMeasurements(measurements).withArm(fit.leftShoulder, fit.leftWrist));
    const QVector<Seam> seams = SeamFinder::find(panels);

    ClothSimulator simulator;
    simulator.setBody(fit.mesh);
    simulator.setGarment(panels, seams);
    QCOMPARE(simulator.seamCount(), 12);
    QElapsedTimer timer;
    timer.start();
    int frames = 0;
    for (; frames < 75; ++frames)
    {
        simulator.step();
    }
    qDebug() << simulator.particleCount() << "particles," << frames << "frames in" << timer.elapsed() << "ms";

    QVERIFY(simulator.isSewn());
    QVERIFY2(simulator.maxSeamGap() < 1.0f, qPrintable(QString::number(simulator.maxSeamGap())));
    QVERIFY2(simulator.maxPenetration() < 1.0f, qPrintable(QString::number(simulator.maxPenetration())));

    float top = std::numeric_limits<float>::lowest();
    for (const GarmentPanel &panel : simulator.panels())
    {
        top = qMax(top, panel.shape.bounds().maximum.y());
    }
    const auto shoulders = static_cast<float>(measurements.value(BodyMeasurement::ShoulderTipHeight));
    QVERIFY2(top > shoulders, qPrintable(QStringLiteral("%1 below the shoulders at %2").arg(top).arg(shoulders)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::measurementsPanelUnits()
{
    // The panel shows the measurements in Seamly2D's units; the body is always in centimeters.
    BodyMeasurementsPanel panel;
    QCOMPARE(panel.unit(), QStringLiteral("cm"));
    const auto heightBox = [&panel]()
    {
        // The first measurement field is the height.
        return panel.findChildren<QDoubleSpinBox *>().constFirst();
    };
    QCOMPARE(heightBox()->value(), 168.0);

    panel.setUnit(QStringLiteral("mm"));
    QCOMPARE(panel.unit(), QStringLiteral("mm"));
    QCOMPARE(heightBox()->value(), 1680.0);
    QVERIFY(heightBox()->suffix().contains(QStringLiteral("mm")));

    panel.setUnit(QStringLiteral("inch"));
    QVERIFY(qAbs(heightBox()->value() - 168.0 / 2.54) < 0.01);

    // Typing in inches still gives the body in centimeters.
    heightBox()->setValue(81);
    QVERIFY(qAbs(panel.measurements().value(BodyMeasurement::Height) - 81 * 2.54) < 1e-6);

    panel.setUnit(QStringLiteral("furlong")); // unknown: keep the current unit
    QCOMPARE(panel.unit(), QStringLiteral("inch"));
    QCOMPARE(FakeProvider().defaultUnit(), QStringLiteral("cm"));
}

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/** @brief Builds DXF text group by group. */
class DxfText
{
public:
    DxfText &operator()(int code, const QString &value)
    {
        m_text += QString::number(code) + QLatin1Char('\n') + value + QLatin1Char('\n');
        return *this;
    }
    DxfText &operator()(int code, double value) { return (*this)(code, QString::number(value, 'g', 12)); }
    DxfText &point(const QString &layer, const QPointF &p)
    {
        return (*this)(0, QStringLiteral("POINT"))(8, layer)(10, p.x())(20, p.y())(30, 0.0);
    }
    DxfText &line(const QString &layer, const QPointF &a, const QPointF &b)
    {
        return (*this)(0, QStringLiteral("LINE"))(8, layer)(10, a.x())(20, a.y())(30, 0.0)(11, b.x())(21, b.y())(31, 0.0);
    }
    DxfText &text(const QString &layer, const QString &value)
    {
        return (*this)(0, QStringLiteral("TEXT"))(8, layer)(10, 0.0)(20, 0.0)(40, 3.0)(1, value);
    }
    /** @brief An R12 POLYLINE: vertices with bulges. */
    DxfText &polyline(const QString &layer, const QVector<QPointF> &points, const QVector<double> &bulges, bool closed)
    {
        (*this)(0, QStringLiteral("POLYLINE"))(8, layer)(66, 1.0)(70, closed ? 1.0 : 0.0);
        for (int i = 0; i < points.size(); ++i)
        {
            (*this)(0, QStringLiteral("VERTEX"))(8, layer)(10, points.at(i).x())(20, points.at(i).y())(30, 0.0)
                (42, bulges.value(i));
        }
        return (*this)(0, QStringLiteral("SEQEND"));
    }
    QByteArray data() const { return m_text.toUtf8(); }

private:
    QString m_text{};
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief An AAMA pattern in millimeters, two pieces.
 *
 * Front: the seam line is a U, 200 wide, its sides 300 long, closed by a half circle (radius 100) bulging up. The cut
 * line is 10 outside it, 30 below (a hem). Turn points at the cut line's lower corners, a notch on the right side,
 * a grain line, an internal line and a drill hole. Inserted at (1000, 500). Sleeve: a cut line only, three lines and a
 * spline, drawn as separate entities.
 */
QByteArray aamaTestDxf()
{
    DxfText dxf;
    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("HEADER"))(9, QStringLiteral("$ACADVER"))(1, QStringLiteral("AC1009"))
        (9, QStringLiteral("$INSUNITS"))(70, 1.0) // says inches; the AAMA UNITS text wins
        (0, QStringLiteral("ENDSEC"));
    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("BLOCKS"));

    dxf(0, QStringLiteral("BLOCK"))(8, QStringLiteral("1"))(2, QStringLiteral("FRONT"))(70, 0.0)(10, 0.0)(20, 0.0)(30, 0.0);
    dxf.polyline(QStringLiteral("1"), {QPointF(-10, -30), QPointF(210, -30), QPointF(210, 300), QPointF(-10, 300)},
                 {0, 0, 1, 0}, true);
    dxf.polyline(QStringLiteral("14"), {QPointF(0, 0), QPointF(200, 0), QPointF(200, 300), QPointF(0, 300)},
                 {0, 0, 1, 0}, true);
    dxf.point(QStringLiteral("2"), QPointF(-10, -30));
    dxf.point(QStringLiteral("2"), QPointF(210, -30));
    dxf.line(QStringLiteral("4"), QPointF(210, 150), QPointF(204, 150));
    dxf.line(QStringLiteral("7"), QPointF(100, 50), QPointF(100, 250));
    dxf.line(QStringLiteral("8"), QPointF(60, 100), QPointF(140, 100));
    dxf.point(QStringLiteral("13"), QPointF(100, 200));
    dxf.text(QStringLiteral("1"), QStringLiteral("Piece Name: Front"));
    dxf.text(QStringLiteral("1"), QStringLiteral("Quantity: 2"));
    dxf(0, QStringLiteral("ENDBLK"))(8, QStringLiteral("1"));

    dxf(0, QStringLiteral("BLOCK"))(8, QStringLiteral("1"))(2, QStringLiteral("SLEEVE"))(70, 0.0)(10, 0.0)(20, 0.0)(30, 0.0);
    dxf.line(QStringLiteral("1"), QPointF(0, 0), QPointF(300, 0));
    dxf.line(QStringLiteral("1"), QPointF(300, 0), QPointF(300, 100));
    dxf.line(QStringLiteral("1"), QPointF(0, 100), QPointF(0, 0));
    // A clamped cubic B-spline with 4 control points is a Bezier curve: from (300, 100) up to (0, 100).
    dxf(0, QStringLiteral("SPLINE"))(8, QStringLiteral("1"))(70, 8.0)(71, 3.0)(72, 8.0)(73, 4.0)(74, 0.0);
    for (const double knot : {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0})
    {
        dxf(40, knot);
    }
    for (const QPointF &p : {QPointF(300, 100), QPointF(250, 220), QPointF(50, 220), QPointF(0, 100)})
    {
        dxf(10, p.x())(20, p.y())(30, 0.0);
    }
    dxf(0, QStringLiteral("ENDBLK"))(8, QStringLiteral("1"));
    dxf(0, QStringLiteral("ENDSEC"));

    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("ENTITIES"));
    dxf(0, QStringLiteral("INSERT"))(8, QStringLiteral("1"))(2, QStringLiteral("FRONT"))(10, 1000.0)(20, 500.0)(30, 0.0);
    dxf(0, QStringLiteral("INSERT"))(8, QStringLiteral("1"))(2, QStringLiteral("SLEEVE"))(10, 1500.0)(20, 500.0)(30, 0.0);
    dxf.text(QStringLiteral("1"), QStringLiteral("ASTM/D6673/10"));
    dxf.text(QStringLiteral("1"), QStringLiteral("UNITS: METRIC"));
    dxf(0, QStringLiteral("ENDSEC"))(0, QStringLiteral("EOF"));
    return dxf.data();
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Distance from @p p to the polyline (closed) @p polygon. */
double distanceToPolygon(const QPointF &p, const QPolygonF &polygon)
{
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < polygon.size(); ++i)
    {
        const QPointF a = polygon.at(i);
        const QPointF b = polygon.at((i + 1) % polygon.size());
        const QPointF ab = b - a;
        const double l2 = QPointF::dotProduct(ab, ab);
        const double t = l2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / l2, 1.0) : 0.0;
        best = qMin(best, QLineF(p, a + ab * t).length());
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The front's seam line in the pattern (cm, Y down, before placing): a U closed by a half circle. */
QPolygonF frontSeamLine(int samples = 720)
{
    QPolygonF seam;
    seam << QPointF(0, 0) << QPointF(20, 0) << QPointF(20, -30);
    for (int i = 1; i < samples; ++i)
    {
        const double a = M_PI * i / samples;
        seam << QPointF(10 + 10 * std::cos(a), -30 - 10 * std::sin(a));
    }
    seam << QPointF(0, -30);
    return seam;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfReaderParsesDrawing()
{
    DxfReader reader;
    DxfDocument document;
    QVERIFY2(reader.parse(aamaTestDxf(), document), qPrintable(reader.errorString()));
    QCOMPARE(document.header.value(QStringLiteral("$INSUNITS")), QStringLiteral("1"));
    QCOMPARE(document.blocks.size(), 2);
    QVERIFY(document.block(QStringLiteral("front")) != nullptr); // names are not case sensitive
    QCOMPARE(document.entities.size(), 4);
    QCOMPARE(document.entities.at(0).type, QStringLiteral("INSERT"));
    QCOMPARE(document.entities.at(0).text, QStringLiteral("FRONT"));

    const DxfBlock *front = document.block(QStringLiteral("FRONT"));
    const DxfEntity &cut = front->entities.at(0);
    QCOMPARE(cut.type, QStringLiteral("POLYLINE"));
    QCOMPARE(cut.layer, QStringLiteral("1"));
    QVERIFY(cut.closed);
    QCOMPARE(cut.points.size(), 4);
    QCOMPARE(cut.bulges.at(2), 1.0);

    QVERIFY(!reader.parse(QByteArray("AutoCAD Binary DXF\r\n\x1a"), document));
    QVERIFY(!reader.parse(QByteArray("hello"), document));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfBulgeArc()
{
    // A bulge of 1 is a half circle, counterclockwise: from (200, 300) over (100, 400) to (0, 300).
    DxfEntity polyline;
    polyline.type = QStringLiteral("LWPOLYLINE");
    polyline.points = {QPointF(200, 300), QPointF(0, 300)};
    polyline.bulges = {1.0, 0.0};
    const QPolygonF points = polyline.flatten(0.01);
    QVERIFY(points.size() > 20);
    QCOMPARE(points.constFirst(), QPointF(200, 300));
    QCOMPARE(points.constLast(), QPointF(0, 300));
    double highest = 0;
    for (const QPointF &p : points)
    {
        QVERIFY(qAbs(QLineF(p, QPointF(100, 300)).length() - 100) < 1e-6);
        highest = qMax(highest, p.y());
    }
    QVERIFY(qAbs(highest - 400) < 0.02);

    // Negative: clockwise, below.
    polyline.bulges = {-1.0, 0.0};
    double lowest = 1000;
    for (const QPointF &p : polyline.flatten(0.01))
    {
        lowest = qMin(lowest, p.y());
    }
    QVERIFY(qAbs(lowest - 200) < 0.02);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfSplineIsBezier()
{
    DxfEntity spline;
    spline.type = QStringLiteral("SPLINE");
    spline.degree = 3;
    spline.knots = {0, 0, 0, 0, 1, 1, 1, 1};
    spline.points = {QPointF(0, 0), QPointF(0, 10), QPointF(10, 10), QPointF(10, 0)};
    const QPolygonF points = spline.flatten(0.01);
    QVERIFY(QLineF(points.constFirst(), QPointF(0, 0)).length() < 1e-9);
    QVERIFY(QLineF(points.constLast(), QPointF(10, 0)).length() < 1e-6);
    // The Bezier's middle: (5, 7.5).
    double best = 1e9;
    for (const QPointF &p : points)
    {
        best = qMin(best, QLineF(p, QPointF(5, 7.5)).length());
    }
    QVERIFY2(best < 0.05, qPrintable(QString::number(best)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfAamaPieces()
{
    DxfReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(aamaTestDxf(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);
    QVERIFY(pattern.astm);
    QCOMPARE(pattern.unit, QStringLiteral("mm"));
    QCOMPARE(pattern.pieces.size(), 2);

    const DxfPiece &front = pattern.pieces.at(0);
    QCOMPARE(front.name, QStringLiteral("Front"));
    QCOMPARE(front.quantity, 2);
    QVERIFY(front.cutLine.size() > 20);
    QVERIFY(front.sewLine.size() > 20);
    // Placed where the block is inserted.
    QVERIFY(qAbs(front.sewLine.boundingRect().left() - 1000) < 1e-6);
    QVERIFY(qAbs(front.sewLine.boundingRect().bottom() - 900) < 0.05);
    QCOMPARE(front.turnPoints.size(), 2);
    QCOMPARE(front.notches.size(), 1);
    QCOMPARE(front.notches.constFirst().p1(), QPointF(1210, 650)); // starts on the outline
    QVERIFY(front.hasGrainline);
    QCOMPARE(front.grainline, QLineF(1100, 550, 1100, 750));
    QCOMPARE(front.internalLines.size(), 1);
    QCOMPARE(front.drillHoles.size(), 1);

    // The sleeve's outline is made of four separate curves, joined.
    const DxfPiece &sleeve = pattern.pieces.at(1);
    QCOMPARE(sleeve.name, QStringLiteral("SLEEVE"));
    QVERIFY(sleeve.sewLine.isEmpty());
    QVERIFY(sleeve.cutLine.size() > 10);
    const QRectF box = sleeve.cutLine.boundingRect();
    QVERIFY(qAbs(box.width() - 300) < 1e-6);
    QVERIFY(qAbs(box.top() - 500) < 1e-6);
    QVERIFY(qAbs(box.bottom() - 690) < 0.1); // the spline's top: 100 + 0.75 * 120
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfPlainOutlines()
{
    // No AAMA layers: every outermost closed outline is a piece; one running parallel inside it is its sew line,
    // anything else inside is an internal line. Units from the header.
    DxfText dxf;
    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("HEADER"))(9, QStringLiteral("$INSUNITS"))(70, 5.0)
        (0, QStringLiteral("ENDSEC"));
    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("ENTITIES"));
    dxf.polyline(QStringLiteral("CUT"), {QPointF(0, 0), QPointF(40, 0), QPointF(40, 60), QPointF(0, 60)}, {}, true);
    dxf.polyline(QStringLiteral("SEW"), {QPointF(1, 1), QPointF(39, 1), QPointF(39, 59), QPointF(1, 59)}, {}, true);
    dxf.polyline(QStringLiteral("DART"), {QPointF(15, 10), QPointF(20, 30), QPointF(25, 10)}, {}, false);
    dxf(0, QStringLiteral("TEXT"))(8, QStringLiteral("0"))(10, 20.0)(20, 40.0)(40, 2.0)(1, QStringLiteral("Back"));
    // A second piece made of lines.
    dxf.line(QStringLiteral("0"), QPointF(100, 0), QPointF(130, 0));
    dxf.line(QStringLiteral("0"), QPointF(130, 0), QPointF(115, 20));
    dxf.line(QStringLiteral("0"), QPointF(100, 0), QPointF(115, 20)); // reversed: still joins
    dxf(0, QStringLiteral("ENDSEC"))(0, QStringLiteral("EOF"));

    DxfReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(dxf.data(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);
    QVERIFY(!pattern.astm);
    QCOMPARE(pattern.unit, QStringLiteral("cm"));
    QCOMPARE(pattern.pieces.size(), 2);
    QCOMPARE(pattern.pieces.at(0).name, QStringLiteral("Back"));
    QCOMPARE(pattern.pieces.at(0).sewLine.size(), 4);
    QCOMPARE(pattern.pieces.at(0).internalLines.size(), 1);
    QCOMPARE(pattern.pieces.at(1).cutLine.size(), 3);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::outlineFitterCornersAndCurves()
{
    // A U closed by a half circle, sampled densely: two corners at the bottom, three straight lines, and the half
    // circle as a few smooth curves meeting the sides without a kink.
    const QPolygonF seam = frontSeamLine();
    OutlineFitter::Settings settings;
    settings.tolerance = 0.05;
    const FittedPath path = OutlineFitter::fit(seam, true, settings);

    QVERIFY2(path.points.size() <= 8, qPrintable(QString::number(path.points.size())));
    QVERIFY(path.maxDeviation <= 0.05);
    int corners = 0;
    for (const FittedPoint &point : path.points)
    {
        corners += point.corner ? 1 : 0;
    }
    QCOMPARE(corners, 2);
    QCOMPARE(path.points.constFirst().position, QPointF(0, 0)); // starts at a corner
    QVERIFY(path.curveCount() >= 2);

    // The fitted outline stays close to the original everywhere.
    for (const QPointF &p : path.polygon(64))
    {
        QVERIFY2(distanceToPolygon(p, seam) < 0.06, qPrintable(QString::number(distanceToPolygon(p, seam))));
    }

    // Smooth where the half circle meets the sides: the curve leaves along the side.
    for (const FittedPoint &point : path.points)
    {
        if (QLineF(point.position, QPointF(20, -30)).length() < 1e-6)
        {
            QVERIFY(!point.lineAfter);
            QVERIFY(qAbs(QLineF(QPointF(), point.handleOut).angle() - 90) < 0.5); // straight up, Y down
        }
    }

    // A notch becomes a point of the outline.
    const FittedPath notched = OutlineFitter::fit(seam, true, settings, {}, {QPointF(20, -15)});
    bool found = false;
    for (const FittedPoint &point : notched.points)
    {
        found = found || (point.notch && QLineF(point.position, QPointF(20, -15)).length() < 1e-9);
    }
    QVERIFY(found);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfConverterWritesValidPattern()
{
    DxfReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(aamaTestDxf(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);

    DxfImportOptions options;
    options.sourceUnit = pattern.unit;
    options.targetUnit = QStringLiteral("cm");
    options.description = QStringLiteral("Imported");
    const QVector<PreparedPiece> pieces = DxfPatternConverter::prepare(pattern, options);
    QCOMPARE(pieces.size(), 2);

    // The front: seam allowance from the file, 1 cm, with 3 cm along the hem.
    const PreparedPiece &front = pieces.at(0);
    QVERIFY(front.seamAllowance);
    QVERIFY(!front.seamAllowanceBuiltIn);
    QVERIFY2(qAbs(front.width - 1.0) < 0.011, qPrintable(QString::number(front.width)));
    QVERIFY(std::any_of(front.segmentWidths.cbegin(), front.segmentWidths.cend(),
                        [](double w) { return qAbs(w - 3.0) < 0.011; }));
    QVERIFY(front.hasGrainline);
    QVERIFY(qAbs(front.grainline.length() - 20) < 1e-6);
    QVERIFY(qAbs(front.grainline.angle() - 90) < 1e-6); // pointing up
    QCOMPARE(front.internalLines.size(), 1);
    QCOMPARE(front.drillHoles.size(), 1);
    QCOMPARE(front.quantity, 2);
    // The sleeve: its cut line, allowance included.
    QVERIFY(pieces.at(1).seamAllowanceBuiltIn);

    const QByteArray xml = DxfPatternConverter::write(pieces, options);
    QDomDocument dom;
    QVERIFY(dom.setContent(xml));
    const QDomElement root = dom.documentElement();
    QCOMPARE(root.tagName(), QStringLiteral("pattern"));
    QCOMPARE(root.firstChildElement(QStringLiteral("version")).text(), DxfPatternConverter::formatVersion());
    QCOMPARE(root.firstChildElement(QStringLiteral("unit")).text(), QStringLiteral("cm"));
    QCOMPARE(root.elementsByTagName(QStringLiteral("draftBlock")).size(), 2);
    QCOMPARE(root.elementsByTagName(QStringLiteral("piece")).size(), 2);
    QCOMPARE(root.firstChildElement(QStringLiteral("draftBlock")).attribute(QStringLiteral("name")),
             QStringLiteral("Front"));

    // Ids are unique; every reference points at something defined.
    QSet<QString> ids;
    const QDomNodeList all = root.elementsByTagName(QStringLiteral("*"));
    for (int i = 0; i < all.size(); ++i)
    {
        const QDomElement element = all.at(i).toElement();
        if (element.hasAttribute(QStringLiteral("id")))
        {
            QVERIFY2(!ids.contains(element.attribute(QStringLiteral("id"))), qPrintable(element.attribute("id")));
            ids.insert(element.attribute(QStringLiteral("id")));
        }
    }
    for (int i = 0; i < all.size(); ++i)
    {
        const QDomElement element = all.at(i).toElement();
        for (const QString &reference : {QStringLiteral("basePoint"), QStringLiteral("idObject"),
                                         QStringLiteral("pSpline"), QStringLiteral("firstPoint"),
                                         QStringLiteral("secondPoint"), QStringLiteral("path")})
        {
            if (element.hasAttribute(reference))
            {
                QVERIFY2(ids.contains(element.attribute(reference)),
                         qPrintable(element.tagName() + QLatin1Char(' ') + reference));
            }
        }
    }

    // A notch, the hem's allowance on its nodes, an internal path and an anchor.
    const QString text = QString::fromUtf8(xml);
    QVERIFY(text.contains(QStringLiteral("notch=\"true\"")));
    QVERIFY(text.contains(QStringLiteral("after=\"3\"")));
    QVERIFY(text.contains(QStringLiteral("before=\"3\"")));
    QVERIFY(text.contains(QStringLiteral("type=\"anchor\"")));
    QVERIFY(text.contains(QStringLiteral("<iPaths>")));

    // Valid against Seamly2D's pattern schema.
    const QString schema = QFINDTESTDATA("../../libs/ifc/schema/pattern/v0.7.5.xsd");
    if (schema.isEmpty() || QStandardPaths::findExecutable(QStringLiteral("xmllint")).isEmpty())
    {
        QSKIP("xmllint or the schema is not available");
    }
    QTemporaryDir dir;
    const QString file = dir.filePath(QStringLiteral("imported.val"));
    QString error;
    QVERIFY2(DxfPatternConverter::convert(pattern, options, file, &error), qPrintable(error));
    QProcess xmllint;
    xmllint.start(QStringLiteral("xmllint"), {QStringLiteral("--noout"), QStringLiteral("--schema"), schema, file});
    QVERIFY(xmllint.waitForFinished(30000));
    QVERIFY2(xmllint.exitCode() == 0, xmllint.readAllStandardError().constData());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfConverterRebuildsOutline()
{
    // Rebuild the front's seam line the way Seamly2D does, from the file alone: the base point, each point at its
    // length and angle from the one before, and the spline paths from their handle angles and lengths. It must lie
    // on the original half circle and sides.
    DxfReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(aamaTestDxf(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);

    for (const bool fromPrevious : {true, false})
    {
        DxfImportOptions options;
        options.sourceUnit = pattern.unit;
        options.targetUnit = QStringLiteral("cm");
        options.fromPreviousPoint = fromPrevious;
        QDomDocument dom;
        QVERIFY(dom.setContent(DxfPatternConverter::write(DxfPatternConverter::prepare(pattern, options), options)));
        const QDomElement calculation = dom.documentElement()
                                            .firstChildElement(QStringLiteral("draftBlock"))
                                            .firstChildElement(QStringLiteral("calculation"));

        QHash<QString, QPointF> points;
        for (QDomElement e = calculation.firstChildElement(QStringLiteral("point")); !e.isNull();
             e = e.nextSiblingElement(QStringLiteral("point")))
        {
            if (e.attribute(QStringLiteral("type")) == QLatin1String("single"))
            {
                points.insert(e.attribute(QStringLiteral("id")),
                              QPointF(e.attribute(QStringLiteral("x")).toDouble(), e.attribute(QStringLiteral("y")).toDouble()));
                continue;
            }
            QCOMPARE(e.attribute(QStringLiteral("type")), QStringLiteral("endLine"));
            const QPointF base = points.value(e.attribute(QStringLiteral("basePoint")));
            QLineF line(base, base + QPointF(100, 0));
            line.setAngle(e.attribute(QStringLiteral("angle")).toDouble());
            line.setLength(e.attribute(QStringLiteral("length")).toDouble());
            points.insert(e.attribute(QStringLiteral("id")), line.p2());
        }

        // The seam line: the outline's points in the piece's node order; lines between consecutive points, curves
        // where a spline path node is.
        QHash<QString, QString> modelOf;
        const QDomElement modeling = calculation.nextSiblingElement(QStringLiteral("modeling"));
        for (QDomElement e = modeling.firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
        {
            modelOf.insert(e.attribute(QStringLiteral("id")), e.attribute(QStringLiteral("idObject")));
        }
        QHash<QString, QDomElement> splines;
        for (QDomElement e = calculation.firstChildElement(QStringLiteral("spline")); !e.isNull();
             e = e.nextSiblingElement(QStringLiteral("spline")))
        {
            splines.insert(e.attribute(QStringLiteral("id")), e);
        }

        QPolygonF rebuilt;
        const QDomElement nodes = dom.documentElement()
                                      .firstChildElement(QStringLiteral("draftBlock"))
                                      .firstChildElement(QStringLiteral("pieces"))
                                      .firstChildElement(QStringLiteral("piece"))
                                      .firstChildElement(QStringLiteral("nodes"));
        for (QDomElement node = nodes.firstChildElement(); !node.isNull(); node = node.nextSiblingElement())
        {
            const QString object = modelOf.value(node.attribute(QStringLiteral("idObject")));
            if (node.attribute(QStringLiteral("type")) == QLatin1String("NodePoint"))
            {
                rebuilt.append(points.value(object));
                continue;
            }
            const QDomElement spline = splines.value(object);
            QVector<QDomElement> pathPoints;
            for (QDomElement p = spline.firstChildElement(); !p.isNull(); p = p.nextSiblingElement())
            {
                pathPoints.append(p);
            }
            for (int k = 0; k + 1 < pathPoints.size(); ++k)
            {
                const QPointF p0 = points.value(pathPoints.at(k).attribute(QStringLiteral("pSpline")));
                const QPointF p3 = points.value(pathPoints.at(k + 1).attribute(QStringLiteral("pSpline")));
                QLineF c1(p0, p0 + QPointF(100, 0));
                c1.setAngle(pathPoints.at(k).attribute(QStringLiteral("angle2")).toDouble());
                c1.setLength(pathPoints.at(k).attribute(QStringLiteral("length2")).toDouble());
                QLineF c2(p3, p3 + QPointF(100, 0));
                c2.setAngle(pathPoints.at(k + 1).attribute(QStringLiteral("angle1")).toDouble());
                c2.setLength(pathPoints.at(k + 1).attribute(QStringLiteral("length1")).toDouble());
                for (int i = 1; i < 32; ++i)
                {
                    const double t = i / 32.0;
                    const double s = 1 - t;
                    rebuilt.append(p0 * (s * s * s) + c1.p2() * (3 * s * s * t) + c2.p2() * (3 * s * t * t)
                                   + p3 * (t * t * t));
                }
            }
        }

        // The original seam line, as placed: the layout puts the cut line's left and top at 0. The cut line is 1 cm
        // left of the seam line, and its half circle 1 cm above (the seam line's top is at -40).
        QPolygonF original = frontSeamLine();
        original.translate(1, 41);
        QVERIFY(rebuilt.size() > 20);
        double worst = 0;
        for (const QPointF &p : rebuilt)
        {
            worst = qMax(worst, distanceToPolygon(p, original));
        }
        QVERIFY2(worst < 0.06, qPrintable(QStringLiteral("%1 cm off (from previous point: %2)").arg(worst).arg(fromPrevious)));
        // and covers all of it
        for (const QPointF &p : original)
        {
            QVERIFY(distanceToPolygon(p, rebuilt) < 0.06);
        }
        // Clockwise on screen (Y down), as Seamly2D wants the main path.
        double area = 0;
        for (int i = 0; i < rebuilt.size(); ++i)
        {
            const QPointF &a = rebuilt.at(i);
            const QPointF &b = rebuilt.at((i + 1) % rebuilt.size());
            area += a.x() * b.y() - b.x() * a.y();
        }
        QVERIFY(area > 0);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfImportWizardConverts()
{
    QTemporaryDir dir;
    const QString dxf = dir.filePath(QStringLiteral("shirt.dxf"));
    {
        QFile file(dxf);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(aamaTestDxf());
    }

    DxfImportWizard wizard(QStringLiteral("inch"));
    QSignalSpy converted(&wizard, &DxfImportWizard::patternConverted);
    wizard.setFileName(dxf);
    wizard.restart();
    wizard.next(); // reads the file
    QCOMPARE(wizard.pattern().pieces.size(), 2);
    QCOMPARE(wizard.options().sourceUnit, QStringLiteral("mm"));
    QCOMPARE(wizard.options().targetUnit, QStringLiteral("inch")); // Seamly2D's units by default
    wizard.next();
    wizard.next(); // save page: next to the DXF, with its name
    QCOMPARE(wizard.outputFileName(), dir.filePath(QStringLiteral("shirt.val")));

    wizard.accept();
    QCOMPARE(converted.count(), 1);
    QCOMPARE(converted.constFirst().at(0).toString(), dir.filePath(QStringLiteral("shirt.val")));
    QVERIFY(converted.constFirst().at(1).toBool());
    QFile result(dir.filePath(QStringLiteral("shirt.val")));
    QVERIFY(result.open(QIODevice::ReadOnly));
    QVERIFY(result.readAll().contains("<unit>inch</unit>"));

    // A file that is not a DXF stays on the first page.
    DxfImportWizard other(QStringLiteral("cm"));
    const QString text = dir.filePath(QStringLiteral("notes.dxf"));
    {
        QFile file(text);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a drawing");
    }
    other.setFileName(text);
    other.restart();
    const int first = other.currentId();
    other.next();
    QCOMPARE(other.currentId(), first);
}

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief An SVG pattern laid out like FreeSewing's: millimeters, a back piece with its seam line and seam allowance
 * line, notches as <use> of a pair of circles, a grain line with markers, text lines in tspans, and transforms.
 */
QByteArray patternSvg()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\""
        " width=\"500mm\" height=\"400mm\" viewBox=\"0 0 500 400\">\n"
        "<defs>\n"
        "  <g id=\"notch\"><circle cx=\"0\" cy=\"0\" r=\"1.4\" class=\"notch\"/><circle cx=\"0\" cy=\"0\" r=\"2.8\"/></g>\n"
        "  <marker id=\"grainlineFrom\" markerWidth=\"6\" markerHeight=\"4\"><path d=\"M 0,2 L 6,0 L 6,4 z\"/></marker>\n"
        "</defs>\n"
        "<g id=\"stack-back\" transform=\"translate(20, 20)\">\n"
        " <g id=\"part-back\" transform=\"translate(10 0)\">\n"
        "  <path class=\"fabric\" d=\"M 0,0 L 200,0 C 200,100 220,200 200,300 L 0,300 Z\"/>\n"
        "  <path class=\"fabric sa\" d=\"M -10,-10 L 210,-10 C 210,100 230,200 210,310 L -10,310 Z\"/>\n"
        "  <path class=\"note grainline\" marker-start=\"url(#grainlineFrom)\" d=\"M 60 40 L 60 260\"/>\n"
        "  <text x=\"100\" y=\"140\" class=\"center\"><tspan>Back</tspan>"
        "<tspan x=\"100\" dy=\"8\">Cut 2 from main fabric</tspan><tspan x=\"100\" dy=\"8\">Devon v4.10.2</tspan></text>\n"
        "  <use xlink:href=\"#notch\" x=\"0\" y=\"150\"/>\n"
        "  <use xlink:href=\"#notch\" x=\"100\" y=\"300\"/>\n"
        "  <circle cx=\"100\" cy=\"60\" r=\"4\" class=\"button\"/>\n"
        "  <rect x=\"40\" y=\"200\" width=\"90\" height=\"40\" class=\"title\"/>"
        "<text x=\"45\" y=\"210\">FreeSewing</text><text x=\"45\" y=\"220\">Devon denim jacket</text>"
        "<text x=\"45\" y=\"230\">Thursday, Oct 16, 2026</text>\n"
        " </g>\n"
        "</g>\n"
        "<g id=\"stack-pocket\"><rect x=\"300\" y=\"20\" width=\"120\" height=\"140\" class=\"fabric\"/>"
        "<text x=\"360\" y=\"90\">Pocket</text><text x=\"360\" y=\"100\">Cut 4</text></g>\n"
        "<g style=\"display:none\"><rect x=\"300\" y=\"200\" width=\"150\" height=\"150\"/></g>\n"
        "</svg>\n");
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::svgPathCommands()
{
    // Relative and absolute lines, H/V, a cubic, a smooth cubic, a quadratic, an arc (flags packed together) and Z.
    const QByteArray svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100mm\" height=\"100mm\" "
                           "viewBox=\"0 0 100 100\"><path d=\"M10 10 h20 v20 H10 Z m 50 0 l10 0 10 10 "
                           "a10 10 0 01-20 0 z M 0 80 C 0 70 20 70 20 80 S 40 90 40 80 Q 50 60 60 80 T 80 80\"/>"
                           "</svg>";
    SvgReader reader;
    DxfDocument document;
    QVERIFY2(reader.parse(svg, document), qPrintable(reader.errorString()));
    QCOMPARE(document.entities.size(), 3);

    const DxfEntity &square = document.entities.at(0);
    QVERIFY(square.closed);
    QCOMPARE(square.points.size(), 4);
    QCOMPARE(square.points.at(2), QPointF(30, -30)); // millimeters, Y up

    // The second: two lines (the coordinates after "l" are more lines), then a half circle of radius 10 back down
    // to (60, 10), bulging below in SVG (Y down), so above (Y up).
    const DxfEntity &arc = document.entities.at(1);
    QVERIFY(arc.closed);
    QCOMPARE(arc.points.at(1), QPointF(70, -10));
    QCOMPARE(arc.points.at(2), QPointF(80, -20));
    double lowest = 0;
    for (const QPointF &p : arc.points)
    {
        lowest = qMin(lowest, p.y());
    }
    QVERIFY2(qAbs(lowest - -30) < 0.05, qPrintable(QString::number(lowest)));

    const DxfEntity &curves = document.entities.at(2);
    QVERIFY(!curves.closed);
    QVERIFY(curves.points.size() > 20);
    QCOMPARE(curves.points.constLast(), QPointF(80, -80));
    // The cubic's middle: (10, 72.5) in SVG.
    double best = 1e9;
    for (const QPointF &p : curves.points)
    {
        best = qMin(best, QLineF(p, QPointF(10, -72.5)).length());
    }
    QVERIFY2(best < 0.1, qPrintable(QString::number(best)));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::svgUnitsAndTransforms()
{
    // 2 inches wide over a 100 unit viewBox: a user unit is 0.508 mm. Without units: 96 pixels per inch.
    SvgReader reader;
    DxfDocument document;
    QVERIFY(reader.parse("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2in\" height=\"2in\" viewBox=\"0 0 100 100\">"
                         "<g transform=\"translate(10,0) scale(2)\"><line x1=\"0\" y1=\"0\" x2=\"10\" y2=\"0\"/></g></svg>",
                         document));
    QCOMPARE(document.entities.size(), 1);
    QVERIFY(qAbs(document.entities.at(0).points.at(0).x() - 10 * 0.508) < 1e-9);
    QVERIFY(qAbs(document.entities.at(0).points.at(1).x() - 30 * 0.508) < 1e-9);

    QVERIFY(reader.parse("<svg xmlns=\"http://www.w3.org/2000/svg\"><rect x=\"0\" y=\"0\" width=\"96\" height=\"48\"/></svg>",
                         document));
    QVERIFY(qAbs(document.entities.at(0).points.at(1).x() - 25.4) < 1e-9);

    QVERIFY(SvgReader::isSvg("  <?xml version=\"1.0\"?><svg/>"));
    QVERIFY(!SvgReader::isSvg("  0\nSECTION\n"));
    QVERIFY(!reader.parse("<html/>", document));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::svgPatternPieces()
{
    SvgReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(patternSvg(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);
    QCOMPARE(pattern.unit, QStringLiteral("mm"));
    QCOMPARE(pattern.pieces.size(), 2); // the hidden rectangle is not drawn

    const DxfPiece &back = pattern.pieces.at(0);
    QCOMPARE(back.name, QStringLiteral("Back"));
    QCOMPARE(back.quantity, 2);
    QVERIFY(!back.sewLine.isEmpty()); // the seam line inside the seam allowance line
    QVERIFY(qAbs(back.cutLine.boundingRect().left() - 20) < 1e-6);
    QCOMPARE(back.notches.size(), 2);
    QVERIFY(back.hasGrainline);
    QVERIFY(qAbs(back.grainline.length() - 220) < 1e-6);
    QCOMPARE(back.drillHoles.size(), 1); // the button
    QVERIFY(back.internalLines.isEmpty());

    const DxfPiece &pocket = pattern.pieces.at(1);
    QCOMPARE(pocket.name, QStringLiteral("Pocket"));
    QCOMPARE(pocket.quantity, 4);

    // Into a pattern: valid, with the seam allowance from the file.
    DxfImportOptions options;
    options.sourceUnit = pattern.unit;
    const QVector<PreparedPiece> pieces = DxfPatternConverter::prepare(pattern, options);
    QCOMPARE(pieces.size(), 2);
    QVERIFY(qAbs(pieces.at(0).width - 1.0) < 0.011);
    QDomDocument dom;
    QVERIFY(dom.setContent(DxfPatternConverter::write(pieces, options)));
    QCOMPARE(dom.elementsByTagName(QStringLiteral("piece")).size(), 2);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::dxfPlainMarks()
{
    // A drawing exported from a PDF: a piece outline drawn as an open polyline ending where it starts, letters as
    // tiny outlines, notches as small circles on the edge, a grain line, a "cut on fold" bracket along the left edge
    // and a stretch of the edge drawn twice.
    DxfText dxf;
    dxf(0, QStringLiteral("SECTION"))(2, QStringLiteral("ENTITIES"));
    dxf.polyline(QStringLiteral("0"), {QPointF(0, 0), QPointF(300, 0), QPointF(300, 500), QPointF(0, 500), QPointF(0, 0)},
                 {}, false);
    dxf.polyline(QStringLiteral("0"), {QPointF(300, 100), QPointF(300, 300)}, {}, false); // the edge again
    for (int letter = 0; letter < 12; ++letter)
    {
        const double x = 100 + letter * 6;
        dxf.polyline(QStringLiteral("0"), {QPointF(x, 250), QPointF(x + 4, 250), QPointF(x + 4, 256), QPointF(x, 256),
                                           QPointF(x, 250)}, {}, false);
    }
    const auto circle = [](const QPointF &center, double radius)
    {
        QVector<QPointF> points;
        for (int i = 0; i <= 24; ++i)
        {
            points.append(center + radius * QPointF(std::cos(2 * M_PI * i / 24), std::sin(2 * M_PI * i / 24)));
        }
        return points;
    };
    dxf.polyline(QStringLiteral("0"), circle(QPointF(300, 200), 3), {}, false); // notch, outside half
    dxf.polyline(QStringLiteral("0"), circle(QPointF(150, 0), 3), {}, false);   // notch on the bottom
    dxf.polyline(QStringLiteral("0"), circle(QPointF(200, 400), 5), {}, false); // a snap
    dxf.polyline(QStringLiteral("0"), {QPointF(150, 60), QPointF(150, 440)}, {}, false);
    dxf.polyline(QStringLiteral("0"), {QPointF(0, 50), QPointF(12, 50), QPointF(12, 450), QPointF(0, 450)}, {}, false);
    // A piece number, large but still letter sized next to the piece.
    dxf.polyline(QStringLiteral("0"), {QPointF(220, 120), QPointF(240, 120), QPointF(240, 145), QPointF(220, 145),
                                       QPointF(220, 120)}, {}, false);
    dxf(0, QStringLiteral("ENDSEC"))(0, QStringLiteral("EOF"));

    DxfReader reader;
    DxfDocument document;
    QVERIFY(reader.parse(dxf.data(), document));
    const DxfPattern pattern = DxfPatternReader::extract(document);
    QCOMPARE(pattern.pieces.size(), 1);
    const DxfPiece &piece = pattern.pieces.constFirst();
    QCOMPARE(piece.cutLine.size(), 4);
    QCOMPARE(piece.notches.size(), 2);
    QCOMPARE(piece.drillHoles.size(), 1);
    QVERIFY(piece.hasGrainline);
    QVERIFY(qAbs(piece.grainline.length() - 380) < 1e-6);
    QVERIFY(piece.hasMirrorLine);
    QVERIFY(qAbs(piece.mirrorLine.length() - 400) < 1e-6);
    QVERIFY2(piece.internalLines.isEmpty(), qPrintable(QString::number(piece.internalLines.size())));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::fabricPresets()
{
    const QVector<FabricPreset> fabrics = FabricPreset::presets();
    QVERIFY(fabrics.size() >= 5);
    QCOMPARE(fabrics.constFirst().id, QStringLiteral("cotton"));
    QSet<QString> ids;
    for (const FabricPreset &fabric : fabrics)
    {
        QVERIFY(!ids.contains(fabric.id));
        ids.insert(fabric.id);
        QVERIFY(!fabric.name.isEmpty());
    }
    // Weight relative to shirting; knits stretch, wovens do not; denim is stiffer than silk.
    QCOMPARE(FabricPreset::preset(QStringLiteral("cotton")).apply(ClothSettings()).weight, 1.0f);
    QVERIFY(FabricPreset::preset(QStringLiteral("denim")).apply(ClothSettings()).weight > 3.0f);
    QVERIFY(FabricPreset::preset(QStringLiteral("jersey")).stretchCompliance > 0);
    QCOMPARE(FabricPreset::preset(QStringLiteral("denim")).stretchCompliance, 0.0f);
    QVERIFY(FabricPreset::preset(QStringLiteral("denim")).bendCompliance
            < FabricPreset::preset(QStringLiteral("silk")).bendCompliance);
    QCOMPARE(FabricPreset::preset(QStringLiteral("no such fabric")).id, QStringLiteral("cotton"));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::fitMapsOnSphere()
{
    // A cloth dropped on a ball: it lies on the ball at the top (no ease) and hangs off it at the corners.
    GarmentPanel cloth = rectanglePanel(60, 60);
    for (QVector3D &p : cloth.shape.positions)
    {
        p = QVector3D(p.x() - 30, 125, p.y() - 30);
    }
    const auto drape = [&cloth](const QString &fabric)
    {
        auto simulator = std::make_unique<ClothSimulator>();
        simulator->setSettings(FabricPreset::preset(fabric).apply(ClothSettings()));
        simulator->setBody(sphereMesh(QVector3D(0, 100, 0), 20));
        simulator->setGarment({cloth}, {});
        for (int frame = 0; frame < 60; ++frame)
        {
            simulator->step();
        }
        return simulator;
    };

    const auto cotton = drape(QStringLiteral("cotton"));
    const QVector<QVector<float>> strain = cotton->strainMap();
    const QVector<QVector<float>> ease = cotton->easeMap();
    QCOMPARE(strain.size(), 1);
    QCOMPARE(strain.constFirst().size(), cloth.shape.positions.size());
    QCOMPARE(ease.constFirst().size(), cloth.shape.positions.size());

    double average = 0;
    for (const float value : strain.constFirst())
    {
        average += static_cast<double>(value);
    }
    average /= strain.constFirst().size();
    QVERIFY2(qAbs(average) < 0.03, qPrintable(QString::number(average)));

    const QVector<GarmentPanel> draped = cotton->panels();
    int top = 0;
    int corner = 0;
    for (int v = 0; v < draped.constFirst().shape.positions.size(); ++v)
    {
        const QVector3D &p = draped.constFirst().shape.positions.at(v);
        if (p.y() > draped.constFirst().shape.positions.at(top).y())
        {
            top = v;
        }
        if (p.y() < draped.constFirst().shape.positions.at(corner).y())
        {
            corner = v;
        }
    }
    QVERIFY2(ease.constFirst().at(top) < 0.2f, qPrintable(QString::number(ease.constFirst().at(top))));
    QVERIFY2(ease.constFirst().at(corner) > 2.0f, qPrintable(QString::number(ease.constFirst().at(corner))));

    // A knit stretches more under its own weight than leather.
    const auto averageStrain = [](const ClothSimulator &simulator)
    {
        double sum = 0;
        int count = 0;
        for (const QVector<float> &panel : simulator.strainMap())
        {
            for (const float value : panel)
            {
                sum += static_cast<double>(value);
                ++count;
            }
        }
        return sum / qMax(1, count);
    };
    QVERIFY(averageStrain(*drape(QStringLiteral("jersey"))) > averageStrain(*drape(QStringLiteral("leather"))));
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::fitMapColors()
{
    // Stretch: blue pushed together, green relaxed, red pulled tight. Ease: red on the body, blue far from it.
    QVERIFY(FitMap::color(FitMap::Kind::Stretch, -0.1f).blue() > 200);
    QVERIFY(FitMap::color(FitMap::Kind::Stretch, 0.0f).green() > 150);
    QVERIFY(FitMap::color(FitMap::Kind::Stretch, 0.2f).red() > 200);
    QVERIFY(FitMap::color(FitMap::Kind::Ease, 0.0f).red() > 200);
    QVERIFY(FitMap::color(FitMap::Kind::Ease, 10.0f).blue() > 200);
    // In between: blended.
    const QColor middle = FitMap::color(FitMap::Kind::Stretch, 0.015f);
    QVERIFY(middle.red() > 60 && middle.red() < 240);
    QCOMPARE(FitMap::colors(FitMap::Kind::Ease, {0.0f, 1.0f, 2.0f}).size(), 3);
    const QImage legend = FitMap::legend(FitMap::Kind::Ease, 100, 8);
    QCOMPARE(legend.size(), QSize(100, 8));
    QVERIFY(QColor(legend.pixel(0, 4)).red() > 200);
    QVERIFY(QColor(legend.pixel(99, 4)).blue() > 200);
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::piecesHaveSides()
{
    // A side seam with a point at the waist (a slight bend) and a notch, and a hem running straight into a curve:
    // the points along them do not split them. Real corners do.
    QPolygonF outline;
    QVector<int> corners;
    corners << static_cast<int>(outline.size());
    outline << QPointF(0, 0);                        // top left: corner
    corners << static_cast<int>(outline.size());
    outline << QPointF(20, 0);                       // top right: corner
    corners << static_cast<int>(outline.size());
    outline << QPointF(21, 20);                      // waist: a 3 degree bend
    corners << static_cast<int>(outline.size());
    outline << QPointF(22, 30);                      // notch on the side seam
    corners << static_cast<int>(outline.size());
    outline << QPointF(23, 40);                      // bottom right: corner
    corners << static_cast<int>(outline.size());
    outline << QPointF(8, 40);                       // the hem runs into a curve here, smoothly
    for (int i = 1; i <= 16; ++i)
    {
        const double a = M_PI / 2 * i / 16;
        outline << QPointF(8 - 8 * std::sin(a), 40 - 8 * (1 - std::cos(a)));
    }
    // ends at (0, 32), going up: then straight up the left side to the start

    const QVector<int> sides = PieceMesher::sides(outline, corners);
    QCOMPARE(sides, QVector<int>({0, 1, 4}));
    QVERIFY(PieceMesher::turningAngle(outline, 0, 1.0) > 80);
    QVERIFY(PieceMesher::turningAngle(outline, 2, 1.0) < 5);
    QVERIFY(PieceMesher::turningAngle(outline, 5, 1.0) < 10); // the curve leaves along the hem
}

QTEST_MAIN(TST_Seamly3D)
#include "tst_seamly3d.moc"
