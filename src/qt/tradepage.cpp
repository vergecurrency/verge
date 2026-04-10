// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/tradepage.h>

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <QLabel>
#include <QDebug>
#include <QShowEvent>
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
static QString TradeCaptureBlockedText()
{
    return QObject::tr("Trade widget requested screen sharing, which is disabled in VERGE on this platform.");
}

static void LogTradeWebEngineState(QWebEngineView* view, QWebEnginePage* page, const char* stage)
{
    if (!view || !page) {
        qWarning() << "TradePage:" << stage << "view/page missing";
        return;
    }

    QWebEngineProfile* profile = page->profile();
    QWebEngineSettings* settings = page->settings();
    qWarning() << "TradePage:" << stage
               << "viewVisible=" << view->isVisible()
               << "viewSize=" << view->size()
               << "viewUrl=" << view->url()
               << "pageUrl=" << page->url()
               << "requestedUrl=" << page->requestedUrl()
               << "renderPid=" << page->renderProcessPid()
               << "canGoBack=" << page->history()->canGoBack()
               << "canGoForward=" << page->history()->canGoForward();
    if (profile) {
        qWarning() << "TradePage:" << stage
                   << "profileStorageName=" << profile->storageName()
                   << "httpUserAgent=" << profile->httpUserAgent()
                   << "cachePath=" << profile->cachePath()
                   << "persistentStoragePath=" << profile->persistentStoragePath()
                   << "offTheRecord=" << profile->isOffTheRecord();
    }
    if (settings) {
        qWarning() << "TradePage:" << stage
                   << "setting JavascriptEnabled="
                   << settings->testAttribute(QWebEngineSettings::JavascriptEnabled)
                   << "LocalContentCanAccessRemoteUrls="
                   << settings->testAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls)
                   << "LocalContentCanAccessFileUrls="
                   << settings->testAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls)
                   << "JavascriptCanOpenWindows="
                   << settings->testAttribute(QWebEngineSettings::JavascriptCanOpenWindows)
                   << "PluginsEnabled="
                   << settings->testAttribute(QWebEngineSettings::PluginsEnabled)
                   << "FullScreenSupportEnabled="
                   << settings->testAttribute(QWebEngineSettings::FullScreenSupportEnabled)
                   << "ScreenCaptureEnabled="
                   << settings->testAttribute(QWebEngineSettings::ScreenCaptureEnabled)
                   << "WebGLEnabled="
                   << settings->testAttribute(QWebEngineSettings::WebGLEnabled)
                   << "Accelerated2dCanvasEnabled="
                   << settings->testAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled)
                   << "PlaybackRequiresUserGesture="
                   << settings->testAttribute(QWebEngineSettings::PlaybackRequiresUserGesture);
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
    TradeWebEnginePage* page = new TradeWebEnginePage(view);
#if defined(Q_OS_MAC)
    InstallTradeCaptureGuards(page);
    page->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
#endif
    view->setPage(page);
    LogTradeWebEngineState(view, page, "after-page-created");

    connect(view, &QWebEngineView::loadStarted, this, [this, view, page]() {
        if (m_statusLabel) {
            m_statusLabel->setText(tr("Loading Trade widget..."));
            m_statusLabel->show();
        }
        qWarning() << "TradePage: WebEngine load started"
                   << "viewUrl=" << view->url()
                   << "pageUrl=" << page->url()
                   << "requestedUrl=" << page->requestedUrl();
        LogTradeWebEngineState(view, page, "loadStarted");
    });
    connect(view, &QWebEngineView::loadProgress, this, [view, page](int progress) {
        qWarning() << "TradePage: WebEngine load progress=" << progress
                   << "viewUrl=" << view->url()
                   << "pageUrl=" << page->url()
                   << "requestedUrl=" << page->requestedUrl()
                   << "renderPid=" << page->renderProcessPid();
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view, page](bool ok) {
        qWarning() << "TradePage: WebEngine load finished ok=" << ok
                   << "viewUrl=" << view->url()
                   << "pageUrl=" << page->url()
                   << "requestedUrl=" << page->requestedUrl()
                   << "renderPid=" << page->renderProcessPid();
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
        qWarning() << "TradePage: loadingChanged status=" << static_cast<int>(info.status())
                   << "code=" << info.errorCode()
                   << "domain=" << info.errorDomain()
                   << "description=" << info.errorString()
                   << "url=" << info.url()
                   << "currentViewUrl=" << view->url()
                   << "currentPageUrl=" << page->url()
                   << "requestedUrl=" << page->requestedUrl()
                   << "renderPid=" << page->renderProcessPid();
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
            if (m_statusLabel) {
                m_statusLabel->setText(TradeLoadFailureText());
                m_statusLabel->show();
            }
        }
    });
