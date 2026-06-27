// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/vergegui.h>

#if defined(_WIN32) && defined(__clang__)
#pragma comment(linker, "/alternatename:??$memswap@$0BA@@=??$memswap@$0BA@@")
#endif
/* VERGE_MSVC_PROTOBUF_MEMSWAP_ALIAS */

#include <chainparams.h>
#include <qt/clientmodel.h>
#include <fs.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/intro.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/splashscreen.h>
#include <qt/utilitydialog.h>
#include <qt/winshutdownmonitor.h>

#ifdef ENABLE_WALLET
#include <qt/paymentserver.h>
#include <qt/walletmodel.h>
#endif

#include <init.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <rpc/server.h>
#include <ui_interface.h>
#include <uint256.h>
#include <util/system.h>
#include <warnings.h>

#include <walletinitinterface.h>

#include <memory>
#include <stdint.h>

#include <boost/thread.hpp>

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QSslConfiguration>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif

// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)
Q_DECLARE_METATYPE(uint256)

static void InitMessage(const std::string &message)
{
    LogPrintf("init message: %s\n", message);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("verge-core", psz).toStdString();
}

static QString GetLangTerritory()
{
    QSettings settings;
    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = QLocale::system().name();
    // 2) Language from QSettings
    QString lang_territory_qsettings = settings.value("language", "").toString();
    if(!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;
    // 3) -lang command line argument
    lang_territory = QString::fromStdString(gArgs.GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}

static QString BuildAppStyleSheet()
{
    return QStringLiteral(R"(
QMainWindow, QDialog {
    background-color: #121722;
    color: #d7dbe5;
}

QWidget#VERGEGUI, QWidget#RPCConsole {
    border: 1px solid #2a3347;
}

QWidget {
    color: #d7dbe5;
    font-family: "Inter", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 14px;
}

QWidget#OverviewPage {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QFrame#OverviewBalanceCard,
QFrame#OverviewTransactionsCard {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 16px;
}

QFrame#OverviewBalanceCard:hover,
QFrame#OverviewTransactionsCard:hover,
QFrame#ReceiveRequestCard:hover,
QFrame#ReceiveHistoryCard:hover,
QFrame#SendCoinControlCard:hover,
QFrame#SendFeeCard:hover,
QFrame#SendFeeSelectionCard:hover,
QFrame#SendCoinsEntryCard:hover,
QFrame#TransactionDateRangeCard:hover,
QFrame#CoinControlToolbarCard:hover {
    border-color: #3a4763;
}

QLabel#OverviewSectionTitle {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 17px;
    font-weight: 600;
}

QLabel#OverviewPrimaryBalance {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 18px;
    font-weight: 600;
}

QLabel#OverviewSecondaryBalance {
    color: #d7dbe5;
    font-weight: 600;
}

QLabel#OverviewMutedLabel {
    color: #8e9bb3;
}

QLabel#OverviewAlertBanner {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop:0 #6c3c16, stop:1 #9d6c1d);
    color: #fff4da;
    border: 1px solid #bc8b35;
    border-radius: 10px;
    padding: 8px 10px;
}

QFrame#OverviewDivider {
    color: #263149;
    background-color: #263149;
}

QListView {
    outline: none;
}

QListView::item {
    margin: 0;
}

QListView::item:hover {
    background-color: rgba(43, 60, 90, 0.18);
    border-radius: 12px;
}

QWidget#TransactionsPage,
QWidget#TransactionViewPage {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QComboBox#TransactionFilterSelect,
QComboBox#TransactionFilterCompact,
QLineEdit#TransactionFilterSearch,
QLineEdit#TransactionFilterAmount,
QDateTimeEdit#TransactionRangeDate {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    color: #d7dbe5;
    min-height: 34px;
    padding: 4px 10px;
}

QComboBox#TransactionFilterCompact {
    padding: 0;
    min-width: 24px;
    max-width: 24px;
}

QLineEdit#TransactionFilterSearch {
    min-width: 260px;
}

QFrame#TransactionDateRangeCard {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 14px;
}

QLabel#TransactionRangeLabel {
    color: #8e9bb3;
}

QPushButton#TransactionsExportButton {
    background-color: #26324a;
    border: 1px solid #3c5279;
    border-radius: 10px;
    color: #ecf1ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#TransactionsExportButton:hover {
    background-color: #304163;
}

QTableView#transactionView {
    background-color: rgba(22, 29, 42, 0.96);
    alternate-background-color: #1b2231;
    border: 1px solid #2a3347;
    border-radius: 16px;
    color: #d7dbe5;
    gridline-color: #243147;
    selection-background-color: #2e4a7b;
    selection-color: #f5f8ff;
}

QHeaderView#TransactionTableHeader::section,
QTableView#transactionView QHeaderView::section {
    background-color: #171d2a;
    border: 0;
    border-bottom: 1px solid #2a3347;
    color: #8e9bb3;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
    padding: 10px 12px;
}

QTableView#transactionView::item {
    border-bottom: 1px solid #202a3d;
    padding: 8px 10px;
}

QTableView#transactionView::item:hover {
    background-color: rgba(46, 74, 123, 0.24);
}

QDialog#ReceiveCoinsDialog,
QWidget#ReceiveCoinsDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QFrame#ReceiveRequestCard,
QFrame#ReceiveHistoryCard {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 16px;
}

QLabel#ReceiveIntroText {
    color: #8e9bb3;
}

QLabel#ReceiveSectionTitle {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 17px;
    font-weight: 600;
}

QLineEdit#ReceiveTextField,
QAbstractSpinBox#ReceiveAmountField,
QCheckBox#ReceiveStealthToggle {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    color: #d7dbe5;
    min-height: 34px;
    padding: 4px 10px;
}

QCheckBox#ReceiveStealthToggle {
    padding-left: 10px;
    padding-right: 10px;
}

QPushButton#ReceivePrimaryButton {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2d6f51, stop:1 #3a9768);
    border: 1px solid #49a26f;
    border-radius: 10px;
    color: #f3fff8;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
    min-height: 38px;
    padding: 6px 16px;
}

QPushButton#ReceivePrimaryButton:hover {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #33815d, stop:1 #43a975);
}

QPushButton#ReceivePrimaryButton:pressed {
    background-color: #2d6f51;
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#ReceiveSecondaryButton {
    background-color: #223049;
    border: 1px solid #374b72;
    border-radius: 10px;
    color: #e7eefc;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#ReceiveSecondaryButton:hover {
    background-color: #2b3c5a;
}

QTableView#ReceiveHistoryTable {
    background-color: rgba(19, 25, 37, 0.9);
    border: 1px solid #263149;
    border-radius: 14px;
    color: #d7dbe5;
    gridline-color: #223049;
    selection-background-color: #2e4a7b;
    selection-color: #f5f8ff;
}

QHeaderView#ReceiveHistoryHeader::section,
QTableView#ReceiveHistoryTable QHeaderView::section {
    background-color: #171d2a;
    border: 0;
    border-bottom: 1px solid #2a3347;
    color: #8e9bb3;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
    padding: 10px 12px;
}

QTableView#ReceiveHistoryTable::item {
    border-bottom: 1px solid #202a3d;
    padding: 8px 10px;
}

QTableView#ReceiveHistoryTable::item:hover {
    background-color: rgba(46, 74, 123, 0.24);
}

QDialog#TransactionDescDialog,
QDialog#ReceiveRequestDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QTextEdit#TransactionDetailsText,
QTextEdit#ReceiveRequestDetails {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 16px;
    color: #d7dbe5;
    font-family: "Inter", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 14px;
    line-height: 1.5;
    padding: 10px 12px;
}

QTextEdit#TransactionDetailsText a,
QTextEdit#ReceiveRequestDetails a {
    color: #8fc0ff;
}

QLabel#ReceiveRequestQRCode {
    background-color: #05070c;
    border: 1px solid #2a3347;
    border-radius: 18px;
    color: #d7dbe5;
    padding: 18px;
}

QPushButton#ReceiveRequestActionButton,
QPushButton#DialogSecondaryButton {
    background-color: #223049;
    border: 1px solid #374b72;
    border-radius: 10px;
    color: #e7eefc;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#ReceiveRequestActionButton:hover,
QPushButton#DialogSecondaryButton:hover {
    background-color: #2b3c5a;
}

QPushButton#ReceiveRequestActionButton:pressed,
QPushButton#DialogSecondaryButton:pressed {
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#DialogPrimaryButton {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3167d1, stop:1 #4890ff);
    border: 1px solid #5b8df0;
    border-radius: 10px;
    color: #f5f9ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
    min-height: 36px;
    padding: 6px 16px;
}

QPushButton#DialogPrimaryButton:hover {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b75e4, stop:1 #5a9eff);
}

QPushButton#DialogPrimaryButton:pressed {
    background-color: #326ad6;
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#DialogPrimaryButton:disabled {
    background-color: #1a2436;
    border-color: #2d3a53;
    color: #7f8ca6;
}

QMessageBox,
QMessageBox#SendConfirmationDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QMessageBox QLabel,
QMessageBox#SendConfirmationDialog QLabel {
    color: #d7dbe5;
}

QMessageBox QLabel#qt_msgbox_label,
QMessageBox#SendConfirmationDialog QLabel#qt_msgbox_label {
    color: #f3f6ff;
    font-size: 14px;
    line-height: 1.5;
}

QMessageBox QLabel#qt_msgboxex_icon_label,
QMessageBox QLabel#qt_msgbox_icon_label {
    min-width: 40px;
}

QDialog#AddressBookPage {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QLabel#AddressBookIntroText {
    color: #8e9bb3;
}

QLineEdit#AddressBookSearchField,
QLineEdit#AddressBookFormField,
QValidatedLineEdit#AddressBookFormField {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    color: #d7dbe5;
    min-height: 34px;
    padding: 4px 10px;
}

QTableView#AddressBookTable {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 16px;
    color: #d7dbe5;
    gridline-color: #243147;
    selection-background-color: #2e4a7b;
    selection-color: #f5f8ff;
}

QHeaderView#AddressBookTableHeader::section,
QTableView#AddressBookTable QHeaderView::section {
    background-color: #171d2a;
    border: 0;
    border-bottom: 1px solid #2a3347;
    color: #8e9bb3;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
    padding: 10px 12px;
}

QTableView#AddressBookTable::item {
    border-bottom: 1px solid #202a3d;
    padding: 8px 10px;
}

QTableView#AddressBookTable::item:hover {
    background-color: rgba(46, 74, 123, 0.24);
}

QPushButton#AddressBookPrimaryButton {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2d6f51, stop:1 #3a9768);
    border: 1px solid #49a26f;
    border-radius: 10px;
    color: #f3fff8;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
    min-height: 36px;
    padding: 6px 16px;
}

