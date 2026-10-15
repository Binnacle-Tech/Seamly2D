/******************************************************************************
 *   @file   makehumanfitter.cpp
 **  @brief  Fits a MakeHuman body to body measurements.
 **
 **  Ported to C++ for Seamly2D from MakeHuman 1.x
 **  (https://github.com/makehumancommunity/makehuman), the bisection of plugins/0_modeling_a_measurement.py (MeasurementValueConverter).
 **
 **  @copyright
 **  Copyright (C) 2001-2020  MakeHuman Team (www.makehumancommunity.org)
 **  Copyright (C) 2026 Seamly2D project (C++ port and changes)
 **
 **  This program is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU Affero General Public License as
 **  published by the Free Software Foundation, either version 3 of the
 **  License, or (at your option) any later version.
 **
 **  This program is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU Affero General Public License for more details.
 **
 **  You should have received a copy of the GNU Affero General Public License
 **  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 **  It is combined with the GPL-3.0 parts of Seamly2D as permitted by
 **  section 13 of both licenses.
 **
 *****************************************************************************/

#include "makehumanfitter.h"

#include <QSet>
#include <QtMath>

#include <cmath>
#include <functional>
#include <limits>
#include <queue>

#include "../meshslicer.h"

namespace Seamly3D
{

namespace
{

// Landmark vertices of the MakeHuman base mesh, found by analysing its geometry: the nape (back of the neck at the
// base, also MakeHuman's nape-to-waist ruler start), the left shoulder tip (MakeHuman's shoulder ruler end), the
// highest point of the left armpit (where a level cut first separates the arm from the torso), the crotch (where a
// level cut first separates the legs) and the front of the head at its widest level.
const int napeVertex = 1491;
const int shoulderTipVertex = 8274;
const int armpitVertex = 8314;
const int crotchVertex = 6431;
const int headLevelVertex = 131;
const int neckSideVertex = 7478;      // left side of the neck base (MakeHuman's shoulder ruler start)
const int neckFrontVertex = 1522;     // front of the neck base, where the chest turns into the neck
const float neckRegionMinY = 5.2f, neckRegionMaxY = 6.1f, neckRegionMaxX = 0.8f;
// MakeHuman's arm rulers: shoulder tip to elbow (8274 -> 10037), elbow to wrist (10040 -> 10548).
const int elbowUpperVertex = 10037;
const int elbowLowerVertex = 10040;
const int wristVertex = 10548;
const float elbowRadius = 1.2f; // decimeters around the elbow that the elbow cut may cross

// Fitting: weight of landmark heights relative to girths, and the cost of a modifier at value 1, in cm of girth error.
// Tuned on bodies with consistent and with made-up measurements: girths still land within about 0.1 cm while the
// body stays free of the creases that extreme modifier values make.
const double heightWeight = 0.5;
const double minBreastAge = 13.0; // years
const double regularization = 0.5;

// Base mesh regions in decimeters (Y up, facing +Z).
const float bustMinX = 0.45f, bustMaxX = 1.4f, bustMinY = 2.8f, bustMaxY = 4.4f, bustMinZ = 0.6f;
const float extremeBand = 0.5f; // vertices this close to the lowest/highest point can be the floor/top
const float headBand = 0.6f;    // half height of the head band that is sliced for the head circumference

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief bisect finds x in [low, high] where f(x) = 0, for f monotonic in either direction. When there is no root in
 * the range it returns the end that comes closest.
 */
double bisect(const std::function<double(double)> &f, double low, double high, double tolerance, int iterations = 40)
{
    double fLow = f(low);
    double fHigh = f(high);
    if ((fLow > 0) == (fHigh > 0))
    {
        return qAbs(fLow) < qAbs(fHigh) ? low : high;
    }

    for (int i = 0; i < iterations; ++i)
    {
        const double middle = (low + high) / 2;
        const double fMiddle = f(middle);
        if (qAbs(fMiddle) < tolerance)
        {
            return middle;
        }
        if ((fMiddle > 0) == (fLow > 0))
        {
            low = middle;
            fLow = fMiddle;
        }
        else
        {
            high = middle;
        }
    }
    return (low + high) / 2;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief solve solves the square system a * x = b by Gaussian elimination with partial pivoting. */
QVector<double> solve(QVector<QVector<double>> a, QVector<double> b)
{
    const int n = static_cast<int>(b.size());
    for (int column = 0; column < n; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < n; ++row)
        {
            if (qAbs(a.at(row).at(column)) > qAbs(a.at(pivot).at(column)))
            {
                pivot = row;
            }
        }
        std::swap(a[column], a[pivot]);
        std::swap(b[column], b[pivot]);
        if (qAbs(a.at(column).at(column)) < 1e-12)
        {
            continue;
        }
        for (int row = column + 1; row < n; ++row)
        {
            const double factor = a.at(row).at(column) / a.at(column).at(column);
            for (int k = column; k < n; ++k)
            {
                a[row][k] -= factor * a.at(column).at(k);
            }
            b[row] -= factor * b.at(column);
        }
    }

    QVector<double> x(n, 0.0);
    for (int row = n - 1; row >= 0; --row)
    {
        if (qAbs(a.at(row).at(row)) < 1e-12)
        {
            continue;
        }
        double sum = b.at(row);
        for (int k = row + 1; k < n; ++k)
        {
            sum -= a.at(row).at(k) * x.at(k);
        }
        x[row] = sum / a.at(row).at(row);
    }
    return x;
}

//---------------------------------------------------------------------------------------------------------------------
int mirrorVertex(const QVector<QVector3D> &positions, int vertex)
{
    const QVector3D target(-positions.at(vertex).x(), positions.at(vertex).y(), positions.at(vertex).z());
    int best = vertex;
    float bestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < positions.size(); ++i)
    {
        const float distance = (positions.at(i) - target).lengthSquared();
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief shortestPath runs Dijkstra over the mesh edges, using only vertices @p allowed accepts. */
QVector<int> shortestPath(const MakeHumanData &data, int from, int to, const std::function<bool(int)> &allowed)
{
    const QVector<QVector3D> &p = data.basePositions;
    const int n = static_cast<int>(p.size());

    QVector<QVector<int>> neighbours(n);
    for (int i = 0; i + 2 < data.triangles.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const auto a = static_cast<int>(data.triangles.at(i + k));
            const auto b = static_cast<int>(data.triangles.at(i + (k + 1) % 3));
            if (allowed(a) && allowed(b))
            {
                neighbours[a].append(b);
                neighbours[b].append(a);
            }
        }
    }

    QVector<double> distance(n, std::numeric_limits<double>::max());
    QVector<int> previous(n, -1);
    using Item = std::pair<double, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
    distance[from] = 0;
    queue.push({0, from});
    while (!queue.empty())
    {
        const auto [d, v] = queue.top();
        queue.pop();
        if (d > distance.at(v))
        {
            continue;
        }
        if (v == to)
        {
            break;
        }
        for (const int w : neighbours.at(v))
        {
            const double candidate = d + static_cast<double>((p.at(w) - p.at(v)).length());
            if (candidate < distance.at(w))
            {
                distance[w] = candidate;
                previous[w] = v;
                queue.push({candidate, w});
            }
        }
    }

    if (previous.at(to) < 0)
    {
        return {from, to}; // no path inside the region: measure straight across
    }

    QVector<int> path;
    for (int v = to; v >= 0; v = previous.at(v))
    {
        path.prepend(v);
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
double polylineLength(const QVector<QVector3D> &positions, const QVector<int> &vertices)
{
    double length = 0;
    for (int i = 1; i < vertices.size(); ++i)
    {
        length += static_cast<double>((positions.at(vertices.at(i)) - positions.at(vertices.at(i - 1))).length());
    }
    return length;
}

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief smoothLength measures a path found along mesh edges. Edge paths zigzag and come out about 10% longer than a
 * tape laid on the surface; a few rounds of moving each inner point to the middle of its neighbours straighten them.
 * Fixed points (the landmarks the path goes through) do not move.
 */
double smoothLength(const QVector<QVector3D> &positions, const QVector<int> &vertices, const QSet<int> &fixed,
                    bool closed)
{
    QVector<QVector3D> points;
    points.reserve(vertices.size());
    for (const int v : vertices)
    {
        points.append(positions.at(v));
    }
    if (closed && points.size() > 1)
    {
        points.removeLast(); // the first vertex is repeated at the end
    }

    const int n = static_cast<int>(points.size());
    for (int round = 0; round < 4 && n > 2; ++round)
    {
        QVector<QVector3D> smoothed = points;
        for (int i = 0; i < n; ++i)
        {
            const bool end = !closed && (i == 0 || i == n - 1);
            const int vertex = vertices.at(i);
            if (end || fixed.contains(vertex))
            {
                continue;
            }
            const QVector3D &before = points.at((i + n - 1) % n);
            const QVector3D &after = points.at((i + 1) % n);
            smoothed[i] = (before + after) / 2;
        }
        points = smoothed;
    }

    double length = 0;
    for (int i = 1; i < n; ++i)
    {
        length += static_cast<double>((points.at(i) - points.at(i - 1)).length());
    }
    if (closed && n > 2)
    {
        length += static_cast<double>((points.constFirst() - points.constLast()).length());
    }
    return length;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The girth measurements and the rulers (summed) that measure them. */
struct RulerMeasure
{
    BodyMeasurement measurement;
    QStringList     rulers;
};

const QVector<RulerMeasure> &rulerMeasures()
{
    static const QVector<RulerMeasure> table = {
        {BodyMeasurement::BustCirc, {QStringLiteral("measure-bust-circ")}},
        {BodyMeasurement::UnderbustCirc, {QStringLiteral("measure-underbust-circ")}},
        {BodyMeasurement::WaistCirc, {QStringLiteral("measure-waist-circ")}},
        {BodyMeasurement::HipCirc, {QStringLiteral("measure-hips-circ")}},
        {BodyMeasurement::ThighCirc, {QStringLiteral("measure-thigh-circ")}},
        {BodyMeasurement::KneeCirc, {QStringLiteral("measure-knee-circ")}},
        {BodyMeasurement::CalfCirc, {QStringLiteral("measure-calf-circ")}},
        {BodyMeasurement::AnkleCirc, {QStringLiteral("measure-ankle-circ")}},
        {BodyMeasurement::WristCirc, {QStringLiteral("measure-wrist-circ")}},
        {BodyMeasurement::ArmLength,
         {QStringLiteral("measure-upperarm-length"), QStringLiteral("measure-lowerarm-length")}},
    };
    return table;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
MakeHumanTape::MakeHumanTape(const MakeHumanData &data)
{
    const QVector<QVector3D> &base = data.basePositions;
    const int n = static_cast<int>(base.size());

    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const QVector3D &p : base)
    {
        minY = qMin(minY, p.y());
        maxY = qMax(maxY, p.y());
    }

    QSet<int> probe;
    for (int i = 0; i < n; ++i)
    {
        const QVector3D &p = base.at(i);
        if (p.y() < minY + extremeBand)
        {
            m_floor.append(i);
        }
        if (p.y() > maxY - extremeBand)
        {
            m_top.append(i);
        }
        if (p.x() > bustMinX && p.x() < bustMaxX && p.y() > bustMinY && p.y() < bustMaxY && p.z() > bustMinZ)
        {
            m_bust.append(i);
        }
    }

    m_armpitMirror = mirrorVertex(base, armpitVertex);
    m_shoulderMirror = mirrorVertex(base, shoulderTipVertex);

    // Shoulder tip to shoulder tip over the back: stay behind the tips and above the armpits.
    const float backZ = qMax(base.at(shoulderTipVertex).z(), base.at(m_shoulderMirror).z()) + 0.05f;
    const float lowestY = base.at(armpitVertex).y();
    m_shoulderPath = shortestPath(data, shoulderTipVertex, m_shoulderMirror,
                                  [&base, backZ, lowestY](int v)
                                  { return base.at(v).z() <= backZ && base.at(v).y() >= lowestY; });

    // Neck base: shortest paths nape -> left side -> front -> right side -> nape, staying on the neck.
    const auto onNeck = [&base](int v)
    {
        const QVector3D &q = base.at(v);
        return q.y() > neckRegionMinY && q.y() < neckRegionMaxY && qAbs(q.x()) < neckRegionMaxX;
    };
    const int neckSideMirror = mirrorVertex(base, neckSideVertex);
    const int corners[] = {napeVertex, neckSideVertex, neckFrontVertex, neckSideMirror, napeVertex};
    m_neckCorners = {napeVertex, neckSideVertex, neckFrontVertex, neckSideMirror};
    for (int c = 0; c + 1 < 5; ++c)
    {
        QVector<int> segment = shortestPath(data, corners[c], corners[c + 1], onNeck);
        if (!m_neckBasePath.isEmpty())
        {
            segment.removeFirst(); // shared corner
        }
        m_neckBasePath += segment;
    }

    // Head triangles near the level of its widest part, left arm triangles around the elbow and around the armpit.
    const float headY = base.at(headLevelVertex).y();
    const QVector3D elbow = (base.at(elbowUpperVertex) + base.at(elbowLowerVertex)) / 2;
    const float armMinX = base.at(armpitVertex).x() - 0.05f;
    const float armMaxY = base.at(shoulderTipVertex).y() + 0.3f;
    const float armMinY = base.at(armpitVertex).y() - 1.2f;
    m_positions = QVector<QVector3D>(n);
    for (int i = 0; i + 2 < data.triangles.size(); i += 3)
    {
        bool head = true;
        bool nearElbow = true;
        bool upperArm = true;
        for (int k = 0; k < 3; ++k)
        {
            const QVector3D &v = base.at(static_cast<int>(data.triangles.at(i + k)));
            head = head && qAbs(v.y() - headY) < headBand;
            nearElbow = nearElbow && (v - elbow).length() < elbowRadius;
            upperArm = upperArm && v.x() > armMinX && v.y() < armMaxY && v.y() > armMinY;
        }
        for (const auto &[inside, list] : {std::make_pair(head, &m_headTriangles),
                                           std::make_pair(nearElbow, &m_elbowTriangles),
                                           std::make_pair(upperArm, &m_upperArmTriangles)})
        {
            if (inside)
            {
                for (int k = 0; k < 3; ++k)
                {
                    list->append(data.triangles.at(i + k));
                    probe.insert(static_cast<int>(data.triangles.at(i + k)));
                }
            }
        }
    }

    for (const QString &ruler : MakeHumanBody::rulerNames())
    {
        for (const int v : MakeHumanBody::ruler(ruler))
        {
            probe.insert(v);
        }
    }
    for (const int v : {napeVertex, shoulderTipVertex, m_shoulderMirror, armpitVertex, m_armpitMirror, crotchVertex,
                        headLevelVertex, elbowUpperVertex, elbowLowerVertex, wristVertex})
    {
        probe.insert(v);
    }
    for (const QVector<int> *set : {&m_floor, &m_top, &m_bust, &m_shoulderPath, &m_neckBasePath})
    {
        for (const int v : *set)
        {
            probe.insert(v);
        }
    }

    m_probe = probe.values().toVector();
    std::sort(m_probe.begin(), m_probe.end());
}

//---------------------------------------------------------------------------------------------------------------------
QVector<int> MakeHumanTape::shoulderPath() const
{
    return m_shoulderPath;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<int> MakeHumanTape::neckBasePath() const
{
    return m_neckBasePath;
}

//---------------------------------------------------------------------------------------------------------------------
int MakeHumanTape::bustPoint(const QVector<QVector3D> &p) const
{
    if (m_lockedBustPoint >= 0)
    {
        return m_lockedBustPoint;
    }
    int point = m_bust.isEmpty() ? napeVertex : m_bust.constFirst();
    for (const int v : m_bust)
    {
        if (p.at(v).z() > p.at(point).z())
        {
            point = v;
        }
    }
    return point;
}

//---------------------------------------------------------------------------------------------------------------------
int MakeHumanTape::sideVertex(const QVector<QVector3D> &p, const QVector<int> &loop) const
{
    int side = loop.constFirst();
    for (const int v : loop)
    {
        if (p.at(v).x() > p.at(side).x())
        {
            side = v;
        }
    }
    return side;
}

//---------------------------------------------------------------------------------------------------------------------
void MakeHumanTape::lockLandmarks(const MakeHumanBody &body)
{
    for (const int v : m_probe)
    {
        m_positions[v] = body.position(v);
    }
    m_lockedBustPoint = -1;
    m_lockedBustPoint = bustPoint(m_positions);
    m_lockedWaistSide = sideVertex(m_positions, MakeHumanBody::ruler(QStringLiteral("measure-waist-circ")));
    m_lockedHipSide = sideVertex(m_positions, MakeHumanBody::ruler(QStringLiteral("measure-hips-circ")));
}

//---------------------------------------------------------------------------------------------------------------------
QVector<double> MakeHumanTape::measure(const MakeHumanBody &body, double height, double *scale) const
{
    QVector<QVector3D> &p = m_positions;
    for (const int v : m_probe)
    {
        p[v] = body.position(v);
    }

    float floor = std::numeric_limits<float>::max();
    for (const int v : m_floor)
    {
        floor = qMin(floor, p.at(v).y());
    }
    float top = std::numeric_limits<float>::lowest();
    for (const int v : m_top)
    {
        top = qMax(top, p.at(v).y());
    }

    // Decimeters of the unscaled body to centimeters of the scaled one.
    const double rawHeight = static_cast<double>(top - floor) * 10.0;
    const double s = rawHeight > 0 ? height / rawHeight : 1.0;
    if (scale != nullptr)
    {
        *scale = s;
    }
    const double toCm = 10.0 * s;

    QVector<double> values(BodyMeasurements::count(), 0.0);
    const auto set = [&values](BodyMeasurement measurement, double value)
    { values[static_cast<int>(measurement)] = value; };
    const auto heightOf = [&](float y) { return static_cast<double>(y - floor) * toCm; };
    const auto meanHeight = [&](const QVector<int> &vertices)
    {
        double sum = 0;
        for (const int v : vertices)
        {
            sum += static_cast<double>(p.at(v).y());
        }
        return vertices.isEmpty() ? 0.0 : (sum / vertices.size() - static_cast<double>(floor)) * toCm;
    };
    const auto sideHeight = [&](const QVector<int> &loop, int locked)
    { return heightOf(p.at(locked >= 0 ? locked : sideVertex(p, loop)).y()); };
    const auto cutMesh = [&p](const QVector<quint32> &triangles)
    {
        // Shares the positions; the copy goes away before the next measure writes to them.
        Mesh mesh;
        mesh.positions = p;
        mesh.indices = triangles;
        return mesh;
    };

    set(BodyMeasurement::Height, static_cast<double>(top - floor) * toCm);
    set(BodyMeasurement::NeckBackHeight, heightOf(p.at(napeVertex).y()));
    set(BodyMeasurement::ShoulderTipHeight,
        heightOf((p.at(shoulderTipVertex).y() + p.at(m_shoulderMirror).y()) / 2));
    set(BodyMeasurement::ArmpitHeight, heightOf((p.at(armpitVertex).y() + p.at(m_armpitMirror).y()) / 2));

    set(BodyMeasurement::BustpointHeight, heightOf(p.at(bustPoint(p)).y()));
    set(BodyMeasurement::WaistHeight,
        sideHeight(MakeHumanBody::ruler(QStringLiteral("measure-waist-circ")), m_lockedWaistSide));
    set(BodyMeasurement::HipHeight,
        sideHeight(MakeHumanBody::ruler(QStringLiteral("measure-hips-circ")), m_lockedHipSide));
    set(BodyMeasurement::CrotchHeight, heightOf(p.at(crotchVertex).y()));
    set(BodyMeasurement::KneeHeight, meanHeight(MakeHumanBody::ruler(QStringLiteral("measure-knee-circ"))));
    set(BodyMeasurement::CalfHeight, meanHeight(MakeHumanBody::ruler(QStringLiteral("measure-calf-circ"))));
    set(BodyMeasurement::AnkleHeight, meanHeight(MakeHumanBody::ruler(QStringLiteral("measure-ankle-circ"))));

    for (const RulerMeasure &measure : rulerMeasures())
    {
        double length = 0;
        for (const QString &ruler : measure.rulers)
        {
            length += polylineLength(p, MakeHumanBody::ruler(ruler));
        }
        set(measure.measurement, length * toCm);
    }

    set(BodyMeasurement::ShoulderWidth, smoothLength(p, m_shoulderPath, {}, false) * toCm);
    set(BodyMeasurement::HeadCirc, MeshSlicer::girthAtHeight(cutMesh(m_headTriangles), p.at(headLevelVertex).y()) * toCm);

    // A cut square to a limb can also cross other parts further away; take the ring around the limb.
    const auto ringAround = [&](const QVector<quint32> &triangles, const QVector3D &origin, const QVector3D &normal)
    {
        double girth = 0;
        float nearest = std::numeric_limits<float>::max();
        for (const MeshSlicer::Contour &contour : MeshSlicer::sliceByPlane(cutMesh(triangles), origin, normal))
        {
            if (!contour.closed || contour.points.isEmpty())
            {
                continue;
            }
            QVector3D centre;
            for (const QVector3D &point : contour.points)
            {
                centre += point;
            }
            centre /= static_cast<float>(contour.points.size());
            const float distance = (centre - origin).length();
            if (distance < nearest)
            {
                nearest = distance;
                girth = contour.length();
            }
        }
        return girth;
    };

    const QVector3D elbow = (p.at(elbowUpperVertex) + p.at(elbowLowerVertex)) / 2;
    set(BodyMeasurement::ElbowCirc, ringAround(m_elbowTriangles, elbow, p.at(wristVertex) - p.at(elbowLowerVertex)) * toCm);

    // Upper arm at armpit level: square to the upper arm, through the point of its axis level with the armpit.
    const QVector3D shoulder = p.at(shoulderTipVertex);
    const QVector3D upperArm = elbow - shoulder;
    const float along = QVector3D::dotProduct(p.at(armpitVertex) - shoulder, upperArm) / upperArm.lengthSquared();
    // Just below the armpit: through the armpit itself the arm is still joined to the torso.
    set(BodyMeasurement::UpperArmCirc,
        ringAround(m_upperArmTriangles, shoulder + upperArm * (qMax(along, 0.15f) + 0.08f), upperArm) * toCm);

    set(BodyMeasurement::NeckCirc, smoothLength(p, m_neckBasePath, m_neckCorners, true) * toCm);
    return values;
}

//---------------------------------------------------------------------------------------------------------------------
double MakeHumanFit::maxError() const
{
    double error = 0;
    for (const Entry &entry : entries)
    {
        if (entry.fitted)
        {
            error = qMax(error, qAbs(entry.achieved - entry.target));
        }
    }
    return error;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<BodyMeasurement> MakeHumanFit::unfitted() const
{
    QVector<BodyMeasurement> result;
    for (const Entry &entry : entries)
    {
        if (!entry.fitted)
        {
            result.append(entry.measurement);
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
MakeHumanFit MakeHumanFitter::fit(const BodyMeasurements &measurements, const QSharedPointer<const MakeHumanData> &data)
{
    MakeHumanFit result;
    if (data.isNull())
    {
        result.errorString = tr("The MakeHuman body data is not available.");
        return result;
    }

    const BodyMeasurements m = measurements.sanitized();
    const double targetHeight = m.value(BodyMeasurement::Height);
    MakeHumanTape tape(*data);

    MakeHumanBody body(data);
    MakeHumanMacros macros;
    macros.gender = m.gender();
    macros.muscle = m.muscle();
    macros.age = MakeHumanMacros::ageFromYears(m.age());

    const auto measured = [&](BodyMeasurement measurement)
    { return tape.measure(body, targetHeight).at(static_cast<int>(measurement)); };

    // ---- Macros: overall height and build. -------------------------------------------------------------------------
    const auto fitHeightMacro = [&]()
    {
        macros.height = bisect(
            [&](double value)
            {
                macros.height = value;
                body.setMacros(macros);
                return body.heightCm() - targetHeight;
            },
            0.0, 1.0, 0.1);
        body.setMacros(macros);
    };

    fitHeightMacro();
    macros.weight = bisect(
        [&](double value)
        {
            macros.weight = value;
            body.setMacros(macros);
            const QVector<double> v = tape.measure(body, targetHeight);
            const auto ratio = [&](BodyMeasurement girth)
            { return v.at(static_cast<int>(girth)) / m.value(girth); };
            return (ratio(BodyMeasurement::BustCirc) + ratio(BodyMeasurement::WaistCirc)
                    + ratio(BodyMeasurement::HipCirc)) / 3.0 - 1.0;
        },
        0.0, 1.0, 1e-3);
    body.setMacros(macros);
    fitHeightMacro();

    // Children have no bust to shape: flat chest (smallest cup) and no breast modifier below this age.
    const bool shapeBreasts = macros.gender < 0.5 && m.age() >= minBreastAge;
    if (m.age() < minBreastAge)
    {
        macros.breastSize = 0.0;
        body.setMacros(macros);
    }
    if (shapeBreasts)
    {
        const double wantedDifference = m.value(BodyMeasurement::BustCirc) - m.value(BodyMeasurement::UnderbustCirc);
        macros.breastSize = bisect(
            [&](double value)
            {
                macros.breastSize = value;
                body.setMacros(macros);
                return (measured(BodyMeasurement::BustCirc) - measured(BodyMeasurement::UnderbustCirc))
                       - wantedDifference;
            },
            0.0, 1.0, 0.1);
        body.setMacros(macros);
    }

    // ---- Modifiers: every measurement at once (Levenberg-Marquardt). ----------------------------------------------
    tape.lockLandmarks(body);

    QVector<int> targets; // measurements in the residual; the height is exact through the scale
    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        if (static_cast<BodyMeasurement>(i) != BodyMeasurement::Height)
        {
            targets.append(i);
        }
    }
    const int rows = static_cast<int>(targets.size());
    const int columns = static_cast<int>(data->modifiers.size());

    // The solver works on u with modifier = limit * tanh(u): no bounds to clamp against, and modifiers approach
    // their limits smoothly instead of sticking to them.
    const double limit = MakeHumanBody::modifierLimit;
    const auto toModifiers = [limit](const QVector<double> &u)
    {
        QVector<double> values(u.size());
        for (int i = 0; i < u.size(); ++i)
        {
            values[i] = limit * std::tanh(u.at(i));
        }
        return values;
    };

    // Girths decide how clothes fit, so they count fully; landmark heights count half. Every modifier also pays
    // for moving away from zero, so among the bodies that fit, the most natural one wins, and measurements that
    // would need an extreme (distorted) body are matched as far as a natural body allows.
    // Children have no bust: their lowbust and bustpoint are not measured on the body (the lowbust tape sits under
    // an adult bust, and pulling it carves a rib line at an adult height into a child).
    const bool child = m.age() < minBreastAge;
    const auto ignored = [child](BodyMeasurement measurement)
    {
        return child
               && (measurement == BodyMeasurement::UnderbustCirc || measurement == BodyMeasurement::BustpointHeight);
    };

    QVector<double> weights(rows, 1.0);
    for (int i = 0; i < rows; ++i)
    {
        const auto measurement = static_cast<BodyMeasurement>(targets.at(i));
        if (ignored(measurement))
        {
            weights[i] = 0.0;
        }
        else if (BodyMeasurements::group(measurement) == BodyMeasurementGroup::Height
                 || measurement == BodyMeasurement::HeadCirc)
        {
            weights[i] = heightWeight;
        }
    }

    // Measurement part first (cm, weighted), then the regularization of every modifier.
    const auto residuals = [&](const QVector<double> &u)
    {
        const QVector<double> values = toModifiers(u);
        body.setModifierValues(values);
        const QVector<double> v = tape.measure(body, targetHeight);
        QVector<double> r(rows + columns);
        for (int i = 0; i < rows; ++i)
        {
            r[i] = weights.at(i) * (m.value(static_cast<BodyMeasurement>(targets.at(i))) - v.at(targets.at(i)));
        }
        for (int j = 0; j < columns; ++j)
        {
            r[rows + j] = -regularization * values.at(j);
        }
        return r;
    };
    const auto cost = [](const QVector<double> &r)
    {
        double sum = 0;
        for (const double value : r)
        {
            sum += value * value;
        }
        return sum;
    };
    const auto worst = [&](const QVector<double> &r)
    {
        double w = 0;
        for (int i = 0; i < rows; ++i)
        {
            if (weights.at(i) > 0)
            {
                w = qMax(w, qAbs(r.at(i)) / weights.at(i));
            }
        }
        return w;
    };

    // Modifiers the solver may move.
    QVector<bool> free(columns, true);
    if (!shapeBreasts)
    {
        free[static_cast<int>(MakeHumanData::modifierNames().indexOf(QStringLiteral("breast-trans")))] = false;
    }
    if (child)
    {
        free[static_cast<int>(MakeHumanData::modifierNames().indexOf(QStringLiteral("measure-underbust-circ")))] = false;
    }

    QVector<double> x(columns, 0.0);
    QVector<double> r = residuals(x);
    double currentCost = cost(r);
    double lambda = 1e-3;

    int stalled = 0;
    for (int iteration = 0; iteration < 60 && worst(r) >= tolerance && stalled < 3; ++iteration)
    {
        // Jacobian of the measurements by forward differences.
        const int residualCount = rows + columns;
        QVector<QVector<double>> jacobian(residualCount, QVector<double>(columns));
        for (int j = 0; j < columns; ++j)
        {
            if (!free.at(j))
            {
                continue; // zero column: the solve leaves it alone
            }
            QVector<double> moved = x;
            const double h = 0.01;
            moved[j] += h;
            const QVector<double> rMoved = residuals(moved);
            for (int i = 0; i < residualCount; ++i)
            {
                jacobian[i][j] = (r.at(i) - rMoved.at(i)) / h;
            }
        }

        QVector<QVector<double>> normal(columns, QVector<double>(columns, 0.0));
        QVector<double> gradient(columns, 0.0);
        for (int a = 0; a < columns; ++a)
        {
            for (int i = 0; i < residualCount; ++i)
            {
                gradient[a] += jacobian.at(i).at(a) * r.at(i);
            }
            for (int b = a; b < columns; ++b)
            {
                double sum = 0;
                for (int i = 0; i < residualCount; ++i)
                {
                    sum += jacobian.at(i).at(a) * jacobian.at(i).at(b);
                }
                normal[a][b] = sum;
                normal[b][a] = sum;
            }
        }

        bool improved = false;
        for (int attempt = 0; attempt < 8 && !improved; ++attempt)
        {
            QVector<QVector<double>> damped = normal;
            for (int a = 0; a < columns; ++a)
            {
                damped[a][a] += lambda * (normal.at(a).at(a) + 1e-3);
            }
            const QVector<double> delta = solve(damped, gradient);

            QVector<double> candidate(columns);
            for (int a = 0; a < columns; ++a)
            {
                candidate[a] = x.at(a) + delta.at(a);
            }
            const QVector<double> rCandidate = residuals(candidate);
            const double candidateCost = cost(rCandidate);
            if (candidateCost < currentCost)
            {
                // Stop once the steps stop paying off: the remaining error is what the body can not do.
                stalled = candidateCost > currentCost * 0.995 ? stalled + 1 : 0;
                x = candidate;
                r = rCandidate;
                currentCost = candidateCost;
                lambda = qMax(lambda / 3, 1e-7);
                improved = true;
            }
            else
            {
                lambda *= 5;
            }
        }
        if (!improved)
        {
            break; // as close as the body can get
        }
    }
    body.setModifierValues(toModifiers(x));

    // ---- Result. ---------------------------------------------------------------------------------------------------
    const QVector<double> achieved = tape.measure(body, targetHeight, &result.scale);
    result.macros = macros;
    result.modifiers = toModifiers(x);
    result.mesh = body.mesh(result.scale);

    for (int i = 0; i < BodyMeasurements::count(); ++i)
    {
        MakeHumanFit::Entry entry;
        entry.measurement = static_cast<BodyMeasurement>(i);
        entry.target = m.value(entry.measurement);
        entry.achieved = achieved.at(i);
        entry.fitted = !ignored(entry.measurement);
        result.entries.append(entry);
    }

    result.valid = true;
    return result;
}

} // namespace Seamly3D
