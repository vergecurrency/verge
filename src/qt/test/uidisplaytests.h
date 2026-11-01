// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_TEST_UIDISPLAYTESTS_H
#define VERGE_QT_TEST_UIDISPLAYTESTS_H

#include <QObject>

class UIDisplayTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void confirmationBadges();
    void badgeRendering();
    void numericBondSorting();
};

#endif // VERGE_QT_TEST_UIDISPLAYTESTS_H
