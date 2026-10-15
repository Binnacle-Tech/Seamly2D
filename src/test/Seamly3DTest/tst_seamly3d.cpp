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

    // The defaults are a consistent body.
    QVERIFY(BodyMeasurements().sanitized() == BodyMeasurements());
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::scaleToHeight()
{
    BodyMeasurements measurements;
    measurements.scaleToHeight(BodyMeasurements::defaultValue(BodyMeasurement::Height) * 1.1);

    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::WaistHeight),
                          BodyMeasurements::defaultValue(BodyMeasurement::WaistHeight) * 1.1));
    QVERIFY(qFuzzyCompare(measurements.value(BodyMeasurement::ArmLength),
                          BodyMeasurements::defaultValue(BodyMeasurement::ArmLength) * 1.1));
    // Girths are not lengths: they stay.
    QCOMPARE(measurements.value(BodyMeasurement::WaistCirc),
             BodyMeasurements::defaultValue(BodyMeasurement::WaistCirc));
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

    // Formulas are skipped (default stays) and missing heights follow the person's height.
    QCOMPARE(measurements.value(BodyMeasurement::HipCirc), BodyMeasurements::defaultValue(BodyMeasurement::HipCirc));
    const double scale = 70 * 2.54 / BodyMeasurements::defaultValue(BodyMeasurement::Height);
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
    QVERIFY(data->targets.size() > 100);

    // Every measure modifier and its ruler are there.
    for (const QString &name : MakeHumanBody::modifierNames())
    {
        QVERIFY2(data->indexOf(QStringLiteral("measure/") + name + QStringLiteral("-incr")) >= 0, qPrintable(name));
        QVERIFY2(data->indexOf(QStringLiteral("measure/") + name + QStringLiteral("-decr")) >= 0, qPrintable(name));
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
    QHash<QString, double> f = macros.factors();
    QCOMPARE(f.value(QStringLiteral("female")), 0.5);
    QCOMPARE(f.value(QStringLiteral("young")), 1.0);
    QCOMPARE(f.value(QStringLiteral("old")), 0.0);
    QCOMPARE(f.value(QStringLiteral("averageweight")), 1.0);
    QCOMPARE(f.value(QStringLiteral("maxheight")), 0.0);
    QVERIFY(qFuzzyCompare(f.value(QStringLiteral("caucasian")), 1.0 / 3.0));

    macros.weight = 0.75;
    macros.height = 0.0;
    macros.age = 0.25;
    f = macros.factors();
    QCOMPARE(f.value(QStringLiteral("maxweight")), 0.5);
    QCOMPARE(f.value(QStringLiteral("averageweight")), 0.5);
    QCOMPARE(f.value(QStringLiteral("minheight")), 1.0);
    QVERIFY(qAbs(f.value(QStringLiteral("young")) - 0.2) < 1e-9);
    QVERIFY(qAbs(f.value(QStringLiteral("child")) - 0.8) < 1e-9);
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
    QTest::addColumn<BodyMeasurements>("measurements");

    QTest::newRow("default") << BodyMeasurements();

    BodyMeasurements man;
    man.setGender(1.0);
    man.scaleToHeight(182);
    man.setValue(BodyMeasurement::NeckCirc, 40);
    man.setValue(BodyMeasurement::BustCirc, 102);
    man.setValue(BodyMeasurement::UnderbustCirc, 92);
    man.setValue(BodyMeasurement::WaistCirc, 88);
    man.setValue(BodyMeasurement::HipCirc, 100);
    man.setValue(BodyMeasurement::UpperArmCirc, 32);
    man.setValue(BodyMeasurement::WristCirc, 17.5);
    man.setValue(BodyMeasurement::ArmLength, 63);
    QTest::newRow("man") << man;

    BodyMeasurements petite;
    petite.scaleToHeight(155);
    petite.setValue(BodyMeasurement::BustCirc, 108);
    petite.setValue(BodyMeasurement::UnderbustCirc, 90);
    petite.setValue(BodyMeasurement::WaistCirc, 90);
    petite.setValue(BodyMeasurement::HipCirc, 114);
    petite.setValue(BodyMeasurement::ThighCirc, 64);
    QTest::newRow("petite plus") << petite;
}

//---------------------------------------------------------------------------------------------------------------------
void TST_Seamly3D::makeHumanFit()
{
    QFETCH(BodyMeasurements, measurements);

    const MakeHumanFit fit = MakeHumanFitter::fit(measurements);
    QVERIFY2(fit.valid, qPrintable(fit.errorString));
    QVERIFY(fit.mesh.isValid());

    for (const MakeHumanFit::Entry &entry : fit.entries)
    {
        if (entry.fitted)
        {
            QVERIFY2(qAbs(entry.achieved - entry.target) < 0.1,
                     qPrintable(QStringLiteral("%1: %2 instead of %3")
                                    .arg(BodyMeasurements::name(entry.measurement))
                                    .arg(entry.achieved)
                                    .arg(entry.target)));
        }
    }
    QVERIFY(fit.maxError() < 0.1);

    // Height, bust, waist and hip are among the fitted measurements.
    QVERIFY(!fit.unfitted().contains(BodyMeasurement::Height));
    QVERIFY(!fit.unfitted().contains(BodyMeasurement::BustCirc));
    QVERIFY(!fit.unfitted().contains(BodyMeasurement::WaistCirc));
    QVERIFY(!fit.unfitted().contains(BodyMeasurement::HipCirc));
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

QTEST_GUILESS_MAIN(TST_Seamly3D)
#include "tst_seamly3d.moc"
