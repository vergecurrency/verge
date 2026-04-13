// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/tradepage.h>

#include <logging.h>

#include <QLabel>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShowEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
#include <QtWebEngineCore/QWebEngineCertificateError>
#include <QtWebEngineCore/QWebEngineFullScreenRequest>
#include <QtWebEngineCore/QWebEngineHistory>
#include <QtWebEngineCore/QWebEngineNavigationRequest>
#include <QtWebEngineCore/QWebEngineNewWindowRequest>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>
#if QT_VERSION >= 0x060800
#include <QtWebEngineCore/QWebEngineDesktopMediaRequest>
#include <QtWebEngineCore/QWebEnginePermission>
#endif
#include <QtWebEngineWidgets/QWebEngineView>
#if QT_VERSION >= 0x060200
#include <QtWebEngineCore/QWebEngineLoadingInfo>
#endif
#endif

namespace {
static const char* STEALTHEX_WIDGET_URL = "https://stealthex.io/widget/1c5c64de-0ac0-4b79-a393-e447de460c42";

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
static void LogTradeDiagnostic(const QString& message)
{
    qWarning().noquote() << message;
    LogPrintf("GUI: %s\n", message.toStdString());
}

static QString TradeCaptureBlockedText()
{
    return QObject::tr("Trade widget requested screen sharing, which is disabled in VERGE on this platform.");
}

static QUrl TradeWidgetUrl()
{
    const QString override_url = qEnvironmentVariable("VERGE_TRADEPAGE_DEBUG_URL");
    if (!override_url.isEmpty()) {
        return QUrl::fromUserInput(override_url);
    }
    return QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL));
}

static void LogTradeWebEngineState(QWebEngineView* view, QWebEnginePage* page, const char* stage)
{
    if (!view || !page) {
        LogTradeDiagnostic(QStringLiteral("TradePage: %1 view/page missing").arg(QString::fromLatin1(stage)));
        return;
    }

    QWebEngineProfile* profile = page->profile();
    QWebEngineSettings* settings = page->settings();
    LogTradeDiagnostic(QStringLiteral("TradePage: %1 viewVisible=%2 viewSize=%3x%4 viewUrl=%5 pageUrl=%6 requestedUrl=%7 renderPid=%8 canGoBack=%9 canGoForward=%10")
                           .arg(QString::fromLatin1(stage))
                           .arg(view->isVisible())
                           .arg(view->size().width())
                           .arg(view->size().height())
                           .arg(view->url().toString())
                           .arg(page->url().toString())
                           .arg(page->requestedUrl().toString())
                           .arg(page->renderProcessPid())
                           .arg(page->history()->canGoBack())
                           .arg(page->history()->canGoForward()));
    if (profile) {
        LogTradeDiagnostic(QStringLiteral("TradePage: %1 profileStorageName=%2 httpUserAgent=%3 cachePath=%4 persistentStoragePath=%5 offTheRecord=%6")
                               .arg(QString::fromLatin1(stage))
                               .arg(profile->storageName())
                               .arg(profile->httpUserAgent())
                               .arg(profile->cachePath())
                               .arg(profile->persistentStoragePath())
                               .arg(profile->isOffTheRecord()));
    }
    if (settings) {
        LogTradeDiagnostic(QStringLiteral("TradePage: %1 setting JavascriptEnabled=%2 LocalContentCanAccessRemoteUrls=%3 LocalContentCanAccessFileUrls=%4 JavascriptCanOpenWindows=%5 PluginsEnabled=%6 FullScreenSupportEnabled=%7 ScreenCaptureEnabled=%8 WebGLEnabled=%9 Accelerated2dCanvasEnabled=%10 PlaybackRequiresUserGesture=%11")
                               .arg(QString::fromLatin1(stage))
                               .arg(settings->testAttribute(QWebEngineSettings::JavascriptEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls))
                               .arg(settings->testAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls))
                               .arg(settings->testAttribute(QWebEngineSettings::JavascriptCanOpenWindows))
                               .arg(settings->testAttribute(QWebEngineSettings::PluginsEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::FullScreenSupportEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::ScreenCaptureEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::WebGLEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled))
                               .arg(settings->testAttribute(QWebEngineSettings::PlaybackRequiresUserGesture)));
    }
}

