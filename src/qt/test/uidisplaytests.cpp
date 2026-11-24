// Copyright (c) 2026 Verge
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/test/uidisplaytests.h>

#include <qt/numerictablewidgetitem.h>
#include <qt/transactionstatusdelegate.h>
#include <qt/overviewpage.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/vergeunits.h>
#include <qt/walletmodel.h>
#include <qt/addresstablemodel.h>
#include <qt/receivecoinsdialog.h>
#include <qt/receiverequestdialog.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/vergeamountfield.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <chainparams.h>
#include <key_io.h>
#include <pos/pqcrypto.h>
#include <validation.h>
#include <test/setup_common.h>
#include <wallet/wallet.h>

#include <QHeaderView>
#include <QImage>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTest>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QStandardItemModel>

#include <limits>
#include <memory>

void UIDisplayTests::pqReceiveFlow()
{
    if (!pos::experimental::MLDSA44Available()) QSKIP("ML-DSA-44 is unavailable");
    const std::string network = Params().NetworkIDString();
    BasicTestingSetup setup(CBaseChainParams::REGTEST);
    CBlockIndex genesis;
    struct Restore {
        std::string network;
        CBlockIndex* tip;
        ~Restore() { chainActive.SetTip(tip); SelectParams(network); }
    } restore{network, chainActive.Tip()};
    SelectParams(CBaseChainParams::REGTEST);
    genesis.nHeight = 0;
    chainActive.SetTip(&genesis);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const fs::path directory = fs::path(temporary.path().toStdString()) / "wallet";
    fs::create_directories(directory);
    auto wallet = std::make_shared<CWallet>("pq-receive-test", WalletDatabase::Create(directory));
    bool first_run = false;
    QVERIFY(wallet->LoadWallet(first_run) == DBErrors::LOAD_OK);
    {
        // Production initialization creates keys before exposing an encryptable wallet.
        LOCK(wallet->cs_wallet);
        WalletBatch batch(wallet->GetDBHandle());
        QVERIFY(wallet->GenerateNewKey(batch).IsValid());
    }
    auto node = interfaces::MakeNode();
    OptionsModel options(*node);
    std::unique_ptr<const PlatformStyle> style(PlatformStyle::instantiate("other"));
    WalletModel model(interfaces::MakeWallet(wallet), *node, style.get(), &options);
    ReceiveCoinsDialog page(style.get());
    page.setModel(&model);
    auto* types = page.findChild<QComboBox*>("addressType");
    auto* messaging = page.findChild<QCheckBox*>("allowMessaging");
    QVERIFY(types && messaging);
    const int pq = types->findText("Post-quantum (PQ)");
    const int legacy = types->findText("Legacy");
    const int stealth = types->findText("Stealth");
    QVERIFY(pq >= 0 && legacy >= 0 && stealth >= 0);
    QCOMPARE(types->currentIndex(), pq);
    QVERIFY(!messaging->isEnabled());
    types->setCurrentIndex(legacy);
    QVERIFY(messaging->isEnabled());
    messaging->setChecked(true);
    types->setCurrentIndex(stealth);
    QVERIFY(!messaging->isEnabled());
    QVERIFY(!messaging->isChecked());
    types->setCurrentIndex(pq);

    // Direct callers cannot generate PQ receiving credentials on an inactive network.
    UpdatePoSPostQuantumForTesting(false);
    std::string address, error;
    QVERIFY(!model.wallet().createPQReceiveAddress("inactive", address, error));
    QVERIFY(address.empty());
    QVERIFY(!wallet->HasPQKeys());
    QMetaObject::invokeMethod(&page, "updatePQAvailability", Qt::DirectConnection,
        Q_ARG(interfaces::WalletBalances, model.wallet().getBalances()));
    QVERIFY(types->currentIndex() != pq);
    QVERIFY(!(types->model()->flags(types->model()->index(pq, 0)) & Qt::ItemIsEnabled));
    UpdatePoSPostQuantumForTesting(true);
    QMetaObject::invokeMethod(&page, "updatePQAvailability", Qt::DirectConnection,
        Q_ARG(interfaces::WalletBalances, model.wallet().getBalances()));
    QCOMPARE(types->currentIndex(), pq);
    QVERIFY(types->model()->flags(types->model()->index(pq, 0)) & Qt::ItemIsEnabled);

    const SecureString passphrase{"pq-receive-test"};
    QVERIFY(wallet->EncryptWallet(passphrase));
    QVERIFY(wallet->IsLocked());
    QVERIFY(!model.wallet().createPQReceiveAddress("locked", address, error));
    QVERIFY(address.empty());
    QString reason;
    QVERIFY(model.getAddressTableModel()->addPQRow("cancel unlock", reason).isEmpty());
    QVERIFY(!wallet->HasPQKeys());
    QObject::connect(&model, &WalletModel::requireUnlock, [&wallet, &passphrase]() {
        QVERIFY(wallet->Unlock(passphrase));
    });
    auto* history = model.getRecentRequestsTableModel();
    QCOMPARE(history->rowCount({}), 0);

    QTimer dialogs;
    dialogs.setInterval(10);
    bool save = false;
    bool cancel_file = false;
    bool unexpected = false;
    int ticks = 0;
    const QString backup = temporary.path() + "/pq-backup.dat";
    QObject::connect(&dialogs, &QTimer::timeout, [&]() {
        ++ticks;
        QWidget* modal = QApplication::activeModalWidget();
        if (ticks > 300) {
            unexpected = true;
            if (auto* dialog = qobject_cast<QDialog*>(modal)) dialog->reject();
        } else if (auto* question = qobject_cast<QMessageBox*>(modal)) {
            auto* button = question->button(save ? QMessageBox::Save : QMessageBox::Cancel);
            if (button) button->click();
            else {
                qWarning() << "Unexpected PQ receive dialog:" << question->windowTitle() << question->text();
                unexpected = true;
                question->accept();
            }
        } else if (auto* file = qobject_cast<QFileDialog*>(modal)) {
            if (cancel_file) file->reject();
            else {
                file->selectFile(backup);
                static_cast<QDialog*>(file)->accept();
            }
        }
    });
    dialogs.start();
    QVERIFY(QMetaObject::invokeMethod(&page, "on_receiveButton_clicked", Qt::DirectConnection));
    QVERIFY(!unexpected);
    QCOMPARE(history->rowCount({}), 0);
    QVERIFY(page.findChildren<ReceiveRequestDialog*>().isEmpty());
    QVERIFY(wallet->IsLocked());
    QVERIFY(wallet->HasPQKeys());

    save = true;
    cancel_file = true;
    ticks = 0;
    QVERIFY(QMetaObject::invokeMethod(&page, "on_receiveButton_clicked", Qt::DirectConnection));
    QVERIFY(!unexpected);
    QCOMPARE(history->rowCount({}), 0);
    QVERIFY(page.findChildren<ReceiveRequestDialog*>().isEmpty());
    cancel_file = false;
    ticks = 0;
    page.findChild<VERGEAmountField*>("ReceiveAmountField")->setValue(123 * COIN);
    QVERIFY(QMetaObject::invokeMethod(&page, "on_receiveButton_clicked", Qt::DirectConnection));
    dialogs.stop();
    QVERIFY(!unexpected);
    QVERIFY(fs::exists(fs::path(backup.toStdString())));
    QVERIFY(wallet->IsLocked());
    QCOMPARE(history->rowCount({}), 1);
    const auto& recipient = history->entry(0).recipient;
    const auto destination = DecodeDestination(recipient.address.toStdString());
    QVERIFY(boost::get<PQKeyHash>(&destination));
    QVERIFY(wallet->HavePQKey(boost::get<PQKeyHash>(destination)));
    QCOMPARE(recipient.amount, CAmount(123 * COIN));
    QCOMPARE(model.getAddressTableModel()->purposeForAddress(recipient.address), QString("receive"));
    const auto requests = page.findChildren<ReceiveRequestDialog*>();
    QCOMPARE(requests.size(), 1);
    auto* details = requests[0]->findChild<QTextEdit*>("ReceiveRequestDetails");
    QVERIFY(details && details->toPlainText().contains(recipient.address));
    SendCoinsRecipient parsed;
    QVERIFY(GUIUtil::parseVERGEURI(GUIUtil::formatVERGEURI(recipient), &parsed));
    QCOMPARE(parsed.address, recipient.address);
    QCOMPARE(parsed.amount, recipient.amount);
#ifdef USE_QRCODE
    auto* qr = requests[0]->findChild<QRImageWidget*>("ReceiveRequestQRCode");
    QVERIFY(qr && !qr->exportImage().isNull());
#endif
    requests[0]->close();

    for (int points : {10, 12, 16}) {
        QFont font = page.font();
        font.setPointSize(points);
        page.setFont(font);
        page.resize(QSize(1000, 800).expandedTo(page.minimumSizeHint()));
        page.show();
        QCoreApplication::processEvents();
        QVERIFY(types->width() >= types->sizeHint().width());
        QVERIFY(types->geometry().bottom() < messaging->geometry().top());
        const QString screenshots = qEnvironmentVariable("VERGE_QT_TEST_SCREENSHOTS");
        if (!screenshots.isEmpty()) QVERIFY(page.grab().save(screenshots + QString("/pq-receive-%1.png").arg(points)));
    }
    wallet->GetDBHandle().Flush(true);
}

