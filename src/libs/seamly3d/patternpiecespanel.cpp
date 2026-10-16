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
#include <QPixmap>
#include <QGridLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
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

} // anonymous namespace

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel::PatternPiecesPanel(QWidget *parent)
    : QWidget(parent)
    , m_list(new QTreeWidget(this))
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
    , m_followTimer(new QTimer(this))
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
                emit arrangementChanged();
            });
    layout->addWidget(m_list, 1);

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
    connect(m_fabricCombo, &QComboBox::currentIndexChanged, this, &PatternPiecesPanel::fabricChanged);
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
                emit fitMapChanged();
            });
    auto *legendLayout = new QGridLayout(m_legend);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->addWidget(m_legendBar, 0, 0, 1, 2);
    legendLayout->addWidget(m_legendLow, 1, 0, Qt::AlignLeft);
    legendLayout->addWidget(m_legendHigh, 1, 1, Qt::AlignRight);
    m_legend->setVisible(false);

    auto *fabricForm = new QFormLayout;
    fabricForm->addRow(tr("Fabric:"), m_fabricCombo);
    fabricForm->addRow(tr("Fit map:"), m_fitMapCombo);
    fabricForm->addRow(QString(), m_legend);
    layout->addLayout(fabricForm);

    // Pattern edits come in bursts (every point of a drag): reload once they settle.
    m_followTimer->setSingleShot(true);
    m_followTimer->setInterval(400);
    connect(m_followTimer, &QTimer::timeout, this, &PatternPiecesPanel::reload);

    setProvider(nullptr);
}

//---------------------------------------------------------------------------------------------------------------------
PatternPiecesPanel::~PatternPiecesPanel() = default;

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
            s.placement = PieceArrangement::guess(piece.name);
            s.pair = piece.quantity >= 2 || piece.onFold;
            m_settings.insert(piece.id, s);
        }
    }

    rebuildList();
    m_status->setText(m_pieces.isEmpty() ? tr("The pattern has no pieces in the layout yet.")
                                         : tr("%n piece(s). Choose where each one starts around the body.", "",
                                              static_cast<int>(m_pieces.size())));
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
    rebuildList();
    emit arrangementChanged();
}

//---------------------------------------------------------------------------------------------------------------------
void PatternPiecesPanel::rebuildList()
{
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
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
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
                    emit arrangementChanged();
                });
        m_list->setItemWidget(item, PlacementColumn, placement);
    }

    m_updatingList = false;
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
        for (const int corner : piece.corners)
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
    for (int i = 0; i < m_pieces.size(); ++i)
    {
        const PatternPiece &piece = m_pieces.at(i);
        const Settings s = settings(piece.id);
        const FlatPiece &flat = flatPiece(piece);

        const QVector<Mesh> placed = PieceArrangement::place(flat.mesh, piece.seamLine.boundingRect(), s.placement,
                                                             s.pair, s.flip, frame, asideStart);
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

} // namespace Seamly3D
