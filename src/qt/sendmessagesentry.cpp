// Copyright (c) 2026 The Verge Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/sendmessagesentry.h>
#include <qt/forms/ui_sendmessagesentry.h>

#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/messagemodel.h>
#include <qt/walletmodel.h>
#include <smsg/smessage.h>

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>

SendMessagesEntry::SendMessagesEntry(QWidget* parent) :
    QFrame(parent),
    ui(new Ui::SendMessagesEntry),
    model(nullptr),
    messageCountLabel(nullptr)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
    ui->addressBookButton->setIcon(QIcon());
    ui->pasteButton->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());
#endif

    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    ui->sendTo->setPlaceholderText(tr("Recipient Verge address"));
    ui->publicKey->setPlaceholderText(tr("Auto-fills if known, otherwise paste recipient pubkey"));
    ui->publicKey->setToolTip(tr("Recipient public key. If this address is already known locally, the field fills automatically."));
    ui->messageText->setPlaceholderText(tr("Type an encrypted message"));

    messageCountLabel = new QLabel(this);
    messageCountLabel->setStyleSheet(QStringLiteral("background-color: rgb(0, 0, 0); color: rgb(92, 255, 122);"));
    ui->gridLayout->addWidget(messageCountLabel, 7, 2, 1, 1, Qt::AlignRight);

    connect(ui->messageText, SIGNAL(textChanged()), this, SLOT(updateMessageCountdown()));
    updateMessageCountdown();
}

SendMessagesEntry::~SendMessagesEntry()
{
    delete ui;
}

void SendMessagesEntry::setModel(MessageModel* modelIn)
{
    model = modelIn;
    clear();
}

void SendMessagesEntry::clear()
{
    ui->sendTo->clear();
    ui->addAsLabel->clear();
    ui->publicKey->clear();
    ui->messageText->clear();
    ui->messageText->setErrorText(tr("The message cannot be empty."));
    updateMessageCountdown();
}

bool SendMessagesEntry::validate()
{
    if (!model) {
        return false;
    }

    bool valid = true;
    QString sendTo = ui->sendTo->text().trimmed();
    QString publicKey = ui->publicKey->text().trimmed();

    if (!model->getWalletModel()->validateAddress(sendTo)) {
        ui->sendTo->setValid(false);
        valid = false;
    }

    if (ui->messageText->toPlainText().trimmed().isEmpty()) {
        ui->messageText->setValid(false);
        valid = false;
    }

    if (valid && publicKey.isEmpty()) {
        if (!resolveKnownPublicKey(sendTo, true)) {
            ui->publicKey->setValid(false);
            valid = false;
        }
    }

    return valid;
}

SendMessagesRecipient SendMessagesEntry::getValue()
{
    SendMessagesRecipient recipient;
    recipient.address = ui->sendTo->text().trimmed();
    recipient.label = ui->addAsLabel->text().trimmed();
    recipient.pubkey = ui->publicKey->text().trimmed();
    recipient.message = ui->messageText->toPlainText().trimmed();
    return recipient;
}

bool SendMessagesEntry::isClear()
{
    return ui->sendTo->text().trimmed().isEmpty()
        && ui->publicKey->text().trimmed().isEmpty()
        && ui->messageText->toPlainText().trimmed().isEmpty();
}

void SendMessagesEntry::setValue(const SendMessagesRecipient& value)
{
    ui->sendTo->setText(value.address);
    ui->addAsLabel->setText(value.label);
    ui->publicKey->setText(value.pubkey);
    static_cast<QPlainTextEdit*>(ui->messageText)->setPlainText(value.message);
    updateMessageCountdown();
}

void SendMessagesEntry::loadRow(int row)
{
    if (!model) {
        return;
    }

    const QModelIndex typeIndex = model->index(row, MessageModel::Type, QModelIndex());
    const bool received = model->data(typeIndex, Qt::DisplayRole).toString() == MessageModel::Received;
    const QString sendTo = model->data(model->index(row, received ? MessageModel::FromAddress : MessageModel::ToAddress, QModelIndex()), Qt::DisplayRole).toString();
    const QString label = model->data(model->index(row, MessageModel::Label, QModelIndex()), Qt::DisplayRole).toString();

    ui->sendTo->setText(sendTo);
    if (!label.isEmpty() && label != tr("(no label)")) {
        ui->addAsLabel->setText(label);
    }
}

void SendMessagesEntry::setRemoveEnabled(bool enabled)
{
    ui->deleteButton->setEnabled(enabled);
}

QWidget* SendMessagesEntry::setupTabChain(QWidget* prev)
{
    QWidget::setTabOrder(prev, ui->sendTo);
    QWidget::setTabOrder(ui->sendTo, ui->addAsLabel);
    QWidget::setTabOrder(ui->addAsLabel, ui->publicKey);
    QWidget::setTabOrder(ui->publicKey, ui->messageText);
    QWidget::setTabOrder(ui->messageText, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendMessagesEntry::setFocus()
{
    ui->sendTo->setFocus();
}

void SendMessagesEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

void SendMessagesEntry::on_pasteButton_clicked()
{
    ui->sendTo->setText(QApplication::clipboard()->text());
}

void SendMessagesEntry::on_addressBookButton_clicked()
{
    if (!model) {
        return;
    }

    AddressBookPage dlg(model->getWalletModel()->getPlatformStyle(), AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getWalletModel()->getAddressTableModel());
    if (dlg.exec()) {
        ui->sendTo->setText(dlg.getReturnValue());
        ui->messageText->setFocus();
    }
}

void SendMessagesEntry::on_sendTo_textChanged(const QString& address)
{
    updateLabel(address);
    resolveKnownPublicKey(address.trimmed(), true);
}

bool SendMessagesEntry::updateLabel(const QString& address)
{
    if (!model) {
        return false;
    }

    const QString associatedLabel = model->getWalletModel()->getAddressTableModel()->labelForAddress(address);
    if (!associatedLabel.isEmpty()) {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

bool SendMessagesEntry::resolveKnownPublicKey(const QString& address, bool updateField)
{
    if (!model || address.isEmpty()) {
        return false;
    }

    QString resolvedAddress = address;
    QString resolvedPubKey;
    if (!model->getAddressOrPubkey(resolvedAddress, resolvedPubKey) || resolvedPubKey.isEmpty()) {
        return false;
    }

    if (updateField && ui->publicKey->text().trimmed() != resolvedPubKey) {
        const QSignalBlocker blocker(ui->publicKey);
        ui->publicKey->setText(resolvedPubKey);
    }

    return true;
}

void SendMessagesEntry::enforceMessageLimit()
{
    QString text = ui->messageText->toPlainText();
    QByteArray utf8 = text.toUtf8();
    if (utf8.size() <= static_cast<int>(smsg::SMSG_MAX_MSG_BYTES)) {
        return;
    }

    utf8.truncate(smsg::SMSG_MAX_MSG_BYTES);
    const QString truncated = QString::fromUtf8(utf8);
    if (truncated == text) {
        return;
    }

    ui->messageText->blockSignals(true);
    ui->messageText->setPlainText(truncated);
    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->messageText->setTextCursor(cursor);
    ui->messageText->blockSignals(false);
}

void SendMessagesEntry::updateMessageCountdown()
{
    enforceMessageLimit();
    if (!messageCountLabel) {
        return;
    }

    const int remaining = static_cast<int>(smsg::SMSG_MAX_MSG_BYTES) - ui->messageText->toPlainText().toUtf8().size();
    messageCountLabel->setText(tr("Characters left: %1").arg(remaining));
}