QPushButton#AddressBookPrimaryButton:hover {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #33815d, stop:1 #43a975);
}

QPushButton#AddressBookPrimaryButton:pressed {
    background-color: #2d6f51;
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#AddressBookSecondaryButton {
    background-color: #223049;
    border: 1px solid #374b72;
    border-radius: 10px;
    color: #e7eefc;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#AddressBookSecondaryButton:hover {
    background-color: #2b3c5a;
}

QPushButton#AddressBookSecondaryButton:pressed {
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#AddressBookDangerButton {
    background-color: #5c2330;
    border: 1px solid #944053;
    border-radius: 10px;
    color: #ffecef;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#AddressBookDangerButton:hover {
    background-color: #732d3c;
}

QPushButton#AddressBookDangerButton:pressed {
    padding-top: 7px;
    padding-bottom: 5px;
}

QDialog#EditAddressDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QDialog#CoinControlDialog,
QDialog#OpenURIDialog,
QDialog#SignVerifyMessageDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QFrame#CoinControlToolbarCard {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 14px;
}

QPushButton#CoinControlPrimaryButton {
    background-color: #223049;
    border: 1px solid #374b72;
    border-radius: 10px;
    color: #e7eefc;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#CoinControlPrimaryButton:hover {
    background-color: #2b3c5a;
}

QPushButton#CoinControlPrimaryButton:pressed {
    padding-top: 7px;
    padding-bottom: 5px;
}

QRadioButton#CoinControlModeToggle {
    color: #d7dbe5;
    spacing: 8px;
}

QLabel#CoinControlLockedLabel {
    color: #8e9bb3;
    font-weight: 600;
}

QLabel#CoinControlStatLabel {
    color: #8e9bb3;
}

QLabel#CoinControlStatValue {
    color: #f3f6ff;
    font-weight: 600;
}

QTreeWidget#CoinControlTree {
    background-color: rgba(22, 29, 42, 0.96);
    alternate-background-color: #1b2231;
    border: 1px solid #2a3347;
    border-radius: 16px;
    color: #d7dbe5;
    outline: none;
    selection-background-color: #2e4a7b;
    selection-color: #f5f8ff;
}

QHeaderView#CoinControlTreeHeader::section,
QTreeWidget#CoinControlTree QHeaderView::section {
    background-color: #171d2a;
    border: 0;
    border-bottom: 1px solid #2a3347;
    color: #8e9bb3;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
    padding: 10px 12px;
}

QTreeWidget#CoinControlTree::item {
    border-bottom: 1px solid #202a3d;
    padding: 7px 10px;
}

QTreeWidget#CoinControlTree::item:hover {
    background-color: rgba(46, 74, 123, 0.24);
}

QLineEdit#OpenUriField,
QValidatedLineEdit#OpenUriField,
QLineEdit#SignVerifyField,
QValidatedLineEdit#SignVerifyField,
QPlainTextEdit#SignVerifyMessageBox {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    color: #d7dbe5;
    min-height: 34px;
    padding: 4px 10px;
}

QLabel#SignVerifyIntroText {
    color: #8e9bb3;
}

QLabel#SignVerifySectionLabel {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
}

QLabel#SignVerifyStatusLabel {
    font-weight: 600;
}

QToolButton#SignVerifyIconButton,
QPushButton#SignVerifyIconButton {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    min-width: 34px;
    min-height: 34px;
    padding: 4px;
}

QToolButton#SignVerifyIconButton:hover,
QPushButton#SignVerifyIconButton:hover {
    background-color: #223049;
    border-color: #415783;
}

QTabWidget#SignVerifyTabs::pane {
    border: 1px solid #2a3347;
    border-radius: 16px;
    background-color: rgba(22, 29, 42, 0.96);
    top: -1px;
}

QTabWidget#SignVerifyTabs QTabBar::tab {
    background-color: #1a2231;
    border: 1px solid #2e394f;
    border-bottom: none;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
    padding: 9px 14px;
    margin-right: 4px;
}

QTabWidget#SignVerifyTabs QTabBar::tab:selected {
    background-color: #26324a;
}

QDialog#SendCoinsDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1, stop:0 #101621, stop:0.55 #121a28, stop:1 #0f1520);
}

QFrame#SendCoinControlCard,
QFrame#SendFeeCard,
QFrame#SendFeeSelectionCard,
QWidget#SendEntriesContainer,
QFrame#SendCoinsEntryCard {
    background-color: rgba(22, 29, 42, 0.96);
    border: 1px solid #2a3347;
    border-radius: 16px;
}

QScrollArea#SendEntriesScrollArea {
    background: transparent;
    border: 0;
}

QLabel#SendSectionTitle {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 16px;
    font-weight: 600;
}

QLabel#SendMutedLabel,
QLabel#SendEntryMessage {
    color: #8e9bb3;
}

QLabel#SendWarningLabel {
    color: #ffb8bf;
}

QLabel#SendBalanceValue {
    color: #f3f6ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 16px;
    font-weight: 600;
}

QValidatedLineEdit#SendTextField,
QValidatedLineEdit#SendEntryField,
QLineEdit#SendEntryField,
QAbstractSpinBox#SendAmountField,
QAbstractSpinBox#SendEntryAmount {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    color: #d7dbe5;
    min-height: 34px;
    padding: 4px 10px;
}

QPushButton#SendPrimaryButton {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3167d1, stop:1 #4890ff);
    border: 1px solid #5b8df0;
    border-radius: 10px;
    color: #f5f9ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
    min-height: 38px;
    padding: 6px 16px;
}

QPushButton#SendPrimaryButton:hover {
    background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3b75e4, stop:1 #5a9eff);
}

QPushButton#SendPrimaryButton:pressed {
    background-color: #326ad6;
    padding-top: 7px;
    padding-bottom: 5px;
}

QPushButton#SendSecondaryButton,
QPushButton#SendEntryUseBalance {
    background-color: #223049;
    border: 1px solid #374b72;
    border-radius: 10px;
    color: #e7eefc;
    min-height: 36px;
    padding: 6px 14px;
}

QPushButton#SendSecondaryButton:hover,
QPushButton#SendEntryUseBalance:hover {
    background-color: #2b3c5a;
}

QPushButton#SendSecondaryButton:pressed,
QPushButton#SendEntryUseBalance:pressed {
    padding-top: 7px;
    padding-bottom: 5px;
}

QToolButton#SendEntryToolButton,
QToolButton#SendEntryDeleteButton {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 10px;
    min-width: 34px;
    min-height: 34px;
    padding: 4px;
}

QToolButton#SendEntryToolButton:hover {
    background-color: #223049;
    border-color: #415783;
}

QToolButton#SendEntryDeleteButton:hover {
    background-color: #6b2734;
    border-color: #a24658;
}

QCheckBox#SendEntryCheckbox {
    color: #d7dbe5;
    spacing: 8px;
}

QWidget#SendCoinControlStats QLabel {
    color: #d7dbe5;
}

QWidget#CustomTitleBar {
    background-color: #0f1521;
    border-bottom: 1px solid #2a3347;
}

QLabel#CustomTitleLabel {
    color: #f2f5ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-weight: 600;
}

QToolBar#ShellToolbar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop:0 #141c2a, stop:0.55 #162032, stop:1 #111a29);
    border: 0;
    border-bottom: 1px solid #2a3347;
    spacing: 10px;
    padding: 10px 14px;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton {
    background: rgba(23, 31, 45, 0.5);
    border: 1px solid rgba(58, 76, 110, 0.32);
    border-radius: 14px;
    color: #dfe6f7;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 14px;
    font-weight: 600;
    icon-size: 18px;
    min-height: 42px;
    padding: 10px 16px;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:hover {
    background: rgba(36, 49, 74, 0.88);
    border-color: #47618f;
    color: #f5f8ff;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:pressed {
    background: rgba(31, 42, 63, 0.98);
    border-color: #5672a8;
    padding-top: 11px;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:checked {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop:0 #284674, stop:1 #325d97);
    border-color: #6b9cff;
    color: #ffffff;
}

QLabel#ShellWalletSelectorLabel {
    color: #94a3bd;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
}

QComboBox#ShellWalletSelector {
    background-color: rgba(20, 28, 40, 0.94);
    border: 1px solid #32425f;
    border-radius: 11px;
    color: #edf2ff;
    min-height: 38px;
    min-width: 180px;
    padding: 4px 12px;
}

QComboBox#ShellWalletSelector:hover {
    border-color: #4b6ea8;
}

QStatusBar#ShellStatusBar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop:0 #111723, stop:1 #141c29);
    border-top: 1px solid #2a3347;
}

QFrame#ShellStatusClusterCard {
    background: rgba(16, 23, 35, 0.94);
    border: 1px solid #2d374a;
    border-radius: 18px;
}

QFrame#ShellSyncStatusCard {
    background: rgba(17, 23, 35, 0.96);
    border: 1px solid #2f3a52;
    border-radius: 16px;
}

QFrame#ShellSyncStatusCard[syncState="connecting"] {
    background: rgba(62, 42, 17, 0.95);
    border-color: #9f7231;
}

QFrame#ShellSyncStatusCard[syncState="syncing"] {
    background: rgba(18, 34, 58, 0.96);
    border-color: #486fb9;
}

QLabel#SyncStatusLabel {
    color: #f4f7ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 12px;
    font-weight: 600;
}

QWidget#WalletSecurityChip,
QWidget#ProxyStatusChip,
QWidget#NetworkStatusChip,
QWidget#ChainStatusChip {
    background: rgba(23, 31, 45, 0.9);
    border: 1px solid #303b51;
    border-radius: 12px;
}

QWidget#WalletSecurityChip {
    padding: 0 2px;
}

QWidget#ProxyStatusChip {
    background: rgba(54, 41, 18, 0.92);
    border-color: #8f6932;
}

QWidget#NetworkStatusChip[networkState="warming"] {
    background: rgba(62, 42, 17, 0.95);
    border-color: #a67833;
}

QWidget#NetworkStatusChip[networkState="live"] {
    background: rgba(14, 58, 48, 0.95);
    border-color: #2f9a75;
}

QWidget#NetworkStatusChip[networkState="offline"] {
    background: rgba(73, 27, 34, 0.95);
    border-color: #a24a58;
}

QWidget#ChainStatusChip[syncState="connecting"] {
    background: rgba(62, 42, 17, 0.95);
    border-color: #a67833;
}

QWidget#ChainStatusChip[syncState="syncing"] {
    background: rgba(18, 34, 58, 0.96);
    border-color: #4e79c9;
}

