// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/vergeunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <QAbstractItemDelegate>
#include <QDateTime>
#include <QFrame>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPainter>

#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

namespace {
void ApplyCardShadow(QWidget* widget)
{
    if (!widget) return;
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(30.0);
    effect->setOffset(0, 10);
    effect->setColor(QColor(88, 28, 140, 96));
    widget->setGraphicsEffect(effect);
}
}

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle *_platformStyle, QObject *parent=nullptr):
        QAbstractItemDelegate(parent), unit(VERGEUnits::XVG),
        platformStyle(_platformStyle)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect cardRect = mainRect.adjusted(0, 4, -2, -4);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QColor("#2a3347"));
        painter->setBrush(QColor("#171d2a"));
        painter->drawRoundedRect(cardRect, 12, 12);

        QRect decorationRect(cardRect.left() + 16, cardRect.top() + 14, 18, 18);
        int xspace = decorationRect.right() + 14;
        const int amountWidth = 130;
        const int typeWidth = 96;
        QRect amountRect(cardRect.right() - amountWidth - 16, cardRect.top() + 18, amountWidth, 28);
        QRect dateRect(xspace, cardRect.top() + 10, amountRect.left() - xspace - 12, 20);
        QRect typeRect(xspace, cardRect.top() + 32, typeWidth, 22);
        QRect addressRect(typeRect.right() + 10, cardRect.top() + 30, amountRect.left() - typeRect.right() - 20, 26);
        icon = platformStyle->SingleColorIcon(icon);
        if (!icon.isNull()) {
            icon.paint(painter, decorationRect);
        }

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString type = index.sibling(index.row(), TransactionTableModel::Type).data(Qt::DisplayRole).toString();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        const qreal basePointSize = option.font.pointSizeF() > 0 ? option.font.pointSizeF() : 11.0;
        QFont dateFont = option.font;
        dateFont.setPointSizeF(basePointSize * 1.08);
        dateFont.setWeight(QFont::Medium);
        QFont typeFont = option.font;
        typeFont.setPointSizeF(basePointSize * 1.12);
        typeFont.setWeight(QFont::DemiBold);
        QFont addressFont = option.font;
        addressFont.setPointSizeF(basePointSize * 1.15);
        addressFont.setWeight(QFont::Medium);
        QFont amountFont = option.font;
        amountFont.setPointSizeF(basePointSize * 1.15);
        amountFont.setWeight(QFont::DemiBold);

        painter->setFont(dateFont);
        painter->setPen(QColor("#8e9bb3"));
        painter->drawText(dateRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        QColor typeColor("#8ea2c4");
        if (type == tr("Received")) {
            typeColor = QColor("#4fd38a");
        } else if (type == tr("Sent")) {
            typeColor = QColor("#ff6f7d");
        }
        painter->setFont(typeFont);
        painter->setPen(typeColor);
        painter->drawText(typeRect, Qt::AlignLeft|Qt::AlignVCenter, type);

        painter->setFont(addressFont);
        painter->setPen(QColor("#ecf1ff"));
        const QFontMetrics addressMetrics(addressFont);
        const QString addressText = addressMetrics.elidedText(address, Qt::ElideRight, addressRect.width());
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, addressText, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool())
        {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 6, addressRect.top(), 16, addressRect.height());
            iconWatchonly.paint(painter, watchonlyRect);
        }

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setFont(amountFont);
        painter->setPen(foreground);
        QString amountText = VERGEUnits::formatWithUnit(unit, amount, true, VERGEUnits::separatorAlways);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(OVERVIEW_DECORATION_SIZE, 72);
    }

    int unit;
    const PlatformStyle *platformStyle;

};
#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);
    setObjectName("OverviewPage");
    ui->frame->setObjectName("OverviewBalanceCard");
    ui->frame_2->setObjectName("OverviewTransactionsCard");
    ui->labelAlerts->setObjectName("OverviewAlertBanner");
    ui->label_5->setObjectName("OverviewSectionTitle");
    ui->label_4->setObjectName("OverviewSectionTitle");
    ui->labelBalance->setObjectName("OverviewPrimaryBalance");
    ui->labelTotal->setObjectName("OverviewPrimaryBalance");
    ui->labelUnconfirmed->setObjectName("OverviewSecondaryBalance");
    ui->labelImmature->setObjectName("OverviewSecondaryBalance");
    ui->labelWatchAvailable->setObjectName("OverviewSecondaryBalance");
    ui->labelWatchPending->setObjectName("OverviewSecondaryBalance");
    ui->labelWatchImmature->setObjectName("OverviewSecondaryBalance");
    ui->labelWatchTotal->setObjectName("OverviewSecondaryBalance");

    const QList<QLabel*> mutedLabels = {
        ui->labelBalanceText,
        ui->labelPendingText,
        ui->labelImmatureText,
        ui->labelTotalText,
        ui->labelSpendable,
        ui->labelWatchonly
    };
    for (QLabel* label : mutedLabels) {
        label->setObjectName("OverviewMutedLabel");
    }

    const QList<QFrame*> separators = {ui->line, ui->lineWatchBalance};
    for (QFrame* separator : separators) {
        separator->setObjectName("OverviewDivider");
    }

    m_balances.balance = -1;

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = platformStyle->SingleColorIcon(":/icons/warning");
    icon.addPixmap(icon.pixmap(QSize(64,64), QIcon::Normal), QIcon::Disabled); // also set the disabled icon because we are using a disabled QPushButton to work around missing HiDPI support of QLabel (https://bugreports.qt.io/browse/QTBUG-42503)
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(OVERVIEW_DECORATION_SIZE, OVERVIEW_DECORATION_SIZE));
    ui->listTransactions->setSpacing(8);
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * 80);
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ApplyCardShadow(ui->frame);
    ApplyCardShadow(ui->frame_2);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
    connect(ui->labelTransactionsStatus, SIGNAL(clicked()), this, SLOT(handleOutOfSyncWarningClicks()));
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    m_balances = balances;
    ui->labelBalance->setText(VERGEUnits::formatWithUnit(unit, balances.balance, false, VERGEUnits::separatorAlways));
    ui->labelUnconfirmed->setText(VERGEUnits::formatWithUnit(unit, balances.unconfirmed_balance, false, VERGEUnits::separatorAlways));
    ui->labelImmature->setText(VERGEUnits::formatWithUnit(unit, balances.immature_balance, false, VERGEUnits::separatorAlways));
    ui->labelTotal->setText(VERGEUnits::formatWithUnit(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance, false, VERGEUnits::separatorAlways));
    ui->labelWatchAvailable->setText(VERGEUnits::formatWithUnit(unit, balances.watch_only_balance, false, VERGEUnits::separatorAlways));
    ui->labelWatchPending->setText(VERGEUnits::formatWithUnit(unit, balances.unconfirmed_watch_only_balance, false, VERGEUnits::separatorAlways));
    ui->labelWatchImmature->setText(VERGEUnits::formatWithUnit(unit, balances.immature_watch_only_balance, false, VERGEUnits::separatorAlways));
    ui->labelWatchTotal->setText(VERGEUnits::formatWithUnit(unit, balances.watch_only_balance + balances.unconfirmed_watch_only_balance + balances.immature_watch_only_balance, false, VERGEUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;
    bool showWatchOnlyImmature = balances.immature_watch_only_balance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly)
        ui->labelWatchImmature->hide();
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        interfaces::Wallet& wallet = model->wallet();
        interfaces::WalletBalances balances = wallet.getBalances();
        setBalance(balances);
        connect(model, SIGNAL(balanceChanged(interfaces::WalletBalances)), this, SLOT(setBalance(interfaces::WalletBalances)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        updateWatchOnlyLabels(wallet.haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    // update the display unit, to not use the default ("XVG")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if (m_balances.balance != -1) {
            setBalance(m_balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}
