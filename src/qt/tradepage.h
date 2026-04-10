// Copyright (c) 2026 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_TRADEPAGE_H
#define VERGE_QT_TRADEPAGE_H

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class TradePage : public QWidget
{
public:
    explicit TradePage(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void ensureInitialized();

    QVBoxLayout* m_layout{nullptr};
    QLabel* m_statusLabel{nullptr};
    bool m_initialized{false};
};

#endif // VERGE_QT_TRADEPAGE_H