QWidget#ChainStatusChip[syncState="synced"] {
    background: rgba(14, 58, 48, 0.95);
    border-color: #2f9a75;
}

QLabel#StatusChipText,
QLabel#UnitDisplayStatusText {
    color: #f0f4ff;
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 12px;
    font-weight: 600;
}

QLabel#WalletStatusIcon,
QLabel#StatusChipIcon {
    min-width: 16px;
    min-height: 16px;
}

QMenuBar, QToolBar, QGroupBox::title, QTabBar::tab {
    font-family: "Space Grotesk", "Inter", "Noto Sans", sans-serif;
    font-size: 14px;
}

QToolButton#CustomTitleButton, QToolButton#CustomTitleCloseButton {
    color: #ecf1ff;
    background: #1a2231;
    border: 1px solid #2e394f;
    border-radius: 6px;
    icon-size: 18px;
    min-width: 28px;
    min-height: 22px;
    padding: 1px 6px;
}

QToolButton#CustomTitleButton:hover {
    background: #26324a;
    border-color: #3a4f79;
}

QToolButton#CustomTitleCloseButton:hover {
    background: #8f2f3a;
    border-color: #b44956;
}

QFrame, QGroupBox {
    border-radius: 8px;
}

QMenuBar {
    background-color: #171d2a;
    border-bottom: 1px solid #2a3347;
}

QMenuBar::item {
    background: transparent;
    padding: 6px 10px;
    border-radius: 6px;
}

QMenuBar::item:selected {
    background: #24304a;
}

QMenu {
    background-color: #171d2a;
    border: 1px solid #2a3347;
    border-radius: 8px;
    padding: 6px;
}

QMenu::item {
    padding: 6px 14px;
    border-radius: 6px;
}

QMenu::item:selected {
    background: #2d3d60;
}

QToolBar {
    background-color: #171d2a;
    border: 0;
    border-bottom: 1px solid #2a3347;
    spacing: 6px;
    padding: 4px 8px;
}

QToolBar::separator {
    background: #2a3347;
    width: 1px;
    margin: 4px 6px;
}

QToolButton {
    color: #d7dbe5;
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    icon-size: 48px;
    padding: 6px 10px;
}

QToolButton:hover {
    background: #24304a;
    border-color: #2e3f5f;
}

QToolButton:pressed, QToolButton:checked {
    background: #2d3d60;
    border-color: #3a4f79;
    color: #ecf1ff;
}

QPushButton {
    background-color: #25324b;
    border: 1px solid #3a4c6e;
    border-radius: 8px;
    icon-size: 48px;
    padding: 6px 12px;
}

QPushButton:hover {
    background-color: #2d3d60;
}

QPushButton:pressed {
    background-color: #1f2a3f;
}

QPushButton:focus,
QToolButton:focus,
QComboBox#ShellWalletSelector:focus,
QLineEdit:focus,
QPlainTextEdit:focus,
QTextEdit:focus,
QAbstractSpinBox:focus {
    border-color: #6fa6ff;
}

QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #171d2a;
    border: 1px solid #313c52;
    border-radius: 8px;
    padding: 4px 8px;
    selection-background-color: #3e5f9c;
}

QPlainTextEdit, QTextEdit {
    font-family: "JetBrains Mono", "Fira Code", "DejaVu Sans Mono", monospace;
    font-size: 13px;
}

QAbstractItemView, QTableView, QListView, QTreeView {
    background-color: #151b27;
    alternate-background-color: #1b2231;
    border: 1px solid #2e394f;
    border-radius: 8px;
    gridline-color: #2a3347;
    selection-background-color: #3e5f9c;
    selection-color: #ecf1ff;
}

QHeaderView::section {
    background-color: #1b2231;
    border: 0;
    border-bottom: 1px solid #2a3347;
    padding: 6px;
}

QTabWidget::pane {
    border: 1px solid #2e394f;
    border-radius: 8px;
    top: -1px;
}

QTabBar::tab {
    background-color: #1a2231;
    border: 1px solid #2e394f;
    border-bottom: none;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
    padding: 7px 12px;
    margin-right: 4px;
}

QTabBar::tab:selected {
    background-color: #26324a;
}

QStatusBar {
    background-color: #121722;
    border-top: 1px solid #2a3347;
}

QToolTip {
    background-color: #20283a;
    color: #d7dbe5;
    border: 1px solid #3a4b69;
    border-radius: 6px;
    padding: 4px 6px;
}

QWidget#RPCConsole {
    background-color: #121722;
    color: #d7dbe5;
}

QDialog#HelpMessageDialog {
    background-color: #121722;
    color: #d7dbe5;
}

QDialog[customChrome="true"] {
    background-color: #121722;
    color: #d7dbe5;
    border: 1px solid #2a3347;
}

QMainWindow[customChrome="true"] {
    background-color: #121722;
    border: 1px solid #2a3347;
}

QWidget#ShellWindowControls {
    background: transparent;
}

QToolButton#ShellWindowControlButton,
QToolButton#ShellWindowCloseButton {
    background: #1a2231;
    border: 1px solid #313c52;
    border-radius: 6px;
    padding: 2px;
}

QToolButton#ShellWindowControlButton:hover {
    background: #26324a;
    border-color: #6fa6ff;
}

QToolButton#ShellWindowCloseButton:hover {
    background: #5a1d2a;
    border-color: #ff6b86;
}

QDialog[customChrome="true"] QWidget#CustomDialogContent {
    background-color: #121722;
}

QDialog#HelpMessageDialog QTextEdit,
QDialog#HelpMessageDialog QScrollArea,
QDialog#HelpMessageDialog QWidget#scrollAreaWidgetContents {
    background-color: #171d2a;
    border: 1px solid #2e394f;
    border-radius: 8px;
}

QDialog#HelpMessageDialog QLabel#HelpAboutMessage {
    color: #d7dbe5;
}

QDialog#HelpMessageDialog QLabel#HelpAboutMessage a {
    color: #9bc2ff;
}

QWidget#ShutdownWindow {
    background-color: #121722;
    color: #d7dbe5;
    border: 1px solid #2a3347;
    border-radius: 8px;
}

QWidget#ShutdownWindow QLabel#ShutdownMessage {
    color: #d7dbe5;
}

QWidget#SplashScreen {
    background-color: #121722;
    border: 1px solid #2a3347;
}

QWidget#SendCoinControlStats {
    background: transparent;
    border: 0;
}

QWidget#RPCConsole QTabWidget::pane {
    border: 1px solid #2e394f;
    border-radius: 8px;
    background-color: #151b27;
}

QWidget#RPCConsole QGroupBox {
    background-color: #171d2a;
    border: 1px solid #2e394f;
    border-radius: 8px;
    margin-top: 8px;
    padding-top: 8px;
}

QWidget#RPCConsole QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
    color: #dfe6f7;
}

/* 2026 visual system override */
QMainWindow,
QDialog,
QWidget#OverviewPage,
QWidget#TransactionsPage,
QWidget#TransactionViewPage,
QDialog#ReceiveCoinsDialog,
QWidget#ReceiveCoinsDialog,
QDialog#SendCoinsDialog,
QWidget#SendCoinsDialog,
QWidget#TradePage,
QWidget#GamesPage {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 #0b1114,
        stop: 0.58 #0f171d,
        stop: 1 #121d25);
    color: #e4edf2;
}

QLabel#OverviewSectionTitle,
QLabel#OverviewPrimaryBalance,
QLabel#ReceiveSectionTitle,
QLabel#ShellWalletSelectorLabel,
QLabel#SyncStatusLabel,
QLabel#TradeTitle,
QLabel#GamesTitle,
QLabel#StatusChipText,
QLabel#UnitDisplayStatusText {
    color: #f2f7fa;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
}

QFrame#OverviewBalanceCard,
QFrame#OverviewTransactionsCard,
QFrame#ReceiveRequestCard,
QFrame#ReceiveHistoryCard,
QFrame#SendCoinControlCard,
QFrame#SendFeeCard,
QFrame#SendFeeSelectionCard,
QFrame#SendCoinsEntryCard,
QFrame#TransactionDateRangeCard,
QFrame#CoinControlToolbarCard,
QFrame#ShellStatusClusterCard,
QFrame#ShellSyncStatusCard,
QFrame#TradeHeroCard,
QFrame#TradeContentCard,
QFrame#GamesHeaderCard,
QFrame#GamesLibraryCard,
QWidget#GamesPlayPage {
    background-color: rgba(16, 24, 29, 0.97);
    border: 1px solid #273945;
    border-radius: 18px;
}

QFrame#OverviewBalanceCard:hover,
QFrame#OverviewTransactionsCard:hover,
QFrame#ReceiveRequestCard:hover,
QFrame#ReceiveHistoryCard:hover,
QFrame#SendCoinControlCard:hover,
QFrame#SendFeeCard:hover,
QFrame#SendFeeSelectionCard:hover,
QFrame#SendCoinsEntryCard:hover,
QFrame#TransactionDateRangeCard:hover,
QFrame#CoinControlToolbarCard:hover,
QFrame#TradeHeroCard:hover,
QFrame#TradeContentCard:hover,
QFrame#GamesHeaderCard:hover,
QFrame#GamesLibraryCard:hover {
    border-color: #3d5a69;
}

QToolBar#ShellToolbar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #0d161b,
        stop: 0.58 #101b22,
        stop: 1 #132029);
    border-bottom: 1px solid #243541;
    padding: 12px 16px;
    spacing: 12px;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton {
    background: rgba(15, 24, 29, 0.84);
    border: 1px solid rgba(63, 89, 101, 0.48);
    border-radius: 15px;
    color: #d8e5eb;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    min-height: 42px;
    padding: 10px 16px;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:hover {
    background: rgba(24, 39, 47, 0.95);
    border-color: #4d7282;
    color: #f3f8fa;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:pressed {
    background: rgba(19, 31, 37, 0.98);
    border-color: #5b8799;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:checked {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #197b66,
        stop: 1 #22a987);
    border-color: #66dfb4;
    color: #f7fffb;
}

QStatusBar#ShellStatusBar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #0d151a,
        stop: 1 #101b22);
    border-top: 1px solid #243541;
}

QWidget#WalletSecurityChip,
QWidget#ProxyStatusChip,
QWidget#NetworkStatusChip,
QWidget#ChainStatusChip {
    background: rgba(20, 30, 36, 0.92);
    border: 1px solid #31444f;
    border-radius: 13px;
}

QWidget#ProxyStatusChip {
    background: rgba(68, 45, 18, 0.92);
    border-color: #aa7f3c;
}

QWidget#NetworkStatusChip[networkState="warming"],
QWidget#ChainStatusChip[syncState="connecting"] {
    background: rgba(70, 47, 19, 0.94);
    border-color: #c08d40;
}

