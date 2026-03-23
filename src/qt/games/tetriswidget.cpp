// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/games/tetriswidget.h>

#include <QKeyEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QTimerEvent>

namespace {
const int FIGURES[7][4] = {
    {1, 3, 5, 7},
    {2, 4, 5, 7},
    {3, 5, 4, 6},
    {3, 5, 4, 7},
    {2, 3, 5, 7},
    {3, 5, 7, 6},
    {2, 3, 4, 5},
};
}

TetrisWidget::TetrisWidget(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_timerId = startTimer(m_tickIntervalMs);
    resetGame();
}

QSize TetrisWidget::minimumSizeHint() const
{
    return QSize(320, 480);
}

QSize TetrisWidget::sizeHint() const
{
    return minimumSizeHint();
}

void TetrisWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setFocus();
}

void TetrisWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(10, 14, 24));

    const QRect board = gameRect();
    painter.fillRect(board, QColor(24, 28, 40));

    QPixmap background(QStringLiteral(":/games/tetris/background"));
    if (!background.isNull()) {
        painter.drawPixmap(board, background.scaled(board.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    QPixmap tiles(QStringLiteral(":/games/tetris/tiles"));
    const bool hasTiles = !tiles.isNull();

    auto drawCell = [&](int x, int y, int colorIndex) {
        const QRect r = cellRect(x, y);
        if (hasTiles && colorIndex > 0) {
            const int tileX = colorIndex * kCellSize;
            painter.drawPixmap(r, tiles, QRect(tileX, 0, kCellSize, kCellSize));
        } else {
            painter.fillRect(r.adjusted(1, 1, -1, -1), colorForIndex(colorIndex));
        }
    };

    for (int y = 0; y < kRows; ++y) {
        for (int x = 0; x < kCols; ++x) {
            if (m_field[y][x] > 0) {
                drawCell(x, y, m_field[y][x]);
            }
        }
    }

    for (const QPoint& cell : m_currentCells) {
        if (cell.y() >= 0) {
            drawCell(cell.x(), cell.y(), m_currentColor);
        }
    }

    QPixmap frame(QStringLiteral(":/games/tetris/frame"));
    if (!frame.isNull()) {
        const QRect frameRect = board.adjusted(-28, -31, 28, 31);
        const QRect frameSourceRect(12, 18, 213, 388);
        painter.drawPixmap(frameRect, frame, frameSourceRect);
    }

    painter.setPen(Qt::white);
    painter.drawText(rect().adjusted(0, 10, 0, -10), Qt::AlignTop | Qt::AlignHCenter, tr("Tetris"));
}

void TetrisWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        resetGame();
        update();
        return;
    }

    if (event->key() == Qt::Key_Down) {
        m_tickIntervalMs = 50;
        killTimer(m_timerId);
        m_timerId = startTimer(m_tickIntervalMs);
        applyGravityTick();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const int dx = (event->key() == Qt::Key_Left) ? -1 : 1;
        m_previousCells = m_currentCells;
        for (QPoint& cell : m_currentCells) {
            cell.rx() += dx;
        }
        if (!canPlace(m_currentCells)) {
            m_currentCells = m_previousCells;
        }
        update();
        return;
    }

    if (event->key() == Qt::Key_Up) {
        m_previousCells = m_currentCells;
        const QPoint pivot = m_currentCells[1];
        for (QPoint& cell : m_currentCells) {
            const int x = cell.y() - pivot.y();
            const int y = cell.x() - pivot.x();
            cell.setX(pivot.x() - x);
            cell.setY(pivot.y() + y);
        }
        if (!canPlace(m_currentCells)) {
            m_currentCells = m_previousCells;
        }
        update();
        return;
    }

    QWidget::keyPressEvent(event);
}

void TetrisWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_timerId) {
        QWidget::timerEvent(event);
        return;
    }
    applyGravityTick();
}

