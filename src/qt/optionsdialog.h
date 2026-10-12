// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_OPTIONSDIALOG_H
#define VERGE_QT_OPTIONSDIALOG_H

#include <amount.h>

#include <QDialog>
#include <QValidator>

class OptionsModel;
class QValidatedLineEdit;
class WalletModel;

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;
QT_END_NAMESPACE

namespace Ui {
class OptionsDialog;
}

/** Proxy address widget validator, checks for a valid proxy address.
 */
class ProxyAddressValidator : public QValidator
{
    Q_OBJECT

public:
    explicit ProxyAddressValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Preferences dialog. */
class OptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget *parent, bool enableWallet);
    ~OptionsDialog();

    void setModel(OptionsModel *model);
    void setWalletModel(WalletModel *walletModel);
    void setMapper();

private Q_SLOTS:
    /* set OK button state (enabled / disabled) */
    void setOkButtonState(bool fState);
    void on_resetButton_clicked();
    void on_openVERGEConfButton_clicked();
    void on_okButton_clicked();
    void on_cancelButton_clicked();
    
    void on_hideTrayIcon_stateChanged(int fState);

    void togglePruneWarning(bool enabled);
    void showRestartWarning(bool fPersistent = false);
    void clearStatusLabel();
    void updateProxyValidationState();
    /* query the networks, for which the default proxy is used */
    void updateDefaultProxyNets();
    void refreshStakeBonds();
    void updateBondActionState();
    void unbondSelected();
    void unbondAll();

Q_SIGNALS:
    void proxyIpChecks(QValidatedLineEdit *pUiProxyIp, int nProxyPort);

private:
    Ui::OptionsDialog *ui;
    OptionsModel *model;
    WalletModel *walletModel;
    bool initialStakingEnabled;
    CAmount initialStakingReserve;
    QDataWidgetMapper *mapper;
    QLabel *stakingSummaryLabel;
    QTableWidget *bondTable;
    QPushButton *refreshBondsButton;
    QPushButton *unbondSelectedButton;
    QPushButton *unbondAllButton;
    QTimer *stakingRefreshTimer;

    void unbondRows(bool all);
};

#endif // VERGE_QT_OPTIONSDIALOG_H
