// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/optionsdialog.h>
#include <qt/forms/ui_optionsdialog.h>

#include <qt/vergeunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <validation.h> // for DEFAULT_SCRIPTCHECK_THREADS and MAX_SCRIPTCHECK_THREADS
#include <netbase.h>
#include <txdb.h> // for -dbcache defaults

#include <QDataWidgetMapper>
#include <QDir>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    walletModel(0),
    initialStakingEnabled(false),
    initialStakingReserve(0),
    mapper(0),
    stakingSummaryLabel(new QLabel(this)),
    bondTable(new QTableWidget(this)),
    refreshBondsButton(new QPushButton(this)),
    unbondSelectedButton(new QPushButton(this)),
    unbondAllButton(new QPushButton(this)),
    stakingRefreshTimer(new QTimer(this))
{
    ui->setupUi(this);
    GUIUtil::EnableThemedDialogChrome(this);
    ui->stakingGroup->setEnabled(false);
    connect(ui->enableStaking, SIGNAL(toggled(bool)),
            ui->stakingReserveBalance, SLOT(setEnabled(bool)));
    ui->stakingReserveBalance->setStyleSheet(QStringLiteral(
        "QAbstractSpinBox, QAbstractSpinBox QLineEdit {"
        " background-color: #ffffff; color: #111827;"
        " selection-background-color: #5b21b6; selection-color: #ffffff; }"));

    stakingSummaryLabel->setObjectName(QStringLiteral("StakingSummaryLabel"));
    bondTable->setObjectName(QStringLiteral("StakingBondTable"));
    bondTable->setColumnCount(4);
    bondTable->setHorizontalHeaderLabels(
        QStringList() << tr("Amount") << tr("Confirmations")
                      << tr("Status") << tr("Outpoint"));
    bondTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    bondTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    bondTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    bondTable->setAlternatingRowColors(true);
    bondTable->setMinimumHeight(150);
    bondTable->verticalHeader()->setVisible(false);
    bondTable->horizontalHeader()->setStretchLastSection(true);
    bondTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    bondTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    bondTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    refreshBondsButton->setObjectName(QStringLiteral("DialogSecondaryButton"));
    refreshBondsButton->setText(tr("Refresh"));
    refreshBondsButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refreshBondsButton->setToolTip(tr("Refresh bond status from the active chain."));
    unbondSelectedButton->setObjectName(QStringLiteral("AddressBookDangerButton"));
    unbondSelectedButton->setText(tr("Unbond selected"));
    unbondSelectedButton->setToolTip(
        tr("Begin unbonding each selected complete bond."));
    unbondAllButton->setObjectName(QStringLiteral("AddressBookDangerButton"));
    unbondAllButton->setText(tr("Unbond all"));
    unbondAllButton->setToolTip(
        tr("Disable automatic staking and begin unbonding every bond."));

    QHBoxLayout *bondActions = new QHBoxLayout();
    bondActions->addWidget(refreshBondsButton);
    bondActions->addStretch();
    bondActions->addWidget(unbondSelectedButton);
    bondActions->addWidget(unbondAllButton);
    ui->stakingLayout->addWidget(stakingSummaryLabel);
    ui->stakingLayout->addWidget(bondTable);
    ui->stakingLayout->addLayout(bondActions);

    connect(refreshBondsButton, SIGNAL(clicked()), this, SLOT(refreshStakeBonds()));
    connect(unbondSelectedButton, SIGNAL(clicked()), this, SLOT(unbondSelected()));
    connect(unbondAllButton, SIGNAL(clicked()), this, SLOT(unbondAll()));
    connect(bondTable, SIGNAL(itemSelectionChanged()),
            this, SLOT(updateBondActionState()));
    connect(stakingRefreshTimer, SIGNAL(timeout()),
            this, SLOT(refreshStakeBonds()));
    stakingRefreshTimer->setInterval(30000);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    static const uint64_t GiB = 1024 * 1024 * 1024;
    static const uint64_t nMinDiskSpace = MIN_DISK_SPACE_FOR_BLOCK_FILES / GiB +
                          (MIN_DISK_SPACE_FOR_BLOCK_FILES % GiB) ? 1 : 0;
    ui->pruneSize->setMinimum(nMinDiskSpace);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);
    ui->pruneWarning->setVisible(false);
    ui->pruneWarning->setStyleSheet("QLabel { color: red; }");

    ui->pruneSize->setEnabled(false);
    connect(ui->prune, SIGNAL(toggled(bool)), ui->pruneSize, SLOT(setEnabled(bool)));

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

    ui->proxyIpTor->setEnabled(false);
    ui->proxyPortTor->setEnabled(false);
    ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyIpTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyPortTor, SLOT(setEnabled(bool)));
    connect(ui->connectSocksTor, SIGNAL(toggled(bool)), this, SLOT(updateProxyValidationState()));

    /* Window elements init */
