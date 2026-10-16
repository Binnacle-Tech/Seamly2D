/******************************************************************************
 *   @file   outlinefitter.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Turns a densely sampled outline into corners, straight lines and a few smooth curves.
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

#include "outlinefitter.h"

#include <QLineF>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
double length(const QPointF &v)
{
    return std::sqrt(QPointF::dotProduct(v, v));
}

//---------------------------------------------------------------------------------------------------------------------
QPointF normalized(const QPointF &v)
{
    const double l = length(v);
    return l > 1e-12 ? v / l : QPointF();
}

//---------------------------------------------------------------------------------------------------------------------
QPointF bezier(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, double t)
{
    const double s = 1 - t;
    return p0 * (s * s * s) + p1 * (3 * s * s * t) + p2 * (3 * s * t * t) + p3 * (t * t * t);
}

//---------------------------------------------------------------------------------------------------------------------
QPointF bezierDerivative(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, double t)
{
    const double s = 1 - t;
    return (p1 - p0) * (3 * s * s) + (p2 - p1) * (6 * s * t) + (p3 - p2) * (3 * t * t);
}

//---------------------------------------------------------------------------------------------------------------------
QPointF bezierSecond(const QPointF &p0, const QPointF &p1, const QPointF &p2, const QPointF &p3, double t)
{
    return (p2 - p1 * 2 + p0) * (6 * (1 - t)) + (p3 - p2 * 2 + p1) * (6 * t);
}

//---------------------------------------------------------------------------------------------------------------------
double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double l2 = QPointF::dotProduct(ab, ab);
    const double t = l2 > 0 ? qBound(0.0, QPointF::dotProduct(p - a, ab) / l2, 1.0) : 0.0;
    return length(p - (a + ab * t));
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The outline, walked from a starting vertex: index 0 is the start, a closed outline wraps around. */
class Walk
{
public:
    Walk(const QPolygonF &points, bool closed, int start)
        : m_points(points), m_closed(closed), m_start(start), m_n(static_cast<int>(points.size()))
    {}

    int     original(int i) const { return m_closed ? ((m_start + i) % m_n + m_n) % m_n : qBound(0, i, m_n - 1); }
    QPointF at(int i) const { return m_points.at(original(i)); }

    /** @brief Tangent at an inner point, from its neighbours (weighted for uneven spacing). */
    QPointF central(int i) const
    {
        const QPointF before = at(i) - at(i - 1);
        const QPointF after = at(i + 1) - at(i);
        const double h1 = length(before);
        const double h2 = length(after);
        if (h1 < 1e-12 || h2 < 1e-12)
        {
            return normalized(after + before);
        }
        return normalized((after * (h1 / h2) + before * (h2 / h1)) / (h1 + h2));
    }

    /** @brief Tangent at the start of a stretch going forward (direction > 0) or at its end (< 0), one sided. */
    QPointF oneSided(int i, int direction, int available) const
    {
        const QPointF p0 = at(i);
        const QPointF p1 = at(i + direction);
        if (available < 2)
        {
            return normalized((p1 - p0) * direction);
        }
        const QPointF p2 = at(i + 2 * direction);
        const double s1 = length(p1 - p0);
        const double s2 = s1 + length(p2 - p1);
        if (s1 < 1e-12 || s2 - s1 < 1e-12)
        {
            return normalized((p1 - p0) * direction);
        }
        // Derivative at p0 of the parabola through the three points, by arc length.
        const QPointF d = p0 * (-(s1 + s2) / (s1 * s2)) + p1 * (s2 / (s1 * (s2 - s1))) - p2 * (s1 / (s2 * (s2 - s1)));
        // Too short a stretch to trust the curvature: fall back to the first edge.
        const QPointF t = normalized(d);
        return t.isNull() ? normalized((p1 - p0) * direction) : t * direction;
    }

private:
    const QPolygonF &m_points;
    bool             m_closed;
    int              m_start;
    int              m_n;
};

//---------------------------------------------------------------------------------------------------------------------
struct Segment
{
    int     from;
    int     to;
    QPointF handleOut; // from the start point
    QPointF handleIn;  // from the end point
    double  error;
    int     worst;
};

//---------------------------------------------------------------------------------------------------------------------
/**
 * @brief fitSegment fits one cubic from point @p a to point @p b of the walk, leaving and arriving along the tangents
 * (least squares for the handle lengths, Schneider's method, with a few parameter refinements).
 */
