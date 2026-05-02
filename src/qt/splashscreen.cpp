// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/splashscreen.h>

#include <qt/networkstyle.h>

#include <clientversion.h>
#include <init.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <util/system.h>
#include <ui_interface.h>
#include <version.h>

#include <QApplication>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>

#include <cmath>

namespace {
constexpr int SPLASH_WIDTH = 500;
constexpr int SPLASH_HEIGHT = 500;
constexpr int SPLASH_CHROME_HEIGHT = 36;
constexpr int TEXT_GRADIENT_PERIOD = 900;
} // namespace

SplashScreen::SplashScreen(interfaces::Node& node, Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(0, f),
    curColor(QColor(255, 255, 255)),
    curAlignment(Qt::AlignCenter | Qt::AlignHCenter),
    m_backgroundPhase(0),
    m_textGradientOffset(0),
    m_isFinishing(false),
    m_coreSignalsConnected(false),
    m_node(node)
{
    setObjectName("SplashScreen");
    setAttribute(Qt::WA_DeleteOnClose);
#ifndef Q_OS_MAC
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
#endif

    const int paddingLeft = 24;
    const int paddingRight = 20;
    qreal devicePixelRatio = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();
#endif

    const QString versionText = QString::fromStdString(FormatFullVersion());
    const QString titleAddText = networkStyle->getTitleAddText();

    const QString titleFontFamily = QStringLiteral("Space Grotesk");
    const QString bodyFontFamily = QStringLiteral("Inter");

    const QSize logicalSplashSize(SPLASH_WIDTH, SPLASH_HEIGHT);
    const QSize splashSize(static_cast<int>(logicalSplashSize.width() * devicePixelRatio),
                           static_cast<int>(logicalSplashSize.height() * devicePixelRatio));
    pixmap = QPixmap(splashSize);
#if QT_VERSION > 0x050100
    pixmap.setDevicePixelRatio(devicePixelRatio);
#endif
    pixmap.fill(Qt::transparent);

    QPainter pixPaint(&pixmap);
    pixPaint.setRenderHint(QPainter::Antialiasing, true);
    const QRect splashRect(QPoint(0, 0), logicalSplashSize);
    const QRect chromeRect(0, 0, SPLASH_WIDTH, SPLASH_CHROME_HEIGHT);

    QPixmap icon(":/icons/splash1");
    pixPaint.drawPixmap(splashRect, icon);

    QLinearGradient chromeGradient(chromeRect.topLeft(), chromeRect.bottomLeft());
    chromeGradient.setColorAt(0.0, QColor(18, 10, 38, 245));
    chromeGradient.setColorAt(1.0, QColor(7, 5, 19, 228));
    pixPaint.fillRect(chromeRect, chromeGradient);
    pixPaint.fillRect(QRect(0, chromeRect.bottom(), SPLASH_WIDTH, 1), QColor(255, 80, 205, 110));

    QFont chromeFont(titleFontFamily);
    chromeFont.setPointSize(10);
    chromeFont.setBold(true);
    pixPaint.setFont(chromeFont);
    pixPaint.setPen(QColor(247, 244, 255));
    const QString chromeTitleBase = tr("Verge Core") + " " + versionText;
    const QString chromeTitle = titleAddText.isEmpty() ? chromeTitleBase : chromeTitleBase + " " + titleAddText;
    pixPaint.drawText(chromeRect.adjusted(14, 0, -14, 0), Qt::AlignVCenter | Qt::AlignLeft, chromeTitle);

    if (!titleAddText.isEmpty()) {
        QFont badgeFont(bodyFontFamily);
        badgeFont.setPointSize(9);
        badgeFont.setBold(true);
        pixPaint.setFont(badgeFont);
        QFontMetrics badgeMetrics(badgeFont);
        const int badgeWidth = badgeMetrics.horizontalAdvance(titleAddText) + 22;
        const QRect badgeRect(SPLASH_WIDTH - badgeWidth - 16, 8, badgeWidth, 20);
        pixPaint.setPen(QPen(QColor(118, 236, 255, 180), 1));
        pixPaint.setBrush(QColor(19, 14, 40, 180));
        pixPaint.drawRoundedRect(badgeRect, 10, 10);
        pixPaint.setPen(QColor(244, 249, 255));
        pixPaint.drawText(badgeRect, Qt::AlignCenter, titleAddText);
    }

    QFont versionFont(bodyFontFamily);
    versionFont.setPointSize(12);
    versionFont.setBold(true);
    pixPaint.setFont(versionFont);
    pixPaint.setPen(QColor(249, 246, 255, 232));
    pixPaint.drawText(QRect(paddingLeft, SPLASH_HEIGHT - 56, SPLASH_WIDTH - paddingLeft - paddingRight, 24),
        Qt::AlignRight | Qt::AlignVCenter, versionText);

    pixPaint.end();

    setWindowTitle(titleAddText.isEmpty() ? tr("Verge Core") : tr("Verge Core") + " " + titleAddText);
    curMessage = tr("Launching Verge Core...");
    curColor = QColor(255, 255, 255);
    curAlignment = Qt::AlignCenter | Qt::AlignHCenter;

    resize(logicalSplashSize);
    setFixedSize(logicalSplashSize);
    QScreen* const screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect splashGeometry(QPoint(), logicalSplashSize);
        move(screen->geometry().center() - splashGeometry.center());
    }

    initializeBubbles();
    subscribeToCoreSignals();
    installEventFilter(this);

    m_backgroundAnimationTimer.setInterval(45);
    connect(&m_backgroundAnimationTimer, &QTimer::timeout, this, [this]() {
        if (m_isFinishing) {
            return;
        }
        m_backgroundPhase = (m_backgroundPhase + 1) % 1000000;
        update();
    });
    m_backgroundAnimationTimer.start();

    m_textGradientTimer.setInterval(50);
    connect(&m_textGradientTimer, &QTimer::timeout, this, [this]() {
        if (m_isFinishing) {
            return;
        }
        m_textGradientOffset = (m_textGradientOffset + 5) % TEXT_GRADIENT_PERIOD;
        update();
    });
    m_textGradientTimer.start();
}