QWidget#NetworkStatusChip[networkState="live"],
QWidget#ChainStatusChip[syncState="synced"] {
    background: rgba(14, 63, 52, 0.95);
    border-color: #39b88d;
}

QWidget#NetworkStatusChip[networkState="offline"] {
    background: rgba(77, 29, 35, 0.94);
    border-color: #bd5f6c;
}

QWidget#ChainStatusChip[syncState="syncing"],
QFrame#ShellSyncStatusCard[syncState="syncing"] {
    background: rgba(15, 47, 59, 0.95);
    border-color: #4cb6d6;
}

QFrame#ShellSyncStatusCard[syncState="connecting"] {
    background: rgba(70, 47, 19, 0.94);
    border-color: #c08d40;
}

QPushButton#ReceivePrimaryButton,
QPushButton#SendPrimaryButton,
QPushButton#DialogPrimaryButton,
QPushButton#AddressBookPrimaryButton,
QPushButton#CoinControlPrimaryButton,
QPushButton#GamesBackButton {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #17755f,
        stop: 1 #23a785);
    border: 1px solid #59d6ac;
    border-radius: 12px;
    color: #f5fff9;
    min-height: 38px;
}

QPushButton#ReceivePrimaryButton:hover,
QPushButton#SendPrimaryButton:hover,
QPushButton#DialogPrimaryButton:hover,
QPushButton#AddressBookPrimaryButton:hover,
QPushButton#CoinControlPrimaryButton:hover,
QPushButton#GamesBackButton:hover {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #1b8a70,
        stop: 1 #2ab893);
}

QPushButton#ReceiveSecondaryButton,
QPushButton#SendSecondaryButton,
QPushButton#SendEntryUseBalance,
QPushButton#DialogSecondaryButton,
QPushButton#ReceiveRequestActionButton,
QPushButton#TransactionsExportButton,
QPushButton#AddressBookSecondaryButton {
    background: #132028;
    border: 1px solid #38505e;
    border-radius: 12px;
    color: #e4edf2;
}

QPushButton#ReceiveSecondaryButton:hover,
QPushButton#SendSecondaryButton:hover,
QPushButton#SendEntryUseBalance:hover,
QPushButton#DialogSecondaryButton:hover,
QPushButton#ReceiveRequestActionButton:hover,
QPushButton#TransactionsExportButton:hover,
QPushButton#AddressBookSecondaryButton:hover {
    background: #1a2b35;
    border-color: #4b6878;
}

QPushButton#GamesLauncherButton {
    text-align: left;
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #111b22,
        stop: 1 #15242c);
    border: 1px solid #30434f;
    border-radius: 16px;
    color: #ecf4f7;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 17px;
    font-weight: 600;
    padding: 14px 18px;
}

QPushButton#GamesLauncherButton:hover {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #14242b,
        stop: 1 #19313a);
    border-color: #4f6e7c;
}

QPushButton#GamesLauncherButton:pressed {
    background: #122029;
    border-color: #5a7c8d;
}

QLabel#TradeTitle,
QLabel#GamesTitle {
    font-size: 24px;
    font-weight: 700;
}

QLabel#TradeSubtitle,
QLabel#GamesSubtitle,
QLabel#GamesIntroLabel,
QLabel#GamesHelpLabel {
    color: #8fa5b0;
}

QLabel#TradeStatusLabel {
    background: rgba(22, 56, 48, 0.72);
    border: 1px solid #3b7b67;
    border-radius: 12px;
    color: #dbf8ec;
    padding: 12px 14px;
}

QWidget#GamesStack {
    background: transparent;
    border: 0;
}

QWidget#GamesCanvas,
QWebEngineView#TradeWebView {
    background-color: #091015;
    border: 1px solid #233640;
    border-radius: 16px;
}

QDialog#CoinControlDialog,
QDialog#OpenURIDialog,
QDialog#SignVerifyMessageDialog,
QDialog#TransactionDescDialog,
QDialog#ReceiveRequestDialog,
QDialog#EditAddressDialog,
QDialog#SendConfirmationDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 #0c1216,
        stop: 0.62 #101920,
        stop: 1 #13212a);
    color: #e4edf2;
}

QTextEdit#TransactionDetailsText,
QTextEdit#ReceiveRequestDetails {
    background-color: rgba(14, 22, 27, 0.97);
    border: 1px solid #2b414d;
    border-radius: 16px;
    color: #e3edf2;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    padding: 12px 14px;
}

QTextEdit#TransactionDetailsText a,
QTextEdit#ReceiveRequestDetails a {
    color: #72d8bb;
}

QPushButton#AddressBookDangerButton {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #6c2632,
        stop: 1 #95424f);
    border: 1px solid #cb717d;
    border-radius: 12px;
    color: #fff3f5;
    min-height: 36px;
}

QPushButton#AddressBookDangerButton:hover {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #81303d,
        stop: 1 #ab4e5b);
}

QPushButton#AddressBookDangerButton:pressed {
    background: #6b2833;
}

QFrame#CoinControlToolbarCard {
    background-color: rgba(15, 24, 29, 0.97);
    border: 1px solid #28404b;
    border-radius: 16px;
}

QPushButton#CoinControlPrimaryButton {
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-weight: 600;
}

QRadioButton#CoinControlModeToggle,
QCheckBox#SendEntryCheckbox,
QCheckBox#ReceiveStealthToggle {
    color: #dce8ed;
    spacing: 8px;
}

QLabel#CoinControlLockedLabel,
QLabel#SendMutedLabel,
QLabel#SendEntryMessage,
QLabel#SignVerifyIntroText {
    color: #91a8b2;
}

QLabel#CoinControlStatLabel {
    color: #87a0aa;
}

QLabel#CoinControlStatValue,
QLabel#SendBalanceValue {
    color: #f4fbfd;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-weight: 700;
}

QTreeWidget#CoinControlTree {
    background-color: rgba(13, 20, 25, 0.98);
    alternate-background-color: #132028;
    border: 1px solid #263b46;
    border-radius: 16px;
    color: #dce8ed;
    selection-background-color: #196f60;
    selection-color: #f4fffb;
}

QHeaderView#CoinControlTreeHeader::section,
QTreeWidget#CoinControlTree QHeaderView::section {
    background-color: #132028;
    border: 0;
    border-bottom: 1px solid #263b46;
    color: #93acb7;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 13px;
    font-weight: 600;
    padding: 10px 12px;
}

QTreeWidget#CoinControlTree::item {
    border-bottom: 1px solid #1f313b;
    padding: 7px 10px;
}

QTreeWidget#CoinControlTree::item:hover {
    background-color: rgba(27, 71, 81, 0.26);
}

QLabel#SendSectionTitle,
QLabel#SignVerifySectionLabel {
    color: #f2f8fb;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-size: 16px;
    font-weight: 700;
}

QLabel#SendWarningLabel,
QLabel#SignVerifyStatusLabel {
    color: #f2b8aa;
    font-weight: 600;
}

QValidatedLineEdit#SendTextField,
QValidatedLineEdit#SendEntryField,
QLineEdit#SendEntryField,
QAbstractSpinBox#SendAmountField,
QAbstractSpinBox#SendEntryAmount,
QLineEdit#OpenUriField,
QValidatedLineEdit#OpenUriField,
QLineEdit#SignVerifyField,
QValidatedLineEdit#SignVerifyField,
QPlainTextEdit#SignVerifyMessageBox {
    background-color: #10191f;
    border: 1px solid #30454f;
    border-radius: 12px;
    color: #e4edf2;
    min-height: 36px;
    padding: 5px 10px;
    selection-background-color: #1a7d67;
}

QValidatedLineEdit#SendTextField:focus,
QValidatedLineEdit#SendEntryField:focus,
QLineEdit#SendEntryField:focus,
QAbstractSpinBox#SendAmountField:focus,
QAbstractSpinBox#SendEntryAmount:focus,
QLineEdit#OpenUriField:focus,
QValidatedLineEdit#OpenUriField:focus,
QLineEdit#SignVerifyField:focus,
QValidatedLineEdit#SignVerifyField:focus,
QPlainTextEdit#SignVerifyMessageBox:focus {
    border-color: #65ddb4;
}

QScrollArea#SendEntriesScrollArea,
QWidget#SendEntriesContainer {
    background: transparent;
    border: 0;
}

QToolButton#SendEntryToolButton,
QToolButton#SignVerifyIconButton,
QPushButton#SignVerifyIconButton {
    background-color: #10191f;
    border: 1px solid #30454f;
    border-radius: 11px;
    min-width: 34px;
    min-height: 34px;
}

QToolButton#SendEntryToolButton:hover,
QToolButton#SignVerifyIconButton:hover,
QPushButton#SignVerifyIconButton:hover {
    background-color: #17313a;
    border-color: #4b7381;
}

QToolButton#SendEntryDeleteButton {
    background-color: #10191f;
    border: 1px solid #4e3a3e;
    border-radius: 11px;
    min-width: 34px;
    min-height: 34px;
}

QToolButton#SendEntryDeleteButton:hover {
    background-color: #5a2933;
    border-color: #bf6875;
}

QTabWidget#SignVerifyTabs::pane {
    border: 1px solid #29404b;
    border-radius: 16px;
    background-color: rgba(15, 24, 29, 0.97);
}

QTabWidget#SignVerifyTabs QTabBar::tab {
    background-color: #132028;
    border: 1px solid #2c434e;
    border-bottom: none;
    border-top-left-radius: 11px;
    border-top-right-radius: 11px;
    color: #bdd0d8;
    font-family: "Montserrat", "Noto Sans", "DejaVu Sans", sans-serif;
    font-weight: 600;
    padding: 9px 14px;
}

QTabWidget#SignVerifyTabs QTabBar::tab:selected {
    background-color: #1a6f60;
    border-color: #52cfa5;
    color: #f5fffb;
}

/* 2026 retrowave override */
QMainWindow,
QDialog,
QWidget#OverviewPage,
QWidget#TransactionsPage,
QWidget#TransactionViewPage,
QDialog#ReceiveCoinsDialog,
QWidget#ReceiveCoinsDialog,
QDialog#SendCoinsDialog,
QWidget#SendCoinsDialog,
QWidget#TradePage,
QWidget#GamesPage,
QDialog#AboutQtDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 #050208,
        stop: 0.48 #12051d,
        stop: 1 #1b0a2c);
    color: #faf7ff;
}