#ifdef Q_OS_MAC
    /* remove Window tab on Mac */
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
#endif

    /* remove Wallet tab in case of -disablewallet */
    if (!enableWallet) {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
    }

    /* Display elements init */
    QDir translations(":translations");

    ui->vergeAtStartup->setToolTip(ui->vergeAtStartup->toolTip().arg(tr(PACKAGE_NAME)));
    ui->vergeAtStartup->setText(ui->vergeAtStartup->text().arg(tr(PACKAGE_NAME)));

    ui->openVERGEConfButton->setToolTip(ui->openVERGEConfButton->toolTip().arg(tr(PACKAGE_NAME)));

    ui->lang->setToolTip(ui->lang->toolTip().arg(tr(PACKAGE_NAME)));
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString &langStr : translations.entryList())
    {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language - country (locale name)", e.g. "German - Germany (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" - ") + QLocale::countryToString(locale.country()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
        else
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language (locale name)", e.g. "German (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
    }
#if QT_VERSION >= 0x040700
    ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");
#endif

    ui->unit->setModel(new VERGEUnits(this));

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* setup/change UI elements when proxy IPs are invalid/valid */
    ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
    ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
    connect(ui->proxyIp, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPort, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
    connect(ui->proxyPortTor, SIGNAL(textChanged(const QString&)), this, SLOT(updateProxyValidationState()));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setWalletModel(WalletModel *_walletModel)
{
    walletModel = _walletModel;
    const bool available = walletModel != nullptr;
    ui->stakingGroup->setEnabled(available);
    if (!available) return;

    interfaces::Wallet& wallet = walletModel->wallet();
    initialStakingEnabled = wallet.isStakingEnabled();
    initialStakingReserve = wallet.getStakingReserveBalance();
    ui->enableStaking->setChecked(initialStakingEnabled);
    ui->stakingReserveBalance->setValue(initialStakingReserve);
    refreshStakeBonds();
    stakingRefreshTimer->start();
}

void OptionsDialog::refreshStakeBonds()
{
    QStringList selectedOutpoints;
    if (bondTable->selectionModel()) {
        for (const QModelIndex& index :
             bondTable->selectionModel()->selectedRows()) {
            QTableWidgetItem *item = bondTable->item(index.row(), 0);
            if (item) {
                selectedOutpoints.push_back(
                    item->data(Qt::UserRole).toString() +
                    QStringLiteral(":") +
                    QString::number(item->data(Qt::UserRole + 1).toULongLong()));
            }
        }
    }
    bondTable->setRowCount(0);
    if (!walletModel) {
        stakingSummaryLabel->setText(tr("No wallet loaded"));
        updateBondActionState();
        return;
    }

    const std::vector<interfaces::WalletStakeBond> bonds =
        walletModel->wallet().getStakeBonds();
    CAmount total = 0;
    CAmount eligible = 0;
    const int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    for (const interfaces::WalletStakeBond& bond : bonds) {
        const int row = bondTable->rowCount();
        bondTable->insertRow(row);
        total += bond.amount;
        if (bond.eligible) eligible += bond.amount;

        QTableWidgetItem *amount = new QTableWidgetItem(
            VERGEUnits::formatWithUnit(displayUnit, bond.amount, false,
                                       VERGEUnits::separatorAlways));
        amount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        amount->setData(Qt::UserRole, QString::fromStdString(bond.txid.GetHex()));
        amount->setData(Qt::UserRole + 1, static_cast<qulonglong>(bond.vout));
        const bool canUnbond =
            !bond.withdrawal_locked && !bond.eligibility_locked;
        amount->setData(Qt::UserRole + 2, canUnbond);
        bondTable->setItem(row, 0, amount);

        QTableWidgetItem *confirmations =
            new QTableWidgetItem(QString::number(bond.confirmations));
        confirmations->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        bondTable->setItem(row, 1, confirmations);

        QString status;
        if (bond.withdrawal_locked || bond.eligibility_locked) {
            status = tr("Locked");
        } else if (bond.eligible) {
            status = tr("Eligible");
        } else if (bond.mature) {
            status = tr("Awaiting snapshot");
        } else {
            status = tr("Maturing");
        }
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        bondTable->setItem(row, 2, statusItem);
        const QString outpoint =
            QString::fromStdString(bond.txid.GetHex()) + QStringLiteral(":") +
            QString::number(bond.vout);
        bondTable->setItem(row, 3, new QTableWidgetItem(outpoint));
        if (!canUnbond) {
            for (int column = 0; column < bondTable->columnCount(); ++column) {
                QTableWidgetItem *item = bondTable->item(row, column);
                item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
                item->setToolTip(tr("This bond is currently consensus-locked."));
            }
        } else if (selectedOutpoints.contains(outpoint)) {
            bondTable->selectionModel()->select(
                bondTable->model()->index(row, 0),
                QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }

    stakingSummaryLabel->setText(
        tr("%1 owned bonds | %2 bonded | %3 eligible")
            .arg(bonds.size())
            .arg(VERGEUnits::formatWithUnit(
                displayUnit, total, false, VERGEUnits::separatorAlways))
            .arg(VERGEUnits::formatWithUnit(
                displayUnit, eligible, false, VERGEUnits::separatorAlways)));
    updateBondActionState();
}

void OptionsDialog::updateBondActionState()
{
    const bool available = walletModel != nullptr;
    bool canUnbondAny = false;
    for (int row = 0; row < bondTable->rowCount(); ++row) {
        if (bondTable->item(row, 0)->data(Qt::UserRole + 2).toBool()) {
            canUnbondAny = true;
            break;
        }
    }
    unbondSelectedButton->setEnabled(
        available && !bondTable->selectionModel()->selectedRows().isEmpty());
    unbondAllButton->setEnabled(available && canUnbondAny);
    refreshBondsButton->setEnabled(available);
}

void OptionsDialog::unbondSelected()
{
    unbondRows(false);
}

void OptionsDialog::unbondAll()
{
    unbondRows(true);
}

void OptionsDialog::unbondRows(bool all)
{
    if (!walletModel) return;
    QModelIndexList rows;
    if (all) {
        for (int row = 0; row < bondTable->rowCount(); ++row) {
            if (bondTable->item(row, 0)->data(Qt::UserRole + 2).toBool()) {
                rows.push_back(bondTable->model()->index(row, 0));
            }
        }
    } else {
        rows = bondTable->selectionModel()->selectedRows();
    }
    if (rows.isEmpty()) return;

    struct SelectedBond {
        uint256 txid;
        uint32_t vout;
    };
    std::vector<SelectedBond> selectedBonds;
    selectedBonds.reserve(rows.size());
    CAmount total = 0;
    const std::vector<interfaces::WalletStakeBond> bonds =
        walletModel->wallet().getStakeBonds();
    for (const QModelIndex& index : rows) {
        const uint256 txid = uint256S(bondTable->item(index.row(), 0)
            ->data(Qt::UserRole).toString().toStdString());
        const QString txidText = QString::fromStdString(txid.GetHex());
        const uint32_t vout = static_cast<uint32_t>(
            bondTable->item(index.row(), 0)->data(Qt::UserRole + 1).toULongLong());
        for (const interfaces::WalletStakeBond& bond : bonds) {
            if (QString::fromStdString(bond.txid.GetHex()) == txidText &&
                bond.vout == vout) {
                total += bond.amount;
                break;
            }
        }
        selectedBonds.push_back(SelectedBond{txid, vout});
    }
    const int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    const QString amount = VERGEUnits::formatWithUnit(
        displayUnit, total, false, VERGEUnits::separatorAlways);
    const uint32_t unbondingBlocks =
        walletModel->wallet().getStakeUnbondingBlocks();
    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, all ? tr("Unbond all stake") : tr("Unbond selected stake"),
        tr("Begin unbonding %1 complete bond(s) totaling %2? The principal "
           "will remain locked for %3 accepted blocks, and transaction "
           "fees require separate available funds. Automatic staking will "
           "be disabled to prevent replacement bonds.")
            .arg(selectedBonds.size()).arg(amount)
            .arg(QLocale().toString(static_cast<qulonglong>(unbondingBlocks))),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) return;

    WalletModel::UnlockContext unlock(walletModel->requestUnlock());
    if (!unlock.isValid()) return;

    if (!walletModel->wallet().setStakingPolicy(
            false, walletModel->wallet().getStakingReserveBalance())) {
        QMessageBox::critical(this, tr("Staking settings"),
                              tr("Automatic staking could not be disabled."));
        return;
    }
    ui->enableStaking->setChecked(false);
    initialStakingEnabled = false;

    int completed = 0;
    int64_t latestUnlockHeight = 0;
    QStringList errors;
    for (const SelectedBond& bond : selectedBonds) {
        interfaces::WalletUnbond result;
        std::string error;
        if (walletModel->wallet().unbondStake(
                bond.txid, bond.vout, result, error)) {
            ++completed;
            latestUnlockHeight = std::max(latestUnlockHeight,
                                          result.unlock_height);
        } else {
            errors.push_back(QStringLiteral("%1:%2 - %3")
                .arg(QString::fromStdString(bond.txid.GetHex()))
                .arg(bond.vout)
                .arg(QString::fromStdString(error)));
        }
    }
    refreshStakeBonds();

    if (completed > 0) {
        QMessageBox::information(
            this, tr("Unbonding started"),
            tr("%1 unbond transaction(s) were created. The latest unlock "
               "height is %2.").arg(completed).arg(latestUnlockHeight));
    }
    if (!errors.isEmpty()) {
        QMessageBox::critical(
            this, tr("Unbonding error"),
            tr("Some bonds could not be unbonded:\n%1")
                .arg(errors.join(QStringLiteral("\n"))));
    }
}

void OptionsDialog::setModel(OptionsModel *_model)
{
    this->model = _model;

    if(_model)
    {
        /* check if client restart is needed and show persistent message */
        if (_model->isRestartRequired())
            showRestartWarning(true);

        QString strLabel = _model->getOverriddenByCommandLine();
        if (strLabel.isEmpty())
            strLabel = tr("none");
        ui->overriddenByCommandLineLabel->setText(strLabel);

        mapper->setModel(_model);
        setMapper();
        mapper->toFirst();

        updateDefaultProxyNets();
    }

    /* warn when one of the following settings changes by user action (placed here so init via mapper doesn't trigger them) */

    /* Main */
    connect(ui->prune, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->prune, SIGNAL(clicked(bool)), this, SLOT(togglePruneWarning(bool)));
    connect(ui->pruneSize, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    connect(ui->databaseCache, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    connect(ui->threadsScriptVerif, SIGNAL(valueChanged(int)), this, SLOT(showRestartWarning()));
    /* Wallet */
    connect(ui->spendZeroConfChange, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Network */
    connect(ui->allowIncoming, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    connect(ui->connectSocksTor, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning()));
    /* Display */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
    connect(ui->thirdPartyTxUrls, SIGNAL(textChanged(const QString &)), this, SLOT(showRestartWarning()));
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->vergeAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    mapper->addMapping(ui->prune, OptionsModel::Prune);
    mapper->addMapping(ui->pruneSize, OptionsModel::PruneSize);

    /* Wallet */
    mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
    mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
    mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
}

void OptionsDialog::setOkButtonState(bool fState)
{
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_resetButton_clicked()
{
    if(model)
    {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm options reset"),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if(btnRetVal == QMessageBox::Cancel)
            return;

        /* reset all options and close GUI */
        model->Reset();
        QApplication::quit();
    }
}

void OptionsDialog::on_openVERGEConfButton_clicked()
{
    /* explain the purpose of the config file */
    QMessageBox::information(this, tr("Configuration options"),
        tr("The configuration file is used to specify advanced user options which override GUI settings. "
           "Additionally, any command-line options will override this configuration file."));

    /* show an error if there was some problem opening the file */
    if (!GUIUtil::openVERGEConf())
        QMessageBox::critical(this, tr("Error"), tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked()
{
    if (walletModel) {
        if (!ui->stakingReserveBalance->validate()) return;
        const CAmount reserve = ui->stakingReserveBalance->value();
        const bool enabled = ui->enableStaking->isChecked();
        if (enabled && (!initialStakingEnabled ||
                        reserve < initialStakingReserve)) {
            const QString amount = VERGEUnits::formatWithUnit(
                VERGEUnits::XVG, reserve, false,
                VERGEUnits::separatorAlways);
            const QMessageBox::StandardButton choice = QMessageBox::question(
                this, tr("Enable automatic staking"),
                tr("The wallet may create an on-chain bond from mature "
                   "spendable funds above the amount kept available (%1). "
                   "Existing bonds will not be resized. Continue?").arg(amount),
                QMessageBox::Yes | QMessageBox::Cancel,
                QMessageBox::Cancel);
            if (choice != QMessageBox::Yes) return;
        }
        if (!walletModel->wallet().setStakingPolicy(
                enabled, reserve)) {
            QMessageBox::critical(this, tr("Staking settings"),
                                  tr("The staking settings could not be saved."));
            return;
        }
    }
    mapper->submit();
    accept();
    updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        ui->minimizeToTray->setChecked(false);
        ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::togglePruneWarning(bool enabled)
{
    ui->pruneWarning->setVisible(!ui->pruneWarning->isVisible());
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
        // clear non-persistent status label after 10 seconds
        // Todo: should perhaps be a class attribute, if we extend the use of statusLabel
        QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
    }
}

void OptionsDialog::clearStatusLabel()
{
    ui->statusLabel->clear();
    if (model && model->isRestartRequired()) {
        showRestartWarning(true);
    }
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
    QValidatedLineEdit *otherProxyWidget = (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
    if (pUiProxyIp->isValid() && (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0) && (!ui->proxyPortTor->isEnabled() || ui->proxyPortTor->text().toInt() > 0))
    {
        setOkButtonState(otherProxyWidget->isValid()); //only enable ok button if both proxys are valid
        clearStatusLabel();
    }
    else
    {
        setOkButtonState(false);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    proxyType proxy;
    std::string strProxy;
    QString strDefaultProxyGUI;

    model->node().getProxy(NET_IPV4, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv4->setChecked(true) : ui->proxyReachIPv4->setChecked(false);

    model->node().getProxy(NET_IPV6, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachIPv6->setChecked(true) : ui->proxyReachIPv6->setChecked(false);

    model->node().getProxy(NET_TOR, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachTor->setChecked(true) : ui->proxyReachTor->setChecked(false);
	
	model->node().getProxy(NET_I2P, proxy);
    strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
    strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
    (strProxy == strDefaultProxyGUI.toStdString()) ? ui->proxyReachI2p->setChecked(true) : ui->proxyReachI2p->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString().c_str(), DEFAULT_GUI_PROXY_PORT));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