SplashScreen::~SplashScreen()
{
    beginFinish();
}

void SplashScreen::initializeBubbles()
{
    static const QColor bubblePalette[] = {
        QColor(255, 92, 200),
        QColor(116, 236, 255),
        QColor(184, 112, 255),
        QColor(255, 170, 94),
        QColor(255, 118, 226)
    };
    static const qreal bubblePositions[] = {
        0.08, 0.17, 0.28, 0.40, 0.53, 0.66, 0.79,
        0.91, 0.14, 0.35, 0.59, 0.73, 0.85
    };
    static const qreal bubbleOffsets[] = {
        18.0, 72.0, 133.0, 205.0, 264.0, 328.0, 391.0,
        448.0, 516.0, 579.0, 642.0, 708.0, 776.0
    };
    static const int bubbleCount = sizeof(bubblePositions) / sizeof(bubblePositions[0]);

    m_bubbles.clear();
    m_bubbles.reserve(bubbleCount);

    for (int i = 0; i < bubbleCount; ++i) {
        Bubble bubble;
        bubble.normalizedX = bubblePositions[i];
        bubble.radius = 20.0 + (i % 5) * 8.5 + ((i + 2) % 3) * 3.0;
        bubble.riseSpeed = 0.92 + (i % 4) * 0.14 + (i / 4) * 0.04;
        bubble.driftAmplitude = 12.0 + (i % 3) * 7.0 + ((i + 1) % 2) * 2.0;
        bubble.driftSpeed = 0.015 + (i % 5) * 0.0028;
        bubble.offset = bubbleOffsets[i];
        bubble.opacity = (0.15 + (i % 4) * 0.035) * 1.25;
        bubble.color = bubblePalette[i % (sizeof(bubblePalette) / sizeof(bubblePalette[0]))];
        m_bubbles.push_back(bubble);
    }
}

void SplashScreen::drawAnimatedBackground(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient baseGradient(0, 0, 0, height());
    baseGradient.setColorAt(0.0, QColor(6, 4, 16));
    baseGradient.setColorAt(0.42, QColor(25, 9, 48));
    baseGradient.setColorAt(1.0, QColor(3, 3, 12));
    painter.fillRect(rect(), baseGradient);

    QRadialGradient upperGlow(QPointF(width() * 0.50, height() * 0.16), width() * 0.70);
    upperGlow.setColorAt(0.0, QColor(161, 82, 255, 112));
    upperGlow.setColorAt(0.45, QColor(255, 81, 198, 44));
    upperGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), upperGlow);

    QLinearGradient lowerGlow(0, height() * 0.58, 0, height());
    lowerGlow.setColorAt(0.0, QColor(0, 0, 0, 0));
    lowerGlow.setColorAt(0.55, QColor(51, 15, 88, 46));
    lowerGlow.setColorAt(1.0, QColor(76, 231, 255, 80));
    painter.fillRect(rect(), lowerGlow);

    const qreal travelSpan = height() + 220.0;
    for (const Bubble& bubble : m_bubbles) {
        const qreal x = bubble.normalizedX * width() +
            std::sin((m_backgroundPhase + bubble.offset) * bubble.driftSpeed) * bubble.driftAmplitude;
        const qreal travel = std::fmod((m_backgroundPhase * bubble.riseSpeed) + bubble.offset,
            travelSpan + (bubble.radius * 2.0));
        const qreal y = height() + bubble.radius - travel;

        const QPointF center(x, y);
        QRadialGradient bubbleGradient(center, bubble.radius);
        QColor innerColor = bubble.color;
        const qreal innerOpacity = bubble.opacity * 1.45 > 1.0 ? 1.0 : bubble.opacity * 1.45;
        innerColor.setAlphaF(innerOpacity);
        QColor middleColor = bubble.color;
        middleColor.setAlphaF(bubble.opacity * 0.62);
        QColor outerColor = bubble.color;
        outerColor.setAlpha(0);
        bubbleGradient.setColorAt(0.0, innerColor);
        bubbleGradient.setColorAt(0.55, middleColor);
        bubbleGradient.setColorAt(1.0, outerColor);

        QColor outlineColor = bubble.color;
        const qreal outlineOpacity = bubble.opacity * 1.95 > 1.0 ? 1.0 : bubble.opacity * 1.95;
        outlineColor.setAlphaF(outlineOpacity);

        painter.setBrush(bubbleGradient);
        painter.setPen(QPen(outlineColor, 1.2));
        painter.drawEllipse(center, bubble.radius, bubble.radius);

        QColor highlightColor(255, 255, 255);
        highlightColor.setAlphaF(bubble.opacity * 0.42);
        painter.setBrush(highlightColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x - bubble.radius * 0.26, y - bubble.radius * 0.34),
            bubble.radius * 0.18, bubble.radius * 0.18);
    }

    painter.restore();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    Q_UNUSED(obj);
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if (keyEvent->key() == Qt::Key_Q) {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::beginFinish()
{
    if (m_isFinishing) {
        return;
    }

    m_isFinishing = true;
    m_backgroundAnimationTimer.stop();
    m_textGradientTimer.stop();
    removeEventFilter(this);
    unsubscribeFromCoreSignals();
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    Q_UNUSED(mainWin);

    beginFinish();

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater();
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignCenter|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(255,255,255)));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + std::string("\n") +
            (resume_possible ? _("(press q to shutdown and continue later)")
                                : _("press q to shutdown")) +
            strprintf("\n%d", nProgress) + "%");
}
#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet)
{
    m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, false)));
    m_connected_wallets.emplace_back(std::move(wallet));
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    if (m_coreSignalsConnected) {
        return;
    }

    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node.handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
        QTimer::singleShot(0, this, [this, wallet = std::move(wallet)]() mutable {
            if (m_isFinishing) {
                return;
            }
            ConnectWallet(std::move(wallet));
        });
    });
