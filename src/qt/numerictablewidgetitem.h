// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_NUMERICTABLEWIDGETITEM_H
#define VERGE_QT_NUMERICTABLEWIDGETITEM_H

#include <QTableWidgetItem>

class NumericTableWidgetItem : public QTableWidgetItem
{
public:
    static constexpr int SortValueRole = Qt::UserRole + 3;

    NumericTableWidgetItem(const QString& text, qint64 value) : QTableWidgetItem(text)
    {
        setData(SortValueRole, value);
    }

    bool operator<(const QTableWidgetItem& other) const override
    {
        return data(SortValueRole).toLongLong() < other.data(SortValueRole).toLongLong();
    }
};

#endif // VERGE_QT_NUMERICTABLEWIDGETITEM_H
