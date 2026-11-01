// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_TRANSACTIONSTATUSDELEGATE_H
#define VERGE_QT_TRANSACTIONSTATUSDELEGATE_H

#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>

#include <QApplication>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyledItemDelegate>

class TransactionStatusDelegate : public QStyledItemDelegate
{
public:
    explicit TransactionStatusDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    static bool isConfirmed(const QModelIndex& index)
    {
        const int status = index.data(TransactionTableModel::StatusRole).toInt();
        return index.data(TransactionTableModel::DepthRole).toLongLong() > 0 &&
            status != TransactionStatus::Conflicted &&
            status != TransactionStatus::Abandoned &&
            status != TransactionStatus::NotAccepted;
    }

    static QString badgeText(const QModelIndex& index)
    {
        switch (index.data(TransactionTableModel::StatusRole).toInt()) {
        case TransactionStatus::Conflicted:
            return QCoreApplication::translate("TransactionTableModel", "Conflicted");
        case TransactionStatus::Abandoned:
            return QCoreApplication::translate("TransactionTableModel", "Abandoned");
        case TransactionStatus::NotAccepted:
            return QCoreApplication::translate("TransactionTableModel", "Not accepted");
        default:
            return isConfirmed(index)
                ? QCoreApplication::translate("TransactionTableModel", "Confirmed")
                : QCoreApplication::translate("TransactionTableModel", "Unconfirmed");
        }
    }

    static QSize badgeSize(const QModelIndex& index, const QFont& font)
    {
        const QFontMetrics metrics(font);
        return QSize(metrics.horizontalAdvance(badgeText(index)) + 20, metrics.height() + 8);
    }

    static QSize maximumBadgeSize(const QFont& font)
    {
        const QFontMetrics metrics(font);
        const QStringList labels = {
            QCoreApplication::translate("TransactionTableModel", "Confirmed"),
            QCoreApplication::translate("TransactionTableModel", "Unconfirmed"),
            QCoreApplication::translate("TransactionTableModel", "Conflicted"),
            QCoreApplication::translate("TransactionTableModel", "Abandoned"),
            QCoreApplication::translate("TransactionTableModel", "Not accepted")
        };
        int width = 0;
        for (const QString& label : labels) {
            width = qMax(width, metrics.horizontalAdvance(label));
        }
        return QSize(width + 22, metrics.height() + 8);
    }

    static void paintBadge(QPainter* painter, const QRect& rect,
                           const QModelIndex& index, const QFont& font)
    {
        if (rect.isEmpty()) return;
        painter->save();
        painter->setClipRect(rect);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setFont(font);
        const bool confirmed = isConfirmed(index);
        painter->setPen(confirmed ? QColor("#4fd38a") : QColor("#ff6f7d"));
        painter->setBrush(confirmed ? QColor("#15372a") : QColor("#421f2a"));
        painter->drawRoundedRect(rect.adjusted(0, 0, -1, -1), 4, 4);
        const QRect textRect = rect.adjusted(10, 0, -10, 0);
        const QString text = QFontMetrics(font).elidedText(
            badgeText(index), Qt::ElideRight, qMax(0, textRect.width()));
        painter->drawText(textRect, Qt::AlignCenter, text);
        painter->restore();
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem background(option);
        initStyleOption(&background, index);
        background.text.clear();
        background.icon = QIcon();
        const QStyle* style = option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, option.widget);

        QSize size = badgeSize(index, option.font);
        size.setWidth(qMin(size.width(), qMax(0, option.rect.width() - 16)));
        size.setHeight(qMin(size.height(), qMax(0, option.rect.height() - 8)));
        const QRect rect(option.rect.left() + 8,
                         option.rect.center().y() - size.height() / 2,
                         size.width(), size.height());
        paintBadge(painter, rect, index, option.font);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        return badgeSize(index, option.font) + QSize(18, 12);
    }
};

#endif // VERGE_QT_TRANSACTIONSTATUSDELEGATE_H
