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
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QVector2D>
#include <QtTest>

#include "bodymeasurements.h"
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

QTEST_MAIN(TST_Seamly3D)
#include "tst_seamly3d.moc"
