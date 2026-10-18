/******************************************************************************
 *   @file   clothsimulator.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Drapes a sewn garment on the body.
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

#include "clothsimulator.h"

#include <QHash>
#include <QPair>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QtMath>

#include <algorithm>
#include <functional>
#include <limits>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
/** @brief parallelFor runs task(0) .. task(count - 1) on the threads of the global pool, and waits for them. */
void parallelFor(int count, const std::function<void(int)> &task)
{
    if (count <= 1 || QThread::idealThreadCount() <= 1)
    {
        for (int i = 0; i < count; ++i)
        {
            task(i);
        }
        return;
    }
    QSemaphore done;
    QThreadPool *pool = QThreadPool::globalInstance();
    for (int i = 1; i < count; ++i)
    {
        pool->start([&task, &done, i]()
                    {
                        task(i);
                        done.release();
                    });
    }
    task(0);
    done.acquire(count - 1);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief chunks splits @p count items into as many runs as there are threads, a few times over for balance. */
int chunks(int count)
{
    return qBound(1, qMin(count / 256, QThread::idealThreadCount() * 4), 64);
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The point of triangle abc closest to p, and its barycentric coordinates (Ericson, Real-Time Collision
 *  Detection, 5.1.5). */
QVector3D closestOnTriangle(const QVector3D &p, const QVector3D &a, const QVector3D &b, const QVector3D &c, float &u,
                            float &v, float &w)
{
    const QVector3D ab = b - a;
    const QVector3D ac = c - a;
    const QVector3D ap = p - a;
    const float d1 = QVector3D::dotProduct(ab, ap);
    const float d2 = QVector3D::dotProduct(ac, ap);
    if (d1 <= 0 && d2 <= 0)
    {
        u = 1; v = 0; w = 0;
        return a;
    }

    const QVector3D bp = p - b;
    const float d3 = QVector3D::dotProduct(ab, bp);
    const float d4 = QVector3D::dotProduct(ac, bp);
    if (d3 >= 0 && d4 <= d3)
    {
        u = 0; v = 1; w = 0;
        return b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0)
    {
        const float t = d1 / (d1 - d3);
        u = 1 - t; v = t; w = 0;
        return a + ab * t;
    }

    const QVector3D cp = p - c;
    const float d5 = QVector3D::dotProduct(ab, cp);
    const float d6 = QVector3D::dotProduct(ac, cp);
    if (d6 >= 0 && d5 <= d6)
    {
        u = 0; v = 0; w = 1;
        return c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0)
    {
        const float t = d2 / (d2 - d6);
        u = 1 - t; v = 0; w = t;
        return a + ac * t;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
    {
        const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        u = 0; v = 1 - t; w = t;
        return b + (c - b) * t;
    }

    const float denominator = 1.0f / (va + vb + vc);
    v = vb * denominator;
    w = vc * denominator;
    u = 1 - v - w;
    return a + ab * v + ac * w;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
void BodyCollider::build(const Mesh &body, float reach, float cellSize)
{
    clear();
    if (body.isEmpty() || reach <= 0 || cellSize <= 0)
    {
        return;
    }
    m_reach = reach;
    m_cellSize = cellSize;

    Mesh oriented = body;
    oriented.computeNormals();

    // Normals point out of the body when the triangles wind counterclockwise seen from outside: then the enclosed
    // volume is positive. Flip them otherwise.
    double volume = 0;
    for (int t = 0; t + 2 < oriented.indices.size(); t += 3)
    {
        const QVector3D &a = oriented.positions.at(static_cast<int>(oriented.indices.at(t)));
        const QVector3D &b = oriented.positions.at(static_cast<int>(oriented.indices.at(t + 1)));
        const QVector3D &c = oriented.positions.at(static_cast<int>(oriented.indices.at(t + 2)));
        volume += static_cast<double>(QVector3D::dotProduct(a, QVector3D::crossProduct(b, c)));
    }
    const float outward = volume < 0 ? -1.0f : 1.0f;

    m_triangles.reserve(oriented.triangleCount());
    for (int t = 0; t + 2 < oriented.indices.size(); t += 3)
    {
        const int i[3] = {static_cast<int>(oriented.indices.at(t)), static_cast<int>(oriented.indices.at(t + 1)),
                          static_cast<int>(oriented.indices.at(t + 2))};
        Triangle triangle;
        triangle.a = oriented.positions.at(i[0]);
        triangle.b = oriented.positions.at(i[1]);
        triangle.c = oriented.positions.at(i[2]);
        const QVector3D face = QVector3D::crossProduct(triangle.b - triangle.a, triangle.c - triangle.a);
        if (face.lengthSquared() <= 1e-12f)
        {
            continue; // degenerate
        }
        triangle.face = face.normalized() * outward;
        triangle.na = oriented.normals.at(i[0]) * outward;
        triangle.nb = oriented.normals.at(i[1]) * outward;
        triangle.nc = oriented.normals.at(i[2]) * outward;
        triangle.center = (triangle.a + triangle.b + triangle.c) / 3;
        triangle.radius = qMax(qMax((triangle.a - triangle.center).length(), (triangle.b - triangle.center).length()),
                               (triangle.c - triangle.center).length());
        m_triangles.append(triangle);
    }

    const Bounds3D bounds = oriented.bounds();
    m_origin = bounds.minimum - QVector3D(reach, reach, reach);
    const QVector3D extent = bounds.size() + QVector3D(2 * reach, 2 * reach, 2 * reach);
    for (int k = 0; k < 3; ++k)
    {
        m_size[k] = qMax(1, qCeil(extent[k] / cellSize));
    }

    // Each cell lists the triangles whose box, grown by the reach, overlaps it. Count first, then fill.
    const auto cellRange = [this, reach](const Triangle &triangle, int low[3], int high[3])
    {
        for (int k = 0; k < 3; ++k)
        {
            const float minimum = qMin(qMin(triangle.a[k], triangle.b[k]), triangle.c[k]) - reach;
            const float maximum = qMax(qMax(triangle.a[k], triangle.b[k]), triangle.c[k]) + reach;
            low[k] = qBound(0, static_cast<int>((minimum - m_origin[k]) / m_cellSize), m_size[k] - 1);
            high[k] = qBound(0, static_cast<int>((maximum - m_origin[k]) / m_cellSize), m_size[k] - 1);
        }
    };

    const int cellCount = m_size[0] * m_size[1] * m_size[2];
    QVector<int> count(cellCount + 1, 0);
    int low[3];
    int high[3];
    for (const Triangle &triangle : qAsConst(m_triangles))
    {
        cellRange(triangle, low, high);
        for (int z = low[2]; z <= high[2]; ++z)
            for (int y = low[1]; y <= high[1]; ++y)
                for (int x = low[0]; x <= high[0]; ++x)
                    ++count[cellIndex(x, y, z) + 1];
    }
    m_cellStart.resize(cellCount + 1);
    m_cellStart[0] = 0;
    for (int i = 0; i < cellCount; ++i)
    {
        m_cellStart[i + 1] = m_cellStart.at(i) + count.at(i + 1);
    }
    m_cellTriangles.resize(m_cellStart.at(cellCount));
    QVector<int> fill = m_cellStart;
    for (int t = 0; t < m_triangles.size(); ++t)
    {
        cellRange(m_triangles.at(t), low, high);
        for (int z = low[2]; z <= high[2]; ++z)
            for (int y = low[1]; y <= high[1]; ++y)
                for (int x = low[0]; x <= high[0]; ++x)
                    m_cellTriangles[fill[cellIndex(x, y, z)]++] = t;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void BodyCollider::clear()
{
    m_triangles.clear();
    m_cellStart.clear();
    m_cellTriangles.clear();
    m_size[0] = m_size[1] = m_size[2] = 0;
    m_reach = 0;
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyCollider::isEmpty() const
{
    return m_triangles.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
float BodyCollider::reach() const
{
    return m_reach;
}

//---------------------------------------------------------------------------------------------------------------------
int BodyCollider::cellIndex(int x, int y, int z) const
{
    return (z * m_size[1] + y) * m_size[0] + x;
}

//---------------------------------------------------------------------------------------------------------------------
float BodyCollider::closest(const QVector3D &point, QVector3D *surface, QVector3D *normal) const
{
    if (m_triangles.isEmpty())
    {
        return m_reach;
    }

    int cell[3];
    for (int k = 0; k < 3; ++k)
    {
        const float offset = (point[k] - m_origin[k]) / m_cellSize;
        if (offset < 0 || offset >= static_cast<float>(m_size[k]))
        {
            return m_reach;
        }
        cell[k] = static_cast<int>(offset);
    }

    const int index = cellIndex(cell[0], cell[1], cell[2]);
    float best = m_reach * m_reach;
    int bestTriangle = -1;
    QVector3D bestPoint;
    float bu = 0, bv = 0, bw = 0;
    for (int i = m_cellStart.at(index); i < m_cellStart.at(index + 1); ++i)
    {
        const Triangle &triangle = m_triangles.at(m_cellTriangles.at(i));
        // No point of the triangle is nearer than its bounding sphere: skip those that cannot beat the best so far.
        const float beyond = (point - triangle.center).length() - triangle.radius;
        if (beyond > 0 && beyond * beyond >= best)
        {
            continue;
        }
        float u, v, w;
        const QVector3D onTriangle = closestOnTriangle(point, triangle.a, triangle.b, triangle.c, u, v, w);
        const float distance = (point - onTriangle).lengthSquared();
        if (distance < best)
        {
            best = distance;
            bestTriangle = m_cellTriangles.at(i);
            bestPoint = onTriangle;
            bu = u;
            bv = v;
            bw = w;
        }
    }
    if (bestTriangle < 0)
    {
        return m_reach;
    }

    // Inside or outside: the side of the smoothed surface (vertex normals blended at the closest point), which is
    // also right where the closest point is on an edge or a corner.
    const Triangle &triangle = m_triangles.at(bestTriangle);
    QVector3D n = triangle.na * bu + triangle.nb * bv + triangle.nc * bw;
    n = n.lengthSquared() > 1e-12f ? n.normalized() : triangle.face;
    const QVector3D offset = point - bestPoint;
    const float distance = qSqrt(best);
    if (surface != nullptr)
    {
        *surface = bestPoint;
    }
    if (normal != nullptr)
    {
        *normal = n;
    }
    return QVector3D::dotProduct(offset, n) < 0 ? -distance : distance;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::setSettings(const ClothSettings &settings)
{
    m_settings = settings;
}

//---------------------------------------------------------------------------------------------------------------------
const ClothSettings &ClothSimulator::settings() const
{
    return m_settings;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::setBody(const Mesh &body)
{
    // Reach: the thickness plus the farthest a particle may travel in one step (see maxStep).
    m_body.build(body, m_settings.thickness + maxStep, 1.0f);
    m_bodyMesh = body;
    m_easeBody.clear();
    m_easeLimit = 0;
    m_floor = body.isEmpty() ? std::numeric_limits<float>::lowest() : body.bounds().minimum.y();
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::clear()
{
    m_panels.clear();
    m_offset.clear();
    m_position.clear();
    m_previous.clear();
    m_velocity.clear();
    m_inverseMass.clear();
    m_stretch.clear();
    m_bend.clear();
    m_ranges.clear();
    m_seams.clear();
    m_seamCount = 0;
    m_sewingTime = 0;
    m_time = 0;
    m_maxSpeed = 0;
    m_averageSpeed = 0;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::setGarment(const QVector<GarmentPanel> &panels, const QVector<Seam> &seams)
{
    clear();
    m_panels = panels;

    for (const GarmentPanel &panel : panels)
    {
        const int offset = static_cast<int>(m_position.size());
        m_offset.append(offset);
        m_position.append(panel.shape.positions);
        m_velocity.append(QVector<QVector3D>(panel.shape.positions.size()));
        Ranges ranges;
        ranges.stretchBegin = static_cast<int>(m_stretch.size());
        ranges.bendBegin = static_cast<int>(m_bend.size());
        const auto closeRanges = [this, &ranges]()
        {
            ranges.stretchEnd = static_cast<int>(m_stretch.size());
            ranges.bendEnd = static_cast<int>(m_bend.size());
            m_ranges.append(ranges);
        };
        if (!panel.sewable)
        {
            m_inverseMass.append(QVector<float>(panel.shape.positions.size(), 0.0f));
            closeRanges();
            continue;
        }

        // Each particle weighs as much as the fabric around it: a third of the area of its triangles. (Equal weights
        // would make the side of a piece with more particles heavier.) Weights are relative, 1 for 1 cm².
        QVector<float> area(panel.shape.positions.size(), 0.0f);
        for (int t = 0; t + 2 < panel.rest.indices.size(); t += 3)
        {
            const int a = static_cast<int>(panel.rest.indices.at(t));
            const int b = static_cast<int>(panel.rest.indices.at(t + 1));
            const int c = static_cast<int>(panel.rest.indices.at(t + 2));
            const float third = QVector3D::crossProduct(panel.rest.positions.at(b) - panel.rest.positions.at(a),
                                                        panel.rest.positions.at(c) - panel.rest.positions.at(a))
                                    .length()
                                / 6.0f;
            area[a] += third;
            area[b] += third;
            area[c] += third;
        }
        for (const float particleArea : area)
        {
            const float mass = particleArea * qMax(0.01f, m_settings.weight);
            m_inverseMass.append(mass > 1e-6f ? 1.0f / mass : 0.0f);
        }

        // Stretch: every edge keeps its length on the flat piece. Bend: across every inner edge, the two opposite
        // corners keep their flat distance, softly.
        QHash<QPair<int, int>, int> opposite;
        const auto rest = [&panel](int a, int b)
        { return (panel.rest.positions.at(a) - panel.rest.positions.at(b)).length(); };
        for (int t = 0; t + 2 < panel.rest.indices.size(); t += 3)
        {
            const int v[3] = {static_cast<int>(panel.rest.indices.at(t)),
                              static_cast<int>(panel.rest.indices.at(t + 1)),
                              static_cast<int>(panel.rest.indices.at(t + 2))};
            for (int k = 0; k < 3; ++k)
            {
                const int a = v[k];
                const int b = v[(k + 1) % 3];
                const int c = v[(k + 2) % 3];
                const QPair<int, int> edge(qMin(a, b), qMax(a, b));
                const auto it = opposite.constFind(edge);
                if (it == opposite.constEnd())
                {
                    opposite.insert(edge, c);
                    m_stretch.append({offset + a, offset + b, rest(a, b)});
                }
                else if (it.value() != c)
                {
                    m_bend.append({offset + it.value(), offset + c, rest(it.value(), c)});
                }
            }
        }

        closeRanges();
    }
    m_previous = m_position;

    for (const Seam &seam : seams)
    {
        if (seam.panelA >= 0 && seam.panelA < m_panels.size() && seam.panelB >= 0 && seam.panelB < m_panels.size()
            && m_panels.at(seam.panelA).sewable && m_panels.at(seam.panelB).sewable)
        {
            addSeam(seam);
            ++m_seamCount;
        }
    }

    float widest = 0;
    for (const SeamLink &link : qAsConst(m_seams))
    {
        widest = qMax(widest, link.start);
    }
    // Sew first, then let the garment settle under its full weight.
    m_sewingTime = widest / qMax(1.0f, m_settings.sewingSpeed) + 0.3f;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::addSeam(const Seam &seam)
{
    const GarmentPanel &panelA = m_panels.at(seam.panelA);
    const GarmentPanel &panelB = m_panels.at(seam.panelB);
    QVector<int> a = panelA.edgeVertices(seam.edgeA);
    QVector<int> b = panelB.edgeVertices(seam.edgeB);
    if (a.size() < 2 || b.size() < 2)
    {
        return;
    }
    if (seam.reversed)
    {
        std::reverse(b.begin(), b.end());
    }

    // Where each point lies along its edge, 0 at the start to 1 at the end, measured on the flat piece.
    const auto parameters = [](const GarmentPanel &panel, const QVector<int> &vertices)
    {
        QVector<float> s(vertices.size(), 0);
        for (int i = 1; i < vertices.size(); ++i)
        {
            s[i] = s.at(i - 1)
                   + (panel.rest.positions.at(vertices.at(i)) - panel.rest.positions.at(vertices.at(i - 1))).length();
        }
        const float total = qMax(1e-6f, s.constLast());
        for (float &value : s)
        {
            value /= total;
        }
        return s;
    };
    // Only the part of edge A from startA to endA is sewn: measure A's points along that part (0 to 1 on it).
    QVector<float> sa = parameters(panelA, a);
    const auto startA = static_cast<float>(seam.startA);
    const float spanA = qMax(1e-6f, static_cast<float>(seam.endA) - startA);
    for (float &value : sa)
    {
        value = (value - startA) / spanA;
    }
    // Likewise the part of B, from startB to endB in its own direction: in reverse, from 1 - endB to 1 - startB.
    QVector<float> sb = parameters(panelB, b);
    const auto startB = static_cast<float>(seam.reversed ? 1.0 - seam.endB : seam.startB);
    const float spanB = qMax(1e-6f, static_cast<float>(seam.endB - seam.startB));
    for (float &value : sb)
    {
        value = (value - startB) / spanB;
    }

    // Each point of one side is held to the point at the same place along the other side, and the other way round.
    const auto link = [this](const QVector<int> &from, const QVector<float> &sFrom, int fromOffset,
                             const QVector<int> &to, const QVector<float> &sTo, int toOffset)
    {
        int segment = 0;
        for (int i = 0; i < from.size(); ++i)
        {
            const float s = sFrom.at(i);
            if (s < -1e-4f || s > 1 + 1e-4f)
            {
                continue; // outside the part sewn
            }
            while (segment + 2 < to.size() && sTo.at(segment + 1) < s)
            {
                ++segment;
            }
            const float length = sTo.at(segment + 1) - sTo.at(segment);
            const float t = length > 1e-6f ? qBound(0.0f, (s - sTo.at(segment)) / length, 1.0f) : 0.0f;
            SeamLink seamLink{fromOffset + from.at(i), toOffset + to.at(segment), toOffset + to.at(segment + 1), t, 0};
            if (seamLink.a == seamLink.b0 || seamLink.a == seamLink.b1)
            {
                continue; // the tip of a dart
            }
            seamLink.start = (m_position.at(seamLink.a) - seamPoint(seamLink)).length();
            m_seams.append(seamLink);
        }
    };
    link(a, sa, m_offset.at(seam.panelA), b, sb, m_offset.at(seam.panelB));
    link(b, sb, m_offset.at(seam.panelB), a, sa, m_offset.at(seam.panelA));
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D ClothSimulator::seamPoint(const SeamLink &link) const
{
    return m_position.at(link.b0) * (1 - link.t) + m_position.at(link.b1) * link.t;
}

//---------------------------------------------------------------------------------------------------------------------
bool ClothSimulator::isEmpty() const
{
    return m_position.isEmpty();
}

//---------------------------------------------------------------------------------------------------------------------
int ClothSimulator::particleCount() const
{
    return static_cast<int>(m_position.size());
}

//---------------------------------------------------------------------------------------------------------------------
int ClothSimulator::seamCount() const
{
    return m_seamCount;
}

//---------------------------------------------------------------------------------------------------------------------
float ClothSimulator::time() const
{
    return m_time;
}

//---------------------------------------------------------------------------------------------------------------------
bool ClothSimulator::isSewn() const
{
    return m_time >= m_sewingTime;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::step()
{
    if (m_position.isEmpty())
    {
        return;
    }

    const QVector<QVector3D> start = m_position;
    const int substeps = qMax(1, m_settings.substeps);
    const float h = m_settings.frameTime / static_cast<float>(substeps);
    for (int k = 0; k < substeps; ++k)
    {
        m_time += h;
        const float gravity = m_settings.gravity * (isSewn() ? 1.0f : m_settings.sewingGravity);
        substep(h, gravity, m_settings.sewingSpeed * m_time);
    }

    // Speeds over the whole frame: within it, particles touching the body tremble by hundredths of a millimeter
    // between the constraints and the body, which in a step this short reads as centimeters per second.
    m_maxSpeed = 0;
    double total = 0;
    int moving = 0;
    for (int i = 0; i < m_position.size(); ++i)
    {
        if (m_inverseMass.at(i) > 0)
        {
            const float speed = (m_position.at(i) - start.at(i)).length() / m_settings.frameTime;
            m_maxSpeed = qMax(m_maxSpeed, speed);
            total += static_cast<double>(speed);
            ++moving;
        }
    }
    m_averageSpeed = moving > 0 ? static_cast<float>(total / moving) : 0.0f;
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::substep(float h, float gravity, float sewn)
{
    const float keep = qExp(-m_settings.damping * h);
    const int count = static_cast<int>(m_position.size());
    QVector3D *position = m_position.data();
    QVector3D *previous = m_previous.data();
    QVector3D *velocity = m_velocity.data();
    const float *inverseMass = m_inverseMass.constData();
    for (int i = 0; i < count; ++i)
    {
        if (inverseMass[i] <= 0)
        {
            continue;
        }
        velocity[i].setY(velocity[i].y() - gravity * h);
        velocity[i] *= keep;
        previous[i] = position[i];
        position[i] += velocity[i] * h;
    }

    // Stretch and seams together, a few times: once lets a hanging garment stretch several percent. The panels share
    // no particles, so each is solved on a thread of its own; seams join them, one after the other.
    const int panels = static_cast<int>(m_ranges.size());
    for (int iteration = 0; iteration < qMax(1, m_settings.iterations); ++iteration)
    {
        parallelFor(panels,
                    [this, h](int p)
                    {
                        const Ranges &r = m_ranges.at(p);
                        solveDistances(m_stretch, r.stretchBegin, r.stretchEnd, m_settings.stretchCompliance, h);
                    });
        solveSeams(sewn);
    }
    parallelFor(panels,
                [this, h](int p)
                {
                    const Ranges &r = m_ranges.at(p);
                    solveDistances(m_bend, r.bendBegin, r.bendEnd, m_settings.bendCompliance, h);
                });

    // The body, and the velocities the step leaves: particle by particle, in runs side by side.
    const int runs = chunks(count);
    parallelFor(runs,
                [this, count, runs, h](int run)
                { collide(static_cast<int>(static_cast<qint64>(count) * run / runs),
                          static_cast<int>(static_cast<qint64>(count) * (run + 1) / runs), h); });
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::solveDistances(const QVector<Distance> &constraints, int begin, int end, float compliance,
                                    float h)
{
    const float alpha = compliance / (h * h);
    QVector3D *position = m_position.data();
    const float *inverseMass = m_inverseMass.constData();
    const Distance *constraint = constraints.constData();
    for (int k = begin; k < end; ++k)
    {
        const Distance &c = constraint[k];
        const float wa = inverseMass[c.a];
        const float wb = inverseMass[c.b];
        const QVector3D d = position[c.a] - position[c.b];
        const float length = d.length();
        if (length < 1e-9f)
        {
            continue;
        }
        const float lambda = -(length - c.rest) / (wa + wb + alpha);
        const QVector3D correction = d * (lambda / length);
        position[c.a] += correction * wa;
        position[c.b] -= correction * wb;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::solveSeams(float sewn)
{
    // Each seam closes at the sewing speed: the gap allowed shrinks from where it started to nothing.
    for (const SeamLink &link : qAsConst(m_seams))
    {
        const float allowed = qMax(0.0f, link.start - sewn);
        const QVector3D d = m_position.at(link.a) - seamPoint(link);
        const float length = d.length();
        if (length <= allowed || length < 1e-9f)
        {
            continue;
        }
        const float wa = m_inverseMass.at(link.a);
        const float w0 = m_inverseMass.at(link.b0) * (1 - link.t);
        const float w1 = m_inverseMass.at(link.b1) * link.t;
        const float w = wa + w0 * (1 - link.t) + w1 * link.t;
        if (w <= 0)
        {
            continue;
        }
        const QVector3D correction = d * ((length - allowed) / (w * length));
        m_position[link.a] -= correction * wa;
        m_position[link.b0] += correction * w0;
        m_position[link.b1] += correction * w1;
    }
}

//---------------------------------------------------------------------------------------------------------------------
void ClothSimulator::collide(int begin, int end, float h)
{
    const float thickness = m_settings.thickness;
    const float friction = qBound(0.0f, m_settings.friction, 1.0f);
    QVector3D *position = m_position.data();
    const QVector3D *previous = m_previous.constData();
    QVector3D *velocity = m_velocity.data();
    const float *inverseMass = m_inverseMass.constData();
    for (int i = begin; i < end; ++i)
    {
        if (inverseMass[i] <= 0)
        {
            continue;
        }
        QVector3D &p = position[i];

        // However hard the constraints pull (a seam pulled through the body while the body pushes back), no particle
        // moves farther in one step than the body collider reaches: the body always catches it. Nor faster than a
        // speed limit, whatever the step: sewn too fast, a sleeve whips round the arm and tangles.
        const QVector3D step = p - previous[i];
        const float distance = step.length();
        const float limit = qMin(maxStep, speedLimit * h);
        if (distance > limit)
        {
            p = previous[i] + step * (limit / distance);
        }

        QVector3D surface;
        QVector3D normal;
        bool touched = false;
        if (p.y() < m_floor + thickness)
        {
            p.setY(m_floor + thickness);
            normal = QVector3D(0, 1, 0);
            touched = true;
        }
        else if (m_body.closest(p, &surface, &normal) < thickness)
        {
            // Out along the surface normal only, by as much as is missing: moving the cloth sideways too (onto the
            // normal through the closest point) makes it flip between neighbouring triangles in hollows.
            const float depth = thickness - QVector3D::dotProduct(p - surface, normal);
            p += normal * depth;
            touched = true;
        }

        if (touched && friction > 0)
        {
            // Friction: take back part of the sliding along the surface during this step.
            const QVector3D moved = p - previous[i];
            p -= (moved - normal * QVector3D::dotProduct(moved, normal)) * friction;
        }

        velocity[i] = (p - previous[i]) / h;
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<GarmentPanel> ClothSimulator::panels() const
{
    QVector<GarmentPanel> result = m_panels;
    for (int p = 0; p < result.size(); ++p)
    {
        Mesh &shape = result[p].shape;
        for (int v = 0; v < shape.positions.size(); ++v)
        {
            shape.positions[v] = m_position.at(m_offset.at(p) + v);
        }
        shape.computeNormals();
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
float ClothSimulator::maxSeamGap() const
{
    float gap = 0;
    for (const SeamLink &link : m_seams)
    {
        gap = qMax(gap, (m_position.at(link.a) - seamPoint(link)).length());
    }
    return gap;
}

//---------------------------------------------------------------------------------------------------------------------
float ClothSimulator::maxSpeed() const
{
    return m_maxSpeed;
}

//---------------------------------------------------------------------------------------------------------------------
float ClothSimulator::averageSpeed() const
{
    return m_averageSpeed;
}

//---------------------------------------------------------------------------------------------------------------------
float ClothSimulator::maxPenetration() const
{
    float depth = 0;
    for (int i = 0; i < m_position.size(); ++i)
    {
        if (m_inverseMass.at(i) > 0)
        {
            depth = qMax(depth, -m_body.closest(m_position.at(i)));
        }
    }
    return depth;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QVector<float>> ClothSimulator::strainMap() const
{
    QVector<float> sum(m_position.size(), 0.0f);
    QVector<float> rest(m_position.size(), 0.0f);
    for (const Distance &edge : m_stretch)
    {
        if (edge.rest <= 1e-6f)
        {
            continue;
        }
        // Weighed by length: how much longer the fabric around the particle is, as a whole. (Tiny edges would
        // otherwise show large shares of tiny lengths.)
        const float extra = (m_position.at(edge.a) - m_position.at(edge.b)).length() - edge.rest;
        sum[edge.a] += extra;
        sum[edge.b] += extra;
        rest[edge.a] += edge.rest;
        rest[edge.b] += edge.rest;
    }
    QVector<QVector<float>> result;
    for (int p = 0; p < m_panels.size(); ++p)
    {
        QVector<float> values(m_panels.at(p).shape.positions.size(), 0.0f);
        for (int v = 0; v < values.size(); ++v)
        {
            const int i = m_offset.at(p) + v;
            values[v] = rest.at(i) > 0 ? sum.at(i) / rest.at(i) : 0.0f;
        }
        result.append(values);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QVector<float>> ClothSimulator::easeMap(float limit) const
{
    // A collider of its own, reaching as far as the map shows, in coarser cells.
    if (m_easeLimit < limit && !m_bodyMesh.isEmpty())
    {
        m_easeBody.build(m_bodyMesh, limit + m_settings.thickness, 4.0f);
        m_easeLimit = limit;
    }
    QVector<QVector<float>> result;
    for (int p = 0; p < m_panels.size(); ++p)
    {
        QVector<float> values(m_panels.at(p).shape.positions.size(), limit);
        for (int v = 0; v < values.size(); ++v)
        {
            const float distance = m_easeBody.closest(m_position.at(m_offset.at(p) + v)) - m_settings.thickness;
            values[v] = qBound(0.0f, distance, limit);
        }
        result.append(values);
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
ClothSettings FabricPreset::apply(ClothSettings settings) const
{
    settings.stretchCompliance = stretchCompliance;
    settings.bendCompliance = bendCompliance;
    settings.friction = friction;
    settings.thickness = thickness;
    settings.damping = damping;
    settings.weight = static_cast<float>(gramsPerSquareMeter) / 120.0f;
    return settings;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<FabricPreset> FabricPreset::presets()
{
    const auto tr = [](const char *text) { return QCoreApplication::translate("Seamly3D::FabricPreset", text); };
    //        id,                 name,                    g/m², stretch, bend,   friction, thickness, damping
    return {{QStringLiteral("cotton"), tr("Cotton shirting"), 120, 0.0f, 4e-4f, 0.30f, 0.40f, 2.0f},
            {QStringLiteral("linen"), tr("Linen"), 180, 0.0f, 2e-4f, 0.35f, 0.40f, 2.0f},
            {QStringLiteral("denim"), tr("Denim"), 380, 0.0f, 2e-5f, 0.40f, 0.45f, 2.5f},
            {QStringLiteral("jersey"), tr("Cotton jersey (knit)"), 180, 4e-6f, 6e-4f, 0.35f, 0.40f, 2.0f},
            {QStringLiteral("silk"), tr("Silk"), 60, 0.0f, 2e-3f, 0.15f, 0.30f, 1.5f},
            {QStringLiteral("fleece"), tr("Fleece"), 280, 1e-6f, 1e-4f, 0.45f, 0.60f, 3.0f},
            {QStringLiteral("leather"), tr("Leather"), 800, 0.0f, 5e-6f, 0.50f, 0.50f, 3.0f}};
}

//---------------------------------------------------------------------------------------------------------------------
FabricPreset FabricPreset::preset(const QString &id)
{
    const QVector<FabricPreset> all = presets();
    for (const FabricPreset &fabric : all)
    {
        if (fabric.id == id)
        {
            return fabric;
        }
    }
    return all.constFirst();
}

} // namespace Seamly3D