class TradeWebEnginePage : public QWebEnginePage
{
public:
    explicit TradeWebEnginePage(QObject* parent) : QWebEnginePage(parent) {}
    TradeWebEnginePage(QWebEngineProfile* profile, QObject* parent) : QWebEnginePage(profile, parent) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString& message,
                                  int lineNumber,
                                  const QString& sourceID) override
    {
        qWarning() << "TradePage: console level=" << static_cast<int>(level)
                   << "line=" << lineNumber
                   << "source=" << sourceID
                   << "message=" << message;
        QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
    }

    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override
    {
        qWarning() << "TradePage: navigation request type=" << static_cast<int>(type)
                   << "mainFrame=" << isMainFrame
                   << "url=" << url;

        if (!isMainFrame) {
            return true;
        }
        if (url.isEmpty()) {
            qWarning() << "TradePage: allowing empty main-frame navigation request";
            return true;
        }
        const QString scheme = url.scheme().toLower();
        if (scheme != "http" && scheme != "https") {
            qWarning() << "TradePage: allowing non-http(s) main-frame navigation"
                       << "scheme=" << scheme << "url=" << url;
            return true;
        }
        const QString host = url.host().toLower();
        const bool allowed = host == "stealthex.io" || host.endsWith(".stealthex.io");
        qWarning() << "TradePage:" << (allowed ? "allowing" : "blocking")
                   << "main-frame navigation host=" << host << "url=" << url;
        return allowed;
    }
};

static QString TradeLoadFailureText()
{
    return QObject::tr("Trade widget failed to load. Check debug.log for the Qt WebEngine error details.");
}

static QString TradeWaitingForNetworkText()
{
    return QObject::tr("Waiting for Tor/network connectivity before loading the Trade widget...");
}

static QString TradeRetryingAfterNetworkErrorText()
{
    return QObject::tr("Trade widget is waiting for Tor/network connectivity. Retrying automatically...");
}

static QString TradeProbeUrlString()
{
    const QUrl load_url = TradeWidgetUrl();
    if (load_url.isValid() && !load_url.scheme().isEmpty() && !load_url.host().isEmpty()) {
        return load_url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment).toString();
    }
    return load_url.toString();
}

static int TradeConnectivityRetryDelayMs(int attempt)
{
    if (attempt <= 1) return 5000;
    if (attempt == 2) return 10000;
    if (attempt == 3) return 15000;
    return 30000;
}
#endif
} // namespace

TradePage::TradePage(QWidget* parent) : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_statusLabel = new QLabel(tr("Loading Trade widget..."), this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMargin(16);
    m_layout->addWidget(m_statusLabel);
}

void TradePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensureInitialized();
}

void TradePage::updateStatusLabel(const QString& text)
{
    if (!m_statusLabel) {
        return;
    }
    m_statusLabel->setText(text);
    m_statusLabel->show();
}

void TradePage::ensureInitialized()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
    m_networkAccessManager = new QNetworkAccessManager(this);
    m_connectivityRetryTimer = new QTimer(this);
    m_connectivityRetryTimer->setSingleShot(true);
    connect(m_connectivityRetryTimer, &QTimer::timeout, this, [this]() {
        startConnectivityProbe();
    });

    updateStatusLabel(TradeWaitingForNetworkText());
    startConnectivityProbe();
