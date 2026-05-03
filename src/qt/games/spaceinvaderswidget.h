// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_GAMES_SPACEINVADERSWIDGET_H
#define VERGE_QT_GAMES_SPACEINVADERSWIDGET_H

#include <QHideEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QPointF>
#include <QRectF>
#include <QShowEvent>
#include <QSize>
#include <QTimer>
#include <QWidget>

#include <vector>

class SpaceInvadersWidget : public QWidget
{
public:
    explicit SpaceInvadersWidget(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Bullet {
        QRectF rect;
        bool enemy{false};
    };

    struct Alien {
        QRectF rect;
        int score{50};
        bool alive{true};
    };

    static constexpr int kAlienRows = 3;
    static constexpr int kAlienCols = 7;

    void resetGame();
    QRect gameRect() const;
    bool isPaused() const;
    bool isRunning() const;
    void updateGame();
    void firePlayerBullet();
    void fireEnemyBullet();
    void ensureTimerRunning();
    void stopTimer();

    std::vector<Alien> m_aliens;
    std::vector<Bullet> m_bullets;
    QRectF m_playerRect;
    int m_tickIntervalMs{16};
    QTimer m_timer;
    int m_score{0};
    int m_enemyDirection{1};
    int m_enemyFireCooldown{0};
    bool m_moveLeft{false};
    bool m_moveRight{false};
    bool m_manualPause{false};
    bool m_hiddenPause{false};
    bool m_gameOver{false};
    bool m_playerWon{false};
};

#endif // VERGE_QT_GAMES_SPACEINVADERSWIDGET_H
