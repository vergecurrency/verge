// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/games/pongwidget.h>

#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

PongWidget::PongWidget(QWidget* parent) : QWidget(parent)
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

QSize PongWidget::minimumSizeHint() const
{
    return QSize(640, 520);
}

QSize PongWidget::sizeHint() const
{
    return minimumSizeHint();
}

void PongWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_hiddenPause = false;
    ensureTimerRunning();
    setFocus();
    update();
}

void PongWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_hiddenPause = true;
    stopTimer();
    update();
}

void PongWidget::keyPressEvent(QKeyEvent* event)
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

    QWidget::keyPressEvent(event);
}

void PongWidget::keyReleaseEvent(QKeyEvent* event)
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

void PongWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(10, 14, 24));

    const QRect board = gameRect();
    painter.fillRect(board, QColor(18, 24, 38));

    const QPixmap background(QStringLiteral(":/games/pong/background"));
    const QPixmap ballPixmap(QStringLiteral(":/games/pong/ball"));
    const QPixmap paddlePixmap(QStringLiteral(":/games/pong/paddle"));
    const QPixmap blockPixmaps[] = {
        QPixmap(QStringLiteral(":/games/pong/block01")),
        QPixmap(QStringLiteral(":/games/pong/block02")),
        QPixmap(QStringLiteral(":/games/pong/block03")),
        QPixmap(QStringLiteral(":/games/pong/block04")),
        QPixmap(QStringLiteral(":/games/pong/block05")),
    };

    if (!background.isNull()) {
        painter.drawPixmap(board, background.scaled(board.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    for (const Block& block : m_blocks) {
        if (!block.active) {
            continue;
        }
        const int index = std::clamp(block.colorIndex, 0, 4);
        if (!blockPixmaps[index].isNull()) {
            painter.drawPixmap(block.rect.toRect(), blockPixmaps[index].scaled(block.rect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else {
            static const QColor fallbackColors[] = {
                QColor(255, 89, 94),
                QColor(255, 146, 43),
                QColor(255, 202, 58),
                QColor(138, 201, 38),
                QColor(76, 201, 240),
            };
            painter.fillRect(block.rect, fallbackColors[index]);
        }
    }

    if (!paddlePixmap.isNull()) {
        painter.drawPixmap(m_paddleRect.toRect(), paddlePixmap.scaled(m_paddleRect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.fillRect(m_paddleRect, QColor(240, 240, 240));
    }

    if (!ballPixmap.isNull()) {
        painter.drawPixmap(m_ballRect.toRect(), ballPixmap.scaled(m_ballRect.size().toSize(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.fillRect(m_ballRect, QColor(255, 255, 255));
    }

    painter.setPen(Qt::white);
    painter.drawText(board.adjusted(10, 8, -10, -8), Qt::AlignTop | Qt::AlignLeft, tr("Score: %1").arg(m_score));
    painter.drawText(board.adjusted(10, 8, -10, -8), Qt::AlignTop | Qt::AlignRight, tr("R to restart"));

    if (isPaused() || !isRunning()) {
        painter.fillRect(board, QColor(0, 0, 0, 136));
        QRect overlayRect = board.adjusted(70, 90, -70, -90);
        painter.setPen(Qt::white);
        painter.setFont(QFont(painter.font().family(), 16, QFont::Bold));
        const QString title = m_gameOver ? (m_playerWon ? tr("You Win") : tr("Game Over")) : tr("Paused");
        painter.drawText(overlayRect, Qt::AlignCenter, title);

        painter.setFont(QFont(painter.font().family(), 10));
        const QString subtitle = m_gameOver ? tr("Press Space or R to play again.") : tr("Press P to resume.");
        painter.drawText(overlayRect.adjusted(0, 50, 0, 0), Qt::AlignCenter, subtitle);
    }
}

void PongWidget::resetGame()
{
    m_blocks.clear();
    m_score = 0;
    m_moveLeft = false;
    m_moveRight = false;
    m_manualPause = false;
    m_hiddenPause = false;
    m_gameOver = false;
    m_playerWon = false;

    const QRect board = gameRect();
    m_paddleRect = QRectF(board.center().x() - 52.0, board.bottom() - 28.0, 104.0, 14.0);
    m_ballRect = QRectF(board.center().x() - 8.0, board.center().y() + 60.0, 16.0, 16.0);
    m_ballVelocity = QPointF(6.0, -5.0);

    const qreal blockWidth = 40.0;
    const qreal blockHeight = 18.0;
    const qreal startX = board.left() + 44.0;
    const qreal startY = board.top() + 34.0;
    const qreal stepX = 43.0;
    const qreal stepY = 20.0;

    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 10; ++col) {
            Block block;
            block.rect = QRectF(startX + col * stepX, startY + row * stepY, blockWidth, blockHeight);
            block.colorIndex = row % 5;
            m_blocks.push_back(block);
        }
    }

    stopTimer();
    if (isVisible()) {
        ensureTimerRunning();
    }
}

QRect PongWidget::gameRect() const
{
    QRect r = rect().adjusted(24, 24, -24, -24);
    const qreal targetAspect = 520.0 / 450.0;
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

bool PongWidget::isPaused() const
{
    return m_manualPause || m_hiddenPause;
}

bool PongWidget::isRunning() const
{
    return !m_gameOver;
}

void PongWidget::updateGame()
{
    const QRect board = gameRect();
    const qreal paddleSpeed = 7.0;

    if (m_moveLeft && !m_moveRight) {
        m_paddleRect.translate(-paddleSpeed, 0.0);
    } else if (m_moveRight && !m_moveLeft) {
        m_paddleRect.translate(paddleSpeed, 0.0);
    }

    if (m_paddleRect.left() < board.left() + 4.0) {
        m_paddleRect.moveLeft(board.left() + 4.0);
    }
    if (m_paddleRect.right() > board.right() - 4.0) {
        m_paddleRect.moveRight(board.right() - 4.0);
    }

    m_ballRect.translate(m_ballVelocity);

    bounceFromWalls(board);
    bounceFromPaddle();
    bounceFromBlocks();

    if (m_ballRect.top() > board.bottom()) {
        m_gameOver = true;
        m_playerWon = false;
    }

    const bool anyBlocksLeft = std::any_of(m_blocks.begin(), m_blocks.end(), [](const Block& block) {
        return block.active;
    });
    if (!anyBlocksLeft) {
        m_gameOver = true;
        m_playerWon = true;
    }

    update();
}

void PongWidget::bounceFromWalls(const QRect& board)
{
    if (m_ballRect.left() <= board.left()) {
        m_ballRect.moveLeft(board.left());
        m_ballVelocity.rx() = std::abs(m_ballVelocity.x());
    }
    if (m_ballRect.right() >= board.right()) {
        m_ballRect.moveRight(board.right());
        m_ballVelocity.rx() = -std::abs(m_ballVelocity.x());
    }
    if (m_ballRect.top() <= board.top()) {
        m_ballRect.moveTop(board.top());
        m_ballVelocity.ry() = std::abs(m_ballVelocity.y());
    }
}

void PongWidget::bounceFromBlocks()
{
    for (Block& block : m_blocks) {
        if (!block.active || !m_ballRect.intersects(block.rect)) {
            continue;
        }

        block.active = false;
        m_score += 10;

        const qreal overlapLeft = std::abs(m_ballRect.right() - block.rect.left());
        const qreal overlapRight = std::abs(block.rect.right() - m_ballRect.left());
        const qreal overlapTop = std::abs(m_ballRect.bottom() - block.rect.top());
        const qreal overlapBottom = std::abs(block.rect.bottom() - m_ballRect.top());
        const qreal minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});

        if (minOverlap == overlapLeft) {
            m_ballRect.moveRight(block.rect.left());
            m_ballVelocity.rx() = -std::abs(m_ballVelocity.x());
        } else if (minOverlap == overlapRight) {
            m_ballRect.moveLeft(block.rect.right());
            m_ballVelocity.rx() = std::abs(m_ballVelocity.x());
        } else if (minOverlap == overlapTop) {
            m_ballRect.moveBottom(block.rect.top());
            m_ballVelocity.ry() = -std::abs(m_ballVelocity.y());
        } else {
            m_ballRect.moveTop(block.rect.bottom());
            m_ballVelocity.ry() = std::abs(m_ballVelocity.y());
        }
        break;
    }
}

void PongWidget::bounceFromPaddle()
{
    if (!m_ballRect.intersects(m_paddleRect) || m_ballVelocity.y() <= 0.0) {
        return;
    }

    m_ballRect.moveBottom(m_paddleRect.top());

    const qreal paddleCenter = m_paddleRect.center().x();
    const qreal ballCenter = m_ballRect.center().x();
    const qreal normalized = (ballCenter - paddleCenter) / std::max<qreal>(1.0, m_paddleRect.width() / 2.0);
    m_ballVelocity.setX(std::clamp(normalized * 7.0, -7.5, 7.5));
    m_ballVelocity.setY(-(2.0 + QRandomGenerator::global()->bounded(5)));
}

void PongWidget::ensureTimerRunning()
{
    m_timer.start(16);
}

void PongWidget::stopTimer()
{
    m_timer.stop();
}