void UIDisplayTests::pqOverviewBalances()
{
    const std::string network = Params().NetworkIDString();
    struct RestoreNetwork {
        std::string network;
        ~RestoreNetwork() { SelectParams(network); }
    } restore{network};
    SelectParams(CBaseChainParams::REGTEST);
    auto node = interfaces::MakeNode();
    OptionsModel options(*node);
    std::unique_ptr<const PlatformStyle> style(PlatformStyle::instantiate("other"));
    auto wallet = std::make_shared<CWallet>("pq-ui-test", WalletDatabase::CreateMock());
    WalletModel model(interfaces::MakeWallet(wallet), *node, style.get(), &options);
    OverviewPage page(style.get());
    page.setWalletModel(&model);
    interfaces::WalletBalances balances;
    balances.balance = 10 * COIN;
    balances.unconfirmed_balance = 2 * COIN;
    balances.immature_balance = 3 * COIN;
    balances.staked_balance = 4 * COIN;
    balances.legacy_balance = 7 * COIN;
    balances.pq_balance = 12 * COIN;
    balances.pq_enabled = true;
    page.setBalance(balances);
    auto* legacy = page.findChild<QLabel*>("labelLegacy");
    auto* pq = page.findChild<QLabel*>("labelPQ");
    auto* button = page.findChild<QPushButton*>("migrateLegacyButton");
    // Balance value widgets have themed object names after construction.
    const auto values = page.findChildren<QLabel*>("OverviewSecondaryBalance");
    QVERIFY(!values.empty());
    const int unit = options.getDisplayUnit();
    const QString legacy_text = VERGEUnits::formatWithUnit(unit, balances.legacy_balance, false, VERGEUnits::separatorAlways);
    const QString pq_text = VERGEUnits::formatWithUnit(unit, balances.pq_balance, false, VERGEUnits::separatorAlways);
    for (QLabel* label : values) {
        if (label->text() == legacy_text) legacy = label;
        if (label->text() == pq_text) pq = label;
    }
    QVERIFY(legacy && pq && button);
    QVERIFY(legacy != pq);
    QCOMPARE(legacy->text(), legacy_text);
    QCOMPARE(pq->text(), pq_text);
    const QString total_text = VERGEUnits::formatWithUnit(unit, 19 * COIN, false, VERGEUnits::separatorAlways);
    bool have_total = false;
    for (const auto* label : page.findChildren<QLabel*>("OverviewPrimaryBalance")) {
        have_total |= label->text() == total_text;
    }
    QVERIFY(have_total);
    QVERIFY(button->isEnabled());
    const auto previous = balances;
    balances.pq_enabled = false;
    QVERIFY(balances.balanceChanged(previous));
    page.setBalance(balances);
    QVERIFY(!button->isEnabled());
    balances.pq_enabled = true;
    balances.legacy_balance = 0;
    page.setBalance(balances);
    QVERIFY(!button->isEnabled());
    balances.legacy_balance = 7 * COIN;
    page.setBalance(balances);
    QVERIFY(button->isEnabled());
    bool refused = false;
    QTimer::singleShot(0, [&refused]() {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            refused = true;
            message->accept();
        }
    });
    button->click(); // No active chain or peers: refuse without generating keys or sending.
    QVERIFY(refused);
    QVERIFY(!wallet->HasPQKeys());
    QVERIFY(wallet->mapWallet.empty());

    for (int points : {10, 12, 16}) {
        QFont font = page.font();
        font.setPointSize(points);
        page.setFont(font);
        for (int width : {900, 1280, 1920}) {
            page.resize(QSize(width, 800).expandedTo(page.minimumSizeHint()));
            page.show();
            QCoreApplication::processEvents();
            QVERIFY(button->width() >= button->minimumSizeHint().width());
            QVERIFY(legacy->geometry().bottom() < pq->geometry().top());
            QVERIFY(pq->mapTo(&page, pq->rect().bottomRight()).y() < button->mapTo(&page, QPoint(0, 0)).y());
            QVERIFY(legacy->fontMetrics().horizontalAdvance(legacy->text()) <= legacy->contentsRect().width());
            QVERIFY(pq->fontMetrics().horizontalAdvance(pq->text()) <= pq->contentsRect().width());
            const QString screenshots = qEnvironmentVariable("VERGE_QT_TEST_SCREENSHOTS");
            if (!screenshots.isEmpty()) {
                QVERIFY(page.grab().save(screenshots + QString("/pq-overview-%1-%2.png").arg(points).arg(width)));
            }
        }
    }
}

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
