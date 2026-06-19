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
#include <QScreen>

#include <algorithm>
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
    m_progressPercent(-1),
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

    subscribeToCoreSignals();
    installEventFilter(this);

    m_backgroundAnimationTimer.setInterval(125);
    connect(&m_backgroundAnimationTimer, &QTimer::timeout, this, [this]() {
        if (m_isFinishing) {
            return;
        }
        m_backgroundPhase = (m_backgroundPhase + 1) % 1000000;
        update();
    });
    m_backgroundAnimationTimer.start();

    m_textGradientTimer.setInterval(125);
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

void SplashScreen::drawAnimatedBackground(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal phase = static_cast<qreal>(m_backgroundPhase);
    const qreal widthF = static_cast<qreal>(width());
    const qreal heightF = static_cast<qreal>(height());
    const qreal topSweep = std::fmod(phase * 2.2, widthF * 2.0) - widthF;
    const qreal midSweep = std::fmod(phase * 1.5, heightF * 2.2) - heightF * 0.6;
    const qreal diagonalSweep = std::fmod(phase * 1.1, widthF * 2.4) - widthF * 0.9;

    QLinearGradient baseGradient(0, 0, widthF, heightF);
    baseGradient.setColorAt(0.0, QColor(4, 2, 14));
    baseGradient.setColorAt(0.34, QColor(12, 4, 32));
    baseGradient.setColorAt(0.68, QColor(8, 3, 22));
    baseGradient.setColorAt(1.0, QColor(2, 1, 10));
    painter.fillRect(rect(), baseGradient);

    QLinearGradient magentaBand(topSweep, 0, topSweep + widthF * 1.35, heightF * 0.78);
    magentaBand.setColorAt(0.0, QColor(0, 0, 0, 0));
    magentaBand.setColorAt(0.28, QColor(176, 70, 235, 10));
    magentaBand.setColorAt(0.50, QColor(122, 46, 196, 36));
    magentaBand.setColorAt(0.72, QColor(188, 96, 232, 14));
    magentaBand.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), magentaBand);

    QLinearGradient cyanBand(widthF, midSweep, 0, midSweep + heightF * 0.95);
    cyanBand.setColorAt(0.0, QColor(0, 0, 0, 0));
    cyanBand.setColorAt(0.22, QColor(52, 202, 216, 8));
    cyanBand.setColorAt(0.52, QColor(64, 214, 226, 34));
    cyanBand.setColorAt(0.78, QColor(72, 154, 196, 12));
    cyanBand.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), cyanBand);

    QLinearGradient diagonalBand(diagonalSweep, heightF, diagonalSweep + widthF * 0.95, 0);
    diagonalBand.setColorAt(0.0, QColor(0, 0, 0, 0));
    diagonalBand.setColorAt(0.30, QColor(78, 168, 172, 4));
    diagonalBand.setColorAt(0.52, QColor(86, 196, 202, 16));
    diagonalBand.setColorAt(0.76, QColor(92, 150, 180, 6));
    diagonalBand.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), diagonalBand);

    QRadialGradient topGlow(QPointF(widthF * 0.5 + std::sin(phase * 0.010) * widthF * 0.16,
                                    heightF * 0.18 + std::cos(phase * 0.008) * heightF * 0.05),
                            widthF * 0.82);
    topGlow.setColorAt(0.0, QColor(78, 30, 172, 54));
    topGlow.setColorAt(0.36, QColor(122, 48, 188, 16));
    topGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), topGlow);

    QRadialGradient lowerGlow(QPointF(widthF * 0.64 + std::cos(phase * 0.007) * widthF * 0.12,
                                      heightF * 0.90),
                              widthF * 0.72);
    lowerGlow.setColorAt(0.0, QColor(58, 190, 202, 28));
    lowerGlow.setColorAt(0.34, QColor(54, 108, 172, 10));
    lowerGlow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(rect(), lowerGlow);

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
    const std::string message = title + std::string("\n") +
            (resume_possible ? _("(press q to shutdown and continue later)") : _("press q to shutdown"));
    QMetaObject::invokeMethod(splash, "showProgressMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, nProgress),
        Q_ARG(int, Qt::AlignCenter|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(255,255,255)));
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
    m_progressPercent = -1;
    update();
}

void SplashScreen::showProgressMessage(const QString& message, int progress, int alignment, const QColor& color)
{
    if (m_isFinishing) {
        return;
    }

    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    m_progressPercent = std::max(0, std::min(100, progress));
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
    const int progressBarHeight = 12;
    const int progressBarSpacing = 10;
    const int panelBottom = 430;
    const int panelWidth = width() - (panelHorizontalMargin * 2);
    const bool showProgressBar = m_progressPercent >= 0;

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

    const int panelHeight = measuredTextRect.height() + (textVerticalPadding * 2)
        + (showProgressBar ? progressBarSpacing + progressBarHeight + 18 : 0);
    const QRect textPanelRect(panelHorizontalMargin, panelBottom - panelHeight, panelWidth, panelHeight);
    QRect textRect = textPanelRect.adjusted(textHorizontalPadding, textVerticalPadding,
        -textHorizontalPadding, -textVerticalPadding);
    if (showProgressBar) {
        textRect.setBottom(textRect.bottom() - progressBarSpacing - progressBarHeight - 18);
    }
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

    if (showProgressBar) {
        const QRect progressTrack(textPanelRect.left() + textHorizontalPadding,
            textRect.bottom() + progressBarSpacing,
            textPanelRect.width() - (textHorizontalPadding * 2),
            progressBarHeight);
        const int progressWidth = std::max(0, (progressTrack.width() * m_progressPercent) / 100);
        const QRect progressFill(progressTrack.left(), progressTrack.top(), progressWidth, progressTrack.height());

        painter.setPen(QPen(QColor(109, 236, 255, 110), 1.0));
        painter.setBrush(QColor(5, 4, 18, 190));
        painter.drawRoundedRect(progressTrack, 6, 6);

        if (progressWidth > 0) {
            QLinearGradient progressGradient(progressTrack.left() - progressTrack.width() + m_textGradientOffset,
                progressTrack.center().y(),
                progressTrack.right() + m_textGradientOffset,
                progressTrack.center().y());
            progressGradient.setSpread(QGradient::RepeatSpread);
            progressGradient.setColorAt(0.00, QColor(255, 84, 211));
            progressGradient.setColorAt(0.35, QColor(125, 238, 255));
            progressGradient.setColorAt(0.70, QColor(189, 119, 255));
            progressGradient.setColorAt(1.00, QColor(255, 84, 211));
            painter.setPen(Qt::NoPen);
            painter.setBrush(progressGradient);
            painter.drawRoundedRect(progressFill, 6, 6);

            QLinearGradient glowGradient(progressTrack.left(), progressTrack.top(), progressTrack.right(), progressTrack.bottom());
            glowGradient.setColorAt(0.0, QColor(255, 84, 211, 50));
            glowGradient.setColorAt(0.5, QColor(125, 238, 255, 95));
            glowGradient.setColorAt(1.0, QColor(189, 119, 255, 50));
            painter.setPen(QPen(QBrush(glowGradient), 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(progressFill.adjusted(1, 1, -1, -1), 5, 5);
        }

        QFont percentFont(QStringLiteral("Inter"), 9);
        percentFont.setBold(true);
        painter.setFont(percentFont);
        painter.setPen(QColor(248, 244, 255, 218));
        painter.drawText(QRect(progressTrack.left(), progressTrack.bottom() + 3, progressTrack.width(), 14),
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(m_progressPercent) + QStringLiteral("%"));
    }
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
