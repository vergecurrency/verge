// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_GAMES_TETRISWIDGET_H
#define VERGE_QT_GAMES_TETRISWIDGET_H

#include <QColor>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPoint>
#include <QRect>
#include <QShowEvent>
#include <QSize>
#include <QTimerEvent>
#include <QWidget>

#include <array>

class TetrisWidget : public QWidget
{
public:
    explicit TetrisWidget(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    static constexpr int kRows = 20;
    static constexpr int kCols = 10;
    static constexpr int kCellSize = 18;

    void resetGame();
    void spawnPiece();
    void applyGravityTick();
    bool canPlace(const std::array<QPoint, 4>& cells) const;
    void lockCurrentPiece();
    void clearLines();
    QRect gameRect() const;
    QRect cellRect(int x, int y) const;
    QColor colorForIndex(int index) const;
    bool isPaused() const;

    std::array<std::array<int, kCols>, kRows> m_field{};
    std::array<QPoint, 4> m_currentCells{};
    std::array<QPoint, 4> m_previousCells{};
    int m_currentColor{1};
    int m_timerId{0};
    int m_tickIntervalMs{300};
    bool m_manualPause{false};
    bool m_hiddenPause{false};
};

#endif // VERGE_QT_GAMES_TETRISWIDGET_H