#else
    if (m_statusLabel) {
        m_statusLabel->setText(
            tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."));
    }
#endif
}

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
void TradePage::startConnectivityProbe()
{
    if (m_webViewLoadStarted || !m_networkAccessManager) {
        return;
    }

    ++m_connectivityAttemptCount;
    const QString probe_url = TradeProbeUrlString();
    LogTradeDiagnostic(QStringLiteral("TradePage: connectivity probe attempt=%1 url=%2")
                           .arg(m_connectivityAttemptCount)
                           .arg(probe_url));

    QNetworkRequest request{QUrl(probe_url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VERGE-TradePage-Probe/1.0"));
    request.setTransferTimeout(10000);

    QNetworkReply* reply = m_networkAccessManager->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleConnectivityResult(reply);
    });
}

void TradePage::scheduleConnectivityRetry(int delay_ms, const QString& reason)
{
    if (m_webViewLoadStarted || !m_connectivityRetryTimer) {
        return;
    }

    LogTradeDiagnostic(QStringLiteral("TradePage: scheduling connectivity retry in %1 ms reason=%2")
                           .arg(delay_ms)
                           .arg(reason));
    updateStatusLabel(TradeRetryingAfterNetworkErrorText());
    m_connectivityRetryTimer->start(delay_ms);
}