QFrame#OverviewBalanceCard,
QFrame#OverviewTransactionsCard,
QFrame#ReceiveRequestCard,
QFrame#ReceiveHistoryCard,
QFrame#SendCoinControlCard,
QFrame#SendFeeCard,
QFrame#SendFeeSelectionCard,
QFrame#SendCoinsEntryCard,
QFrame#TransactionDateRangeCard,
QFrame#CoinControlToolbarCard,
QFrame#ShellStatusClusterCard,
QFrame#ShellSyncStatusCard,
QFrame#TradeHeroCard,
QFrame#TradeContentCard,
QFrame#GamesHeaderCard,
QFrame#GamesLibraryCard,
QWidget#GamesPlayPage,
QWidget#GamesCanvas,
QWebEngineView#TradeWebView {
    background-color: rgba(16, 7, 29, 0.97);
    border: 1px solid #5c2f7f;
    border-radius: 18px;
}

QFrame#OverviewBalanceCard:hover,
QFrame#OverviewTransactionsCard:hover,
QFrame#ReceiveRequestCard:hover,
QFrame#ReceiveHistoryCard:hover,
QFrame#SendCoinControlCard:hover,
QFrame#SendFeeCard:hover,
QFrame#SendFeeSelectionCard:hover,
QFrame#SendCoinsEntryCard:hover,
QFrame#TransactionDateRangeCard:hover,
QFrame#CoinControlToolbarCard:hover,
QFrame#TradeHeroCard:hover,
QFrame#TradeContentCard:hover,
QFrame#GamesHeaderCard:hover,
QFrame#GamesLibraryCard:hover {
    border-color: #b25cff;
}

QLabel,
QLabel#OverviewSectionTitle,
QLabel#OverviewPrimaryBalance,
QLabel#ReceiveSectionTitle,
QLabel#ShellWalletSelectorLabel,
QLabel#SyncStatusLabel,
QLabel#TradeTitle,
QLabel#GamesTitle,
QLabel#StatusChipText,
QLabel#UnitDisplayStatusText,
QLabel#SendSectionTitle,
QLabel#SignVerifySectionLabel {
    color: #faf7ff;
}

QLabel#OverviewMutedLabel,
QLabel#CoinControlLockedLabel,
QLabel#SendMutedLabel,
QLabel#SendEntryMessage,
QLabel#SignVerifyIntroText,
QLabel#CoinControlStatLabel,
QLabel#TradeSubtitle,
QLabel#GamesSubtitle,
QLabel#GamesIntroLabel,
QLabel#GamesHelpLabel {
    color: #c6b2e3;
}

QToolBar#ShellToolbar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #0a0311,
        stop: 0.56 #140621,
        stop: 1 #1e0a31);
    border-bottom: 1px solid #4d236d;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton {
    background: rgba(24, 10, 40, 0.9);
    border: 1px solid rgba(123, 66, 176, 0.58);
    color: #fbf7ff;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:hover {
    background: rgba(42, 15, 69, 0.96);
    border-color: #de66ff;
    color: #ffffff;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:pressed {
    background: rgba(34, 13, 57, 0.98);
    border-color: #ff8bee;
}

QToolBar#ShellToolbar QToolButton#ShellNavButton:checked {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #6a1fb0,
        stop: 1 #e23ca7);
    border-color: #ff9af1;
    color: #ffffff;
}

QStatusBar#ShellStatusBar {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #08030d,
        stop: 1 #140621);
    border-top: 1px solid #4d236d;
}

QWidget#WalletSecurityChip,
QWidget#ProxyStatusChip,
QWidget#NetworkStatusChip,
QWidget#ChainStatusChip,
QFrame#ShellSyncStatusCard {
    background: rgba(23, 9, 39, 0.94);
    border: 1px solid #693895;
    border-radius: 13px;
}

QWidget#ProxyStatusChip {
    background: rgba(56, 20, 50, 0.94);
    border-color: #ff9e5d;
}

QWidget#NetworkStatusChip[networkState="warming"],
QWidget#ChainStatusChip[syncState="connecting"],
QFrame#ShellSyncStatusCard[syncState="connecting"] {
    background: rgba(63, 19, 85, 0.96);
    border-color: #ff7ce5;
}

QWidget#NetworkStatusChip[networkState="live"],
QWidget#ChainStatusChip[syncState="synced"] {
    background: rgba(28, 13, 50, 0.96);
    border-color: #74ecff;
}

QWidget#NetworkStatusChip[networkState="offline"] {
    background: rgba(67, 14, 36, 0.96);
    border-color: #ff74a8;
}

QWidget#ChainStatusChip[syncState="syncing"],
QFrame#ShellSyncStatusCard[syncState="syncing"] {
    background: rgba(25, 12, 58, 0.96);
    border-color: #7de7ff;
}

QPushButton,
QToolButton,
QComboBox,
QLineEdit,
QPlainTextEdit,
QTextEdit,
QAbstractSpinBox,
QAbstractItemView,
QTableView,
QListView,
QTreeView,
QMenu,
QMenuBar,
QTabWidget::pane,
QTabBar::tab {
    color: #faf7ff;
}

QPushButton#ReceivePrimaryButton,
QPushButton#SendPrimaryButton,
QPushButton#DialogPrimaryButton,
QPushButton#AddressBookPrimaryButton,
QPushButton#CoinControlPrimaryButton,
QPushButton#GamesBackButton {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #6821b4,
        stop: 1 #ea3fa8);
    border: 1px solid #ff9bf0;
    color: #ffffff;
}

QPushButton#ReceivePrimaryButton:hover,
QPushButton#SendPrimaryButton:hover,
QPushButton#DialogPrimaryButton:hover,
QPushButton#AddressBookPrimaryButton:hover,
QPushButton#CoinControlPrimaryButton:hover,
QPushButton#GamesBackButton:hover {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #7b2ed0,
        stop: 1 #ff58ba);
}

QPushButton#ReceiveSecondaryButton,
QPushButton#SendSecondaryButton,
QPushButton#SendEntryUseBalance,
QPushButton#DialogSecondaryButton,
QPushButton#ReceiveRequestActionButton,
QPushButton#TransactionsExportButton,
QPushButton#AddressBookSecondaryButton,
QPushButton#GamesLauncherButton,
QToolButton#SendEntryToolButton,
QToolButton#SignVerifyIconButton,
QPushButton#SignVerifyIconButton,
QToolButton#SendEntryDeleteButton {
    background: #180a28;
    border: 1px solid #6a3e92;
    color: #faf7ff;
}

QPushButton#ReceiveSecondaryButton:hover,
QPushButton#SendSecondaryButton:hover,
QPushButton#SendEntryUseBalance:hover,
QPushButton#DialogSecondaryButton:hover,
QPushButton#ReceiveRequestActionButton:hover,
QPushButton#TransactionsExportButton:hover,
QPushButton#AddressBookSecondaryButton:hover,
QPushButton#GamesLauncherButton:hover,
QToolButton#SendEntryToolButton:hover,
QToolButton#SignVerifyIconButton:hover,
QPushButton#SignVerifyIconButton:hover {
    background: #27103f;
    border-color: #d966ff;
}

QToolButton#SendEntryDeleteButton:hover,
QPushButton#AddressBookDangerButton:hover {
    background: #531538;
    border-color: #ff76a8;
}

QPushButton#AddressBookDangerButton {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #5a1638,
        stop: 1 #8f2f71);
    border: 1px solid #ff8fb9;
    color: #fff6fb;
}

QLineEdit,
QPlainTextEdit,
QTextEdit,
QComboBox,
QSpinBox,
QDoubleSpinBox,
QValidatedLineEdit#SendTextField,
QValidatedLineEdit#SendEntryField,
QLineEdit#SendEntryField,
QAbstractSpinBox#SendAmountField,
QAbstractSpinBox#SendEntryAmount,
QLineEdit#OpenUriField,
QValidatedLineEdit#OpenUriField,
QLineEdit#SignVerifyField,
QValidatedLineEdit#SignVerifyField,
QPlainTextEdit#SignVerifyMessageBox {
    background-color: #12081f;
    border: 1px solid #60378a;
    selection-background-color: #7d3cff;
}

QPushButton:focus,
QToolButton:focus,
QComboBox#ShellWalletSelector:focus,
QLineEdit:focus,
QPlainTextEdit:focus,
QTextEdit:focus,
QAbstractSpinBox:focus {
    border-color: #ff8bee;
}

QAbstractItemView,
QTableView,
QListView,
QTreeView,
QTableView#transactionView,
QTableView#ReceiveHistoryTable,
QTableView#AddressBookTable,
QTreeWidget#CoinControlTree {
    background-color: rgba(15, 7, 27, 0.98);
    alternate-background-color: #160b27;
    border: 1px solid #5a327d;
    gridline-color: #3d2059;
    selection-background-color: #792dbe;
    selection-color: #ffffff;
}

QHeaderView::section,
QHeaderView#TransactionTableHeader::section,
QHeaderView#ReceiveHistoryHeader::section,
QHeaderView#AddressBookTableHeader::section,
QHeaderView#CoinControlTreeHeader::section {
    background-color: #180b2a;
    border-bottom: 1px solid #522d73;
    color: #dbc7f4;
}

QMenuBar,
QMenu {
    background-color: #12081f;
    border-color: #522d73;
}

QMenuBar::item:selected,
QMenu::item:selected,
QToolButton:hover,
QToolButton:pressed,
QToolButton:checked {
    background: #2a1045;
    border-color: #d966ff;
}

QTabWidget::pane,
QTabWidget#SignVerifyTabs::pane {
    border: 1px solid #5d337f;
    background-color: rgba(17, 8, 30, 0.98);
}

QTabBar::tab,
QTabWidget#SignVerifyTabs QTabBar::tab {
    background-color: #160a28;
    border: 1px solid #5b347d;
    color: #e8dcfb;
}

QTabBar::tab:selected,
QTabWidget#SignVerifyTabs QTabBar::tab:selected {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0,
        stop: 0 #6622ac,
        stop: 1 #e140a7);
    border-color: #ff96ef;
    color: #ffffff;
}

QToolButton#CustomTitleButton,
QToolButton#CustomTitleCloseButton,
QToolButton#ShellWindowControlButton,
QToolButton#ShellWindowCloseButton {
    background: #190b2b;
    border: 1px solid #5b347d;
    border-radius: 6px;
    color: #faf7ff;
}

QToolButton#CustomTitleButton:hover,
QToolButton#ShellWindowControlButton:hover {
    background: #27103f;
    border-color: #d966ff;
}

QToolButton#ShellWindowCloseButton:hover {
    background: #66122a;
    border-color: #ff6b9d;
}

QToolButton#CustomTitleCloseButton:hover {
    background: #55163a;
    border-color: #ff86b5;
}

QToolTip {
    background-color: #1a0d2c;
    color: #faf7ff;
    border: 1px solid #8b52c6;
}

