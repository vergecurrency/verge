// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_TRADEPAGE_H
#define VERGE_QT_TRADEPAGE_H

#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
class QNetworkAccessManager;
class QNetworkReply;
class QWebEngineView;
#endif

class TradePage : public QWidget
{
public:
    explicit TradePage(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void ensureInitialized();
    void updateStatusLabel(const QString& text);

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
    void startConnectivityProbe();
    void scheduleConnectivityRetry(int delay_ms, const QString& reason);
    void ensureWebViewLoaded();
    void handleConnectivityResult(QNetworkReply* reply);
#endif

    QVBoxLayout* m_layout{nullptr};
    QLabel* m_statusLabel{nullptr};
    bool m_initialized{false};

#if defined(HAVE_QTWEBENGINEWIDGETS) || defined(QT_WEBENGINEWIDGETS_LIB)
    QPointer<QWebEngineView> m_view;
    QTimer* m_connectivityRetryTimer{nullptr};
    QNetworkAccessManager* m_networkAccessManager{nullptr};
    bool m_webViewLoadStarted{false};
    int m_connectivityAttemptCount{0};
#endif
};

#endif // VERGE_QT_TRADEPAGE_H