#endif
    connect(page, &QWebEnginePage::urlChanged, this, [](const QUrl& url) {
        qWarning() << "TradePage: url changed to" << url;
    });
    connect(page, &QWebEnginePage::renderProcessPidChanged, this, [view, page]() {
        qWarning() << "TradePage: render process pid changed to" << page->renderProcessPid()
                   << "viewUrl=" << view->url()
                   << "pageUrl=" << page->url()
                   << "requestedUrl=" << page->requestedUrl();
        LogTradeWebEngineState(view, page, "renderProcessPidChanged");
    });
    connect(page, &QWebEnginePage::navigationRequested, this, [](QWebEngineNavigationRequest& request) {
        qWarning() << "TradePage: navigationRequested url=" << request.url()
                   << "mainFrame=" << request.isMainFrame()
                   << "navigationType=" << static_cast<int>(request.navigationType());
    });
    connect(page, &QWebEnginePage::titleChanged, this, [](const QString& title) {
        qWarning() << "TradePage: title changed to" << title;
    });
    connect(page, &QWebEnginePage::iconUrlChanged, this, [](const QUrl& url) {
        qWarning() << "TradePage: iconUrl changed to" << url;
    });
    connect(page, &QWebEnginePage::windowCloseRequested, this, []() {
        qWarning() << "TradePage: windowCloseRequested";
    });
    connect(page, &QWebEnginePage::newWindowRequested, this, [](QWebEngineNewWindowRequest& request) {
        qWarning() << "TradePage: newWindowRequested url=" << request.requestedUrl()
                   << "destination=" << static_cast<int>(request.destination());
    });
    connect(page, &QWebEnginePage::certificateError, this, [](const QWebEngineCertificateError& error) {
        qWarning() << "TradePage: certificateError type=" << static_cast<int>(error.type())
                   << "url=" << error.url()
                   << "description=" << error.description()
                   << "overridable=" << error.isOverridable();
    });
    connect(page, &QWebEnginePage::fullScreenRequested, this, [](QWebEngineFullScreenRequest request) {
        qWarning() << "TradePage: fullscreen requested toggle=" << request.toggleOn();
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

        qWarning() << "TradePage: denying WebEngine capture permission type="
                   << static_cast<int>(permissionType)
                   << "origin=" << permission.origin();
        permission.deny();
        if (m_statusLabel) {
            m_statusLabel->setText(TradeCaptureBlockedText());
            m_statusLabel->show();
        }
    });
    connect(page, &QWebEnginePage::desktopMediaRequested, this, [this](const QWebEngineDesktopMediaRequest& request) {
        qWarning() << "TradePage: canceling desktop media request from embedded trade widget";
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

        qWarning() << "TradePage: denying WebEngine capture feature="
                   << static_cast<int>(feature)
                   << "origin=" << securityOrigin;
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
        qWarning() << "TradePage: renderer terminated status=" << static_cast<int>(terminationStatus)
                   << "exitCode=" << exitCode
                   << "currentUrl=" << view->url()
                   << "requestedUrl=" << page->requestedUrl()
                   << "pid=" << page->renderProcessPid();
        LogTradeWebEngineState(view, page, "renderProcessTerminated");
        if (m_statusLabel) {
            m_statusLabel->setText(TradeLoadFailureText());
            m_statusLabel->show();
        }
    });
    qWarning() << "TradePage: initiating view->load with url="
               << QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL));
    view->load(QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL)));
    m_layout->addWidget(view);
#else
    if (m_statusLabel) {
        m_statusLabel->setText(
            tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."));
    }
#endif
}