Segment fitSegment(const Walk &walk, int a, int b, const QPointF &tangentA, const QPointF &tangentB)
{
    const QPointF p0 = walk.at(a);
    const QPointF p3 = walk.at(b);
    const double chord = length(p3 - p0);
    Segment segment{a, b, tangentA * (chord / 3), -tangentB * (chord / 3), 0, -1};
    if (b - a < 2 || chord < 1e-12)
    {
        return segment;
    }

    // Chord length parameters.
    QVector<double> u(b - a + 1, 0.0);
    for (int i = a + 1; i <= b; ++i)
    {
        u[i - a] = u.at(i - a - 1) + length(walk.at(i) - walk.at(i - 1));
    }
    const double total = u.constLast();
    for (double &value : u)
    {
        value = total > 0 ? value / total : 0.0;
    }

    double alphaA = chord / 3;
    double alphaB = chord / 3;
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        double c11 = 0;
        double c12 = 0;
        double c22 = 0;
        double x1 = 0;
        double x2 = 0;
        for (int i = a; i <= b; ++i)
        {
            const double t = u.at(i - a);
            const double s = 1 - t;
            const double b0 = s * s * s;
            const double b1 = 3 * s * s * t;
            const double b2 = 3 * s * t * t;
            const double b3 = t * t * t;
            const QPointF a1 = tangentA * b1;
            const QPointF a2 = -tangentB * b2;
            const QPointF rest = walk.at(i) - (p0 * (b0 + b1) + p3 * (b2 + b3));
            c11 += QPointF::dotProduct(a1, a1);
            c12 += QPointF::dotProduct(a1, a2);
            c22 += QPointF::dotProduct(a2, a2);
            x1 += QPointF::dotProduct(rest, a1);
            x2 += QPointF::dotProduct(rest, a2);
        }
        const double det = c11 * c22 - c12 * c12;
        if (qAbs(det) > 1e-18)
        {
            alphaA = (x1 * c22 - x2 * c12) / det;
            alphaB = (c11 * x2 - c12 * x1) / det;
        }
        // Handles pointing backwards or far beyond the chord mean the tangents do not suit: use the safe default.
        if (alphaA < chord * 1e-3 || alphaB < chord * 1e-3 || alphaA > chord * 2 || alphaB > chord * 2)
        {
            alphaA = chord / 3;
            alphaB = chord / 3;
        }

        // Newton step on each parameter: the closest point of the curve to each outline point.
        const QPointF c1 = p0 + tangentA * alphaA;
        const QPointF c2 = p3 - tangentB * alphaB;
        for (int i = a + 1; i < b; ++i)
        {
            double &t = u[i - a];
            const QPointF d = bezier(p0, c1, c2, p3, t) - walk.at(i);
            const QPointF d1 = bezierDerivative(p0, c1, c2, p3, t);
            const QPointF d2 = bezierSecond(p0, c1, c2, p3, t);
            const double denominator = QPointF::dotProduct(d1, d1) + QPointF::dotProduct(d, d2);
            if (qAbs(denominator) > 1e-12)
            {
                t = qBound(0.0, t - QPointF::dotProduct(d, d1) / denominator, 1.0);
            }
        }
    }

    segment.handleOut = tangentA * alphaA;
    segment.handleIn = -tangentB * alphaB;
    const QPointF c1 = p0 + segment.handleOut;
    const QPointF c2 = p3 + segment.handleIn;

    // Error: distance from each outline point to the curve (sampled finely).
    QPolygonF samples;
    const int count = qBound(16, (b - a) * 4, 256);
    for (int k = 0; k <= count; ++k)
    {
        samples.append(bezier(p0, c1, c2, p3, static_cast<double>(k) / count));
    }
    for (int i = a + 1; i < b; ++i)
    {
        double best = std::numeric_limits<double>::max();
        const QPointF p = walk.at(i);
        for (int k = 0; k < count; ++k)
        {
            best = qMin(best, distanceToSegment(p, samples.at(k), samples.at(k + 1)));
        }
        if (best > segment.error)
        {
            segment.error = best;
            segment.worst = i;
        }
    }
    return segment;
}

