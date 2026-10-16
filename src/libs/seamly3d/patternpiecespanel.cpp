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
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "patternprovider.h"

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
    m_flatMeshes.clear();

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
const Mesh &PatternPiecesPanel::flatMesh(const PatternPiece &piece) const
{
    auto it = m_flatMeshes.find(piece.id);
    if (it == m_flatMeshes.end())
    {
        it = m_flatMeshes.insert(piece.id, PieceMesher::mesh(piece.seamLine, meshEdge));
    }
    return it.value();
}

//---------------------------------------------------------------------------------------------------------------------
QVector<Viewport3D::Overlay> PatternPiecesPanel::overlays(const BodyFrame &frame) const
{
    QVector<Viewport3D::Overlay> result;
    float asideStart = 0;
    for (int i = 0; i < m_pieces.size(); ++i)
    {
        const PatternPiece &piece = m_pieces.at(i);
        const Settings s = settings(piece.id);
        const QColor color = pieceColors[i % (sizeof(pieceColors) / sizeof(pieceColors[0]))];

        const QVector<Mesh> placed = PieceArrangement::place(flatMesh(piece), piece.seamLine.boundingRect(),
                                                             s.placement, s.pair, s.flip, frame, asideStart);
        if (s.placement == PiecePlacement::Aside)
        {
            // Next piece in the row starts after this one (and its mirrored copy), with a gap.
            const auto width = static_cast<float>(piece.seamLine.boundingRect().width());
            asideStart += (s.pair ? 2 : 1) * (width + 15.0f);
        }
        for (const Mesh &mesh : placed)
        {
            result.append({mesh, color});
        }
    }
    return result;
}

} // namespace Seamly3D
