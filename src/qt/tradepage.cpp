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
#include <QVBoxLayout>

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
#include <QtWebEngineCore/QWebEngineFullScreenRequest>
#include <QtWebEngineCore/QWebEnginePage>
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
#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
static const char* STEALTHEX_WIDGET_URL = "https://stealthex.io/widget/1c5c64de-0ac0-4b79-a393-e447de460c42";

static QString TradeCaptureBlockedText()
{
    return QObject::tr("Trade widget requested screen, camera, or microphone access, which is disabled in VERGE.");
}

static QString BuildTradeWrapperHtml()
{
    return QString::fromLatin1(R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>VERGE Trade</title>
  <style>
    html, body {
      margin: 0;
      width: 100%;
      height: 100%;
      background: #0f1521;
      overflow: hidden;
    }
    iframe {
      border: 0;
      width: 100%;
      height: 100%;
      display: block;
      background: #0f1521;
    }
  </style>
</head>
<body>
  <iframe
    src="%1"
    referrerpolicy="strict-origin-when-cross-origin"
    sandbox="allow-same-origin allow-scripts allow-forms allow-popups allow-downloads"
    allow="clipboard-read; clipboard-write">
  </iframe>
</body>
</html>
)HTML").arg(QString::fromLatin1(STEALTHEX_WIDGET_URL));
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
    try {
      Object.defineProperty(mediaDevices, 'getUserMedia', {
        configurable: true,
        enumerable: true,
        writable: true,
        value: block('getUserMedia')
      });
    } catch (_) {
      mediaDevices.getUserMedia = block('getUserMedia');
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
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(type);

        // Restrict top-level navigation to StealthEX hostnames.
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
    InstallTradeCaptureGuards(page);
    page->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, false);
    page->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    view->setPage(page);

    connect(view, &QWebEngineView::loadStarted, this, [this]() {
        if (m_statusLabel) {
            m_statusLabel->setText(tr("Loading Trade widget..."));
            m_statusLabel->show();
        }
        qWarning() << "TradePage: WebEngine load started";
    });
    connect(view, &QWebEngineView::loadFinished, this, [this](bool ok) {
        qWarning() << "TradePage: WebEngine load finished ok=" << ok;
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
    connect(page, &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo& info) {
        if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
            qWarning() << "TradePage: load failed code=" << info.errorCode()
                       << "domain=" << info.errorDomain()
                       << "description=" << info.errorString()
                       << "url=" << info.url();
            if (m_statusLabel) {
                m_statusLabel->setText(TradeLoadFailureText());
                m_statusLabel->show();
            }
        }
    });
#endif
    connect(page, &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        qWarning() << "TradePage: rejecting fullscreen request from embedded trade widget";
        request.reject();
        if (m_statusLabel) {
            m_statusLabel->setText(TradeCaptureBlockedText());
            m_statusLabel->show();
        }
    });
#if QT_VERSION >= 0x060800
    connect(page, &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {
        const auto permissionType = permission.permissionType();
        const bool is_capture_permission =
            permissionType == QWebEnginePermission::PermissionType::MediaAudioCapture ||
            permissionType == QWebEnginePermission::PermissionType::MediaVideoCapture ||
            permissionType == QWebEnginePermission::PermissionType::MediaAudioVideoCapture ||
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
            feature == QWebEnginePage::MediaAudioVideoCapture ||
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
            [this](QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode) {
        qWarning() << "TradePage: renderer terminated status=" << static_cast<int>(terminationStatus)
                   << "exitCode=" << exitCode;
        if (m_statusLabel) {
            m_statusLabel->setText(TradeLoadFailureText());
            m_statusLabel->show();
        }
    });
#if defined(Q_OS_MAC)
    view->setUrl(QUrl(QStringLiteral("about:blank")));
    page->setHtml(BuildTradeWrapperHtml(), QUrl(QStringLiteral("https://verge.local/")));
#else
    view->load(QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL)));
#endif
    m_layout->addWidget(view);
#else
    if (m_statusLabel) {
        m_statusLabel->setText(
            tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."));
    }
#endif
}
