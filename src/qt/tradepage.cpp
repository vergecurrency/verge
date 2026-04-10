// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/tradepage.h>

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <logging.h>

#include <QLabel>
#include <QDebug>
#include <QDir>
#include <QShowEvent>
#include <QStandardPaths>
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
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>
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

static bool TradeDisableWebGlForDiagnostics()
{
    return qEnvironmentVariableIsEmpty("VERGE_TRADEPAGE_DISABLE_WEBGL")
        ? true
        : qEnvironmentVariableIntValue("VERGE_TRADEPAGE_DISABLE_WEBGL") != 0;
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

static void InstallTradeCaptureGuards(QWebEnginePage* page)
{
    QWebEngineScript script;
    script.setName(QStringLiteral("verge-trade-capture-guard"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setRunsOnSubFrames(true);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setSourceCode(QString::fromLatin1(R"JS(
(() => {
  const block = (name) => () => Promise.reject(new DOMException(
    name + ' is disabled in this application',
    'NotAllowedError'
  ));
  const install = () => {
    const mediaDevices = navigator.mediaDevices;
    if (!mediaDevices) return;
    try {
      Object.defineProperty(mediaDevices, 'getDisplayMedia', {
        configurable: true,
        enumerable: true,
        writable: true,
        value: block('getDisplayMedia')
      });
    } catch (_) {
      mediaDevices.getDisplayMedia = block('getDisplayMedia');
    }
  };
  install();
  document.addEventListener('readystatechange', install, { once: false });
})();
)JS"));
    page->scripts().insert(script);
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

void TradePage::ensureInitialized()
{
    if (m_initialized) {
        return;
    }
    m_initialized = true;

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
    QWebEngineView* view = new QWebEngineView(this);
    QWebEngineProfile* profile = QWebEngineProfile::defaultProfile();
#if defined(Q_OS_MAC)
    const QString app_data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString trade_profile_dir = QDir(app_data_dir).filePath(QStringLiteral("trade-webengine"));
    const QString cache_dir = QDir(trade_profile_dir).filePath(QStringLiteral("cache"));
    QDir().mkpath(cache_dir);
    profile = new QWebEngineProfile(QStringLiteral("TradePageProfile"), view);
    profile->setPersistentStoragePath(trade_profile_dir);
    profile->setCachePath(cache_dir);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
    LogTradeDiagnostic(QStringLiteral("TradePage: created dedicated macOS profile storage=%1 cache=%2 offTheRecord=%3")
                           .arg(profile->persistentStoragePath())
                           .arg(profile->cachePath())
                           .arg(profile->isOffTheRecord()));
#endif
    TradeWebEnginePage* page = new TradeWebEnginePage(profile, view);
#if defined(Q_OS_MAC)
    InstallTradeCaptureGuards(page);
    page->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    if (TradeDisableWebGlForDiagnostics()) {
        page->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, false);
        page->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, false);
        LogTradeDiagnostic(QStringLiteral("TradePage: macOS disabled WebGL and accelerated 2D canvas"));
    }
#endif
    view->setPage(page);
    LogTradeWebEngineState(view, page, "after-page-created");

    connect(view, &QWebEngineView::loadStarted, this, [this, view, page]() {
        if (m_statusLabel) {
            m_statusLabel->setText(tr("Loading Trade widget..."));
            m_statusLabel->show();
        }
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
            m_statusLabel->setText(TradeLoadFailureText());
            m_statusLabel->show();
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
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
            if (m_statusLabel) {
                m_statusLabel->setText(TradeLoadFailureText());
                m_statusLabel->show();
            }
        }
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
        if (m_statusLabel) {
            m_statusLabel->setText(TradeCaptureBlockedText());
            m_statusLabel->show();
        }
    });
    connect(page, &QWebEnginePage::desktopMediaRequested, this, [this](const QWebEngineDesktopMediaRequest& request) {
        LogTradeDiagnostic(QStringLiteral("TradePage: canceling desktop media request from embedded trade widget"));
        request.cancel();
        if (m_statusLabel) {
            m_statusLabel->setText(TradeCaptureBlockedText());
            m_statusLabel->show();
        }
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
        if (m_statusLabel) {
            m_statusLabel->setText(TradeCaptureBlockedText());
            m_statusLabel->show();
        }
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
        if (m_statusLabel) {
            m_statusLabel->setText(TradeLoadFailureText());
            m_statusLabel->show();
        }
    });
    const QUrl load_url = TradeWidgetUrl();
    LogTradeDiagnostic(QStringLiteral("TradePage: initiating view->load with url=%1 debugOverride=%2 disableWebGl=%3")
                           .arg(load_url.toString())
                           .arg(qEnvironmentVariable("VERGE_TRADEPAGE_DEBUG_URL"))
                           .arg(TradeDisableWebGlForDiagnostics()));
    view->load(load_url);
    m_layout->addWidget(view);
#else
    if (m_statusLabel) {
        m_statusLabel->setText(
            tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."));
    }
#endif
}