#endif
    m_coreSignalsConnected = true;
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    if (!m_coreSignalsConnected) {
        return;
    }

    // Disconnect signals from client
    if (m_handler_init_message) {
        m_handler_init_message->disconnect();
        m_handler_init_message.reset();
    }
    if (m_handler_show_progress) {
        m_handler_show_progress->disconnect();
        m_handler_show_progress.reset();
    }
    if (m_handler_load_wallet) {
        m_handler_load_wallet->disconnect();
        m_handler_load_wallet.reset();
    }
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
    m_coreSignalsConnected = false;
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    if (m_isFinishing) {
        return;
    }

    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawAnimatedBackground(painter);
    painter.drawPixmap(0, 0, pixmap);
    Q_UNUSED(event);

    const int panelHorizontalMargin = 22;
    const int textHorizontalPadding = 16;
    const int textVerticalPadding = 6;
    const int panelBottom = 430;
    const int panelWidth = width() - (panelHorizontalMargin * 2);

    QFont statusFont(QStringLiteral("Inter"), 15);
    painter.setFont(statusFont);
    QFontMetrics statusMetrics(statusFont);
    const QRect measureRect(panelHorizontalMargin + textHorizontalPadding, 0,
        panelWidth - (textHorizontalPadding * 2), height());
    const int textFlags = (curAlignment | Qt::TextWordWrap) & ~Qt::AlignVCenter;
    QRect measuredTextRect = statusMetrics.boundingRect(measureRect, textFlags, curMessage);
    if (measuredTextRect.height() < statusMetrics.lineSpacing()) {
        measuredTextRect.setHeight(statusMetrics.lineSpacing());
    }

    const int panelHeight = measuredTextRect.height() + (textVerticalPadding * 2);
    const QRect textPanelRect(panelHorizontalMargin, panelBottom - panelHeight, panelWidth, panelHeight);
    const QRect textRect = textPanelRect.adjusted(textHorizontalPadding, textVerticalPadding,
        -textHorizontalPadding, -textVerticalPadding);
    QLinearGradient textPanelGradient(textPanelRect.topLeft(), textPanelRect.bottomLeft());
    textPanelGradient.setColorAt(0.0, QColor(14, 8, 30, 150));
    textPanelGradient.setColorAt(1.0, QColor(7, 6, 18, 110));
    painter.setBrush(textPanelGradient);
    painter.setPen(QPen(QColor(255, 109, 225, 74), 1.0));
    painter.drawRoundedRect(textPanelRect, 18, 18);

    QLinearGradient gradient(textRect.left() - textRect.width() + m_textGradientOffset, textRect.center().y(),
                             textRect.left() + m_textGradientOffset, textRect.center().y());
    gradient.setSpread(QGradient::RepeatSpread);
    gradient.setColorAt(0.00, QColor(255, 255, 255, curColor.alpha()));
    gradient.setColorAt(0.25, QColor(255, 118, 226, curColor.alpha()));
    gradient.setColorAt(0.50, QColor(118, 236, 255, curColor.alpha()));
    gradient.setColorAt(0.75, QColor(190, 123, 255, curColor.alpha()));
    gradient.setColorAt(1.00, QColor(255, 255, 255, curColor.alpha()));

    painter.setPen(QPen(QBrush(gradient), 0));
    painter.drawText(textRect, textFlags, curMessage);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
