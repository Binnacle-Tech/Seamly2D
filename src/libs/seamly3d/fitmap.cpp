/******************************************************************************
 *   @file   fitmap.cpp
 **  @author Seamly2D project
 **  @date   16 10, 2026
 **
 **  @brief  Colors showing how a garment fits: stretch and ease.
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

#include "fitmap.h"

#include <QPainter>

namespace Seamly3D
{

namespace
{

struct Stop
{
    float  value;
    QColor color;
};

//---------------------------------------------------------------------------------------------------------------------
const QVector<Stop> &stops(FitMap::Kind kind)
{
    // Stretch as a share of the length; ease in centimeters.
    static const QVector<Stop> stretch{{-0.03f, QColor(40, 90, 220)},
                                       {0.0f, QColor(60, 170, 90)},
                                       {0.03f, QColor(240, 210, 40)},
                                       {0.08f, QColor(220, 40, 40)}};
    static const QVector<Stop> ease{{0.0f, QColor(220, 40, 40)},
                                    {0.5f, QColor(240, 140, 40)},
                                    {1.5f, QColor(240, 210, 40)},
                                    {3.0f, QColor(60, 170, 90)},
                                    {6.0f, QColor(40, 90, 220)}};
    return kind == FitMap::Kind::Ease ? ease : stretch;
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
QColor FitMap::color(Kind kind, float value)
{
    const QVector<Stop> &scale = stops(kind);
    if (value <= scale.constFirst().value)
    {
        return scale.constFirst().color;
    }
    for (int i = 1; i < scale.size(); ++i)
    {
        if (value <= scale.at(i).value)
        {
            const Stop &a = scale.at(i - 1);
            const Stop &b = scale.at(i);
            const float t = (value - a.value) / (b.value - a.value);
            return QColor::fromRgbF(a.color.redF() + (b.color.redF() - a.color.redF()) * t,
                                    a.color.greenF() + (b.color.greenF() - a.color.greenF()) * t,
                                    a.color.blueF() + (b.color.blueF() - a.color.blueF()) * t);
        }
    }
    return scale.constLast().color;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QColor> FitMap::colors(Kind kind, const QVector<float> &values)
{
    QVector<QColor> result;
    result.reserve(values.size());
    for (const float value : values)
    {
        result.append(color(kind, value));
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QImage FitMap::legend(Kind kind, int width, int height)
{
    QImage image(qMax(1, width), qMax(1, height), QImage::Format_RGB32);
    const QVector<Stop> &scale = stops(kind);
    const float low = scale.constFirst().value;
    const float high = scale.constLast().value;
    QPainter painter(&image);
    for (int x = 0; x < image.width(); ++x)
    {
        painter.setPen(color(kind, low + (high - low) * static_cast<float>(x) / qMax(1, image.width() - 1)));
        painter.drawLine(x, 0, x, image.height() - 1);
    }
    return image;
}

//---------------------------------------------------------------------------------------------------------------------
QString FitMap::lowLabel(Kind kind)
{
    return kind == Kind::Ease ? tr("on the body") : tr("pushed together");
}

//---------------------------------------------------------------------------------------------------------------------
QString FitMap::highLabel(Kind kind)
{
    return kind == Kind::Ease ? tr("6 cm or more") : tr("8% stretched");
}

//---------------------------------------------------------------------------------------------------------------------
QString FitMap::name(Kind kind)
{
    switch (kind)
    {
        case Kind::Stretch:
            return tr("Stretch");
        case Kind::Ease:
            return tr("Ease (distance from the body)");
        case Kind::None:
        default:
            return tr("None");
    }
}

} // namespace Seamly3D
