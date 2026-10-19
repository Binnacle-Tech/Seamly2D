/******************************************************************************
 *   @file   piecearrangement.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Placement of pattern pieces around the body.
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

#include "piecearrangement.h"

#include <QMatrix4x4>
#include <QRegularExpression>
#include <QtMath>

#include <functional>
#include <limits>

#include "meshslicer.h"

namespace Seamly3D
{

namespace
{

const float clearance = 4.0f;     // centimeters between the body and the pieces wrapped around it
const float shoulderLift = 4.0f;  // centimeters from the back of the neck up to above the shoulder seams
const float asideSpacing = 15.0f; // centimeters between pieces hung beside the body
const float waistLift = 2.0f;     // centimeters from the waist up to the top of skirts and trousers
const float legClearance = 2.5f;  // centimeters between the legs and the trouser pieces wrapped around them

//---------------------------------------------------------------------------------------------------------------------
/** @brief wrap bends a piece around a vertical cylinder; @p angle is where the piece's u = 0 line goes. */
Mesh wrap(const Mesh &flat, float centerX, float centerZ, float radius, float angle, float top,
          const std::function<float(const QVector3D &)> &u, const std::function<float(const QVector3D &)> &v)
{
    Mesh mesh = flat;
    for (QVector3D &p : mesh.positions)
    {
        const float phi = angle + u(p) / radius;
        const float y = top + v(p);
        p = QVector3D(centerX + radius * qSin(phi), y, centerZ + radius * qCos(phi));
    }
    mesh.computeNormals();
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement opposite(PiecePlacement placement)
{
    switch (placement)
    {
        case PiecePlacement::LeftSide:
            return PiecePlacement::RightSide;
        case PiecePlacement::RightSide:
            return PiecePlacement::LeftSide;
        case PiecePlacement::LeftSleeve:
            return PiecePlacement::RightSleeve;
        case PiecePlacement::RightSleeve:
            return PiecePlacement::LeftSleeve;
        case PiecePlacement::LeftLegFront:
            return PiecePlacement::RightLegFront;
        case PiecePlacement::LeftLegBack:
            return PiecePlacement::RightLegBack;
        case PiecePlacement::RightLegFront:
            return PiecePlacement::LeftLegFront;
        case PiecePlacement::RightLegBack:
            return PiecePlacement::LeftLegBack;
        default:
            return placement;
    }
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief legPiece wraps a trouser piece around the left leg, or the right one (mirrored). */
Mesh legPiece(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool mirrored, bool flip,
              const BodyFrame &frame)
{
    const auto top = static_cast<float>(bounds.top());
    const auto height = static_cast<float>(bounds.height());

    // The crease runs down the middle of the hem; the top of the piece leans to the side seam.
    float hemLeft = std::numeric_limits<float>::max();
    float hemRight = std::numeric_limits<float>::lowest();
    float topSum = 0;
    int topCount = 0;
    for (const QVector3D &p : flat.positions)
    {
        if (p.y() > top + height * 0.8f)
        {
            hemLeft = qMin(hemLeft, p.x());
            hemRight = qMax(hemRight, p.x());
        }
        else if (p.y() < top + height * 0.2f)
        {
            topSum += p.x();
            ++topCount;
        }
    }
    const float crease = hemLeft <= hemRight ? (hemLeft + hemRight) / 2 : static_cast<float>(bounds.center().x());
    float outward = topCount > 0 && topSum / static_cast<float>(topCount) < crease ? -1.0f : 1.0f;
    if (flip)
    {
        outward = -outward;
    }
    // Across the piece from the crease, positive toward the side seam; the center front (or back) is at the top.
    const auto across = [crease, outward](const QVector3D &p) { return outward * (p.x() - crease); };
    float center = 0;
    for (const QVector3D &p : flat.positions)
    {
        if (p.y() < top + height * 0.2f)
        {
            center = qMin(center, across(p));
        }
    }

    const bool back = placement == PiecePlacement::LeftLegBack || placement == PiecePlacement::RightLegBack;
    const bool right = (placement == PiecePlacement::RightLegFront || placement == PiecePlacement::RightLegBack)
                       != mirrored;
    const float waist = frame.waistHeight + waistLift;
    // Down the leg, the piece wraps around the leg; up from the top of the thigh it blends into the hips.
    const float blendLow = frame.crotchHeight - 6.0f;
    const float blendHigh = qMax(frame.hipHeight, blendLow + 4.0f);

    Mesh mesh = flat;
    for (QVector3D &p : mesh.positions)
    {
        const float u = across(p);
        const float y = waist - (p.y() - top);

        const QVector3D axis = frame.legAxis(y);
        const float radius = qMin(frame.legRadius(y) + legClearance, qMax(1.0f, axis.x() - 0.5f));
        const float legAngle = u / radius;
        const float hipAngle = (u - center) / frame.hipRadius;
        const auto around = [back](float angle)
        { return QVector3D(qSin(angle), 0, back ? -qCos(angle) : qCos(angle)); };
        const QVector3D onLeg = QVector3D(axis.x(), y, axis.z()) + around(legAngle) * radius;
        const QVector3D onHips = QVector3D(0, y, 0) + around(hipAngle) * frame.hipRadius;

        float t = qBound(0.0f, (y - blendLow) / (blendHigh - blendLow), 1.0f);
        t = t * t * (3 - 2 * t);
        QVector3D position = onLeg * (1 - t) + onHips * t;
        position.setX(qMax(position.x(), 0.3f)); // a leg stays on its own side of the body
        if (right)
        {
            position.setX(-position.x());
        }
        p = position;
    }
    mesh.computeNormals();
    return mesh;
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief bandPiece wraps a waistband or a collar lengthwise around the waist or the neck. */
Mesh bandPiece(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool half, bool mirrored, bool flip,
               const BodyFrame &frame)
{
    // Along the band and across it, from its top; a band drawn upright is turned (its left side on top).
    const bool lengthwise = bounds.width() >= bounds.height();
    const auto length = static_cast<float>(lengthwise ? bounds.width() : bounds.height());
    const auto width = static_cast<float>(lengthwise ? bounds.height() : bounds.width());
    const auto along = [&bounds, lengthwise](const QVector3D &p)
    { return static_cast<float>(lengthwise ? p.x() - bounds.left() : p.y() - bounds.top()); };
    const auto down = [&bounds, lengthwise](const QVector3D &p)
    { return static_cast<float>(lengthwise ? p.y() - bounds.top() : bounds.right() - p.x()); };

    const bool collar = placement == PiecePlacement::Collar;
    const QVector3D center = collar ? frame.neckCenter : frame.waistCenter;
    const float bottom = collar ? frame.neckBaseHeight : frame.waistHeight + waistLift;
    // Round enough to close the ring, but not inside the body.
    const float round = half ? 2 * length : length;
    const float radius = qMax(round / (2 * static_cast<float>(M_PI)),
                              (collar ? frame.neckRadius : frame.waistRadius) + 1.0f);

    Mesh mesh = flat;
    for (QVector3D &p : mesh.positions)
    {
        float t = along(p);
        if (flip)
        {
            t = length - t;
        }
        // A whole band starts and ends at the center front; half a band runs from the center back to the front.
        float angle = half ? static_cast<float>(M_PI) - t / radius : t / radius;
        if (mirrored)
        {
            angle = -angle;
        }
        p = QVector3D(center.x() + radius * qSin(angle), bottom + width - down(p), center.z() + radius * qCos(angle));
    }
    mesh.computeNormals();
    return mesh;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::fromMeasurements(const BodyMeasurements &measurements)
{
    const BodyMeasurements m = measurements.sanitized();
    const auto value = [&m](BodyMeasurement measurement) { return static_cast<float>(m.value(measurement)); };

    BodyFrame frame;
    // The neck side and the top of the shoulders are above the back of the neck: start the pieces a little higher so
    // their shoulder seams close over the shoulders, not through them.
    frame.neckHeight = value(BodyMeasurement::NeckBackHeight) + shoulderLift;
    frame.shoulderHeight = value(BodyMeasurement::ShoulderTipHeight);
    const float girth = qMax(value(BodyMeasurement::BustCirc), value(BodyMeasurement::HipCirc));
    frame.torsoRadius = girth / (2 * static_cast<float>(M_PI)) + clearance;
    frame.armRadius = value(BodyMeasurement::UpperArmCirc) / (2 * static_cast<float>(M_PI));
    frame.sleeveRadius = frame.armRadius + clearance;

    const float angle = qDegreesToRadians(40.0f);
    frame.leftShoulder = QVector3D(value(BodyMeasurement::ShoulderWidth) * 0.45f, frame.shoulderHeight - frame.armRadius, 0);
    frame.leftWrist = frame.leftShoulder
                      + QVector3D(qSin(angle), -qCos(angle), 0) * value(BodyMeasurement::ArmLength);

    const auto radius = [&value](BodyMeasurement girth) { return value(girth) / (2 * static_cast<float>(M_PI)); };
    frame.waistHeight = value(BodyMeasurement::WaistHeight);
    frame.hipHeight = value(BodyMeasurement::HipHeight);
    frame.crotchHeight = value(BodyMeasurement::CrotchHeight);
    frame.kneeHeight = value(BodyMeasurement::KneeHeight);
    frame.ankleHeight = value(BodyMeasurement::AnkleHeight);
    frame.hipRadius = radius(BodyMeasurement::HipCirc) + clearance;
    frame.thighRadius = radius(BodyMeasurement::ThighCirc);
    frame.kneeRadius = radius(BodyMeasurement::KneeCirc);
    frame.ankleRadius = radius(BodyMeasurement::AnkleCirc);
    // The thighs touch at the crotch; the legs stand a little apart.
    const float legX = qMax(frame.thighRadius, radius(BodyMeasurement::HipCirc) * 0.5f);
    frame.leftHip = QVector3D(legX, frame.crotchHeight, 0);
    frame.leftKnee = QVector3D(legX + 1.0f, frame.kneeHeight, 0);

    frame.neckBaseHeight = value(BodyMeasurement::NeckBackHeight);
    frame.neckRadius = radius(BodyMeasurement::NeckCirc);
    frame.waistRadius = radius(BodyMeasurement::WaistCirc);
    frame.neckCenter = QVector3D(0, frame.neckBaseHeight, 0);
    frame.waistCenter = QVector3D(0, frame.waistHeight, 0);
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::withNeckAndWaist(const Mesh &body) const
{
    // The loop nearest the center line at that height: the arms hang farther out.
    const auto loop = [&body](float height, QVector3D *center, float *radius)
    {
        float nearest = std::numeric_limits<float>::max();
        for (const MeshSlicer::Contour &contour : MeshSlicer::sliceAtHeight(body, height))
        {
            if (!contour.closed || contour.points.size() < 3 || contour.length() < 10.0)
            {
                continue;
            }
            QVector3D sum;
            double length = 0;
            for (int i = 0; i < contour.points.size(); ++i)
            {
                const QVector3D &a = contour.points.at(i);
                const QVector3D &b = contour.points.at((i + 1) % contour.points.size());
                const float segment = (b - a).length();
                sum += (a + b) / 2 * segment;
                length += static_cast<double>(segment);
            }
            const QVector3D middle = sum / static_cast<float>(length);
            if (qAbs(middle.x()) < nearest)
            {
                nearest = qAbs(middle.x());
                *center = middle;
                *radius = static_cast<float>(length / (2 * M_PI));
            }
        }
        return nearest < 5.0f;
    };

    BodyFrame frame = *this;
    QVector3D center;
    float radius = 0;
    // Above the back of the neck, where the neck is measured: lower, the cut runs into the shoulders.
    if (loop(neckBaseHeight + 4.0f, &center, &radius))
    {
        frame.neckCenter = QVector3D(center.x(), neckBaseHeight, center.z());
        frame.neckRadius = radius;
    }
    if (loop(waistHeight, &center, &radius))
    {
        frame.waistCenter = center;
        frame.waistRadius = radius;
    }
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::withLegs(const Mesh &body) const
{
    // The left leg is the biggest loop left of the center, but not as far out as the hand.
    const auto leg = [&body, this](float height, QVector3D *center, float *radius)
    {
        double longest = 0;
        for (const MeshSlicer::Contour &contour : MeshSlicer::sliceAtHeight(body, height))
        {
            if (!contour.closed || contour.points.size() < 3)
            {
                continue;
            }
            QVector3D sum;
            double length = 0;
            for (int i = 0; i < contour.points.size(); ++i)
            {
                const QVector3D &a = contour.points.at(i);
                const QVector3D &b = contour.points.at((i + 1) % contour.points.size());
                const float segment = (b - a).length();
                sum += (a + b) / 2 * segment;
                length += static_cast<double>(segment);
            }
            if (length < 1e-6)
            {
                continue;
            }
            const QVector3D middle = sum / static_cast<float>(length);
            if (middle.x() < 1.0f || middle.x() > hipRadius * 1.2f || length <= longest)
            {
                continue;
            }
            longest = length;
            *center = middle;
            *radius = static_cast<float>(length / (2 * M_PI));
        }
        return longest > 0;
    };

    QVector3D thigh;
    QVector3D knee;
    float thighR = 0;
    float kneeR = 0;
    const float thighHeight = crotchHeight - 5.0f;
    if (!leg(thighHeight, &thigh, &thighR) || !leg(kneeHeight, &knee, &kneeR) || thighHeight - kneeHeight < 1.0f)
    {
        return *this;
    }
    BodyFrame frame = *this;
    const QVector3D step = (thigh - knee) / (thighHeight - kneeHeight); // per centimeter up
    frame.leftHip = knee + step * (crotchHeight - kneeHeight);
    frame.leftKnee = knee;
    frame.thighRadius = thighR;
    frame.kneeRadius = kneeR;
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
QVector3D BodyFrame::legAxis(float height) const
{
    const float span = leftHip.y() - leftKnee.y();
    if (qAbs(span) < 1e-3f)
    {
        return QVector3D(leftHip.x(), height, leftHip.z());
    }
    QVector3D axis = leftKnee + (leftHip - leftKnee) * ((height - leftKnee.y()) / span);
    axis.setY(height);
    return axis;
}

//---------------------------------------------------------------------------------------------------------------------
float BodyFrame::legRadius(float height) const
{
    if (height >= crotchHeight)
    {
        return thighRadius;
    }
    if (height >= kneeHeight)
    {
        const float t = (height - kneeHeight) / qMax(1.0f, crotchHeight - kneeHeight);
        return kneeRadius + (thighRadius - kneeRadius) * t;
    }
    const float t = qBound(0.0f, (height - ankleHeight) / qMax(1.0f, kneeHeight - ankleHeight), 1.0f);
    return ankleRadius + (kneeRadius - ankleRadius) * t;
}

//---------------------------------------------------------------------------------------------------------------------
BodyFrame BodyFrame::withArm(const QVector3D &shoulder, const QVector3D &wrist) const
{
    BodyFrame frame = *this;
    if ((wrist - shoulder).length() > 1.0f)
    {
        frame.leftShoulder = shoulder;
        frame.leftWrist = wrist;
    }
    return frame;
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement PieceArrangement::guess(const QString &name, const QPolygonF &outline)
{
    const QString n = name.toLower();
    // Long keywords anywhere ("Topsleeve"); short ones at the start of a word, so "outside" is not a side and
    // German compounds ("Seitenteil") still count.
    static const QRegularExpression separator(QStringLiteral("[^\\w]+"),
                                              QRegularExpression::UseUnicodePropertiesOption);
    const QStringList words = n.split(separator, Qt::SkipEmptyParts);
    const auto has = [&n, &words](std::initializer_list<const char *> keywords)
    {
        for (const char *keyword : keywords)
        {
            const QString k = QString::fromUtf8(keyword);
            if (k.size() >= 5 || k.contains(QLatin1Char(' ')))
            {
                if (n.contains(k))
                {
                    return true;
                }
                continue;
            }
            for (const QString &word : words)
            {
                if (word.startsWith(k))
                {
                    return true;
                }
            }
        }
        return false;
    };

    // English, then the languages Seamly2D is most used in (German, French, Spanish, Dutch, Italian...).
    if (has({"sleeve", "ärmel", "aermel", "manche", "manga", "mouw", "manica"}))
    {
        return PiecePlacement::LeftSleeve;
    }
    if (has({"waistband", "waist band", "bund", "ceinture", "cintur", "pretina", "tailleband"}))
    {
        return PiecePlacement::Waistband;
    }
    if (has({"collar", "neckband", "neck band", "kragen", "col", "cuello", "kraag", "colletto"}))
    {
        return PiecePlacement::Collar;
    }
    if (isYoke(name))
    {
        // On top of the front or the back (most yokes are at the back).
        return has({"front", "vorder", "devant", "delantero", "voor", "davanti"}) ? PiecePlacement::Front
                                                                                 : PiecePlacement::Back;
    }
    if (has({"pocket", "cuff", "facing", "band", "belt", "tasche", "poche", "bolsillo", "zak"}))
    {
        return PiecePlacement::Aside;
    }
    const bool back = has({"back", "rück", "rueck", "dos", "espalda", "rug", "dietro", "retro"});
    if (has({"trouser", "pant", "short", "jean", "leg", "hose", "bein", "pantal", "broek", "calzon"}))
    {
        return back ? PiecePlacement::LeftLegBack : PiecePlacement::LeftLegFront;
    }
    if (has({"skirt", "rock", "jupe", "falda", "rok", "gonna", "saia"}))
    {
        return back ? PiecePlacement::LowerBack : PiecePlacement::LowerFront;
    }
    if (has({"back", "rück", "rueck", "dos", "espalda", "rug", "dietro", "retro"}))
    {
        return PiecePlacement::Back;
    }
    if (has({"side", "seite", "côté", "cote", "lado", "zij"}))
    {
        return PiecePlacement::LeftSide;
    }
    if (has({"front", "vorder", "devant", "delantero", "voor", "davanti"}))
    {
        return PiecePlacement::Front;
    }
    return outline.size() >= 3 ? guessFromShape(outline) : PiecePlacement::Aside;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceArrangement::isYoke(const QString &name)
{
    const QString n = name.toLower();
    for (const char *word : {"yoke", "passe", "sattel", "canes", "carré", "spalla"})
    {
        if (n.contains(QString::fromUtf8(word)))
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
bool PieceArrangement::isUnderSleeve(const QString &name)
{
    const QString n = name.toLower();
    for (const char *word : {"under", "unter", "lower", "dessous", "onder", "sotto", "inferior", "bajo"})
    {
        if (n.contains(QLatin1String(word)))
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
int PieceArrangement::centerFirst(const PatternPiece &piece)
{
    if (piece.onFold)
    {
        return 0;
    }
    const QString n = piece.name.toLower();
    const auto has = [&n](std::initializer_list<const char *> words)
    {
        for (const char *word : words)
        {
            if (n.contains(QString::fromUtf8(word)))
            {
                return true;
            }
        }
        return false;
    };
    // The inside panel of a front is the one next to the center ("inside" is not a side).
    if (has({"center", "centre", "middle", "mitte", "milieu", "medio", "midden", "centrale", "inside", "inner",
             "innen"}))
    {
        return 1;
    }
    if (has({"outside", "outer", "side", "seite", "côté", "lado", "zij", "fianco"}))
    {
        return 3;
    }
    return 2;
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement PieceArrangement::guessFromShape(const QPolygonF &outline)
{
    const QRectF bounds = outline.boundingRect();
    const double width = bounds.width();
    const double height = bounds.height();
    if (outline.size() < 3 || qMax(width, height) < 22.0)
    {
        return PiecePlacement::Aside; // pockets, flaps, cuffs, tabs
    }
    if (width >= 3.0 * height && height <= 15.0)
    {
        return width >= 50.0 ? PiecePlacement::Waistband : PiecePlacement::Collar;
    }
    if (height >= 5.0 * width)
    {
        return PiecePlacement::Aside; // facings, plackets, ties
    }

    // A sleeve cap: a dome, highest in the middle, much narrower near its top than the sleeve is wide, and the
    // sleeve no wider at the hem. (A two-piece sleeve's cap is lopsided: that takes its name.)
    const auto widthAt = [&outline](double y)
    {
        double left = std::numeric_limits<double>::max();
        double right = std::numeric_limits<double>::lowest();
        for (int i = 0; i < outline.size(); ++i)
        {
            const QPointF a = outline.at(i);
            const QPointF b = outline.at((i + 1) % outline.size());
            if ((a.y() - y) * (b.y() - y) <= 0 && !qFuzzyCompare(a.y(), b.y()))
            {
                const double x = a.x() + (b.x() - a.x()) * (y - a.y()) / (b.y() - a.y());
                left = qMin(left, x);
                right = qMax(right, x);
            }
        }
        return right > left ? right - left : 0.0;
    };
    int top = 0;
    for (int i = 1; i < outline.size(); ++i)
    {
        if (outline.at(i).y() < outline.at(top).y())
        {
            top = i;
        }
    }
    double widest = 0;
    for (int step = 1; step < 20; ++step)
    {
        widest = qMax(widest, widthAt(bounds.top() + height * step / 20.0));
    }
    const double crown = qAbs(outline.at(top).x() - bounds.center().x()) / width;
    const double cap = widthAt(bounds.top() + height * 0.08);
    const double hem = widthAt(bounds.top() + height * 0.95);
    if (height >= 15.0 && crown < 0.15 && cap < 0.6 * widest && hem <= widest)
    {
        return PiecePlacement::LeftSleeve;
    }
    if (height < 25.0 && qMax(width, height) < 35.0)
    {
        return PiecePlacement::Aside; // yokes, pocket bags
    }
    if (height >= 80.0)
    {
        return PiecePlacement::LeftLegFront;
    }
    return PiecePlacement::Front;
}

//---------------------------------------------------------------------------------------------------------------------
double PieceArrangement::neckDepth(const QPolygonF &outline)
{
    if (outline.size() < 3)
    {
        return 0;
    }
    const QRectF bounds = outline.boundingRect();
    int top = 0;
    for (int i = 1; i < outline.size(); ++i)
    {
        if (outline.at(i).y() < outline.at(top).y())
        {
            top = i;
        }
    }
    // The center line is the side the neck is on; its top, where the neckline meets it.
    const bool left = outline.at(top).x() - bounds.left() < bounds.right() - outline.at(top).x();
    const double side = left ? bounds.left() : bounds.right();
    double centerTop = std::numeric_limits<double>::max();
    for (const QPointF &point : outline)
    {
        if (qAbs(point.x() - side) < 0.5)
        {
            centerTop = qMin(centerTop, point.y());
        }
    }
    return centerTop < std::numeric_limits<double>::max() ? centerTop - outline.at(top).y() : 0.0;
}

//---------------------------------------------------------------------------------------------------------------------
QString PieceArrangement::displayName(PiecePlacement placement)
{
    switch (placement)
    {
        case PiecePlacement::Front:
            return tr("Front");
        case PiecePlacement::Back:
            return tr("Back");
        case PiecePlacement::LeftSide:
            return tr("Left side");
        case PiecePlacement::RightSide:
            return tr("Right side");
        case PiecePlacement::LeftSleeve:
            return tr("Left sleeve");
        case PiecePlacement::RightSleeve:
            return tr("Right sleeve");
        case PiecePlacement::LowerFront:
            return tr("Front, from the waist");
        case PiecePlacement::LowerBack:
            return tr("Back, from the waist");
        case PiecePlacement::LeftLegFront:
            return tr("Left leg, front");
        case PiecePlacement::LeftLegBack:
            return tr("Left leg, back");
        case PiecePlacement::RightLegFront:
            return tr("Right leg, front");
        case PiecePlacement::RightLegBack:
            return tr("Right leg, back");
        case PiecePlacement::Waistband:
            return tr("Waistband");
        case PiecePlacement::Collar:
            return tr("Collar");
        case PiecePlacement::Aside:
            return tr("Beside the body");
        case PiecePlacement::Hidden:
        default:
            return tr("Hidden");
    }
}

//---------------------------------------------------------------------------------------------------------------------
namespace
{
const char *const placementIds[] = {"front",           "back",           "left-side",       "right-side",
                                    "left-sleeve",     "right-sleeve",   "lower-front",     "lower-back",
                                    "left-leg-front",  "left-leg-back",  "right-leg-front", "right-leg-back",
                                    "waistband",       "collar",         "aside",           "hidden"};
static_assert(sizeof(placementIds) / sizeof(placementIds[0]) == static_cast<size_t>(PiecePlacement::Count),
              "every placement needs an id");
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QString PieceArrangement::placementId(PiecePlacement placement)
{
    const auto index = static_cast<int>(placement);
    return index >= 0 && index < static_cast<int>(PiecePlacement::Count) ? QString::fromLatin1(placementIds[index])
                                                                           : QStringLiteral("aside");
}

//---------------------------------------------------------------------------------------------------------------------
PiecePlacement PieceArrangement::placementFromId(const QString &id, bool *known)
{
    for (int i = 0; i < static_cast<int>(PiecePlacement::Count); ++i)
    {
        if (id == QLatin1String(placementIds[i]))
        {
            if (known != nullptr)
            {
                *known = true;
            }
            return static_cast<PiecePlacement>(i);
        }
    }
    if (known != nullptr)
    {
        *known = false;
    }
    return PiecePlacement::Aside;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Mesh> PieceArrangement::place(const Mesh &flat, const QRectF &bounds, PiecePlacement placement, bool pair,
                                      bool flip, const BodyFrame &frame, float asideStart,
                                      const PieceAdjustment &adjustment)
{
    if (adjustment.isNull())
    {
        return placeAsDrawn(flat, bounds, placement, pair, flip, frame, asideStart);
    }

    // Turned: the piece spun about its middle, as if drawn that way.
    Mesh turned = flat;
    QRectF turnedBounds = bounds;
    if (!qFuzzyIsNull(adjustment.turn))
    {
        const QPointF middle = bounds.center();
        const double angle = qDegreesToRadians(static_cast<double>(adjustment.turn));
        const auto c = static_cast<float>(std::cos(angle));
        const auto s = static_cast<float>(std::sin(angle));
        // Pattern Y points down: counterclockwise as drawn is clockwise in these coordinates.
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        for (QVector3D &p : turned.positions)
        {
            const float x = p.x() - static_cast<float>(middle.x());
            const float y = p.y() - static_cast<float>(middle.y());
            p.setX(static_cast<float>(middle.x()) + x * c + y * s);
            p.setY(static_cast<float>(middle.y()) - x * s + y * c);
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
        turnedBounds = QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }
    QVector<Mesh> result = placeAsDrawn(turned, turnedBounds, placement, pair, flip, frame, asideStart);

    // Then along and about the axis of its place; the mirrored copy the mirrored way.
    for (int copy = 0; copy < result.size(); ++copy)
    {
        Mesh &mesh = result[copy];
        QVector3D middle;
        for (const QVector3D &p : qAsConst(mesh.positions))
        {
            middle += p;
        }
        middle /= static_cast<float>(qMax(1, static_cast<int>(mesh.positions.size())));
        QVector3D origin;
        QVector3D up;
        axis(placement, frame, middle.x() < 0 ? -1.0f : 1.0f, &origin, &up);
        const bool turnsAround = placement != PiecePlacement::Aside && placement != PiecePlacement::Hidden;
        // The mirror image of a turn about one limb is the opposite turn about the other.
        const float degrees = turnsAround ? (copy == 0 ? adjustment.around : -adjustment.around) : 0.0f;
        QMatrix4x4 rotation;
        rotation.rotate(degrees, up);
        for (QVector3D &p : mesh.positions)
        {
            p = origin + rotation.map(p - origin) + up * adjustment.lift;
        }
        mesh.computeNormals();
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
void PieceArrangement::axis(PiecePlacement placement, const BodyFrame &frame, float side, QVector3D *origin,
                            QVector3D *up)
{
    const auto mirrored = [side](const QVector3D &p) { return QVector3D(side * p.x(), p.y(), p.z()); };
    *origin = QVector3D(0, 0, 0);
    *up = QVector3D(0, 1, 0);
    switch (placement)
    {
        case PiecePlacement::LeftSleeve:
        case PiecePlacement::RightSleeve:
            *origin = mirrored(frame.leftShoulder);
            *up = (*origin - mirrored(frame.leftWrist)).normalized();
            break;
        case PiecePlacement::LeftLegFront:
        case PiecePlacement::LeftLegBack:
        case PiecePlacement::RightLegFront:
        case PiecePlacement::RightLegBack:
            *origin = mirrored(frame.leftHip);
            *up = (*origin - mirrored(frame.leftKnee)).normalized();
            break;
        case PiecePlacement::Waistband:
            *origin = frame.waistCenter;
            break;
        case PiecePlacement::Collar:
            *origin = frame.neckCenter;
            break;
        default:
            break;
    }
}

//---------------------------------------------------------------------------------------------------------------------
float PieceArrangement::aroundRadius(PiecePlacement placement, const BodyFrame &frame)
{
    switch (placement)
    {
        case PiecePlacement::LeftSleeve:
        case PiecePlacement::RightSleeve:
            return frame.sleeveRadius;
        case PiecePlacement::LeftLegFront:
        case PiecePlacement::LeftLegBack:
        case PiecePlacement::RightLegFront:
        case PiecePlacement::RightLegBack:
            return frame.thighRadius + 2.5f;
        case PiecePlacement::LowerFront:
        case PiecePlacement::LowerBack:
            return frame.hipRadius;
        case PiecePlacement::Waistband:
            return frame.waistRadius + 1.0f;
        case PiecePlacement::Collar:
            return frame.neckRadius + 1.0f;
        default:
            return frame.torsoRadius;
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Mesh> PieceArrangement::placeAsDrawn(const Mesh &flat, const QRectF &bounds, PiecePlacement placement,
                                             bool pair, bool flip, const BodyFrame &frame, float asideStart)
{
    QVector<Mesh> result;
    if (flat.isEmpty() || placement == PiecePlacement::Hidden)
    {
        return result;
    }

    const auto minX = static_cast<float>(bounds.left());
    const auto maxX = static_cast<float>(bounds.right());
    const auto centerX = static_cast<float>(bounds.center().x());
    const auto topY = static_cast<float>(bounds.top());
    const auto width = static_cast<float>(bounds.width());

    // Pattern Y points down; up the body is minus Y. The top of the piece goes to the top of its place.
    const auto v = [topY](const QVector3D &p) { return topY - p.y(); };
    const float sign = flip ? -1.0f : 1.0f;
    const auto centered = [centerX, sign](const QVector3D &p) { return sign * (p.x() - centerX); };

    switch (placement)
    {
        case PiecePlacement::Front:
        case PiecePlacement::Back:
        case PiecePlacement::LowerFront:
        case PiecePlacement::LowerBack:
        {
            const bool front = placement == PiecePlacement::Front || placement == PiecePlacement::LowerFront;
            const bool lower = placement == PiecePlacement::LowerFront || placement == PiecePlacement::LowerBack;
            const float angle = front ? 0.0f : static_cast<float>(M_PI);
            const float radius = lower ? frame.hipRadius : frame.torsoRadius;
            const float top = lower ? frame.waistHeight + waistLift : frame.neckHeight;
            if (!pair)
            {
                result.append(wrap(flat, 0, 0, radius, angle, top, centered, v));
                break;
            }
            // A half piece: one edge on the center line, the mirrored copy on the other side of it.
            const auto fromEdge = [minX, maxX, flip](const QVector3D &p) { return flip ? maxX - p.x() : p.x() - minX; };
            result.append(wrap(flat, 0, 0, radius, angle, top, fromEdge, v));
            result.append(wrap(flat, 0, 0, radius, angle, top,
                               [&fromEdge](const QVector3D &p) { return -fromEdge(p); }, v));
            break;
        }
        case PiecePlacement::Waistband:
        case PiecePlacement::Collar:
        {
            result.append(bandPiece(flat, bounds, placement, pair, false, flip, frame));
            if (pair)
            {
                result.append(bandPiece(flat, bounds, placement, true, true, flip, frame));
            }
            break;
        }
        case PiecePlacement::LeftLegFront:
        case PiecePlacement::LeftLegBack:
        case PiecePlacement::RightLegFront:
        case PiecePlacement::RightLegBack:
        {
            result.append(legPiece(flat, bounds, placement, false, flip, frame));
            if (pair)
            {
                result.append(legPiece(flat, bounds, placement, true, flip, frame));
            }
            break;
        }
        case PiecePlacement::LeftSide:
        case PiecePlacement::RightSide:
        {
            const auto angleOf = [](PiecePlacement side)
            { return side == PiecePlacement::LeftSide ? static_cast<float>(M_PI / 2) : static_cast<float>(-M_PI / 2); };
            result.append(wrap(flat, 0, 0, frame.torsoRadius, angleOf(placement), frame.neckHeight, centered, v));
            if (pair)
            {
                result.append(wrap(flat, 0, 0, frame.torsoRadius, angleOf(opposite(placement)), frame.neckHeight,
                                   [&centered](const QVector3D &p) { return -centered(p); }, v));
            }
            break;
        }
        case PiecePlacement::LeftSleeve:
        case PiecePlacement::RightSleeve:
        {
            // A sleeve wraps around a cylinder along the arm: the top of the cap at the shoulder, the hem toward the
            // wrist, the cap's center line on the outside of the arm, the left of the pattern to the front (a sleeve is
            // drawn as the left one seen from outside: the front seam of a two-piece sleeve, the lower end of its cap,
            // on the left).
            // The other sleeve is its mirror image: the frame round the other arm is mirrored already (out to the
            // other side, forward still forward), so the pattern wraps round it the same way.
            const auto sleeve = [&](PiecePlacement side)
            {
                const float sideSign = side == PiecePlacement::LeftSleeve ? 1.0f : -1.0f;
                const QVector3D shoulder(sideSign * frame.leftShoulder.x(), frame.leftShoulder.y(),
                                         frame.leftShoulder.z());
                const QVector3D wrist(sideSign * frame.leftWrist.x(), frame.leftWrist.y(), frame.leftWrist.z());
                const QVector3D down = (wrist - shoulder).normalized();
                // Outward: square to the arm, toward the side the arm hangs to; forward completes the frame.
                const QVector3D side3(sideSign, 0, 0);
                QVector3D out = side3 - down * QVector3D::dotProduct(side3, down);
                out = out.isNull() ? QVector3D(0, 0, 1) : out.normalized();
                const QVector3D forward = QVector3D::crossProduct(out, down).normalized() * -sideSign;
                const QVector3D axisTop = shoulder - out * (frame.armRadius * 0.8f);

                Mesh mesh = flat;
                for (QVector3D &p : mesh.positions)
                {
                    const float phi = -centered(p) / frame.sleeveRadius;
                    const QVector3D around = out * qCos(phi) + forward * qSin(phi);
                    p = axisTop - down * v(p) + around * frame.sleeveRadius;
                }
                mesh.computeNormals();
                return mesh;
            };
            result.append(sleeve(placement));
            if (pair)
            {
                result.append(sleeve(opposite(placement)));
            }
            break;
        }
        case PiecePlacement::Aside:
        default:
        {
            // A row of flat pieces hung behind the body, facing it, like on a rack.
            const float z = -(frame.torsoRadius + 30.0f);
            const float step = width + asideSpacing;
            const float start = asideStart + width / 2;
            const auto flatAt = [&](float offset, float mirror)
            {
                Mesh mesh = flat;
                for (QVector3D &p : mesh.positions)
                {
                    p = QVector3D(-(start + offset) - mirror * centered(p), frame.neckHeight + v(p), z);
                }
                mesh.computeNormals();
                return mesh;
            };
            result.append(flatAt(0, 1.0f));
            if (pair)
            {
                result.append(flatAt(step, -1.0f));
            }
            break;
        }
    }
    return result;
}

} // namespace Seamly3D