void TradePage::ensureWebViewLoaded()
{
    if (m_webViewLoadStarted) {
        return;
    }
    m_webViewLoadStarted = true;

    QWebEngineView* view = new QWebEngineView(this);
    m_view = view;
    QWebEngineProfile* profile = QWebEngineProfile::defaultProfile();
    TradeWebEnginePage* page = new TradeWebEnginePage(profile, view);
    view->setPage(page);
    m_layout->addWidget(view);
    LogTradeWebEngineState(view, page, "after-page-created");

    connect(view, &QWebEngineView::loadStarted, this, [this, view, page]() {
        updateStatusLabel(tr("Loading Trade widget..."));
        LogTradeDiagnostic(QStringLiteral("TradePage: WebEngine load started viewUrl=%1 pageUrl=%2 requestedUrl=%3")
                               .arg(view->url().toString())
                               .arg(page->url().toString())
                               .arg(page->requestedUrl().toString()));
        LogTradeWebEngineState(view, page, "loadStarted");
    });
    connect(view, &QWebEngineView::loadProgress, this, [view, page](int progress) {
        LogTradeDiagnostic(QStringLiteral("TradePage: WebEngine load progress=%1 viewUrl=%2 pageUrl=%3 requestedUrl=%4 renderPid=%5")
                               .arg(progress)
                               .arg(view->url().toString())
                               .arg(page->url().toString())
                               .arg(page->requestedUrl().toString())
                               .arg(page->renderProcessPid()));
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view, page](bool ok) {
        LogTradeDiagnostic(QStringLiteral("TradePage: WebEngine load finished ok=%1 viewUrl=%2 pageUrl=%3 requestedUrl=%4 renderPid=%5")
                               .arg(ok)
                               .arg(view->url().toString())
                               .arg(page->url().toString())
                               .arg(page->requestedUrl().toString())
                               .arg(page->renderProcessPid()));
        LogTradeWebEngineState(view, page, "loadFinished");
        if (!m_statusLabel) {
            return;
        }
        if (ok) {
            m_statusLabel->hide();
        } else {
            updateStatusLabel(TradeLoadFailureText());
        }
    });
#if QT_VERSION >= 0x060200
    connect(page, &QWebEnginePage::loadingChanged, this, [this, view, page](const QWebEngineLoadingInfo& info) {
        LogTradeDiagnostic(QStringLiteral("TradePage: loadingChanged status=%1 code=%2 domain=%3 description=%4 url=%5 currentViewUrl=%6 currentPageUrl=%7 requestedUrl=%8 renderPid=%9")
                               .arg(static_cast<int>(info.status()))
                               .arg(info.errorCode())
                               .arg(static_cast<int>(info.errorDomain()))
                               .arg(info.errorString())
                               .arg(info.url().toString())
                               .arg(view->url().toString())
                               .arg(page->url().toString())
                               .arg(page->requestedUrl().toString())
                               .arg(page->renderProcessPid()));
        if (info.status() != QWebEngineLoadingInfo::LoadFailedStatus) {
            return;
        }
        if (info.errorCode() == -106) {
            m_webViewLoadStarted = false;
            if (m_view) {
                m_layout->removeWidget(m_view);
                m_view->deleteLater();
                m_view = nullptr;
            }
            scheduleConnectivityRetry(TradeConnectivityRetryDelayMs(m_connectivityAttemptCount), info.errorString());
            return;
        }
        updateStatusLabel(TradeLoadFailureText());
    });
#endif
    connect(page, &QWebEnginePage::urlChanged, this, [](const QUrl& url) {
        LogTradeDiagnostic(QStringLiteral("TradePage: url changed to %1").arg(url.toString()));
    });
    connect(page, &QWebEnginePage::renderProcessPidChanged, this, [view, page]() {
        LogTradeDiagnostic(QStringLiteral("TradePage: render process pid changed to %1 viewUrl=%2 pageUrl=%3 requestedUrl=%4")
                               .arg(page->renderProcessPid())
                               .arg(view->url().toString())
                               .arg(page->url().toString())
                               .arg(page->requestedUrl().toString()));
        LogTradeWebEngineState(view, page, "renderProcessPidChanged");
    });
    connect(page, &QWebEnginePage::navigationRequested, this, [](QWebEngineNavigationRequest& request) {
        LogTradeDiagnostic(QStringLiteral("TradePage: navigationRequested url=%1 mainFrame=%2 navigationType=%3")
                               .arg(request.url().toString())
                               .arg(request.isMainFrame())
                               .arg(static_cast<int>(request.navigationType())));
    });
    connect(page, &QWebEnginePage::titleChanged, this, [](const QString& title) {
        LogTradeDiagnostic(QStringLiteral("TradePage: title changed to %1").arg(title));
    });
    connect(page, &QWebEnginePage::iconUrlChanged, this, [](const QUrl& url) {
        LogTradeDiagnostic(QStringLiteral("TradePage: iconUrl changed to %1").arg(url.toString()));
    });
    connect(page, &QWebEnginePage::windowCloseRequested, this, []() {
        LogTradeDiagnostic(QStringLiteral("TradePage: windowCloseRequested"));
    });
    connect(page, &QWebEnginePage::newWindowRequested, this, [](QWebEngineNewWindowRequest& request) {
        LogTradeDiagnostic(QStringLiteral("TradePage: newWindowRequested url=%1 destination=%2")
                               .arg(request.requestedUrl().toString())
                               .arg(static_cast<int>(request.destination())));
    });
    connect(page, &QWebEnginePage::certificateError, this, [](const QWebEngineCertificateError& error) {
        LogTradeDiagnostic(QStringLiteral("TradePage: certificateError type=%1 url=%2 description=%3 overridable=%4")
                               .arg(static_cast<int>(error.type()))
                               .arg(error.url().toString())
                               .arg(error.description())
                               .arg(error.isOverridable()));
    });
    connect(page, &QWebEnginePage::fullScreenRequested, this, [](QWebEngineFullScreenRequest request) {
        LogTradeDiagnostic(QStringLiteral("TradePage: fullscreen requested toggle=%1").arg(request.toggleOn()));
        request.reject();
    });
#if QT_VERSION >= 0x060800
    connect(page, &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        const auto permissionType = permission.permissionType();
        const bool is_capture_permission =
            permissionType == QWebEnginePermission::PermissionType::MediaAudioCapture ||
            permissionType == QWebEnginePermission::PermissionType::MediaVideoCapture ||
            permissionType == QWebEnginePermission::PermissionType::DesktopVideoCapture ||
            permissionType == QWebEnginePermission::PermissionType::DesktopAudioVideoCapture;
        if (!is_capture_permission) {
            return;
        }

        LogTradeDiagnostic(QStringLiteral("TradePage: denying WebEngine capture permission type=%1 origin=%2")
                               .arg(static_cast<int>(permissionType))
                               .arg(permission.origin().toString()));
        permission.deny();
        updateStatusLabel(TradeCaptureBlockedText());
    });
    connect(page, &QWebEnginePage::desktopMediaRequested, this, [this](const QWebEngineDesktopMediaRequest& request) {
        LogTradeDiagnostic(QStringLiteral("TradePage: canceling desktop media request from embedded trade widget"));
        request.cancel();
        updateStatusLabel(TradeCaptureBlockedText());
    });