//---------------------------------------------------------------------------------------------------------------------
void fitRange(const Walk &walk, int a, int b, const QPointF &tangentA, const QPointF &tangentB, double tolerance,
              int depth, QVector<Segment> &out)
{
    const Segment segment = fitSegment(walk, a, b, tangentA, tangentB);
    if (segment.error <= tolerance || b - a < 2 || depth > 40 || segment.worst <= a || segment.worst >= b)
    {
        out.append(segment);
        return;
    }
    // Not close enough: pass through the point that strays most, and fit both halves.
    const int middle = segment.worst;
    const QPointF tangent = walk.central(middle);
    fitRange(walk, a, middle, tangentA, tangent, tolerance, depth + 1, out);
    fitRange(walk, middle, b, tangent, tangentB, tolerance, depth + 1, out);
}

//---------------------------------------------------------------------------------------------------------------------
int nearestVertex(const QPolygonF &points, const QPointF &p, double within)
{
    int best = -1;
    double bestDistance = within;
    for (int i = 0; i < points.size(); ++i)
    {
        const double d = length(points.at(i) - p);
        if (d <= bestDistance)
        {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

//---------------------------------------------------------------------------------------------------------------------
double turning(const QPolygonF &points, bool closed, int i)
{
    const int n = static_cast<int>(points.size());
    if (!closed && (i == 0 || i == n - 1))
    {
        return 180;
    }
    const QPointF before = points.at(i) - points.at((i + n - 1) % n);
    const QPointF after = points.at((i + 1) % n) - points.at(i);
    const double l = length(before) * length(after);
    if (l < 1e-18)
    {
        return 0;
    }
    return qRadiansToDegrees(std::acos(qBound(-1.0, QPointF::dotProduct(before, after) / l, 1.0)));
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QPainterPath FittedPath::painterPath() const
{
    QPainterPath path;
    const int n = static_cast<int>(points.size());
    if (n == 0)
    {
        return path;
    }
    path.moveTo(points.constFirst().position);
    const int segments = closed ? n : n - 1;
    for (int i = 0; i < segments; ++i)
    {
        const FittedPoint &a = points.at(i);
        const FittedPoint &b = points.at((i + 1) % n);
        if (a.lineAfter)
        {
            path.lineTo(b.position);
        }
        else
        {
            path.cubicTo(a.position + a.handleOut, b.position + b.handleIn, b.position);
        }
    }
    if (closed)
    {
        path.closeSubpath();
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
QPolygonF FittedPath::polygon(int samplesPerCurve) const
{
    QPolygonF polygon;
    const int n = static_cast<int>(points.size());
    if (n == 0)
    {
        return polygon;
    }
    const int segments = closed ? n : n - 1;
    polygon.append(points.constFirst().position);
    for (int i = 0; i < segments; ++i)
    {
        const FittedPoint &a = points.at(i);
        const FittedPoint &b = points.at((i + 1) % n);
        if (a.lineAfter)
        {
            polygon.append(b.position);
            continue;
        }
        for (int k = 1; k <= samplesPerCurve; ++k)
        {
            polygon.append(bezier(a.position, a.position + a.handleOut, b.position + b.handleIn, b.position,
                                  static_cast<double>(k) / samplesPerCurve));
        }
    }
    if (closed && polygon.size() > 1)
    {
        polygon.removeLast(); // back at the start
    }
    return polygon;
}

//---------------------------------------------------------------------------------------------------------------------
int FittedPath::curveCount() const
{
    const int n = static_cast<int>(points.size());
    int count = 0;
    for (int i = 0; i < (closed ? n : n - 1); ++i)
    {
        count += points.at(i).lineAfter ? 0 : 1;
    }
    return count;
}

//---------------------------------------------------------------------------------------------------------------------
FittedPath OutlineFitter::fit(const QPolygonF &outline, bool closed, const Settings &settings,
                              const QVector<QPointF> &corners, const QVector<QPointF> &notches,
                              const QVector<QPointF> &curvePoints)
{
    const double tolerance = qMax(settings.tolerance, 1e-9);
    FittedPath result;
    result.closed = closed;

    // Repeated points out; a closed outline does not repeat its first point.
    QPolygonF q;
    for (const QPointF &p : outline)
    {
        if (q.isEmpty() || length(p - q.constLast()) > tolerance * 0.02)
        {
            q.append(p);
        }
    }
    while (closed && q.size() > 2 && length(q.constFirst() - q.constLast()) <= tolerance * 0.02)
    {
        q.removeLast();
    }
    if (q.size() < 2 || (closed && q.size() < 3))
    {
        for (const QPointF &p : q)
        {
            FittedPoint point;
            point.position = p;
            point.corner = true;
            result.points.append(point);
        }
        return result;
    }

    QVector<char> isCorner(q.size(), 0);
    QVector<char> isNotch(q.size(), 0);
    QVector<char> isHint(q.size(), 0);
    const auto edgeCount = [&q, closed]() { return closed ? static_cast<int>(q.size()) : static_cast<int>(q.size()) - 1; };

    // Notches become points of the outline.
    for (const QPointF &notch : notches)
    {
        int bestEdge = -1;
        double bestDistance = std::numeric_limits<double>::max();
        for (int e = 0; e < edgeCount(); ++e)
        {
            const double d = distanceToSegment(notch, q.at(e), q.at((e + 1) % q.size()));
            if (d < bestDistance)
            {
                bestDistance = d;
                bestEdge = e;
            }
        }
        if (bestEdge < 0)
        {
            continue;
        }
        const QPointF a = q.at(bestEdge);
        const QPointF b = q.at((bestEdge + 1) % q.size());
        const QPointF ab = b - a;
        const double l2 = QPointF::dotProduct(ab, ab);
        const double t = l2 > 0 ? qBound(0.0, QPointF::dotProduct(notch - a, ab) / l2, 1.0) : 0.0;
        const QPointF on = a + ab * t;
        if (length(on - a) <= tolerance * 0.5)
        {
            isNotch[bestEdge] = 1;
        }
        else if (length(on - b) <= tolerance * 0.5)
        {
            isNotch[(bestEdge + 1) % q.size()] = 1;
        }
        else
        {
            q.insert(bestEdge + 1, on);
            isCorner.insert(bestEdge + 1, 0);
            isNotch.insert(bestEdge + 1, 1);
            isHint.insert(bestEdge + 1, 0);
        }
    }

    const double snap = tolerance * 4;
    for (const QPointF &corner : corners)
    {
        const int i = nearestVertex(q, corner, snap);
        if (i >= 0)
        {
            isCorner[i] = 1;
        }
    }
    for (const QPointF &point : curvePoints)
    {
        const int i = nearestVertex(q, point, snap);
        if (i >= 0)
        {
            isHint[i] = 1;
        }
    }

    const int n = static_cast<int>(q.size());
    for (int i = 0; i < n; ++i)
    {
        if (turning(q, closed, i) > settings.cornerAngle)
        {
            isCorner[i] = 1;
        }
    }

    // Long edges are straight lines; their ends break the curves.
    QVector<double> lengths;
    for (int e = 0; e < edgeCount(); ++e)
    {
        lengths.append(length(q.at((e + 1) % n) - q.at(e)));
    }
    QVector<double> sorted = lengths;
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted.isEmpty() ? 0 : sorted.at(sorted.size() / 2);
    QVector<char> straight(edgeCount(), 0);
    for (int e = 0; e < edgeCount(); ++e)
    {
        straight[e] = lengths.at(e) >= settings.straightLength && lengths.at(e) >= 4 * median ? 1 : 0;
    }

    QVector<char> isBreak(n, 0);
    for (int i = 0; i < n; ++i)
    {
        const bool edgeAfter = (closed || i < n - 1) && straight.at(i % edgeCount());
        const bool edgeBefore = (closed || i > 0) && straight.at((i + edgeCount() - 1) % edgeCount());
        isBreak[i] = (isCorner.at(i) || isNotch.at(i) || edgeAfter || edgeBefore) ? 1 : 0;
    }

    int start = 0;
    if (closed)
    {
        // Start at a corner; a smooth closed outline gets one where it turns most.
        int firstCorner = -1;
        int firstBreak = -1;
        for (int i = 0; i < n; ++i)
        {
            if (isCorner.at(i) && firstCorner < 0)
            {
                firstCorner = i;
            }
            if (isBreak.at(i) && firstBreak < 0)
            {
                firstBreak = i;
            }
        }
        if (firstBreak < 0)
        {
            int sharpest = 0;
            for (int i = 1; i < n; ++i)
            {
                if (turning(q, closed, i) > turning(q, closed, sharpest))
                {
                    sharpest = i;
                }
            }
            isCorner[sharpest] = 1;
            isBreak[sharpest] = 1;
            firstCorner = sharpest;
        }
        start = firstCorner >= 0 ? firstCorner : firstBreak;
    }
    const Walk walk(q, closed, start);
    const int last = closed ? n : n - 1; // walk index of the end (back at the start when closed)

    QVector<int> breaks;
    for (int i = 0; i <= last; ++i)
    {
        if (i == 0 || i == last || isBreak.at(walk.original(i)))
        {
            breaks.append(i);
        }
    }

    // Straight runs first: curves next to them leave along them.
    const int runs = static_cast<int>(breaks.size()) - 1;
    QVector<char> lineRun(runs, 0);
    for (int r = 0; r < runs; ++r)
    {
        const int a = breaks.at(r);
        const int b = breaks.at(r + 1);
        double deviation = 0;
        for (int i = a + 1; i < b; ++i)
        {
            deviation = qMax(deviation, distanceToSegment(walk.at(i), walk.at(a), walk.at(b)));
        }
        lineRun[r] = (b - a == 1 || deviation <= tolerance) ? 1 : 0;
        if (lineRun.at(r))
        {
            result.maxDeviation = qMax(result.maxDeviation, deviation);
        }
    }

    const auto makePoint = [&](int i)
    {
        FittedPoint point;
        const int o = walk.original(i);
        point.position = q.at(o);
        point.corner = isCorner.at(o) != 0;
        point.notch = isNotch.at(o) != 0;
        return point;
    };

    result.points.append(makePoint(0));
    for (int r = 0; r < runs; ++r)
    {
        const int a = breaks.at(r);
        const int b = breaks.at(r + 1);
        const bool endIsStart = closed && b == last;

        if (lineRun.at(r))
        {
            result.points.last().lineAfter = true;
            if (!endIsStart)
            {
                result.points.append(makePoint(b));
            }
            continue;
        }

        // Tangents at the ends: along a neighbouring straight line, across a smooth break, or one sided at a corner.
        const int previousRun = r > 0 ? r - 1 : (closed ? runs - 1 : -1);
        const int nextRun = r + 1 < runs ? r + 1 : (closed ? 0 : -1);
        QPointF tangentA;
        if (isCorner.at(walk.original(a)))
        {
            tangentA = walk.oneSided(a, 1, b - a);
        }
        else if (previousRun >= 0 && lineRun.at(previousRun))
        {
            tangentA = normalized(walk.at(a) - walk.at(breaks.at(previousRun)));
        }
        else
        {
            tangentA = walk.central(a);
        }
        QPointF tangentB;
        if (isCorner.at(walk.original(b)))
        {
            tangentB = walk.oneSided(b, -1, b - a);
        }
        else if (nextRun >= 0 && lineRun.at(nextRun))
        {
            const int nextEnd = nextRun == 0 && closed ? breaks.at(1) + last : breaks.at(nextRun + 1);
            tangentB = normalized(walk.at(nextEnd) - walk.at(b));
        }
        else
        {
            tangentB = walk.central(b);
        }

        // Through the given curve points, then as many more as the tolerance needs.
        QVector<int> keys{a};
        for (int i = a + 1; i < b; ++i)
        {
            if (isHint.at(walk.original(i)))
            {
                keys.append(i);
            }
        }
        keys.append(b);

        QVector<Segment> segments;
        for (int k = 0; k + 1 < keys.size(); ++k)
        {
            const QPointF ta = k == 0 ? tangentA : walk.central(keys.at(k));
            const QPointF tb = k + 2 == keys.size() ? tangentB : walk.central(keys.at(k + 1));
            fitRange(walk, keys.at(k), keys.at(k + 1), ta, tb, tolerance, 0, segments);
        }

        for (const Segment &segment : qAsConst(segments))
        {
            result.maxDeviation = qMax(result.maxDeviation, segment.error);
            FittedPoint &from = result.points.last();
            from.lineAfter = false;
            from.handleOut = segment.handleOut;
            if (closed && segment.to == last)
            {
                result.points.first().handleIn = segment.handleIn;
            }
            else
            {
                FittedPoint point = makePoint(segment.to);
                point.handleIn = segment.handleIn;
                result.points.append(point);
            }
        }
    }
    return result;
}

} // namespace Seamly3D