QLabel#TradeStatusLabel {
    background: rgba(54, 18, 76, 0.82);
    border: 1px solid #ff7ce5;
    border-radius: 12px;
    color: #fff6ff;
    padding: 12px 14px;
}

QTextEdit#TransactionDetailsText,
QTextEdit#ReceiveRequestDetails {
    background-color: rgba(18, 8, 31, 0.98);
    border: 1px solid #5e357f;
    color: #faf7ff;
}

QWidget#ShutdownWindow,
QDialog#AboutQtDialog,
QDialog#HelpMessageDialog,
QDialog#CoinControlDialog,
QDialog#OpenURIDialog,
QDialog#SignVerifyMessageDialog,
QDialog#TransactionDescDialog,
QDialog#ReceiveRequestDialog,
QDialog#EditAddressDialog,
QDialog#SendConfirmationDialog {
    background: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 1,
        stop: 0 #07030c,
        stop: 0.55 #12051d,
        stop: 1 #1a0a2b);
    border: 1px solid #5c2f7f;
    color: #faf7ff;
}

QWidget#ShutdownWindow QLabel#ShutdownMessage,
QDialog#AboutQtDialog QLabel#AboutQtMessage {
    color: #faf7ff;
    font-size: 14px;
    padding: 8px 2px;
}

QDialog#AboutQtDialog QLabel#AboutQtMessage a,
QDialog#HelpMessageDialog QLabel#HelpAboutMessage a,
QTextEdit#TransactionDetailsText a,
QTextEdit#ReceiveRequestDetails a {
    color: #7feaff;
}
)");
}

/** Set up translations */
static void initTranslations(QTranslator &qtTranslatorBase, QTranslator &qtTranslator, QTranslator &translatorBase, QTranslator &translator)
{
    // Remove old translators
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    // Get desired locale (e.g. "de_DE")
    // 1) System default language
    QString lang_territory = GetLangTerritory();

    // Convert to "de" only by truncating "_DE"
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

    // Load e.g. verge_de.qm (shortcut "de" needs to be defined in verge.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

    // Load e.g. verge_de_DE.qm (shortcut "de_DE" needs to be defined in verge.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);
}

/* qDebug() message handler --> debug.log */
#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char *msg)
{
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg);
    } else {
        LogPrintf("GUI: %s\n", msg);
    }
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString &msg)
{
    Q_UNUSED(context);
    if (type == QtDebugMsg) {
        LogPrint(BCLog::QT, "GUI: %s\n", msg.toStdString());
    } else {
        LogPrintf("GUI: %s\n", msg.toStdString());
    }
}
#endif

/** Class encapsulating VERGE Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class VERGECore: public QObject
{
    Q_OBJECT
public:
    explicit VERGECore(interfaces::Node& node);

public Q_SLOTS:
    void initialize();
    void shutdown();

Q_SIGNALS:
    void initializeResult(bool success);
    void shutdownResult();
    void runawayException(const QString &message);

private:
    /// Pass fatal exception message to UI thread
    void handleRunawayException(const std::exception *e);

    interfaces::Node& m_node;
};

/** Main VERGE application object */
class VERGEApplication: public QApplication
{
    Q_OBJECT
public:
    explicit VERGEApplication(interfaces::Node& node, int &argc, char **argv);
    ~VERGEApplication();

#ifdef ENABLE_WALLET
    /// Create payment server
    void createPaymentServer();
#endif
    /// parameter interaction/setup based on rules
    void parameterSetup();
    /// Create options model
    void createOptionsModel(bool resetSettings);
    /// Create main window
    void createWindow(const NetworkStyle *networkStyle);
    /// Create splash screen
    void createSplashScreen(const NetworkStyle *networkStyle);

    /// Request core initialization
    void requestInitialize();
    /// Request core shutdown
    void requestShutdown();

    /// Get process return value
    int getReturnValue() const { return returnValue; }

    /// Get window identifier of QMainWindow (VERGEGUI)
    WId getMainWinId() const;

    /// Setup platform style
    void setupPlatformStyle();

public Q_SLOTS:
    void initializeResult(bool success);
    void shutdownResult();
    /// Handle runaway exceptions. Shows a message box with the problem and quits the program.
    void handleRunawayException(const QString &message);
    void addWallet(WalletModel* walletModel);

Q_SIGNALS:
    void requestedInitialize();
    void requestedShutdown();
    void stopThread();
    void splashFinished(QWidget *window);

private:
    QThread *coreThread;
    interfaces::Node& m_node;
    OptionsModel *optionsModel;
    ClientModel *clientModel;
    VERGEGUI *window;
    QTimer *pollShutdownTimer;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer;
    std::vector<WalletModel*> m_wallet_models;
    std::unique_ptr<interfaces::Handler> m_handler_load_wallet;
#endif
    int returnValue;
    const PlatformStyle *platformStyle;
    std::unique_ptr<QWidget> shutdownWindow;

    void startThread();
};

#include <qt/verge.moc>

VERGECore::VERGECore(interfaces::Node& node) :
    QObject(), m_node(node)
{
}

void VERGECore::handleRunawayException(const std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    Q_EMIT runawayException(QString::fromStdString(m_node.getWarnings("gui")));
}