#else
    connect(page, &QWebEnginePage::featurePermissionRequested, this,
            [this, page](const QUrl& securityOrigin, QWebEnginePage::Feature feature) {
        const bool is_capture_feature =
            feature == QWebEnginePage::MediaAudioCapture ||
            feature == QWebEnginePage::MediaVideoCapture ||
            feature == QWebEnginePage::DesktopVideoCapture ||
            feature == QWebEnginePage::DesktopAudioVideoCapture;
        if (!is_capture_feature) {
            return;
        }

        LogTradeDiagnostic(QStringLiteral("TradePage: denying WebEngine capture feature=%1 origin=%2")
                               .arg(static_cast<int>(feature))
                               .arg(securityOrigin.toString()));
        page->setFeaturePermission(
            securityOrigin, feature, QWebEnginePage::PermissionDeniedByUser);
        updateStatusLabel(TradeCaptureBlockedText());
    });
#endif
    connect(view, &QWebEngineView::renderProcessTerminated, this,
            [this, view, page](QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode) {
        LogTradeDiagnostic(QStringLiteral("TradePage: renderer terminated status=%1 exitCode=%2 currentUrl=%3 requestedUrl=%4 pid=%5")
                               .arg(static_cast<int>(terminationStatus))
                               .arg(exitCode)
                               .arg(view->url().toString())
                               .arg(page->requestedUrl().toString())
                               .arg(page->renderProcessPid()));
        LogTradeWebEngineState(view, page, "renderProcessTerminated");
        const bool should_retry = !page->requestedUrl().isEmpty() && page->url().isEmpty();
        if (should_retry) {
            m_webViewLoadStarted = false;
            if (m_view) {
                m_layout->removeWidget(m_view);
                m_view->deleteLater();
                m_view = nullptr;
            }
            scheduleConnectivityRetry(TradeConnectivityRetryDelayMs(m_connectivityAttemptCount),
                                      QStringLiteral("renderer terminated before page load completed"));
            return;
        }
        updateStatusLabel(TradeLoadFailureText());
    });
    const QUrl load_url = TradeWidgetUrl();
    LogTradeDiagnostic(QStringLiteral("TradePage: initiating view->load with url=%1 debugOverride=%2")
                           .arg(load_url.toString())
                           .arg(qEnvironmentVariable("VERGE_TRADEPAGE_DEBUG_URL")));
    view->load(load_url);
}

void TradePage::handleConnectivityResult(QNetworkReply* reply)
{
    if (!reply) {
        return;
    }

    const auto error = reply->error();
    const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString reply_error = reply->errorString();
    const QString final_url = reply->url().toString();

    LogTradeDiagnostic(QStringLiteral("TradePage: connectivity probe finished error=%1 httpStatus=%2 errorString=%3 finalUrl=%4")
                           .arg(static_cast<int>(error))
                           .arg(http_status)
                           .arg(reply_error)
                           .arg(final_url));

    const bool probe_ok =
        error == QNetworkReply::NoError ||
        (http_status >= 200 && http_status < 400) ||
        error == QNetworkReply::ContentAccessDenied ||
        error == QNetworkReply::ContentOperationNotPermittedError;

    reply->deleteLater();

    if (probe_ok) {
        updateStatusLabel(tr("Loading Trade widget..."));
        ensureWebViewLoaded();
        return;
    }

    scheduleConnectivityRetry(TradeConnectivityRetryDelayMs(m_connectivityAttemptCount), reply_error);
}
#endif
