/******************************************************************************
 *   @file   patternpiecespanel.cpp
 **  @author Seamly2D project
 **  @date   15 10, 2026
 **
 **  @brief  The pattern's pieces in the 3D workspace.
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

#include "patternpiecespanel.h"

#include <QCheckBox>
#include <QDir>
#include <QtMath>
#include <QHash>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QIcon>
#include <QDialogButtonBox>
#include <QDialog>
#include <QPixmap>
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "clothsimulator.h"
#include "patternprovider.h"

#include <algorithm>

namespace Seamly3D
{

namespace
{

enum Column
{
    NameColumn = 0,
    PlacementColumn,
    PairColumn,
    FlipColumn,
    ColumnCount
};

// Soft fabric colors, distinct from the skin tone of the body.
const QColor pieceColors[] = {QColor(96, 140, 196), QColor(214, 120, 96),  QColor(108, 168, 120), QColor(196, 160, 72),
                              QColor(152, 112, 180), QColor(80, 164, 172), QColor(200, 104, 150), QColor(132, 132, 148)};

//---------------------------------------------------------------------------------------------------------------------
/** @brief Places where several pieces go side by side; bands and pieces beside the body do not. */
bool spreads(PiecePlacement placement)
{
    switch (placement)
    {
        case PiecePlacement::Waistband:
        case PiecePlacement::Collar:
        case PiecePlacement::Aside:
        case PiecePlacement::Hidden:
            return false;
        default:
            return true;
    }
}

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel::PatternPiecesPanel(QWidget *parent)
    : QWidget(parent)
    , m_list(new QTreeWidget(this))
    , m_adjustBox(new QWidget(this))
    , m_liftSpin(new QDoubleSpinBox(m_adjustBox))
    , m_aroundSpin(new QDoubleSpinBox(m_adjustBox))
    , m_turnSpin(new QDoubleSpinBox(m_adjustBox))
    , m_resetAdjustButton(new QPushButton(tr("Put Back"), m_adjustBox))
    , m_status(new QLabel(this))
    , m_reloadButton(new QPushButton(tr("Reload"), this))
    , m_measurementsButton(new QPushButton(tr("Use Pattern's Measurements"), this))
    , m_followCheckBox(new QCheckBox(tr("Follow pattern changes"), this))
    , m_simulateButton(new QPushButton(tr("Sew && Drape"), this))
    , m_resetButton(new QPushButton(tr("Reset"), this))
    , m_simulationStatus(new QLabel(this))
    , m_fabricCombo(new QComboBox(this))
    , m_fitMapCombo(new QComboBox(this))
    , m_legend(new QWidget(this))
    , m_legendBar(new QLabel(m_legend))
    , m_legendLow(new QLabel(m_legend))
    , m_legendHigh(new QLabel(m_legend))
    , m_seamList(new QTreeWidget(this))
    , m_findSeamsButton(new QPushButton(tr("Find Seams"), this))
    , m_addSeamButton(new QPushButton(tr("Add Seam..."), this))
    , m_removeSeamButton(new QPushButton(tr("Remove"), this))
    , m_sewByClickButton(new QPushButton(tr("Sew by Clicking"), this))
    , m_showSeamsCheck(new QCheckBox(tr("Show seams"), this))
    , m_followTimer(new QTimer(this))
    , m_saveTimer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);

    m_reloadButton->setToolTip(tr("Read the pieces of the open pattern again"));
    connect(m_reloadButton, &QPushButton::clicked, this, &PatternPiecesPanel::reload);
    m_measurementsButton->setToolTip(tr("Fit the body to the measurements the pattern is drafted for"));
    connect(m_measurementsButton, &QPushButton::clicked, this, &PatternPiecesPanel::useMeasurementsRequested);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_reloadButton);
    buttons->addWidget(m_measurementsButton, 1);
    layout->addLayout(buttons);

    m_followCheckBox->setChecked(true);
    m_followCheckBox->setToolTip(tr("Update the pieces here when the pattern changes in 2D"));
    layout->addWidget(m_followCheckBox);

    m_list->setColumnCount(ColumnCount);
    m_list->setHeaderLabels({tr("Piece"), tr("Place"), tr("Pair"), tr("Flip")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(PlacementColumn, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(PairColumn, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(FlipColumn, QHeaderView::ResizeToContents);
    m_list->headerItem()->setToolTip(PairColumn, tr("Show a mirrored copy: cut twice, or cut on the fold"));
    m_list->headerItem()->setToolTip(FlipColumn, tr("Mirror the piece, when its center line is on the other side"));
    connect(m_list, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *item, int column)
            {
                if (m_updatingList || (column != PairColumn && column != FlipColumn))
                {
                    return;
                }
                const auto id = item->data(NameColumn, Qt::UserRole).toUInt();
                Settings s = settings(id);
                s.pair = item->checkState(PairColumn) == Qt::Checked;
                s.flip = item->checkState(FlipColumn) == Qt::Checked;
                m_settings.insert(id, s);
                changed();
                emit arrangementChanged();
            });
    layout->addWidget(m_list, 1);

    // The piece selected in the list, moved by hand (or Ctrl + drag it in 3D).
    auto *adjust = new QFormLayout(m_adjustBox);
    adjust->setContentsMargins(0, 0, 0, 0);
    const QString degrees = QString(QChar(0x00B0));
    m_liftSpin->setRange(-150, 150);
    m_liftSpin->setDecimals(1);
    m_liftSpin->setSuffix(tr(" cm"));
    m_liftSpin->setToolTip(tr("Move the piece up (or down, below zero): along the arm or the leg for sleeves and "
                              "trouser legs"));
    m_aroundSpin->setRange(-180, 180);
    m_aroundSpin->setDecimals(1);
    m_aroundSpin->setSingleStep(5);
    m_aroundSpin->setWrapping(true);
    m_aroundSpin->setSuffix(degrees);
    m_aroundSpin->setToolTip(tr("Move the piece around the body (around the arm or the leg for sleeves and trouser "
                                "legs); its mirrored copy moves the mirrored way"));
    m_turnSpin->setRange(-180, 180);
    m_turnSpin->setDecimals(1);
    m_turnSpin->setSingleStep(15);
    m_turnSpin->setWrapping(true);
    m_turnSpin->setSuffix(degrees);
    m_turnSpin->setToolTip(tr("Turn the piece as it is drawn, before it is placed: for pieces drawn sideways"));
    m_resetAdjustButton->setToolTip(tr("Back where its place puts it"));
    adjust->addRow(tr("Up:"), m_liftSpin);
    adjust->addRow(tr("Around:"), m_aroundSpin);
    adjust->addRow(tr("Turn:"), m_turnSpin);
    adjust->addRow(QString(), m_resetAdjustButton);
    m_adjustBox->setEnabled(false);
    layout->addWidget(m_adjustBox);
    const auto adjusted = [this]()
    {
        const quint32 id = selectedPiece();
        if (m_updatingList || id == 0)
        {
            return;
        }
        Settings s = settings(id);
        s.adjustment.lift = static_cast<float>(m_liftSpin->value());
        s.adjustment.around = static_cast<float>(m_aroundSpin->value());
        s.adjustment.turn = static_cast<float>(m_turnSpin->value());
        m_settings.insert(id, s);
        changed();
        emit arrangementChanged();
    };
    for (QDoubleSpinBox *spin : {m_liftSpin, m_aroundSpin, m_turnSpin})
    {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, adjusted);
    }
    connect(m_resetAdjustButton, &QPushButton::clicked, this,
            [this]()
            {
                const quint32 id = selectedPiece();
                if (id == 0)
                {
                    return;
                }
                Settings s = settings(id);
                s.adjustment = PieceAdjustment();
                m_settings.insert(id, s);
                showAdjustment();
                changed();
                emit arrangementChanged();
            });
    connect(m_list, &QTreeWidget::currentItemChanged, this, &PatternPiecesPanel::showAdjustment);

    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    // Sewing and draping: the pieces are sewn along the seams found between them, then fall onto the body.
    m_simulateButton->setCheckable(true);
    m_simulateButton->setObjectName(QStringLiteral("seamly3dSimulate_Button"));
    m_simulateButton->setToolTip(tr("Sew the pieces together and drape the garment on the body"));
    connect(m_simulateButton, &QPushButton::toggled, this, &PatternPiecesPanel::simulationToggled);
    m_resetButton->setToolTip(tr("Undo the sewing and draping: back to the pieces around the body"));
    connect(m_resetButton, &QPushButton::clicked, this, &PatternPiecesPanel::simulationResetRequested);
    auto *simulation = new QHBoxLayout;
    simulation->addWidget(m_simulateButton, 1);
    simulation->addWidget(m_resetButton);
    layout->addLayout(simulation);
    m_simulationStatus->setWordWrap(true);
    layout->addWidget(m_simulationStatus);

    // Fabric and fit map.
    for (const FabricPreset &fabric : FabricPreset::presets())
    {
        m_fabricCombo->addItem(fabric.name, fabric.id);
        m_fabricCombo->setItemData(m_fabricCombo->count() - 1,
                                   tr("%1 g/m%2").arg(fabric.gramsPerSquareMeter).arg(QChar(0x00B2)), Qt::ToolTipRole);
    }
    m_fabricCombo->setToolTip(tr("What the garment is made of: how it stretches, drapes and weighs"));
    connect(m_fabricCombo, &QComboBox::currentIndexChanged, this,
            [this]()
            {
                changed();
                emit fabricChanged();
            });
    for (const FitMap::Kind kind : {FitMap::Kind::None, FitMap::Kind::Stretch, FitMap::Kind::Ease})
    {
        m_fitMapCombo->addItem(FitMap::name(kind), static_cast<int>(kind));
    }
    m_fitMapCombo->setToolTip(tr("Color the draped garment by how it fits"));
    connect(m_fitMapCombo, &QComboBox::currentIndexChanged, this,
            [this]()
            {
                const FitMap::Kind kind = fitMap();
                m_legend->setVisible(kind != FitMap::Kind::None);
                if (kind != FitMap::Kind::None)
                {
                    m_legendBar->setPixmap(QPixmap::fromImage(FitMap::legend(kind, 160, 10)));
                    m_legendLow->setText(FitMap::lowLabel(kind));
                    m_legendHigh->setText(FitMap::highLabel(kind));
                }
                changed();
                emit fitMapChanged();
            });
    auto *legendLayout = new QGridLayout(m_legend);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->addWidget(m_legendBar, 0, 0, 1, 2);
    legendLayout->addWidget(m_legendLow, 1, 0, Qt::AlignLeft);
    legendLayout->addWidget(m_legendHigh, 1, 1, Qt::AlignRight);
    m_legend->setVisible(false);

    // Seams: found automatically, switched off, or added by hand.
    layout->addWidget(new QLabel(tr("Seams:"), this));
    m_seamList->setObjectName(QStringLiteral("seamList"));
    m_seamList->setHeaderHidden(true);
    m_seamList->setRootIsDecorated(false);
    m_seamList->setMinimumHeight(90);
    m_seamList->setToolTip(tr("Uncheck a seam to leave it open"));
    layout->addWidget(m_seamList, 1);
    connect(m_seamList, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem *item)
            {
                if (m_updatingSeams)
                {
                    return;
                }
                const QString key = item->data(0, Qt::UserRole).toString();
                if (item->checkState(0) == Qt::Checked)
                {
                    m_switchedOff.remove(key);
                }
                else
                {
                    m_switchedOff.insert(key);
                }
                changed();
                emit seamsChanged();
            });
    connect(m_seamList, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *item)
            {
                m_highlight.clear();
                if (item != nullptr)
                {
                    const QString key = item->data(0, Qt::UserRole).toString();
                    for (const SeamSpec &spec : m_foundSeams + m_addedSeams)
                    {
                        if (spec.key() == key)
                        {
                            m_highlight << qMakePair(panelIndex(spec.pieceA, spec.copyA), spec.edgeA)
                                        << qMakePair(panelIndex(spec.pieceB, spec.copyB), spec.edgeB);
                        }
                    }
                }
                emit highlightChanged();
            });
    m_findSeamsButton->setToolTip(tr("Sew every seam found again"));
    connect(m_findSeamsButton, &QPushButton::clicked, this,
            [this]()
            {
                m_switchedOff.clear();
                rebuildSeamList();
                changed();
                emit seamsChanged();
            });
    m_addSeamButton->setToolTip(tr("Sew two edges together"));
    connect(m_addSeamButton, &QPushButton::clicked, this, &PatternPiecesPanel::addSeamDialog);
    m_removeSeamButton->setToolTip(tr("Remove the seam (one found automatically is switched off)"));
    connect(m_removeSeamButton, &QPushButton::clicked, this,
            [this]()
            {
                const QTreeWidgetItem *item = m_seamList->currentItem();
                if (item == nullptr)
                {
                    return;
                }
                const QString key = item->data(0, Qt::UserRole).toString();
                const auto before = m_addedSeams.size();
                m_addedSeams.erase(std::remove_if(m_addedSeams.begin(), m_addedSeams.end(),
                                                  [&key](const SeamSpec &spec) { return spec.key() == key; }),
                                   m_addedSeams.end());
                if (m_addedSeams.size() == before)
                {
                    m_switchedOff.insert(key);
                }
                rebuildSeamList();
                changed();
                emit seamsChanged();
            });
    m_showSeamsCheck->setChecked(true);
    connect(m_showSeamsCheck, &QCheckBox::toggled, this, &PatternPiecesPanel::highlightChanged);
    auto *seamButtons = new QHBoxLayout;
    seamButtons->addWidget(m_findSeamsButton);
    seamButtons->addWidget(m_addSeamButton);
    seamButtons->addWidget(m_removeSeamButton);
    m_sewByClickButton->setCheckable(true);
    m_sewByClickButton->setToolTip(tr("Click an edge of a piece in 3D, then the edge to sew it to; again for the "
                                      "next seam. Click the button again to stop."));
    connect(m_sewByClickButton, &QPushButton::toggled, this, &PatternPiecesPanel::sewingByClickingToggled);
    layout->addLayout(seamButtons);
    layout->addWidget(m_sewByClickButton);
    layout->addWidget(m_showSeamsCheck);

    auto *fabricForm = new QFormLayout;
    fabricForm->addRow(tr("Fabric:"), m_fabricCombo);
    fabricForm->addRow(tr("Fit map:"), m_fitMapCombo);
    fabricForm->addRow(QString(), m_legend);
    layout->addLayout(fabricForm);

    // Pattern edits come in bursts (every point of a drag): reload once they settle.
    m_followTimer->setSingleShot(true);
    m_followTimer->setInterval(400);
    connect(m_followTimer, &QTimer::timeout, this, &PatternPiecesPanel::reload);
    // The setup is written shortly after a change, not at every click.
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, [this]() { saveSetup(); });

    setProvider(nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel::~PatternPiecesPanel()
{
    if (m_saveTimer->isActive())
    {
        saveSetup(); // a change made just before closing
    }
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setProvider(const std::shared_ptr<PatternProvider> &provider)
{
    m_provider = provider;
    m_reloadButton->setEnabled(provider != nullptr);
    m_measurementsButton->setEnabled(provider != nullptr);
    m_followCheckBox->setEnabled(provider != nullptr);
    m_simulateButton->setEnabled(provider != nullptr);
    m_resetButton->setEnabled(provider != nullptr);

    if (provider == nullptr)
    {
        m_status->setText(tr("Open the 3D workspace from Seamly2D to see the pattern's pieces here."));
        return;
    }

    provider->watch(this,
                    [this]()
                    {
                        if (m_followCheckBox->isChecked())
                        {
                            m_followTimer->start();
                        }
                    });
    reload();

    // What was set up for this pattern before.
    const QString file = setupFile();
    if (!file.isEmpty() && QFileInfo::exists(file))
    {
        WorkspaceSetup kept;
        QString error;
        if (WorkspaceSetup::load(file, &kept, &error))
        {
            applySetup(kept);
        }
        else
        {
            m_status->setText(tr("Could not read the 3D setup %1: %2").arg(QDir::toNativeSeparators(file), error));
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
WorkspaceSetup PatternPiecesPanel::setup() const
{
    WorkspaceSetup result;
    for (const PatternPiece &piece : m_pieces)
    {
        const Settings s = settings(piece.id);
        PieceSetup kept;
        kept.id = piece.id;
        kept.name = piece.name;
        kept.placement = s.placement;
        kept.pair = s.pair;
        kept.flip = s.flip;
        kept.adjustment = s.adjustment;
        result.pieces.append(kept);
    }
    result.switchedOff = m_switchedOff.values();
    std::sort(result.switchedOff.begin(), result.switchedOff.end());
    result.added = m_addedSeams;
    result.fabric = fabric();
    result.fitMap = fitMap();
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::applySetup(const WorkspaceSetup &setup)
{
    m_applyingSetup = true;
    // Each piece by its id; one whose id changed (the pattern was rebuilt) by its name, if that is unique.
    for (const PieceSetup &kept : setup.pieces)
    {
        quint32 id = 0;
        bool found = false;
        for (const PatternPiece &piece : m_pieces)
        {
            if (piece.id == kept.id)
            {
                id = piece.id;
                found = true;
                break;
            }
        }
        if (!found)
        {
            int named = 0;
            for (const PatternPiece &piece : m_pieces)
            {
                if (piece.name == kept.name)
                {
                    id = piece.id;
                    ++named;
                }
            }
            found = named == 1;
        }
        if (found)
        {
            Settings s;
            s.placement = kept.placement;
            s.pair = kept.pair;
            s.flip = kept.flip;
            s.adjustment = kept.adjustment;
            m_settings.insert(id, s);
        }
    }
    m_switchedOff = QSet<QString>(setup.switchedOff.cbegin(), setup.switchedOff.cend());
    m_addedSeams = setup.added;
    for (SeamSpec &seam : m_addedSeams)
    {
        seam.added = true;
    }
    if (!setup.fabric.isEmpty())
    {
        setFabric(setup.fabric);
    }
    setFitMap(setup.fitMap);
    m_applyingSetup = false;

    rebuildList();
    rebuildSeamList();
    emit arrangementChanged();
}

//---------------------------------------------------------------------------------------------------------------------
QString PatternPiecesPanel::setupFile() const
{
    return m_provider != nullptr ? WorkspaceSetup::fileFor(m_provider->patternFile()) : QString();
}

//---------------------------------------------------------------------------------------------------------------------
bool PatternPiecesPanel::saveSetup(QString *error)
{
    m_saveTimer->stop();
    const QString file = setupFile();
    if (file.isEmpty())
    {
        return false; // a new pattern: kept here until it is saved, then written with the next change
    }
    QString reason;
    if (!setup().save(file, &reason))
    {
        m_status->setText(tr("Could not save the 3D setup %1: %2").arg(QDir::toNativeSeparators(file), reason));
        if (error != nullptr)
        {
            *error = reason;
        }
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::changed()
{
    if (m_applyingSetup)
    {
        return;
    }
    m_saveTimer->start();
    emit setupChanged();
}

//---------------------------------------------------------------------------------------------------------------------
bool PatternPiecesPanel::hasProvider() const
{
    return m_provider != nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::reload()
{
    if (m_provider == nullptr)
    {
        return;
    }

    QString error;
    setPieces(m_provider->pieces(&error));
    if (!error.isEmpty())
    {
        m_status->setText(m_status->text() + QLatin1Char('\n') + error);
    }
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setPieces(const QVector<PatternPiece> &pieces)
{
    m_pieces = pieces;
    m_flatPieces.clear();

    for (const PatternPiece &piece : m_pieces)
    {
        if (!m_settings.contains(piece.id))
        {
            Settings s;
            s.placement = PieceArrangement::guess(piece.name, piece.seamLine);
            s.pair = piece.quantity >= 2 || piece.onFold;
            m_settings.insert(piece.id, s);
        }
    }

    rebuildList();
    m_status->setText(m_pieces.isEmpty()
                          ? tr("The pattern has no pieces in the layout yet.")
                          : tr("%n piece(s). Choose where each one starts around the body; Ctrl + drag a piece in 3D "
                               "to move it.",
                               "", static_cast<int>(m_pieces.size())));
    emit arrangementChanged();
}

//---------------------------------------------------------------------------------------------------------------------
QVector<PatternPiece> PatternPiecesPanel::pieces() const
{
    return m_pieces;
}

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel::Settings PatternPiecesPanel::settings(quint32 id) const
{
    return m_settings.value(id);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setSettings(quint32 id, const Settings &settings)
{
    m_settings.insert(id, settings);
    changed();
    rebuildList();
    emit arrangementChanged();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::rebuildList()
{
    const quint32 selected = selectedPiece();
    m_updatingList = true;
    m_list->clear();

    for (int i = 0; i < m_pieces.size(); ++i)
    {
        const PatternPiece &piece = m_pieces.at(i);
        const Settings s = settings(piece.id);

        auto *item = new QTreeWidgetItem(m_list);
        item->setText(NameColumn, piece.name);
        item->setData(NameColumn, Qt::UserRole, piece.id);
        const QColor color = pieceColors[i % (sizeof(pieceColors) / sizeof(pieceColors[0]))];
        item->setData(NameColumn, Qt::DecorationRole, color);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(PairColumn, s.pair ? Qt::Checked : Qt::Unchecked);
        item->setCheckState(FlipColumn, s.flip ? Qt::Checked : Qt::Unchecked);

        auto *placement = new QComboBox(m_list);
        for (int p = 0; p < static_cast<int>(PiecePlacement::Count); ++p)
        {
            placement->addItem(PieceArrangement::displayName(static_cast<PiecePlacement>(p)), p);
        }
        placement->setCurrentIndex(static_cast<int>(s.placement));
        const quint32 id = piece.id;
        connect(placement, &QComboBox::currentIndexChanged, this,
                [this, id, placement]()
                {
                    Settings changed = settings(id);
                    changed.placement = static_cast<PiecePlacement>(placement->currentData().toInt());
                    m_settings.insert(id, changed);
                    this->changed();
                    emit arrangementChanged();
                });
        m_list->setItemWidget(item, PlacementColumn, placement);
        if (piece.id == selected)
        {
            m_list->setCurrentItem(item);
        }
    }

    m_updatingList = false;
    showAdjustment();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::showAdjustment()
{
    const quint32 id = selectedPiece();
    const bool updating = m_updatingList;
    m_updatingList = true;
    const PieceAdjustment a = id != 0 ? settings(id).adjustment : PieceAdjustment();
    m_liftSpin->setValue(static_cast<double>(a.lift));
    m_aroundSpin->setValue(static_cast<double>(a.around));
    m_turnSpin->setValue(static_cast<double>(a.turn));
    m_adjustBox->setEnabled(id != 0);
    m_updatingList = updating;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 PatternPiecesPanel::selectedPiece() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item != nullptr ? item->data(NameColumn, Qt::UserRole).toUInt() : 0;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::selectPiece(quint32 id)
{
    for (int i = 0; i < m_list->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_list->topLevelItem(i);
        if (item->data(NameColumn, Qt::UserRole).toUInt() == id)
        {
            m_list->setCurrentItem(item);
            return;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::nudge(quint32 id, int copy, float around, float lift)
{
    if (!m_settings.contains(id))
    {
        return;
    }
    Settings s = settings(id);
    float turned = s.adjustment.around + (copy == 0 ? around : -around);
    while (turned > 180.0f)
    {
        turned -= 360.0f;
    }
    while (turned < -180.0f)
    {
        turned += 360.0f;
    }
    s.adjustment.around = turned;
    s.adjustment.lift = qBound(-150.0f, s.adjustment.lift + lift, 150.0f);
    m_settings.insert(id, s);
    selectPiece(id);
    showAdjustment();
    changed();
    emit arrangementChanged();
}

//---------------------------------------------------------------------------------------------------------------------
const PatternPiecesPanel::FlatPiece &PatternPiecesPanel::flatPiece(const PatternPiece &piece) const
{
    auto it = m_flatPieces.find(piece.id);
    if (it == m_flatPieces.end())
    {
        FlatPiece flat;
        QVector<int> outlineVertex;
        flat.mesh = PieceMesher::mesh(piece.seamLine, meshEdge, &outlineVertex);
        flat.outlineCount = static_cast<int>(PieceMesher::resample(piece.seamLine, meshEdge).size());
        // Edges are whole sides: a seam split by a notch, or where a line runs into a curve, stays one edge.
        for (const int corner : PieceMesher::sides(piece.seamLine, piece.corners))
        {
            const int vertex = outlineVertex.value(corner, -1);
            if (vertex >= 0 && vertex < flat.outlineCount && !flat.corners.contains(vertex))
            {
                flat.corners.append(vertex);
            }
        }
        std::sort(flat.corners.begin(), flat.corners.end());
        if (flat.corners.isEmpty())
        {
            flat.corners.append(0); // no corners known: the whole outline is one edge
        }
        it = m_flatPieces.insert(piece.id, flat);
    }
    return it.value();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setSimulationRunning(bool running)
{
    const QSignalBlocker blocker(m_simulateButton);
    m_simulateButton->setChecked(running);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setSimulationStatus(const QString &text)
{
    m_simulationStatus->setText(text);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setSimulationDetails(const QString &details)
{
    m_simulationStatus->setToolTip(details);
}

//---------------------------------------------------------------------------------------------------------------------
QString PatternPiecesPanel::fabric() const
{
    return m_fabricCombo->currentData().toString();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setFabric(const QString &id)
{
    const int index = m_fabricCombo->findData(id);
    if (index >= 0)
    {
        m_fabricCombo->setCurrentIndex(index);
    }
}

//---------------------------------------------------------------------------------------------------------------------
FitMap::Kind PatternPiecesPanel::fitMap() const
{
    return static_cast<FitMap::Kind>(m_fitMapCombo->currentData().toInt());
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setFitMap(FitMap::Kind kind)
{
    const int index = m_fitMapCombo->findData(static_cast<int>(kind));
    if (index >= 0)
    {
        m_fitMapCombo->setCurrentIndex(index);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QVector<GarmentPanel> PatternPiecesPanel::panels(const BodyFrame &frame) const
{
    QVector<GarmentPanel> result;
    float asideStart = 0;
    QHash<int, float> taken; // centimeters around each place already taken by the pieces before

    // A yoke goes on top of its place, the other pieces there below it.
    QHash<int, float> yokes;
    for (const PatternPiece &piece : m_pieces)
    {
        const Settings s = settings(piece.id);
        if (spreads(s.placement) && PieceArrangement::isYoke(piece.name))
        {
            const int place = static_cast<int>(s.placement);
            yokes[place] = qMax(yokes.value(place), static_cast<float>(piece.seamLine.boundingRect().height()));
        }
    }

    for (int i = 0; i < m_pieces.size(); ++i)
    {
        const PatternPiece &piece = m_pieces.at(i);
        const Settings s = settings(piece.id);
        const FlatPiece &flat = flatPiece(piece);

        // Several pieces in one place (the panels of a princess bodice, a two-piece sleeve) go side by side around
        // it, in the order of the list, rather than on top of each other.
        PieceAdjustment adjustment = s.adjustment;
        const auto width = static_cast<float>(piece.seamLine.boundingRect().width());
        const bool yoke = PieceArrangement::isYoke(piece.name);
        if (spreads(s.placement) && !yoke)
        {
            adjustment.lift -= yokes.value(static_cast<int>(s.placement));
            const int place = static_cast<int>(s.placement);
            if (taken.contains(place))
            {
                const float offset = s.pair ? taken.value(place) : taken.value(place) + width / 2;
                adjustment.around += qRadiansToDegrees(offset / PieceArrangement::aroundRadius(s.placement, frame));
                taken[place] += width;
            }
            else
            {
                taken.insert(place, s.pair ? width : width / 2);
            }
        }

        const QVector<Mesh> placed = PieceArrangement::place(flat.mesh, piece.seamLine.boundingRect(), s.placement,
                                                             s.pair, s.flip, frame, asideStart, adjustment);
        if (s.placement == PiecePlacement::Aside)
        {
            // Next piece in the row starts after this one (and its mirrored copy), with a gap.
            const auto width = static_cast<float>(piece.seamLine.boundingRect().width());
            asideStart += (s.pair ? 2 : 1) * (width + 15.0f);
        }
        for (int copy = 0; copy < placed.size(); ++copy)
        {
            GarmentPanel panel;
            panel.pieceId = piece.id;
            panel.copy = copy;
            panel.name = piece.name;
            panel.color = pieceColors[i % (sizeof(pieceColors) / sizeof(pieceColors[0]))];
            panel.sewable = s.placement != PiecePlacement::Aside;
            panel.band = s.placement == PiecePlacement::Waistband || s.placement == PiecePlacement::Collar;
            panel.rest = flat.mesh;
            panel.shape = placed.at(copy);
            panel.outlineCount = flat.outlineCount;
            panel.corners = flat.corners;
            result.append(panel);
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Viewport3D::Overlay> PatternPiecesPanel::overlays(const BodyFrame &frame) const
{
    return overlays(panels(frame));
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Viewport3D::Overlay> PatternPiecesPanel::overlays(const QVector<GarmentPanel> &panels)
{
    QVector<Viewport3D::Overlay> result;
    result.reserve(panels.size());
    for (const GarmentPanel &panel : panels)
    {
        result.append({panel.shape, panel.color});
    }
    return result;
}

namespace
{
//---------------------------------------------------------------------------------------------------------------------
QColor seamColor(int index)
{
    // Bright, apart from the pieces' colors.
    static const QColor colors[] = {QColor(255, 64, 129), QColor(0, 188, 212), QColor(255, 193, 7),
                                    QColor(124, 77, 255), QColor(118, 255, 3), QColor(255, 87, 34),
                                    QColor(29, 233, 182), QColor(213, 0, 249)};
    return colors[index % static_cast<int>(sizeof(colors) / sizeof(colors[0]))];
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief The point of an edge at @p share of its length, as the panel is now. */
QVector3D edgePoint(const GarmentPanel &panel, const QVector<int> &vertices, const QVector<double> &along, double share)
{
    for (int i = 1; i < vertices.size(); ++i)
    {
        if (along.at(i) >= share || i + 1 == vertices.size())
        {
            const double span = along.at(i) - along.at(i - 1);
            const auto t = static_cast<float>(span > 1e-9 ? qBound(0.0, (share - along.at(i - 1)) / span, 1.0) : 0.0);
            return panel.shape.positions.at(vertices.at(i - 1)) * (1 - t) + panel.shape.positions.at(vertices.at(i)) * t;
        }
    }
    return panel.shape.positions.at(vertices.constFirst());
}

//---------------------------------------------------------------------------------------------------------------------
/** @brief Where each point of an edge is along it, 0 to 1, measured on the flat piece. */
QVector<double> shares(const GarmentPanel &panel, const QVector<int> &vertices)
{
    QVector<double> along(vertices.size(), 0.0);
    for (int i = 1; i < vertices.size(); ++i)
    {
        along[i] = along.at(i - 1)
                   + static_cast<double>(
                       (panel.rest.positions.at(vertices.at(i)) - panel.rest.positions.at(vertices.at(i - 1))).length());
    }
    const double total = qMax(1e-9, along.constLast());
    for (double &value : along)
    {
        value /= total;
    }
    return along;
}
} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
int PatternPiecesPanel::panelIndex(quint32 piece, int copy) const
{
    for (int i = 0; i < m_seamPanels.size(); ++i)
    {
        if (m_seamPanels.at(i).pieceId == piece && m_seamPanels.at(i).copy == copy)
        {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------------------------------------------------------------------
QString PatternPiecesPanel::edgeName(int panel, int edge) const
{
    if (panel < 0 || panel >= m_seamPanels.size())
    {
        return tr("(missing)");
    }
    const GarmentPanel &p = m_seamPanels.at(panel);
    return tr("%1%2, edge %3").arg(p.name, p.copy > 0 ? tr(" (mirrored)") : QString()).arg(edge + 1);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::updateSeams(const QVector<GarmentPanel> &panels)
{
    m_seamPanels = panels;
    m_foundSeams.clear();
    for (const Seam &seam : SeamFinder::find(panels))
    {
        SeamSpec spec;
        spec.pieceA = panels.at(seam.panelA).pieceId;
        spec.copyA = panels.at(seam.panelA).copy;
        spec.edgeA = seam.edgeA;
        spec.pieceB = panels.at(seam.panelB).pieceId;
        spec.copyB = panels.at(seam.panelB).copy;
        spec.edgeB = seam.edgeB;
        spec.reversed = seam.reversed;
        spec.startA = seam.startA;
        spec.endA = seam.endA;
        spec.startB = seam.startB;
        spec.endB = seam.endB;
        m_foundSeams.append(spec);
    }
    rebuildSeamList();
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Seam> PatternPiecesPanel::seams(const QVector<GarmentPanel> &panels, QVector<QColor> *colors) const
{
    QVector<Seam> result;
    const auto indexIn = [&panels](quint32 piece, int copy)
    {
        for (int i = 0; i < panels.size(); ++i)
        {
            if (panels.at(i).pieceId == piece && panels.at(i).copy == copy)
            {
                return i;
            }
        }
        return -1;
    };
    int number = 0;
    for (const SeamSpec &spec : m_foundSeams + m_addedSeams)
    {
        const int color = number++;
        if (m_switchedOff.contains(spec.key()))
        {
            continue;
        }
        Seam seam;
        seam.panelA = indexIn(spec.pieceA, spec.copyA);
        seam.panelB = indexIn(spec.pieceB, spec.copyB);
        if (seam.panelA < 0 || seam.panelB < 0 || spec.edgeA >= panels.at(seam.panelA).edgeCount()
            || spec.edgeB >= panels.at(seam.panelB).edgeCount())
        {
            continue; // the pattern changed: that edge is gone
        }
        seam.edgeA = spec.edgeA;
        seam.edgeB = spec.edgeB;
        seam.reversed = spec.reversed;
        seam.startA = spec.startA;
        seam.endA = spec.endA;
        seam.startB = spec.startB;
        seam.endB = spec.endB;
        result.append(seam);
        if (colors != nullptr)
        {
            colors->append(seamColor(color));
        }
    }
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::addSeam(const SeamSpec &spec)
{
    SeamSpec added = spec;
    added.added = true;
    m_switchedOff.remove(added.key());
    m_addedSeams.append(added);
    rebuildSeamList();
    changed();
    emit seamsChanged();
}

//---------------------------------------------------------------------------------------------------------------------
QVector<PatternPiecesPanel::SeamSpec> PatternPiecesPanel::foundSeams() const
{
    return m_foundSeams;
}

//---------------------------------------------------------------------------------------------------------------------
bool PatternPiecesPanel::showSeams() const
{
    return m_showSeamsCheck->isChecked();
}

//---------------------------------------------------------------------------------------------------------------------
QVector<QPair<int, int>> PatternPiecesPanel::highlightedEdges() const
{
    return m_highlight;
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::rebuildSeamList()
{
    m_updatingSeams = true;
    m_seamList->clear();
    int number = 0;
    for (const SeamSpec &spec : m_foundSeams + m_addedSeams)
    {
        const int color = number++;
        auto *item = new QTreeWidgetItem(m_seamList);
        QString text = edgeName(panelIndex(spec.pieceA, spec.copyA), spec.edgeA) + QStringLiteral(" ")
                       + QChar(0x2194) + QStringLiteral(" ") + edgeName(panelIndex(spec.pieceB, spec.copyB), spec.edgeB);
        if (spec.endA - spec.startA < 0.999 || spec.endB - spec.startB < 0.999)
        {
            text += tr(" (part)");
        }
        if (spec.added)
        {
            text += tr(" %1 added").arg(QChar(0x2013));
        }
        item->setText(0, text);
        item->setToolTip(0, text);
        item->setData(0, Qt::UserRole, spec.key());
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(0, m_switchedOff.contains(spec.key()) ? Qt::Unchecked : Qt::Checked);
        QPixmap swatch(12, 12);
        swatch.fill(seamColor(color));
        item->setIcon(0, QIcon(swatch));
    }
    m_updatingSeams = false;
    m_removeSeamButton->setEnabled(m_seamList->topLevelItemCount() > 0);
    m_addSeamButton->setEnabled(!m_seamPanels.isEmpty());
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::addSeamDialog()
{
    if (m_seamPanels.isEmpty())
    {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Seam"));
    auto *form = new QFormLayout(&dialog);
    QComboBox *pieces[2];
    QComboBox *edges[2];
    QSpinBox *from[2];
    QSpinBox *to[2];
    for (int side = 0; side < 2; ++side)
    {
        pieces[side] = new QComboBox(&dialog);
        edges[side] = new QComboBox(&dialog);
        // Part of the edge: from and to, in percent of its length along it.
        from[side] = new QSpinBox(&dialog);
        to[side] = new QSpinBox(&dialog);
        from[side]->setRange(0, 99);
        to[side]->setRange(1, 100);
        to[side]->setValue(100);
        for (QSpinBox *spin : {from[side], to[side]})
        {
            spin->setSuffix(QStringLiteral(" %"));
        }
        from[side]->setToolTip(tr("Sew from this far along the edge"));
        to[side]->setToolTip(tr("Sew to this far along the edge"));
        auto *part = new QHBoxLayout;
        part->addWidget(new QLabel(tr("from"), &dialog));
        part->addWidget(from[side]);
        part->addWidget(new QLabel(tr("to"), &dialog));
        part->addWidget(to[side]);
        for (int p = 0; p < m_seamPanels.size(); ++p)
        {
            const GarmentPanel &panel = m_seamPanels.at(p);
            if (panel.sewable)
            {
                pieces[side]->addItem(panel.name + (panel.copy > 0 ? tr(" (mirrored)") : QString()), p);
            }
        }
        form->addRow(side == 0 ? tr("Sew:") : tr("To:"), pieces[side]);
        form->addRow(QString(), edges[side]);
        form->addRow(QString(), part);
        connect(from[side], QOverload<int>::of(&QSpinBox::valueChanged), &dialog,
                [side, &from, &to](int value) { to[side]->setValue(qMax(to[side]->value(), value + 1)); });
        connect(to[side], QOverload<int>::of(&QSpinBox::valueChanged), &dialog,
                [side, &from, &to](int value) { from[side]->setValue(qMin(from[side]->value(), value - 1)); });
    }
    auto *join = new QComboBox(&dialog);
    join->addItem(tr("Start to start"), false);
    join->addItem(tr("Start to end"), true);
    join->setToolTip(tr("Which ends of the two edges meet"));
    form->addRow(tr("Join:"), join);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto fillEdges = [this, &pieces, &edges](int side)
    {
        const int p = pieces[side]->currentData().toInt();
        edges[side]->clear();
        if (p < 0 || p >= m_seamPanels.size())
        {
            return;
        }
        for (int e = 0; e < m_seamPanels.at(p).edgeCount(); ++e)
        {
            edges[side]->addItem(tr("Edge %1 (%2 cm)").arg(e + 1).arg(m_seamPanels.at(p).edgeLength(e), 0, 'f', 1), e);
        }
    };
    // Show the edges chosen, and join them the way their ends lie closest.
    const auto changed = [this, &pieces, &edges, join]()
    {
        m_highlight.clear();
        QVector3D ends[2][2];
        bool complete = true;
        for (int side = 0; side < 2; ++side)
        {
            const int p = pieces[side]->currentData().toInt();
            const int e = edges[side]->currentData().toInt();
            if (p < 0 || p >= m_seamPanels.size() || e < 0 || e >= m_seamPanels.at(p).edgeCount())
            {
                complete = false;
                continue;
            }
            m_highlight.append(qMakePair(p, e));
            const QVector<int> vertices = m_seamPanels.at(p).edgeVertices(e);
            ends[side][0] = m_seamPanels.at(p).shape.positions.at(vertices.constFirst());
            ends[side][1] = m_seamPanels.at(p).shape.positions.at(vertices.constLast());
        }
        if (complete)
        {
            const float same = (ends[0][0] - ends[1][0]).length() + (ends[0][1] - ends[1][1]).length();
            const float crossed = (ends[0][0] - ends[1][1]).length() + (ends[0][1] - ends[1][0]).length();
            join->setCurrentIndex(crossed < same ? 1 : 0);
        }
        emit highlightChanged();
    };
    for (int side = 0; side < 2; ++side)
    {
        connect(pieces[side], QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
                [side, fillEdges, changed]()
                {
                    fillEdges(side);
                    changed();
                });
        connect(edges[side], QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, changed);
        fillEdges(side);
    }
    if (pieces[1]->count() > 1)
    {
        pieces[1]->setCurrentIndex(1);
    }
    changed();

    const bool accepted = dialog.exec() == QDialog::Accepted;
    m_highlight.clear();
    emit highlightChanged();
    if (!accepted)
    {
        return;
    }
    const int a = pieces[0]->currentData().toInt();
    const int b = pieces[1]->currentData().toInt();
    if (a < 0 || b < 0 || a >= m_seamPanels.size() || b >= m_seamPanels.size())
    {
        return;
    }
    SeamSpec spec;
    spec.pieceA = m_seamPanels.at(a).pieceId;
    spec.copyA = m_seamPanels.at(a).copy;
    spec.edgeA = edges[0]->currentData().toInt();
    spec.pieceB = m_seamPanels.at(b).pieceId;
    spec.copyB = m_seamPanels.at(b).copy;
    spec.edgeB = edges[1]->currentData().toInt();
    spec.reversed = join->currentData().toBool();
    spec.startA = from[0]->value() / 100.0;
    spec.endA = to[0]->value() / 100.0;
    spec.startB = from[1]->value() / 100.0;
    spec.endB = to[1]->value() / 100.0;
    if (spec.pieceA == spec.pieceB && spec.copyA == spec.copyB && spec.edgeA == spec.edgeB)
    {
        return; // an edge with itself
    }
    addSeam(spec);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::sewEdges(int panelA, int edgeA, int panelB, int edgeB)
{
    if (panelA < 0 || panelB < 0 || panelA >= m_seamPanels.size() || panelB >= m_seamPanels.size()
        || (panelA == panelB && edgeA == edgeB))
    {
        return;
    }
    const GarmentPanel &a = m_seamPanels.at(panelA);
    const GarmentPanel &b = m_seamPanels.at(panelB);
    if (edgeA < 0 || edgeB < 0 || edgeA >= a.edgeCount() || edgeB >= b.edgeCount())
    {
        return;
    }
    // The ends meet the way they lie closest.
    const QVector<int> va = a.edgeVertices(edgeA);
    const QVector<int> vb = b.edgeVertices(edgeB);
    const QVector3D a0 = a.shape.positions.at(va.constFirst());
    const QVector3D a1 = a.shape.positions.at(va.constLast());
    const QVector3D b0 = b.shape.positions.at(vb.constFirst());
    const QVector3D b1 = b.shape.positions.at(vb.constLast());
    SeamSpec spec;
    spec.pieceA = a.pieceId;
    spec.copyA = a.copy;
    spec.edgeA = edgeA;
    spec.pieceB = b.pieceId;
    spec.copyB = b.copy;
    spec.edgeB = edgeB;
    spec.reversed = (a0 - b1).length() + (a1 - b0).length() < (a0 - b0).length() + (a1 - b1).length();
    addSeam(spec);
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setHighlightedEdges(const QVector<QPair<int, int>> &edges)
{
    m_highlight = edges;
    emit highlightChanged();
}

//---------------------------------------------------------------------------------------------------------------------
bool PatternPiecesPanel::isSewingByClicking() const
{
    return m_sewByClickButton->isChecked();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::setSewingByClicking(bool on)
{
    m_sewByClickButton->setChecked(on);
}

//---------------------------------------------------------------------------------------------------------------------
Viewport3D::LineSet PatternPiecesPanel::edgeLine(const GarmentPanel &panel, int edge, const QColor &color, float width,
                                                 double from, double to)
{
    Viewport3D::LineSet set;
    set.color = color;
    set.width = width;
    const QVector<int> vertices = panel.edgeVertices(edge);
    if (vertices.size() < 2)
    {
        return set;
    }
    const QVector<double> along = shares(panel, vertices);
    QVector3D previous = edgePoint(panel, vertices, along, from);
    for (int i = 1; i < vertices.size(); ++i)
    {
        if (along.at(i) <= from)
        {
            continue;
        }
        const QVector3D next = along.at(i) >= to ? edgePoint(panel, vertices, along, to)
                                                 : panel.shape.positions.at(vertices.at(i));
        set.segments << previous << next;
        previous = next;
        if (along.at(i) >= to)
        {
            break;
        }
    }
    return set;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Viewport3D::LineSet> PatternPiecesPanel::seamLines(const QVector<GarmentPanel> &panels,
                                                           const QVector<Seam> &seams, const QVector<QColor> &colors)
{
    QVector<Viewport3D::LineSet> result;
    for (int s = 0; s < seams.size(); ++s)
    {
        const Seam &seam = seams.at(s);
        if (seam.panelA < 0 || seam.panelB < 0 || seam.panelA >= panels.size() || seam.panelB >= panels.size())
        {
            continue;
        }
        const QColor color = colors.value(s, seamColor(s));
        const GarmentPanel &a = panels.at(seam.panelA);
        const GarmentPanel &b = panels.at(seam.panelB);
        result.append(edgeLine(a, seam.edgeA, color, 3.0f, seam.startA, seam.endA));
        result.append(edgeLine(b, seam.edgeB, color, 3.0f, seam.startB, seam.endB));

        // Ties between the points that meet, as tailors baste.
        Viewport3D::LineSet ties;
        ties.color = color;
        ties.width = 1.0f;
        const QVector<int> va = a.edgeVertices(seam.edgeA);
        const QVector<int> vb = b.edgeVertices(seam.edgeB);
        if (va.size() < 2 || vb.size() < 2)
        {
            continue;
        }
        const QVector<double> sa = shares(a, va);
        const QVector<double> sb = shares(b, vb);
        for (int k = 0; k <= 4; ++k)
        {
            const double t = k / 4.0;
            const QVector3D pa = edgePoint(a, va, sa, seam.startA + (seam.endA - seam.startA) * t);
            const double tb = seam.startB + (seam.endB - seam.startB) * (seam.reversed ? 1 - t : t);
            const QVector3D pb = edgePoint(b, vb, sb, tb);
            ties.segments << pa << pb;
        }
        result.append(ties);
    }
    return result;
}

} // namespace Seamly3D
