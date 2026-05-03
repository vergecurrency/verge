// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/openuridialog.h>
#include <qt/forms/ui_openuridialog.h>

#include <qt/guiutil.h>
#include <qt/walletmodel.h>

#include <QUrl>
#include <QDialogButtonBox>
#include <QPushButton>

OpenURIDialog::OpenURIDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenURIDialog)
{
    ui->setupUi(this);
    GUIUtil::EnableThemedDialogChrome(this);
    setObjectName("OpenURIDialog");
    setMinimumSize(620, 190);
    ui->uriEdit->setObjectName("OpenUriField");
    if (QPushButton* ok_button = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok_button->setObjectName("DialogPrimaryButton");
    }
    if (QPushButton* cancel_button = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancel_button->setObjectName("DialogSecondaryButton");
    }
#if QT_VERSION >= 0x040700
    ui->uriEdit->setPlaceholderText("verge:");
#endif
}

OpenURIDialog::~OpenURIDialog()
{
    delete ui;
}

QString OpenURIDialog::getURI()
{
    return ui->uriEdit->text();
}

void OpenURIDialog::accept()
{
    SendCoinsRecipient rcp;
    if(GUIUtil::parseVERGEURI(getURI(), &rcp))
    {
        /* Only accept value URIs */
        QDialog::accept();
    } else {
        ui->uriEdit->setValid(false);
    }
}
