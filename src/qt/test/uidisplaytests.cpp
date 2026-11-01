// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/uidisplaytests.h>

#include <qt/numerictablewidgetitem.h>
#include <qt/transactionstatusdelegate.h>

#include <QHeaderView>
#include <QImage>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTest>

#include <limits>

void UIDisplayTests::confirmationBadges()
{
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    const auto check = [&](TransactionStatus::Status status, qint64 depth,
                           const QString& text, bool confirmed) {
        model.setData(index, status, TransactionTableModel::StatusRole);
        model.setData(index, depth, TransactionTableModel::DepthRole);
        QCOMPARE(TransactionStatusDelegate::badgeText(index), text);
        QCOMPARE(TransactionStatusDelegate::isConfirmed(index), confirmed);
    };

    check(TransactionStatus::Unconfirmed, 0, QStringLiteral("Unconfirmed"), false);
    check(TransactionStatus::Offline, 0, QStringLiteral("Unconfirmed"), false);
    check(TransactionStatus::OpenUntilBlock, 0, QStringLiteral("Unconfirmed"), false);
    check(TransactionStatus::OpenUntilDate, 0, QStringLiteral("Unconfirmed"), false);
    check(TransactionStatus::Confirming, 1, QStringLiteral("Confirmed"), true);
    check(TransactionStatus::Confirmed, 6, QStringLiteral("Confirmed"), true);
    check(TransactionStatus::Immature, 1, QStringLiteral("Confirmed"), true);
    check(TransactionStatus::MaturesWarning, 1, QStringLiteral("Confirmed"), true);
    check(TransactionStatus::Offline, 2, QStringLiteral("Confirmed"), true);
    check(TransactionStatus::Conflicted, -1, QStringLiteral("Conflicted"), false);
    check(TransactionStatus::Abandoned, 0, QStringLiteral("Abandoned"), false);
    check(TransactionStatus::NotAccepted, 0, QStringLiteral("Not accepted"), false);
    check(TransactionStatus::Confirmed, 0, QStringLiteral("Unconfirmed"), false);
}

void UIDisplayTests::badgeRendering()
{
    QStandardItemModel model(1, 1);
    const QModelIndex index = model.index(0, 0);
    QFont font;
    font.setPointSize(11);
    model.setData(index, TransactionStatus::Confirmed, TransactionTableModel::StatusRole);
    model.setData(index, 1, TransactionTableModel::DepthRole);

    const QSize size = TransactionStatusDelegate::badgeSize(index, font);
    QVERIFY(size.width() <= TransactionStatusDelegate::maximumBadgeSize(font).width());
    QVERIFY(size.width() >= QFontMetrics(font).horizontalAdvance(QStringLiteral("Confirmed")) + 20);
    QImage image(size + QSize(8, 8), QImage::Format_ARGB32);
    const QRect rect(QPoint(4, 4), size);
    const auto render = [&]() {
        image.fill(Qt::transparent);
        QPainter painter(&image);
        TransactionStatusDelegate::paintBadge(&painter, rect, index, font);
    };
    render();
    QCOMPARE(image.pixelColor(rect.left() + 5, rect.center().y()), QColor("#15372a"));
    QCOMPARE(image.pixelColor(0, 0).alpha(), 0);

    model.setData(index, TransactionStatus::Unconfirmed, TransactionTableModel::StatusRole);
    model.setData(index, 0, TransactionTableModel::DepthRole);
    render();
    QCOMPARE(image.pixelColor(rect.left() + 5, rect.center().y()), QColor("#421f2a"));

    font.setPointSize(24);
    const QSize larger = TransactionStatusDelegate::badgeSize(index, font);
    QVERIFY(larger.width() > size.width());
    QVERIFY(larger.height() > size.height());
}

void UIDisplayTests::numericBondSorting()
{
    QTableWidget table(4, 4);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.setHorizontalHeaderLabels(QStringList() << QStringLiteral("Amount")
        << QStringLiteral("Confirmations") << QStringLiteral("Blocks to unlock")
        << QStringLiteral("Outpoint"));
    const qint64 values[4][3] = {
        {10000000000, 1000, 100}, {2000000000, 9, 10},
        {9000000000, 80, 2}, {20000000000000000, 2, std::numeric_limits<qint64>::max()}
    };
    const QString amounts[] = {QStringLiteral("10,000 XVG"), QStringLiteral("2,000 XVG"),
        QStringLiteral("9,000 XVG"), QStringLiteral("20,000,000,000 XVG")};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 3; ++column) {
            const QString text = column == 0 ? amounts[row] : column == 2 && row == 3
                ? QStringLiteral("Pending") : QString::number(values[row][column]);
            auto* item = new NumericTableWidgetItem(text, values[row][column]);
            item->setData(Qt::UserRole, row);
            item->setData(Qt::UserRole + 1, row + 10);
            item->setData(Qt::UserRole + 2, true);
            table.setItem(row, column, item);
        }
        table.setItem(row, 3, new QTableWidgetItem(QString::number(row)));
    }
    table.setSortingEnabled(true);
    table.sortByColumn(0, Qt::DescendingOrder);
    table.selectRow(1);
    const int selected = table.item(1, 0)->data(Qt::UserRole).toInt();
    const auto verify = [&](int column, Qt::SortOrder order) {
        for (int row = 0; row < 4; ++row) {
            const int id = table.item(row, 0)->data(Qt::UserRole).toInt();
            QCOMPARE(table.item(row, 3)->text(), QString::number(id));
            QCOMPARE(table.item(row, 0)->data(Qt::UserRole + 1).toInt(), id + 10);
            QVERIFY(table.item(row, 0)->data(Qt::UserRole + 2).toBool());
            if (row > 0) {
                const qint64 previous = table.item(row - 1, column)->data(NumericTableWidgetItem::SortValueRole).toLongLong();
                const qint64 current = table.item(row, column)->data(NumericTableWidgetItem::SortValueRole).toLongLong();
                QVERIFY(order == Qt::AscendingOrder ? previous <= current : previous >= current);
            }
        }
        const QModelIndexList selection = table.selectionModel()->selectedRows();
        QCOMPARE(selection.size(), 1);
        QCOMPARE(table.item(selection.front().row(), 0)->data(Qt::UserRole).toInt(), selected);
    };
    table.resize(640, 240);
    table.show();
    QCoreApplication::processEvents();
    for (int column = 0; column < 3; ++column) {
        table.sortItems(column, Qt::AscendingOrder);
        verify(column, Qt::AscendingOrder);
        QHeaderView* header = table.horizontalHeader();
        QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
            QPoint(header->sectionViewportPosition(column) + header->sectionSize(column) / 2,
                   header->height() / 2));
        QCOMPARE(header->sortIndicatorSection(), column);
        QCOMPARE(header->sortIndicatorOrder(), Qt::DescendingOrder);
        verify(column, Qt::DescendingOrder);
    }
}
