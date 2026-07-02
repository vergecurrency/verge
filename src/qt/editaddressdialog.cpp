// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/editaddressdialog.h>
#include <qt/forms/ui_editaddressdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>

#include <QDataWidgetMapper>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>

namespace {
bool ReceivingLabelExists(const AddressTableModel* model, const QString& label, const QString& exceptAddress)
{
    const QString trimmedLabel = label.trimmed();
    if (!model || trimmedLabel.isEmpty()) {
        return false;
    }

    for (int row = 0; row < model->rowCount(QModelIndex()); ++row) {
        const QModelIndex labelIndex = model->index(row, AddressTableModel::Label, QModelIndex());
        if (model->data(labelIndex, AddressTableModel::TypeRole).toString() != AddressTableModel::Receive) {
            continue;
        }

        const QString address = model->data(model->index(row, AddressTableModel::Address, QModelIndex()), Qt::DisplayRole).toString();
        if (address == exceptAddress) {
            continue;
        }

        if (model->data(labelIndex, Qt::EditRole).toString().trimmed().compare(trimmedLabel, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}
}

EditAddressDialog::EditAddressDialog(Mode _mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditAddressDialog),
    mapper(0),
    mode(_mode),
    model(0)
{
    ui->setupUi(this);
    GUIUtil::EnableThemedDialogChrome(this);
    setObjectName("EditAddressDialog");
    setMinimumSize(560, 220);
    ui->labelEdit->setObjectName("AddressBookFormField");
    ui->addressEdit->setObjectName("AddressBookFormField");
    if (QPushButton* ok_button = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok_button->setObjectName("DialogPrimaryButton");
    }
    if (QPushButton* cancel_button = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancel_button->setObjectName("DialogSecondaryButton");
    }

    GUIUtil::setupAddressWidget(ui->addressEdit, this);

    switch(mode)
    {
    case NewSendingAddress:
        setWindowTitle(tr("New sending address"));
        break;
    case EditReceivingAddress:
        setWindowTitle(tr("Edit receiving address"));
        ui->addressEdit->setEnabled(false);
        break;
    case EditSendingAddress:
        setWindowTitle(tr("Edit sending address"));
        break;
    }

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
}

EditAddressDialog::~EditAddressDialog()
{
    delete ui;
}

void EditAddressDialog::setModel(AddressTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    mapper->setModel(_model);
    mapper->addMapping(ui->labelEdit, AddressTableModel::Label);
    mapper->addMapping(ui->addressEdit, AddressTableModel::Address);
}

void EditAddressDialog::loadRow(int row)
{
    mapper->setCurrentIndex(row);
}

bool EditAddressDialog::saveCurrentRow()
{
    if(!model)
        return false;

    switch(mode)
    {
    case NewSendingAddress:
        address = model->addRow(
                AddressTableModel::Send,
                ui->labelEdit->text(),
                ui->addressEdit->text(),
                model->GetDefaultAddressType());
        break;
    case EditReceivingAddress:
    case EditSendingAddress:
        if(mapper->submit())
        {
            address = ui->addressEdit->text();
        }
        break;
    }
    return !address.isEmpty();
}

void EditAddressDialog::accept()
{
    if(!model)
        return;

    if (mode == EditReceivingAddress && ReceivingLabelExists(model, ui->labelEdit->text(), ui->addressEdit->text())) {
        QMessageBox::warning(this, windowTitle(),
            tr("The label \"%1\" already exists. Choose a different label.").arg(ui->labelEdit->text().trimmed()),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    if(!saveCurrentRow())
    {
        switch(model->getEditStatus())
        {
        case AddressTableModel::OK:
            // Failed with unknown reason. Just reject.
            break;
        case AddressTableModel::NO_CHANGES:
            // No changes were made during edit operation. Just reject.
            break;
        case AddressTableModel::INVALID_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                tr("The entered address \"%1\" is not a valid VERGE address.").arg(ui->addressEdit->text()),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::DUPLICATE_ADDRESS:
            QMessageBox::warning(this, windowTitle(),
                getDuplicateAddressWarning(),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::WALLET_UNLOCK_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("Could not unlock wallet."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;
        case AddressTableModel::KEY_GENERATION_FAILURE:
            QMessageBox::critical(this, windowTitle(),
                tr("New key generation failed."),
                QMessageBox::Ok, QMessageBox::Ok);
            break;

        }
        return;
    }
    QDialog::accept();
}

QString EditAddressDialog::getDuplicateAddressWarning() const
{
    QString dup_address = ui->addressEdit->text();
    QString existing_label = model->labelForAddress(dup_address);
    QString existing_purpose = model->purposeForAddress(dup_address);

    if (existing_purpose == "receive" &&
            (mode == NewSendingAddress || mode == EditSendingAddress)) {
        return tr(
            "Address \"%1\" already exists as a receiving address with label "
            "\"%2\" and so cannot be added as a sending address."
            ).arg(dup_address).arg(existing_label);
    }
    return tr(
        "The entered address \"%1\" is already in the address book with "
        "label \"%2\"."
        ).arg(dup_address).arg(existing_label);
}

QString EditAddressDialog::getAddress() const
{
    return address;
}

void EditAddressDialog::setAddress(const QString &_address)
{
    this->address = _address;
    ui->addressEdit->setText(_address);
}
