// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactiondescdialog.h>
#include <qt/forms/ui_transactiondescdialog.h>

#include <qt/transactiontablemodel.h>

#include <QDialogButtonBox>
#include <QModelIndex>
#include <QPushButton>
#include <QTextDocument>

TransactionDescDialog::TransactionDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TransactionDescDialog)
{
    ui->setupUi(this);
    setMinimumSize(760, 420);
    ui->detailText->setObjectName("TransactionDetailsText");
    ui->detailText->document()->setDocumentMargin(14);
    if (QPushButton* close_button = ui->buttonBox->button(QDialogButtonBox::Close)) {
        close_button->setObjectName("DialogSecondaryButton");
    }
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
