// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/games/spaceinvaderswidget.h>

#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>

#include <algorithm>

SpaceInvadersWidget::SpaceInvadersWidget(QWidget* parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!isPaused() && isRunning()) {
            updateGame();
        }
    });
    resetGame();
}

QSize SpaceInvadersWidget::minimumSizeHint() const
{
    return QSize(640, 480);
}

QSize SpaceInvadersWidget::sizeHint() const
{
    return minimumSizeHint();
}

void SpaceInvadersWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_hiddenPause = false;
    ensureTimerRunning();
    setFocus();
    update();
}

void SpaceInvadersWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_hiddenPause = true;
    stopTimer();
    update();
}

void SpaceInvadersWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        return;
    }

    if (event->key() == Qt::Key_P) {
        m_manualPause = !m_manualPause;
        update();
        return;
    }

    if (event->key() == Qt::Key_R) {
        resetGame();
        update();
        return;
    }

    if (!isRunning()) {
        if (event->key() == Qt::Key_Space) {
            resetGame();
            update();
            return;
        }
        QWidget::keyPressEvent(event);
        return;
    }

    if (isPaused()) {
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_A) {
        m_moveLeft = true;
        return;
    }
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_D) {
        m_moveRight = true;
        return;
    }
    if (event->key() == Qt::Key_Space) {
        firePlayerBullet();
        return;
    }

    QWidget::keyPressEvent(event);
}

void SpaceInvadersWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) {
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_A) {
        m_moveLeft = false;
        return;
    }
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_D) {
        m_moveRight = false;
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void SpaceInvadersWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(7, 10, 18));

    const QRect board = gameRect();
    painter.fillRect(board, QColor(10, 15, 28));

    const QPixmap stars(QStringLiteral(":/games/spaceinvaders/stars"));
    if (!stars.isNull()) {
        painter.drawPixmap(board, stars.scaled(board.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    const QPixmap shipPixmap(QStringLiteral(":/games/spaceinvaders/ship"));
    const QPixmap alien50(QStringLiteral(":/games/spaceinvaders/alien50"));
    const QPixmap alien100(QStringLiteral(":/games/spaceinvaders/alien100"));
    const QPixmap bulletPixmap(QStringLiteral(":/games/spaceinvaders/bullet"));
    const QPixmap losePixmap(QStringLiteral(":/games/spaceinvaders/youlose"));
    const QPixmap winPixmap(QStringLiteral(":/games/spaceinvaders/youwin"));

    if (!shipPixmap.isNull()) {
        painter.drawPixmap(m_playerRect.toRect(), shipPixmap.scaled(m_playerRect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.fillRect(m_playerRect, QColor(95, 231, 255));
    }

    for (const Alien& alien : m_aliens) {
        if (!alien.alive) {
            continue;
        }
        const QPixmap& sprite = alien.score >= 100 ? alien100 : alien50;
        if (!sprite.isNull()) {
            painter.drawPixmap(alien.rect.toRect(), sprite.scaled(alien.rect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else {
            painter.fillRect(alien.rect, alien.score >= 100 ? QColor(255, 133, 82) : QColor(120, 232, 126));
        }
    }

    for (const Bullet& bullet : m_bullets) {
        if (!bulletPixmap.isNull()) {
            painter.drawPixmap(bullet.rect.toRect(), bulletPixmap.scaled(bullet.rect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else {
            painter.fillRect(bullet.rect, bullet.enemy ? QColor(255, 116, 94) : QColor(255, 245, 112));
        }
    }

    painter.setPen(Qt::white);
    painter.drawText(board.adjusted(8, 8, -8, -8), Qt::AlignTop | Qt::AlignLeft, tr("Score: %1").arg(m_score));
    painter.drawText(board.adjusted(8, 8, -8, -8), Qt::AlignTop | Qt::AlignRight, tr("R to restart"));

    if (isPaused() || !isRunning()) {
        painter.fillRect(board, QColor(0, 0, 0, 136));
        QRect overlayRect = board.adjusted(70, 90, -70, -90);

        if (!isRunning()) {
            const QPixmap& result = m_playerWon ? winPixmap : losePixmap;
            if (!result.isNull()) {
                const QSize resultSize(overlayRect.width(), overlayRect.height() / 3);
                QRect imageRect(QPoint(overlayRect.left(), overlayRect.top()), resultSize);
                painter.drawPixmap(imageRect, result.scaled(resultSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }

        painter.setPen(Qt::white);
        painter.setFont(QFont(painter.font().family(), 16, QFont::Bold));
        const QString title = m_gameOver ? (m_playerWon ? tr("Victory") : tr("Game Over")) : tr("Paused");
        painter.drawText(overlayRect, Qt::AlignCenter, title);

        painter.setFont(QFont(painter.font().family(), 10));
        const QString subtitle = m_gameOver ? tr("Press Space or R to play again.") : tr("Press P to resume.");
        painter.drawText(overlayRect.adjusted(0, 50, 0, 0), Qt::AlignCenter, subtitle);
    }
}

void SpaceInvadersWidget::resetGame()
{
    m_aliens.clear();
    m_bullets.clear();
    m_score = 0;
    m_enemyDirection = 1;
    m_enemyFireCooldown = 20;
    m_moveLeft = false;
    m_moveRight = false;
    m_manualPause = false;
    m_hiddenPause = false;
    m_gameOver = false;
    m_playerWon = false;
    stopTimer();
    if (isVisible()) {
        ensureTimerRunning();
    }

    const QRect board = gameRect();
    m_playerRect = QRectF(board.center().x() - 32.0, board.bottom() - 58.0, 64.0, 42.0);

    const qreal alienWidth = 42.0;
    const qreal alienHeight = 34.0;
    const qreal startX = board.left() + 72.0;
    const qreal startY = board.top() + 52.0;
    const qreal stepX = 62.0;
    const qreal stepY = 48.0;

    for (int row = 0; row < kAlienRows; ++row) {
        for (int col = 0; col < kAlienCols; ++col) {
            Alien alien;
            alien.rect = QRectF(startX + col * stepX, startY + row * stepY, alienWidth, alienHeight);
            alien.score = row == 0 ? 100 : 50;
            m_aliens.push_back(alien);
        }
    }
}

QRect SpaceInvadersWidget::gameRect() const
{
    QRect r = rect().adjusted(28, 28, -28, -28);
    const qreal targetAspect = 4.0 / 3.0;
    const qreal currentAspect = static_cast<qreal>(r.width()) / std::max(1, r.height());
    if (currentAspect > targetAspect) {
        const int width = static_cast<int>(r.height() * targetAspect);
        const int x = r.center().x() - width / 2;
        r = QRect(x, r.top(), width, r.height());
    } else {
        const int height = static_cast<int>(r.width() / targetAspect);
        const int y = r.center().y() - height / 2;
        r = QRect(r.left(), y, r.width(), height);
    }
    return r;
}

bool SpaceInvadersWidget::isPaused() const
{
    return m_manualPause || m_hiddenPause;
}

bool SpaceInvadersWidget::isRunning() const
{
    return !m_gameOver;
}

void SpaceInvadersWidget::updateGame()
{
    const QRect board = gameRect();

    const qreal playerSpeed = 6.0;
    if (m_moveLeft && !m_moveRight) {
        m_playerRect.translate(-playerSpeed, 0);
    } else if (m_moveRight && !m_moveLeft) {
        m_playerRect.translate(playerSpeed, 0);
    }
    if (m_playerRect.left() < board.left() + 8.0) {
        m_playerRect.moveLeft(board.left() + 8.0);
    }
    if (m_playerRect.right() > board.right() - 8.0) {
        m_playerRect.moveRight(board.right() - 8.0);
    }

    bool hitEdge = false;
    for (Alien& alien : m_aliens) {
        if (!alien.alive) {
            continue;
        }
        alien.rect.translate(1.4 * m_enemyDirection, 0);
        if (alien.rect.left() <= board.left() + 18.0 || alien.rect.right() >= board.right() - 18.0) {
            hitEdge = true;
        }
    }
    if (hitEdge) {
        m_enemyDirection *= -1;
        for (Alien& alien : m_aliens) {
            if (!alien.alive) {
                continue;
            }
            alien.rect.translate(0, 16.0);
            if (alien.rect.bottom() >= m_playerRect.top() - 4.0) {
                m_gameOver = true;
                m_playerWon = false;
            }
        }
    }

    for (Bullet& bullet : m_bullets) {
        bullet.rect.translate(0, bullet.enemy ? 5.5 : -8.0);
    }

    for (Bullet& bullet : m_bullets) {
        if (bullet.enemy && bullet.rect.intersects(m_playerRect)) {
            m_gameOver = true;
            m_playerWon = false;
        }
        if (!bullet.enemy) {
            for (Alien& alien : m_aliens) {
                if (!alien.alive) {
                    continue;
                }
                if (bullet.rect.intersects(alien.rect)) {
                    alien.alive = false;
                    bullet.rect.moveTop(board.top() - 100.0);
                    m_score += alien.score;
                    break;
                }
            }
        }
    }

    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(), [&](const Bullet& bullet) {
        return bullet.rect.bottom() < board.top() || bullet.rect.top() > board.bottom();
    }), m_bullets.end());

    if (--m_enemyFireCooldown <= 0) {
        fireEnemyBullet();
        m_enemyFireCooldown = 35 + QRandomGenerator::global()->bounded(30);
    }

    const bool aliensRemaining = std::any_of(m_aliens.begin(), m_aliens.end(), [](const Alien& alien) { return alien.alive; });
    if (!aliensRemaining) {
        m_gameOver = true;
        m_playerWon = true;
    }

    update();
}

void SpaceInvadersWidget::firePlayerBullet()
{
    const bool activePlayerBullet = std::any_of(m_bullets.begin(), m_bullets.end(), [](const Bullet& bullet) { return !bullet.enemy; });
    if (activePlayerBullet) {
        return;
    }

    Bullet bullet;
    bullet.rect = QRectF(m_playerRect.center().x() - 5.0, m_playerRect.top() - 18.0, 10.0, 22.0);
    bullet.enemy = false;
    m_bullets.push_back(bullet);
}

void SpaceInvadersWidget::fireEnemyBullet()
{
    std::vector<const Alien*> liveAliens;
    for (const Alien& alien : m_aliens) {
        if (alien.alive) {
            liveAliens.push_back(&alien);
        }
    }
    if (liveAliens.empty()) {
        return;
    }

    const Alien* shooter = liveAliens[QRandomGenerator::global()->bounded(static_cast<int>(liveAliens.size()))];
    Bullet bullet;
    bullet.rect = QRectF(shooter->rect.center().x() - 5.0, shooter->rect.bottom() + 4.0, 10.0, 22.0);
    bullet.enemy = true;
    m_bullets.push_back(bullet);
}

void SpaceInvadersWidget::ensureTimerRunning()
{
    m_timer.start(m_tickIntervalMs);
}

void SpaceInvadersWidget::stopTimer()
{
    m_timer.stop();
}
