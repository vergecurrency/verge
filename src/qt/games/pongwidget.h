// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_GAMES_PONGWIDGET_H
#define VERGE_QT_GAMES_PONGWIDGET_H

#include <QHideEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QRectF>
#include <QShowEvent>
#include <QSize>
#include <QTimer>
#include <QWidget>

#include <vector>

class PongWidget : public QWidget
{
public:
    explicit PongWidget(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Block {
        QRectF rect;
        int colorIndex{0};
        bool active{true};
    };

    void resetGame();
    QRect gameRect() const;
    bool isPaused() const;
    bool isRunning() const;
    void updateGame();
    void bounceFromWalls(const QRect& board);
    void bounceFromBlocks();
    void bounceFromPaddle();
    void ensureTimerRunning();
    void stopTimer();

    std::vector<Block> m_blocks;
    QRectF m_paddleRect;
    QRectF m_ballRect;
    QPointF m_ballVelocity{6.0, -5.0};
    QTimer m_timer;
    int m_score{0};
    bool m_moveLeft{false};
    bool m_moveRight{false};
    bool m_manualPause{false};
    bool m_hiddenPause{false};
    bool m_gameOver{false};
    bool m_playerWon{false};
};

#endif // VERGE_QT_GAMES_PONGWIDGET_H
