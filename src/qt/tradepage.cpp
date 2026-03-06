// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/tradepage.h>

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <QLabel>
#include <QVBoxLayout>

#ifdef HAVE_QTWEBENGINEWIDGETS
#include <QWebEnginePage>
#include <QWebEngineView>
#endif

namespace {
#ifdef HAVE_QTWEBENGINEWIDGETS
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
#endif
} // namespace

TradePage::TradePage(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef HAVE_QTWEBENGINEWIDGETS
    QWebEngineView* view = new QWebEngineView(this);
    view->setPage(new TradeWebEnginePage(view));
    view->load(QUrl(QString::fromLatin1(STEALTHEX_WIDGET_URL)));
    layout->addWidget(view);
#else
    QLabel* message = new QLabel(
        tr("Trade widget is unavailable in this build because Qt WebEngine support is not enabled."), this);
    message->setWordWrap(true);
    message->setMargin(16);
    layout->addWidget(message);
#endif
}
