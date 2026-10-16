/******************************************************************************
 *   @file   measurementguide.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  How to take each body measurement: diagram and description.
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

#include "measurementguide.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QVBoxLayout>

namespace Seamly3D
{

namespace
{

//---------------------------------------------------------------------------------------------------------------------
QString groupTitle(BodyMeasurementGroup group)
{
    switch (group)
    {
        case BodyMeasurementGroup::Height:
            return MeasurementGuideDialog::tr("Heights");
        case BodyMeasurementGroup::Girth:
            return MeasurementGuideDialog::tr("Circumferences");
        case BodyMeasurementGroup::Arm:
        default:
            return MeasurementGuideDialog::tr("Shoulders and arms");
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
MeasurementGuideWidget::MeasurementGuideWidget(int pictureHeight, QWidget *parent)
    : QWidget(parent)
    , m_pictureHeight(pictureHeight)
    , m_picture(new QLabel(this))
    , m_title(new QLabel(this))
    , m_description(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_picture->setAlignment(Qt::AlignCenter);
    m_picture->setMinimumHeight(pictureHeight);
    m_picture->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // The diagrams are drawn on white; keep them on white in dark themes too.
    m_picture->setStyleSheet(QStringLiteral("background: white; border-radius: 4px;"));

    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    m_title->setFont(titleFont);
    m_title->setWordWrap(true);
    m_description->setWordWrap(true);

    layout->addWidget(m_picture);
    layout->addWidget(m_title);
    layout->addWidget(m_description);
    layout->addStretch(1);

    showMeasurement(BodyMeasurement::Height);
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurement MeasurementGuideWidget::measurement() const
{
    return m_measurement;
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementGuideWidget::showMeasurement(BodyMeasurement measurement)
{
    m_measurement = measurement;
    m_title->setText(tr("%1 (%2)").arg(BodyMeasurements::displayName(measurement), BodyMeasurements::code(measurement)));
    m_description->setText(BodyMeasurements::description(measurement));

    // Render the SVG at the size it is shown, so it stays sharp (also on high DPI screens).
    QImageReader reader(BodyMeasurements::diagram(measurement));
    const QSize size = reader.size();
    const qreal ratio = devicePixelRatioF();
    if (size.isValid() && size.height() > 0)
    {
        const int height = qRound(m_pictureHeight * ratio);
        reader.setScaledSize(QSize(qRound(size.width() * height / static_cast<qreal>(size.height())), height));
    }

    const QImage image = reader.read();
    if (image.isNull())
    {
        m_picture->setPixmap(QPixmap());
        m_picture->setText(tr("No picture available"));
        return;
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(ratio);
    m_picture->setPixmap(pixmap);
}

//---------------------------------------------------------------------------------------------------------------------
MeasurementGuideDialog::MeasurementGuideDialog(QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
    , m_guide(new MeasurementGuideWidget(420, this))
{
    setWindowTitle(tr("How to Measure"));

    auto *intro = new QLabel(tr("Measure close to the body, over light clothing, standing straight with your weight on "
                                "both feet. Keep the tape level for circumferences. A helper makes it easier."),
                             this);
    intro->setWordWrap(true);

    // Measurements grouped like in the panel, each group under a non selectable title.
    for (const BodyMeasurementGroup group :
         {BodyMeasurementGroup::Height, BodyMeasurementGroup::Girth, BodyMeasurementGroup::Arm})
    {
        auto *title = new QListWidgetItem(groupTitle(group), m_list);
        QFont font = title->font();
        font.setBold(true);
        title->setFont(font);
        title->setFlags(Qt::NoItemFlags);

        for (int i = 0; i < BodyMeasurements::count(); ++i)
        {
            const auto measurement = static_cast<BodyMeasurement>(i);
            if (BodyMeasurements::group(measurement) == group)
            {
                auto *item = new QListWidgetItem(QStringLiteral("    ") + BodyMeasurements::displayName(measurement),
                                                 m_list);
                item->setData(Qt::UserRole, i);
            }
        }
    }
    m_list->setMinimumWidth(240);
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item)
            {
                if (item != nullptr && item->data(Qt::UserRole).isValid())
                {
                    m_guide->showMeasurement(static_cast<BodyMeasurement>(item->data(Qt::UserRole).toInt()));
                }
            });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *content = new QHBoxLayout;
    content->addWidget(m_list);
    content->addWidget(m_guide, 1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(content, 1);
    layout->addWidget(buttons);

    showMeasurement(BodyMeasurement::Height);
    resize(820, 620);
}

//---------------------------------------------------------------------------------------------------------------------
void MeasurementGuideDialog::showMeasurement(BodyMeasurement measurement)
{
    for (int row = 0; row < m_list->count(); ++row)
    {
        QListWidgetItem *item = m_list->item(row);
        if (item->data(Qt::UserRole).isValid() && item->data(Qt::UserRole).toInt() == static_cast<int>(measurement))
        {
            m_list->setCurrentItem(item);
            break;
        }
    }
    m_guide->showMeasurement(measurement);
}

} // namespace Seamly3D
