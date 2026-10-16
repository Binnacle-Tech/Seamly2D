/******************************************************************************
 *   @file   bodymeasurementspanel.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  Form for entering body measurements in the 3D workspace.
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

#include "bodymeasurementspanel.h"

#include "measurementguide.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Seamly3D
{

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurementsPanel::BodyMeasurementsPanel(QWidget *parent)
    : QWidget(parent)
    , m_unitComboBox(new QComboBox(this))
    , m_genderComboBox(new QComboBox(this))
    , m_muscleSlider(new QSlider(Qt::Horizontal, this))
    , m_ageSpinBox(new QSpinBox(this))
    , m_proportionalCheckBox(new QCheckBox(tr("Scale everything with height"), this))
    , m_sourceLabel(new QLabel(this))
    , m_guide(new MeasurementGuideWidget(170, this))
{
    auto *layout = new QVBoxLayout(this);

    auto *loadButton = new QPushButton(tr("Load from SeamlyMe File..."), this);
    loadButton->setToolTip(tr("Use the measurements of an individual measurements (.vit) file"));
    connect(loadButton, &QPushButton::clicked, this, &BodyMeasurementsPanel::loadVitFile);

    auto *resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setToolTip(tr("Go back to the default measurements for the body shape"));
    connect(resetButton, &QPushButton::clicked, this, &BodyMeasurementsPanel::resetToDefaults);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(loadButton, 1);
    buttons->addWidget(resetButton);
    layout->addLayout(buttons);

    m_sourceLabel->setWordWrap(true);
    m_sourceLabel->setText(tr("Default body. Enter your own measurements below."));
    layout->addWidget(m_sourceLabel);

    auto *options = new QFormLayout;
    m_unitComboBox->addItem(tr("Centimeters"), 1.0);
    m_unitComboBox->addItem(tr("Inches"), 2.54);
    connect(m_unitComboBox, &QComboBox::currentIndexChanged, this, &BodyMeasurementsPanel::unitChanged);
    options->addRow(tr("Units:"), m_unitComboBox);
    m_proportionalCheckBox->setChecked(true);
    m_proportionalCheckBox->setToolTip(tr("When the height changes, scale every measurement by the same factor: the same body shape at the "
                                         "new size. Enter the height first, then adjust what differs."));
    options->addRow(m_proportionalCheckBox);
    layout->addLayout(options);

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // Build: what a tape measure can not tell.
    auto *buildBox = new QGroupBox(tr("Body"), content);
    auto *buildForm = new QFormLayout(buildBox);
    m_genderComboBox->setParent(buildBox);
    m_genderComboBox->addItem(tr("Female"), 0.0);
    m_genderComboBox->addItem(tr("Male"), 1.0);
    connect(m_genderComboBox, &QComboBox::currentIndexChanged, this, &BodyMeasurementsPanel::buildEdited);
    buildForm->addRow(tr("Body shape:"), m_genderComboBox);
    m_muscleSlider->setParent(buildBox);
    m_muscleSlider->setRange(0, 100);
    m_muscleSlider->setToolTip(tr("From soft to muscular"));
    connect(m_muscleSlider, &QSlider::valueChanged, this, &BodyMeasurementsPanel::buildEdited);
    buildForm->addRow(tr("Muscle tone:"), m_muscleSlider);
    m_ageSpinBox->setParent(buildBox);
    m_ageSpinBox->setRange(1, 90);
    m_ageSpinBox->setSuffix(tr(" years"));
    m_ageSpinBox->setKeyboardTracking(false);
    connect(m_ageSpinBox, &QSpinBox::valueChanged, this, &BodyMeasurementsPanel::buildEdited);
    buildForm->addRow(tr("Age:"), m_ageSpinBox);
    contentLayout->addWidget(buildBox);

    const QList<QPair<BodyMeasurementGroup, QString>> groups = {
        {BodyMeasurementGroup::Height, tr("Heights")},
        {BodyMeasurementGroup::Girth, tr("Circumferences")},
        {BodyMeasurementGroup::Arm, tr("Shoulders and arms")}};

    m_spinBoxes.resize(BodyMeasurements::count());
    for (const auto &group : groups)
    {
        auto *box = new QGroupBox(group.second, content);
        auto *form = new QFormLayout(box);
        for (int i = 0; i < BodyMeasurements::count(); ++i)
        {
            const auto measurement = static_cast<BodyMeasurement>(i);
            if (BodyMeasurements::group(measurement) != group.first)
            {
                continue;
            }

            auto *spinBox = new QDoubleSpinBox(box);
            spinBox->setDecimals(1);
            spinBox->setSingleStep(0.5);
            spinBox->setKeyboardTracking(false);
            spinBox->setToolTip(BodyMeasurements::name(measurement));
            connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this, i]() { valueEdited(i); });
            spinBox->installEventFilter(this); // show how to take the measurement being edited
            m_spinBoxes[i] = spinBox;
            form->addRow(BodyMeasurements::displayName(measurement) + QLatin1Char(':'), spinBox);
        }
        contentLayout->addWidget(box);
    }
    contentLayout->addStretch(1);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(content);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scrollArea, 1);

    // How to take the measurement being edited.
    auto *guideBox = new QGroupBox(tr("How to measure"), this);
    auto *guideLayout = new QVBoxLayout(guideBox);
    m_guide->setParent(guideBox);
    guideLayout->addWidget(m_guide);
    auto *guideButton = new QPushButton(tr("All Measurements..."), guideBox);
    guideButton->setToolTip(tr("Pictures and descriptions of every measurement"));
    connect(guideButton, &QPushButton::clicked, this, &BodyMeasurementsPanel::showMeasuringGuide);
    guideLayout->addWidget(guideButton, 0, Qt::AlignRight);
    layout->addWidget(guideBox);

    updateSpinBoxes();
}

//---------------------------------------------------------------------------------------------------------------------
BodyMeasurements BodyMeasurementsPanel::measurements() const
{
    return m_measurements;
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::setMeasurements(const BodyMeasurements &measurements)
{
    if (m_measurements == measurements)
    {
        return;
    }
    m_measurements = measurements;
    updateSpinBoxes();
    emit measurementsChanged(m_measurements);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::loadVitFile()
{
    const QString filter = tr("Individual measurements (*.vit);;All files (*)");
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Load measurements"), m_lastDirectory, filter);
    if (fileName.isEmpty())
    {
        return;
    }
    m_lastDirectory = QFileInfo(fileName).absolutePath();

    BodyMeasurements loaded;
    QStringList applied;
    if (!loaded.readVitFile(fileName, &applied))
    {
        QMessageBox::warning(this, tr("Load measurements"), loaded.errorString());
        return;
    }

    m_sourceLabel->setText(tr("%1: %n measurement(s) used, the rest estimated from the height.", "",
                              static_cast<int>(applied.size()))
                               .arg(QFileInfo(fileName).fileName()));
    m_sourceLabel->setToolTip(applied.join(QLatin1String(", ")));
    setMeasurements(loaded);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::resetToDefaults()
{
    m_sourceLabel->setText(tr("Default body. Enter your own measurements below."));
    m_sourceLabel->setToolTip(QString());
    BodyMeasurements defaults = BodyMeasurements::defaults(m_measurements.gender());
    defaults.setMuscle(m_measurements.muscle());
    defaults.setAge(m_measurements.age());
    setMeasurements(defaults);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::showMeasuringGuide()
{
    auto *dialog = new MeasurementGuideDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->showMeasurement(m_guide->measurement());
    dialog->show();
}

//---------------------------------------------------------------------------------------------------------------------
bool BodyMeasurementsPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        const int index = static_cast<int>(m_spinBoxes.indexOf(qobject_cast<QDoubleSpinBox *>(watched)));
        if (index >= 0)
        {
            m_guide->showMeasurement(static_cast<BodyMeasurement>(index));
        }
    }
    return QWidget::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::valueEdited(int index)
{
    if (m_updating)
    {
        return;
    }

    const auto measurement = static_cast<BodyMeasurement>(index);
    const double centimeters = m_spinBoxes.at(index)->value() * unitFactor();
    m_guide->showMeasurement(measurement);

    if (measurement == BodyMeasurement::Height && m_proportionalCheckBox->isChecked())
    {
        m_measurements.scaleToHeight(centimeters);
        updateSpinBoxes();
    }
    else
    {
        m_measurements.setValue(measurement, centimeters);
    }
    emit measurementsChanged(m_measurements);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::buildEdited()
{
    if (m_updating)
    {
        return;
    }

    const double gender = m_genderComboBox->currentData().toDouble();
    if (!qFuzzyCompare(gender + 1, m_measurements.gender() + 1) && m_measurements.hasDefaultValues())
    {
        // Still the default body: switch to the defaults of the other gender instead of keeping, say, a female
        // lowbust on a male body.
        BodyMeasurements switched = BodyMeasurements::defaults(gender);
        switched.setMuscle(m_measurements.muscle());
        switched.setAge(m_measurements.age());
        m_measurements = switched;
        updateSpinBoxes();
    }

    m_measurements.setGender(gender);
    m_measurements.setMuscle(m_muscleSlider->value() / 100.0);
    m_measurements.setAge(m_ageSpinBox->value());
    emit measurementsChanged(m_measurements);
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::unitChanged()
{
    updateSpinBoxes();
}

//---------------------------------------------------------------------------------------------------------------------
double BodyMeasurementsPanel::unitFactor() const
{
    return m_unitComboBox->currentData().toDouble();
}

//---------------------------------------------------------------------------------------------------------------------
void BodyMeasurementsPanel::updateSpinBoxes()
{
    m_updating = true;
    {
        const QSignalBlocker genderBlocker(m_genderComboBox);
        const QSignalBlocker muscleBlocker(m_muscleSlider);
        const QSignalBlocker ageBlocker(m_ageSpinBox);
        m_ageSpinBox->setValue(qRound(m_measurements.age()));
        m_genderComboBox->setCurrentIndex(m_measurements.gender() < 0.5 ? 0 : 1);
        m_muscleSlider->setValue(qRound(m_measurements.muscle() * 100));
    }

    const double factor = unitFactor();
    const QString suffix = factor == 1.0 ? tr(" cm") : tr(" in");
    for (int i = 0; i < m_spinBoxes.size(); ++i)
    {
        QDoubleSpinBox *spinBox = m_spinBoxes.at(i);
        const QSignalBlocker blocker(spinBox);
        spinBox->setSuffix(suffix);
        spinBox->setRange(1.0 / factor, 300.0 / factor);
        spinBox->setValue(m_measurements.value(static_cast<BodyMeasurement>(i)) / factor);
    }
    m_updating = false;
}

} // namespace Seamly3D
