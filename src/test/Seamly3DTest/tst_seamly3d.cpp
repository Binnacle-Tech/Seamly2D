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

#include "mesh.h"
#include "objmeshreader.h"
#include "orbitcamera.h"
#include "polygontriangulator.h"

using namespace Seamly3D;

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

QTEST_GUILESS_MAIN(TST_Seamly3D)
#include "tst_seamly3d.moc"