void VERGECore::initialize()
{
    try
    {
        qDebug() << __func__ << ": Running initialization in thread";
        bool rv = m_node.appInitMain();
        Q_EMIT initializeResult(rv);
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

void VERGECore::shutdown()
{
    try
    {
        qDebug() << __func__ << ": Running Shutdown in thread";
        m_node.appShutdown();
        qDebug() << __func__ << ": Shutdown finished";
        Q_EMIT shutdownResult();
    } catch (const std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
}

VERGEApplication::VERGEApplication(interfaces::Node& node, int &argc, char **argv):
    QApplication(argc, argv),
    coreThread(0),
    m_node(node),
    optionsModel(0),
    clientModel(0),
    window(0),
    pollShutdownTimer(0),
#ifdef ENABLE_WALLET
    paymentServer(0),
    m_wallet_models(),
#endif
    returnValue(0),
    platformStyle(0)
{
    setQuitOnLastWindowClosed(false);
}

void VERGEApplication::setupPlatformStyle()
{
    // UI per-platform customization
    // This must be done inside the VERGEApplication constructor, or after it, because
    // PlatformStyle::instantiate requires a QApplication
    std::string platformName;
    platformName = gArgs.GetArg("-uiplatform", VERGEGUI::DEFAULT_UIPLATFORM);
    platformStyle = PlatformStyle::instantiate(QString::fromStdString(platformName));
    if (!platformStyle) // Fall back to "other" if specified name not found
        platformStyle = PlatformStyle::instantiate("other");
    assert(platformStyle);
}

VERGEApplication::~VERGEApplication()
{
    if(coreThread)
    {
        qDebug() << __func__ << ": Stopping thread";
        Q_EMIT stopThread();
        if (!coreThread->wait(15000)) {
            qWarning() << __func__ << ": Core thread did not stop in time, terminating thread";
            coreThread->terminate();
            coreThread->wait(3000);
        }
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = 0;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = 0;
#endif
    delete optionsModel;
    optionsModel = 0;
    delete platformStyle;
    platformStyle = 0;
}

#ifdef ENABLE_WALLET
void VERGEApplication::createPaymentServer()
{
    paymentServer = new PaymentServer(this);
}
#endif

void VERGEApplication::createOptionsModel(bool resetSettings)
{
    optionsModel = new OptionsModel(m_node, nullptr, resetSettings);
}

void VERGEApplication::createWindow(const NetworkStyle *networkStyle)
{
    window = new VERGEGUI(m_node, platformStyle, networkStyle, 0);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
}

void VERGEApplication::createSplashScreen(const NetworkStyle *networkStyle)
{
    SplashScreen *splash = new SplashScreen(m_node, Qt::WindowFlags(), networkStyle);
    // We don't hold a direct pointer to the splash screen after creation, but the splash
    // screen will take care of deleting itself when slotFinish happens.
    splash->show();
    splash->raise();
    splash->activateWindow();
    QApplication::processEvents();
    connect(this, SIGNAL(splashFinished(QWidget*)), splash, SLOT(slotFinish(QWidget*)));
    connect(this, SIGNAL(requestedShutdown()), splash, SLOT(close()));
}

void VERGEApplication::startThread()
{
    if(coreThread)
        return;
    coreThread = new QThread(this);
    VERGECore *executor = new VERGECore(m_node);
    executor->moveToThread(coreThread);

    /*  communication to and from thread */
    connect(executor, SIGNAL(initializeResult(bool)), this, SLOT(initializeResult(bool)));
    connect(executor, SIGNAL(shutdownResult()), this, SLOT(shutdownResult()));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    /*  make sure executor object is deleted in its own thread */
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

void VERGEApplication::parameterSetup()
{
    // Default printtoconsole to false for the GUI. GUI programs should not
    // print to the console unnecessarily.
    gArgs.SoftSetBoolArg("-printtoconsole", false);

    m_node.initLogging();
    m_node.initParameterInteraction();
}

void VERGEApplication::requestInitialize()
{
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    Q_EMIT requestedInitialize();
}

void VERGEApplication::requestShutdown()
{
    // Show a simple window indicating shutdown status
    // Do this first as some of the steps may take some time below,
    // for example the RPC console may still be executing a command.
    shutdownWindow.reset(ShutdownWindow::showShutdownWindow(window));

    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    window->hide();
    window->setClientModel(0);
    pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    window->removeAllWallets();
    for (WalletModel *walletModel : m_wallet_models) {
        delete walletModel;
    }
    m_wallet_models.clear();
#endif
    delete clientModel;
    clientModel = 0;

    m_node.startShutdown();

    // Request shutdown from core thread
    Q_EMIT requestedShutdown();

    // Avoid a permanent hung shutdown window if backend shutdown stalls unexpectedly.
    QTimer::singleShot(45000, this, [this]() {
        if (shutdownWindow) {
            qWarning() << __func__ << ": Shutdown timed out; forcing UI exit";
            quit();
        }
    });
}

void VERGEApplication::addWallet(WalletModel* walletModel)
{
#ifdef ENABLE_WALLET
    window->addWallet(walletModel);

    if (m_wallet_models.empty()) {
        window->setCurrentWallet(walletModel->getWalletName());
    }

    connect(walletModel, SIGNAL(coinsSent(WalletModel*, SendCoinsRecipient, QByteArray)),
        paymentServer, SLOT(fetchPaymentACK(WalletModel*, const SendCoinsRecipient&, QByteArray)));

    m_wallet_models.push_back(walletModel);
#endif
}

void VERGEApplication::initializeResult(bool success)
{
    qDebug() << __func__ << ": Initialization result: " << success;
    // Set exit result.
    returnValue = success ? EXIT_SUCCESS : EXIT_FAILURE;
    if(success)
    {
        // Log this only after AppInitMain finishes, as then logging setup is guaranteed complete
        qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
        PaymentServer::LoadRootCAs();
        paymentServer->setOptionsModel(optionsModel);
#endif

        clientModel = new ClientModel(m_node, optionsModel);
        window->setClientModel(clientModel);

#ifdef ENABLE_WALLET
        m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) {
            QMetaObject::invokeMethod(this, "addWallet", Qt::QueuedConnection,
                Q_ARG(WalletModel*, new WalletModel(std::move(wallet), m_node, platformStyle, optionsModel)));
        });

        for (auto& wallet : m_node.getWallets()) {
            addWallet(new WalletModel(std::move(wallet), m_node, platformStyle, optionsModel));
        }
#endif

        // If -min option passed, start window minimized.
        if(gArgs.GetBoolArg("-min", false))
        {
            window->showMinimized();
        }
        else
        {
            window->show();
        }
        Q_EMIT splashFinished(window);

#ifdef ENABLE_WALLET
        // Now that initialization/startup is done, process any command-line
        // verge: URIs or payment requests:
        connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
                         window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
        connect(window, SIGNAL(receivedURI(QString)),
                         paymentServer, SLOT(handleURIOrFile(QString)));
        connect(paymentServer, SIGNAL(message(QString,QString,unsigned int)),
                         window, SLOT(message(QString,QString,unsigned int)));
        QTimer::singleShot(100, paymentServer, SLOT(uiReady()));
#endif
        pollShutdownTimer->start(200);
    } else {
        Q_EMIT splashFinished(window); // Make sure splash screen doesn't stick around during shutdown
        quit(); // Exit first main loop invocation
    }
}

void VERGEApplication::shutdownResult()
{
    shutdownWindow.reset();
    quit(); // Exit second main loop invocation after shutdown finished
}

void VERGEApplication::handleRunawayException(const QString &message)
{
    QMessageBox::critical(0, "Runaway exception", VERGEGUI::tr("A fatal error occurred. VERGE can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(EXIT_FAILURE);
}

WId VERGEApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

static void SetupUIArgs()
{
#ifdef ENABLE_WALLET
    gArgs.AddArg("-allowselfsignedrootcertificates", strprintf("Allow self signed root certificates (default: %u)", DEFAULT_SELFSIGNED_ROOTCERTS), true, OptionsCategory::GUI);
#endif
    gArgs.AddArg("-choosedatadir", strprintf("Choose data directory on startup (default: %u)", DEFAULT_CHOOSE_DATADIR), false, OptionsCategory::GUI);
    gArgs.AddArg("-lang=<lang>", "Set language, for example \"de_DE\" (default: system locale)", false, OptionsCategory::GUI);
    gArgs.AddArg("-min", "Start minimized", false, OptionsCategory::GUI);
    gArgs.AddArg("-resetguisettings", "Reset all settings changed in the GUI", false, OptionsCategory::GUI);
    gArgs.AddArg("-rootcertificates=<file>", "Set SSL root certificates for payment request (default: -system-)", false, OptionsCategory::GUI);
    gArgs.AddArg("-splash", strprintf("Show splash screen on startup (default: %u)", DEFAULT_SPLASHSCREEN), false, OptionsCategory::GUI);
    gArgs.AddArg("-uiplatform", strprintf("Select platform to customize UI for (one of windows, macosx, other; default: %s)", VERGEGUI::DEFAULT_UIPLATFORM), true, OptionsCategory::GUI);
    gArgs.AddArg("-with-unstoppable", "Enable Unstoppable Domain in VERGE to send using Web3 Domain", false, OptionsCategory::OPTIONS);
}

static QByteArray BuildSanitizedWebEngineFlags()
{
    QStringList flag_list = QString::fromLocal8Bit(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS"))
                                .split(' ', Qt::SkipEmptyParts);
    QStringList sanitized_flags;
    sanitized_flags.reserve(flag_list.size());
    for (const QString& flag : flag_list) {
        if (flag.startsWith("--ozone-platform=") ||
            flag.startsWith("--use-gl=") ||
            flag == "--single-process" ||
            flag.startsWith("--proxy-server=") ||
            flag.startsWith("--host-resolver-rules=")) {
            continue;
        }
        sanitized_flags.push_back(flag);
    }
    return sanitized_flags.join(' ').toLocal8Bit();
}

static void AppendWebEngineFlag(QByteArray& flags, const char* flag)
{
    const QByteArray needle(flag);
    if (!flags.contains(needle)) {
        if (!flags.isEmpty() && !flags.endsWith(' ')) {
            flags.append(' ');
        }
        flags.append(needle);
    }
}

static void RemoveWebEngineFlag(QByteArray& flags, const char* flag)
{
    const QString needle = QString::fromLatin1(flag);
    QStringList flag_list = QString::fromLocal8Bit(flags).split(' ', Qt::SkipEmptyParts);
    flag_list.removeAll(needle);
    flags = flag_list.join(' ').toLocal8Bit();
}

static void AppendQtLoggingRule(QByteArray& rules, const char* rule)
{
    const QByteArray needle(rule);
    const QList<QByteArray> parts = rules.split(';');
    for (const QByteArray& part : parts) {
        if (part.trimmed() == needle) {
            return;
        }
    }
    if (!rules.isEmpty() && !rules.endsWith(';')) {
        rules.append(';');
    }
    rules.append(needle);
}

static QString ResolveApplicationDirPath(const char* argv0)
{
#if defined(Q_OS_MAC)
    if (argv0 && argv0[0] != '\0') {
        const QFileInfo executable_info(QDir::current(), QString::fromLocal8Bit(argv0));
        if (executable_info.exists()) {
            return executable_info.absolutePath();
        }
    }
#endif
    if (QCoreApplication::instance()) {
        return QCoreApplication::applicationDirPath();
    }
    return QString();
}

static void ConfigureQtWebEngineBundlePaths(const char* argv0)
{
#if defined(Q_OS_MAC)
    const QString app_dir_path = ResolveApplicationDirPath(argv0);
    if (app_dir_path.isEmpty()) {
        qWarning() << "QtWebEngine: unable to resolve application directory for bundle paths";
        return;
    }

    const QDir app_dir(app_dir_path);
    const QDir contents_dir(app_dir.absoluteFilePath(QStringLiteral("..")));
    const QString frameworks_dir = contents_dir.absoluteFilePath(QStringLiteral("Frameworks"));
    const QString helper_path = frameworks_dir +
        QStringLiteral("/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess");
    const QString framework_resources_path =
        frameworks_dir + QStringLiteral("/QtWebEngineCore.framework/Versions/A/Resources");
    const QString app_resources_path = contents_dir.absoluteFilePath(QStringLiteral("Resources"));

    if (QFileInfo::exists(helper_path)) {
        qputenv("QTWEBENGINEPROCESS_PATH", helper_path.toUtf8());
        qWarning() << "QtWebEngine: using helper process at" << helper_path;
    } else {
        qWarning() << "QtWebEngine: helper process not found at" << helper_path;
    }

    const QString resources_path =
        QFileInfo::exists(framework_resources_path + QStringLiteral("/qtwebengine_resources.pak"))
            ? framework_resources_path
            : app_resources_path;
    if (QFileInfo::exists(resources_path + QStringLiteral("/qtwebengine_resources.pak"))) {
        qputenv("QTWEBENGINE_RESOURCES_PATH", resources_path.toUtf8());
        qWarning() << "QtWebEngine: using resources at" << resources_path;
    } else {
        qWarning() << "QtWebEngine: qtwebengine_resources.pak not found in"
                   << framework_resources_path << "or" << app_resources_path;
    }

    const QString framework_locales_path = framework_resources_path + QStringLiteral("/qtwebengine_locales");
    const QString app_locales_path = app_resources_path + QStringLiteral("/qtwebengine_locales");
    const QString locales_path = QDir(framework_locales_path).exists() ? framework_locales_path : app_locales_path;
    if (QDir(locales_path).exists()) {
        qputenv("QTWEBENGINE_LOCALES_PATH", locales_path.toUtf8());
        qWarning() << "QtWebEngine: using locales at" << locales_path;
    } else {
        qWarning() << "QtWebEngine: qtwebengine_locales directory not found in"
                   << framework_locales_path << "or" << app_locales_path;
    }
#endif
}

static void ConfigureQtWebEngineRuntimeBase(const char* argv0)
{
#if defined(Q_OS_LINUX)
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR")) {
        const QString runtime_dir = QStringLiteral("/tmp/runtime-%1").arg(QString::number(getuid()));
        QDir().mkpath(runtime_dir);
        qputenv("XDG_RUNTIME_DIR", runtime_dir.toUtf8());
    }

    const QByteArray qpa_platform = qgetenv("QT_QPA_PLATFORM");
    if (qpa_platform.isEmpty() || qpa_platform == "wayland" || qpa_platform == "wayland-egl") {
        qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    }

    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        qunsetenv("WAYLAND_DISPLAY");
    }
    if (qEnvironmentVariableIsEmpty("QT_OPENGL")) {
        qputenv("QT_OPENGL", QByteArray("software"));
    }
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")) {
        qputenv("QT_QUICK_BACKEND", QByteArray("software"));
    }
    if (qEnvironmentVariableIsEmpty("LIBGL_ALWAYS_SOFTWARE")) {
        qputenv("LIBGL_ALWAYS_SOFTWARE", QByteArray("1"));
    }

    QByteArray flags = BuildSanitizedWebEngineFlags();
    // Keep WebEngine stable in depends-built Linux environments.
    AppendWebEngineFlag(flags, "--disable-gpu");
    AppendWebEngineFlag(flags, "--disable-gpu-compositing");
    AppendWebEngineFlag(flags, "--no-sandbox");
    AppendWebEngineFlag(flags, "--disable-setuid-sandbox");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", QByteArray("1"));

    const QByteArray dbus_system = qgetenv("DBUS_SYSTEM_BUS_ADDRESS");
    if (dbus_system.isEmpty() || dbus_system.contains("/depends/")) {
        qputenv("DBUS_SYSTEM_BUS_ADDRESS", QByteArray("unix:path=/run/dbus/system_bus_socket"));
    }
#elif defined(Q_OS_MAC)
    ConfigureQtWebEngineBundlePaths(argv0);

    QByteArray flags = BuildSanitizedWebEngineFlags();

    // Do not force Chromium sandbox or macOS capture feature overrides.
    // The embedded trade widget is stable on other platforms without these
    // process-wide switches, and on macOS they have been linked to renderer crashes.
    RemoveWebEngineFlag(flags, "--no-sandbox");
    RemoveWebEngineFlag(flags, "--auto-reject-capture");
    RemoveWebEngineFlag(flags, "--disable-features=ScreenCaptureKitMac,ScreenCaptureKitMacWindow,ScreenCaptureKitMacScreen,UseSCContentSharingPicker,UseScreenCaptureKitForSnapshots");
    AppendWebEngineFlag(flags, "--enable-logging");
    AppendWebEngineFlag(flags, "--log-level=0");
    AppendWebEngineFlag(flags, "--v=1");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    qunsetenv("QTWEBENGINE_DISABLE_SANDBOX");

    QByteArray qt_logging_rules = qgetenv("QT_LOGGING_RULES");
    AppendQtLoggingRule(qt_logging_rules, "qt.webenginecontext.debug=true");
    AppendQtLoggingRule(qt_logging_rules, "qt.webenginecontext=true");
    AppendQtLoggingRule(qt_logging_rules, "qt.webengine.compositor=true");
    qputenv("QT_LOGGING_RULES", qt_logging_rules);

    const QString webengine_runtime_msg =
        QStringLiteral("QtWebEngine: runtime base configured QTWEBENGINE_CHROMIUM_FLAGS=%1 QTWEBENGINE_DISABLE_SANDBOX=%2 QTWEBENGINEPROCESS_PATH=%3 QTWEBENGINE_RESOURCES_PATH=%4 QTWEBENGINE_LOCALES_PATH=%5 QT_LOGGING_RULES=%6")
            .arg(qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS"))
            .arg(qEnvironmentVariable("QTWEBENGINE_DISABLE_SANDBOX"))
            .arg(qEnvironmentVariable("QTWEBENGINEPROCESS_PATH"))
            .arg(qEnvironmentVariable("QTWEBENGINE_RESOURCES_PATH"))
            .arg(qEnvironmentVariable("QTWEBENGINE_LOCALES_PATH"))
            .arg(qEnvironmentVariable("QT_LOGGING_RULES"));
    qWarning().noquote() << webengine_runtime_msg;
    LogPrintf("GUI: %s\n", webengine_runtime_msg.toStdString());
#endif
}

static void ConfigureQtWebEngineProxy(bool use_tor_proxy)
{
#if defined(Q_OS_LINUX)
    QByteArray flags = BuildSanitizedWebEngineFlags();
    AppendWebEngineFlag(flags, "--disable-gpu");
    AppendWebEngineFlag(flags, "--disable-gpu-compositing");
    AppendWebEngineFlag(flags, "--no-sandbox");
    AppendWebEngineFlag(flags, "--disable-setuid-sandbox");

    if (use_tor_proxy) {
        AppendWebEngineFlag(flags, "--proxy-server=socks5://127.0.0.1:9090");
    }

    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
#elif defined(Q_OS_MAC)
    QByteArray flags = BuildSanitizedWebEngineFlags();

    RemoveWebEngineFlag(flags, "--no-sandbox");
    RemoveWebEngineFlag(flags, "--auto-reject-capture");
    RemoveWebEngineFlag(flags, "--disable-features=ScreenCaptureKitMac,ScreenCaptureKitMacWindow,ScreenCaptureKitMacScreen,UseSCContentSharingPicker,UseScreenCaptureKitForSnapshots");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
    const QString webengine_proxy_msg =
        QStringLiteral("QtWebEngine: proxy configuration updated use_tor_proxy=%1 QTWEBENGINE_CHROMIUM_FLAGS=%2")
            .arg(use_tor_proxy)
            .arg(qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS"));
    qWarning().noquote() << webengine_proxy_msg;
    LogPrintf("GUI: %s\n", webengine_proxy_msg.toStdString());
#else
    Q_UNUSED(use_tor_proxy);
#endif
}

#ifndef VERGE_QT_TEST
int main(int argc, char *argv[])
{
    SetupEnvironment();
    ConfigureQtWebEngineRuntimeBase(argc > 0 ? argv[0] : nullptr);

    std::unique_ptr<interfaces::Node> node = interfaces::MakeNode();

    // Do not refer to data directory yet, this can be overridden by Intro::pickDataDirectory

    /// 1. Basic Qt initialization (not dependent on parameters or configuration)
#if QT_VERSION < 0x050000
    // Internal string conversion is all UTF-8
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
#endif

    Q_INIT_RESOURCE(verge);
    Q_INIT_RESOURCE(verge_locale);

#if QT_VERSION > 0x050100
    // Generate high-dpi pixmaps
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= 0x050600
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#if QT_VERSION >= 0x050400
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    VERGEApplication app(*node, argc, argv);

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType< bool* >();
    //   Need to pass name here as CAmount is a typedef (see http://qt-project.org/doc/qt-5/qmetatype.html#qRegisterMetaType)
    //   IMPORTANT if it is no longer a typedef use the normal variant above
    qRegisterMetaType< CAmount >("CAmount");
    qRegisterMetaType< std::function<void()> >("std::function<void()>");
#ifdef ENABLE_WALLET
    qRegisterMetaType<WalletModel*>("WalletModel*");
#endif

    /// 2. Parse command-line options. We do this after qt in order to show an error if there are problems parsing these
    // Command-line options take precedence:
    node->setupServerArgs();
    SetupUIArgs();
    std::string error;
    if (!node->parseParameters(argc, argv, error)) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
            QObject::tr("Error parsing command line arguments: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }

    // Trade WebEngine proxy behavior:
    // default Tor SOCKS5 proxy, and explicit clearnet if --without-tor is set.
    const bool without_tor = gArgs.GetBoolArg("-without-tor", false);
    ConfigureQtWebEngineProxy(!without_tor);

    // Now that the QApplication is setup and we have parsed our parameters, we can set the platform style
    app.setupPlatformStyle();

    /// 3. Application identification
    // must be set before OptionsModel is initialized or translations are loaded,
    // as it is used to locate QSettings
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);
    app.setStyleSheet(BuildAppStyleSheet());
    GUIUtil::SubstituteFonts(GetLangTerritory());

    /// 4. Initialization of translations, so that intro dialog is in user's language
    // Now that QSettings are accessible, initialize translations
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);
    translationInterface.Translate.connect(Translate);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (HelpRequested(gArgs) || gArgs.IsArgSet("-version")) {
        HelpMessageDialog help(*node, nullptr, gArgs.IsArgSet("-version"));
        help.showOrPrint();
        return EXIT_SUCCESS;
    }

    /// 5. Now that settings and translations are available, ask user for data directory
    // User language is set up: pick a data directory
    if (!Intro::pickDataDirectory(*node))
        return EXIT_SUCCESS;

    /// 6. Determine availability of data and blocks directory and parse verge.conf
    /// - Do not call GetDataDir(true) before this step finishes
    if (!fs::is_directory(GetDataDir(false)))
    {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
                              QObject::tr("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
        return EXIT_FAILURE;
    }
    if (!node->readConfigFiles(error)) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
            QObject::tr("Error: Cannot parse configuration file: %1.").arg(QString::fromStdString(error)));
        return EXIT_FAILURE;
    }
    // User wallets should participate in secure messaging by default. The
    // daemon sets the opposite default for infrastructure deployments.
    gArgs.SoftSetBoolArg("-smsg", true);

    /// 7. Determine network (and switch to network specific options)
    // - Do not call Params() before this step
    // - Do this after parsing the configuration file, as the network can be switched there
    // - QSettings() will use the new application name after this, resulting in network-specific settings
    // - Needs to be done before createOptionsModel

    // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
    try {
        node->selectParams(gArgs.GetChainName());
    } catch(std::exception &e) {
        QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QObject::tr("Error: %1").arg(e.what()));
        return EXIT_FAILURE;
    }
#ifdef ENABLE_WALLET
    // Parse URIs on command line -- this can affect Params()
    PaymentServer::ipcParseCommandLine(*node, argc, argv);
#endif

    QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString())));
    assert(!networkStyle.isNull());
    // Allow for separate UI settings for testnets
    QApplication::setApplicationName(networkStyle->getAppName());
    // Re-initialize translations after changing application name (language in network-specific settings can be different)
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