void TetrisWidget::resetGame()
{
    for (auto& row : m_field) {
        row.fill(0);
    }
    m_tickIntervalMs = 300;
    if (m_timerId != 0) {
        killTimer(m_timerId);
    }
    m_timerId = startTimer(m_tickIntervalMs);
    spawnPiece();
}

void TetrisWidget::spawnPiece()
{
    m_currentColor = 1 + QRandomGenerator::global()->bounded(7);
    const int figureIndex = QRandomGenerator::global()->bounded(7);
    for (int i = 0; i < 4; ++i) {
        m_currentCells[i].setX(FIGURES[figureIndex][i] % 2 + 4);
        m_currentCells[i].setY(FIGURES[figureIndex][i] / 2);
    }
    if (!canPlace(m_currentCells)) {
        for (auto& row : m_field) {
            row.fill(0);
        }
    }
}

void TetrisWidget::applyGravityTick()
{
    m_previousCells = m_currentCells;
    for (QPoint& cell : m_currentCells) {
        cell.ry() += 1;
    }

    if (!canPlace(m_currentCells)) {
        m_currentCells = m_previousCells;
        lockCurrentPiece();
        clearLines();
        spawnPiece();
    }

    if (m_tickIntervalMs != 300) {
        m_tickIntervalMs = 300;
        killTimer(m_timerId);
        m_timerId = startTimer(m_tickIntervalMs);
    }

    update();
}

bool TetrisWidget::canPlace(const std::array<QPoint, 4>& cells) const
{
    for (const QPoint& cell : cells) {
        if (cell.x() < 0 || cell.x() >= kCols || cell.y() >= kRows) {
            return false;
        }
        if (cell.y() >= 0 && m_field[cell.y()][cell.x()] != 0) {
            return false;
        }
    }
    return true;
}

void TetrisWidget::lockCurrentPiece()
{
    for (const QPoint& cell : m_currentCells) {
        if (cell.y() >= 0 && cell.y() < kRows && cell.x() >= 0 && cell.x() < kCols) {
            m_field[cell.y()][cell.x()] = m_currentColor;
        }
    }
}

void TetrisWidget::clearLines()
{
    int writeRow = kRows - 1;
    for (int y = kRows - 1; y >= 0; --y) {
        int count = 0;
        for (int x = 0; x < kCols; ++x) {
            if (m_field[y][x] != 0) {
                ++count;
            }
            m_field[writeRow][x] = m_field[y][x];
        }
        if (count < kCols) {
            --writeRow;
        }
    }
    while (writeRow >= 0) {
        m_field[writeRow].fill(0);
        --writeRow;
    }
}

QRect TetrisWidget::gameRect() const
{
    const int margin = 32;
    const int availableWidth = width() - margin * 2;
    const int availableHeight = height() - margin * 2;
    const double boardAspect = static_cast<double>(kCols) / static_cast<double>(kRows);

    int boardWidth = availableWidth;
    int boardHeight = static_cast<int>(boardWidth / boardAspect);
    if (boardHeight > availableHeight) {
        boardHeight = availableHeight;
        boardWidth = static_cast<int>(boardHeight * boardAspect);
    }

    return QRect((width() - boardWidth) / 2, (height() - boardHeight) / 2, boardWidth, boardHeight);
}

QRect TetrisWidget::cellRect(int x, int y) const
{
    const QRect board = gameRect();
    return QRect(board.left() + x * board.width() / kCols,
                 board.top() + y * board.height() / kRows,
                 board.width() / kCols,
                 board.height() / kRows);
}

QColor TetrisWidget::colorForIndex(int index) const
{
    static const QColor colors[] = {
        QColor(0, 0, 0, 0),
        QColor(76, 201, 240),
        QColor(255, 89, 94),
        QColor(138, 201, 38),
        QColor(255, 202, 58),
        QColor(255, 146, 43),
        QColor(106, 76, 147),
        QColor(244, 96, 54),
    };
    if (index < 0 || index >= static_cast<int>(sizeof(colors) / sizeof(colors[0]))) {
        return QColor(255, 255, 255);
    }
    return colors[index];
}
