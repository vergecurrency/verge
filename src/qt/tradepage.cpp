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
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineView>
#if QT_VERSION >= 0x060200
#include <QtWebEngineCore/QWebEngineLoadingInfo>
#endif
#endif

namespace {
#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
static const char* STEALTHEX_WIDGET_URL = "https://stealthex.io/widget/1c5c64de-0ac0-4b79-a393-e447de460c42";
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
        if (url.isEmpty() || url.scheme() == "about" || url.scheme() == "data") {
            return true;
        }
        const QString host = url.host().toLower();
        return host == "stealthex.io" || host.endsWith(".stealthex.io");
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
    view->load(QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL)));
    m_layout->addWidget(view);
#else
    if (m_statusLabel) {
        m_statusLabel->setText(
            tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."));
    }
#endif
}