#ifdef ENABLE_WALLET
    /// 8. URI IPC sending
    // - Do this early as we don't want to bother initializing if we are just calling IPC
    // - Do this *after* setting up the data directory, as the data directory hash is used in the name
    // of the server.
    // - Do this after creating app and setting up translations, so errors are
    // translated properly.
    if (PaymentServer::ipcSendCommandLine())
        exit(EXIT_SUCCESS);

    // Start up the payment server early, too, so impatient users that click on
    // verge: links repeatedly have their payment requests routed to this process:
    app.createPaymentServer();
#endif

    /// 9. Main GUI initialization
    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000
    // Install qDebug() message handler to route to debug.log
    qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)
    // Install global event filter for processing Windows session related Windows messages (WM_QUERYENDSESSION and WM_ENDSESSION)
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
    // Install qDebug() message handler to route to debug.log
    qInstallMessageHandler(DebugMessageHandler);
#endif
    // Allow parameter interaction before we create the options model
    app.parameterSetup();
    // Load GUI settings from QSettings
    app.createOptionsModel(gArgs.GetBoolArg("-resetguisettings", false));

    // Subscribe to global signals from core
    std::unique_ptr<interfaces::Handler> handler = node->handleInitMessage(InitMessage);

    const bool showSplash = gArgs.GetBoolArg("-splash", DEFAULT_SPLASHSCREEN) && !gArgs.GetBoolArg("-min", false);
    LogPrintf("GUI splash launch: %s (default=%s)\n", showSplash ? "true" : "false", DEFAULT_SPLASHSCREEN ? "true" : "false");
    if (showSplash)
        app.createSplashScreen(networkStyle.data());

    int rv = EXIT_SUCCESS;
    try
    {
        app.createWindow(networkStyle.data());
        // Perform base initialization before spinning up initialization/shutdown thread
        // This is acceptable because this function only contains steps that are quick to execute,
        // so the GUI thread won't be held up.
        if (node->baseInitialize()) {
            QTimer::singleShot(0, &app, [&app]() {
                app.requestInitialize();
            });
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
            WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("%1 didn't yet exit safely...").arg(QObject::tr(PACKAGE_NAME)), (HWND)app.getMainWinId());
#endif
            app.exec();
            app.requestShutdown();
            app.exec();
            rv = app.getReturnValue();
        } else {
            // A dialog with detailed error will have been shown by InitError()
            rv = EXIT_FAILURE;
        }
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(node->getWarnings("gui")));
    } catch (...) {
        PrintExceptionContinue(nullptr, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(node->getWarnings("gui")));
    }
    return rv;
}
#endif // VERGE_QT_TEST
