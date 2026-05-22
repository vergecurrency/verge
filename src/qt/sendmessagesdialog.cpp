#include <qt/sendmessagesdialog.h>
#include <qt/forms/ui_sendmessagesdialog.h>
#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/messagemodel.h>
#include <qt/optionsmodel.h>
#include <qt/sendmessagesentry.h>
#include <qt/walletmodel.h>
#include <smsg/smessage.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDataWidgetMapper>
#include <QLocale>
#include <QMessageBox>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextDocument>

SendMessagesDialog::SendMessagesDialog(Mode mode, Type type, QWidget* parent) : QDialog(parent),
                                                                                ui(new Ui::SendMessagesDialog),
                                                                                model(0),
                                                                                mode(mode),
                                                                                type(type)
{
    ui->setupUi(this);
#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
#endif
#if QT_VERSION >= 0x040700
    if (mode == SendMessagesDialog::Encrypted)
        ui->addressFrom->setToolTip(tr("Choose one of your local message-enabled addresses."));
#endif
    addEntry();
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(reject()));
    fNewRecipientAllowed = true;
    if (mode == SendMessagesDialog::Anonymous)
        ui->frameAddressFrom->hide();
    if (type == SendMessagesDialog::Page)
        ui->closeButton->hide();
}
void SendMessagesDialog::setModel(MessageModel* model)
{
    this->model = model;
    refreshAddressFromChoices();
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(i)->widget());
        if (entry)
            entry->setModel(model);
    }
}
void SendMessagesDialog::loadRow(int row)
{
    if (model->data(model->index(row, model->Type, QModelIndex()), Qt::DisplayRole).toString() == MessageModel::Received)
        setSelectedAddressFrom(model->data(model->index(row, model->ToAddress, QModelIndex()), Qt::DisplayRole).toString());
    else
        setSelectedAddressFrom(model->data(model->index(row, model->FromAddress, QModelIndex()), Qt::DisplayRole).toString());
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(i)->widget());
        if (entry)
            entry->loadRow(row);
    }
}
bool SendMessagesDialog::checkMode(Mode mode)
{
    return (mode == this->mode);
}
bool SendMessagesDialog::validate()
{
    if (mode == SendMessagesDialog::Encrypted && ui->addressFrom->currentData().toString().isEmpty()) {
        return false;
    }
    return true;
}
SendMessagesDialog::~SendMessagesDialog()
{
    delete ui;
}
void SendMessagesDialog::on_pasteButton_clicked()
{
    setSelectedAddressFrom(QApplication::clipboard()->text().trimmed());
}
void SendMessagesDialog::on_addressBookButton_clicked()
{
    if (!model)
        return;
    AddressBookPage dlg(model->getWalletModel()->getPlatformStyle(), AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
    dlg.setModel(model->getWalletModel()->getAddressTableModel());
    if (dlg.exec()) {
        setSelectedAddressFrom(dlg.getReturnValue());
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(0)->widget());
        entry->setFocus();
        // findChild( const QString "sentTo")->setFocus();
    }
}
void SendMessagesDialog::on_sendButton_clicked()
{
    QList<SendMessagesRecipient> recipients;
    bool valid = true;
    if (!model)
        return;
    valid = validate();
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            if (entry->validate())
                recipients.append(entry->getValue());
            else
                valid = false;
        }
    }
    if (!valid || recipients.isEmpty()) {
        QMessageBox::warning(this, tr("Send Secure Message"),
            tr("A recipient address, an encrypted message, and a known recipient public key are required. If the key is already known, it will auto-fill when you enter the address."),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    // Format confirmation message
    QStringList formatted;
    for (const SendMessagesRecipient& rcp : recipients) {
        const QString label = rcp.label.toHtmlEscaped();
        const QString address = rcp.address.toHtmlEscaped();
        formatted.append(tr("<b>%1</b> to %2 (%3)").arg(rcp.message.toHtmlEscaped(), label, address));
    }
    fNewRecipientAllowed = false;
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send messages"),
        tr("Are you sure you want to send %1?").arg(formatted.join(tr(" and "))),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (retval != QMessageBox::Yes) {
        fNewRecipientAllowed = true;
        return;
    }
    MessageModel::StatusCode sendstatus;
    if (mode == SendMessagesDialog::Anonymous)
        sendstatus = model->sendMessages(recipients);
    else
        sendstatus = model->sendMessages(recipients, ui->addressFrom->currentData().toString());
    switch (sendstatus) {
    case MessageModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Message"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case MessageModel::InvalidMessage:
        QMessageBox::warning(this, tr("Send Message"),
            tr("The message can't be empty."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case MessageModel::DuplicateAddress:
        QMessageBox::warning(this, tr("Send Message"),
            tr("Duplicate address found, you can only send to each address once, per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case MessageModel::MessageCreationFailed:
        QMessageBox::warning(this, tr("Send Message"),
            tr("Error: Message creation failed."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case MessageModel::MessageCommitFailed:
        QMessageBox::warning(this, tr("Send Message"),
            tr("Error: The message was rejected."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case MessageModel::Aborted: // User aborted, nothing to do
        break;
    case MessageModel::FailedErrorShown: // Send failed, error message was displayed
        break;
    case MessageModel::OK:
        accept();
        break;
    }
    fNewRecipientAllowed = true;
}
void SendMessagesDialog::clear()
{
    // Remove entries until only one left
    while (ui->entries->count())
        delete ui->entries->takeAt(0)->widget();
    addEntry();
    updateRemoveEnabled();
    ui->sendButton->setDefault(true);
}
void SendMessagesDialog::reject()
{
    if (type == SendMessagesDialog::Dialog)
        done(1);
    else
        clear();
}
void SendMessagesDialog::accept()
{
    if (type == SendMessagesDialog::Dialog)
        done(0);
    else
        clear();
}
void SendMessagesDialog::done(int retval)
{
    if (type == SendMessagesDialog::Dialog)
        QDialog::done(retval);
    else
        clear();
}
SendMessagesEntry* SendMessagesDialog::addEntry()
{
    SendMessagesEntry* entry = new SendMessagesEntry(this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendMessagesEntry*)), this, SLOT(removeEntry(SendMessagesEntry*)));
    updateRemoveEnabled();
    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if (bar && isVisible())
        bar->setSliderPosition(bar->maximum());
    return entry;
}
void SendMessagesDialog::refreshAddressFromChoices()
{
    const QString selectedAddress = ui->addressFrom->currentData().toString();
    const QSignalBlocker blocker(ui->addressFrom);
    ui->addressFrom->clear();

#ifdef ENABLE_WALLET
    if (!model || !smsg::fSecMsgEnabled || !smsgModule.pwallet) {
        return;
    }

    LOCK(smsgModule.cs_smsg);
    for (const auto& smsgAddress : smsgModule.addresses) {
        if (!smsgAddress.fReceiveEnabled) {
            continue;
        }

        const QString address = QString::fromStdString(CBitcoinAddress(smsgAddress.address).ToString());
        if (address.isEmpty()) {
            continue;
        }

        QString label;
        if (model && model->getWalletModel() && model->getWalletModel()->getAddressTableModel()) {
            label = model->getWalletModel()->getAddressTableModel()->labelForAddress(address).trimmed();
        }

        const QString display = label.isEmpty()
            ? address
            : tr("%1 (%2)").arg(label, address);
        ui->addressFrom->addItem(display, address);
    }
#endif

    setSelectedAddressFrom(selectedAddress);
}

void SendMessagesDialog::setSelectedAddressFrom(const QString& address)
{
    if (address.isEmpty()) {
        if (ui->addressFrom->count() > 0) {
            ui->addressFrom->setCurrentIndex(0);
        }
        return;
    }

    const int index = ui->addressFrom->findData(address);
    if (index >= 0) {
        ui->addressFrom->setCurrentIndex(index);
    } else if (ui->addressFrom->count() > 0) {
        ui->addressFrom->setCurrentIndex(0);
    }
}
void SendMessagesDialog::updateRemoveEnabled()
{
    // Remove buttons are enabled as soon as there is more than one send-entry
    bool enabled = (ui->entries->count() > 1);
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(i)->widget());
        if (entry)
            entry->setRemoveEnabled(enabled);
    }
    setupTabChain(0);
}
void SendMessagesDialog::removeEntry(SendMessagesEntry* entry)
{
    delete entry;
    updateRemoveEnabled();
}
QWidget* SendMessagesDialog::setupTabChain(QWidget* prev)
{
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendMessagesEntry* entry = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->sendButton);
    return ui->sendButton;
}
void SendMessagesDialog::pasteEntry(const SendMessagesRecipient& rv)
{
    if (!fNewRecipientAllowed)
        return;
    SendMessagesEntry* entry = 0;
    // Replace the first entry if it is still unused
    if (ui->entries->count() == 1) {
        SendMessagesEntry* first = qobject_cast<SendMessagesEntry*>(ui->entries->itemAt(0)->widget());
        if (first->isClear())
            entry = first;
    }
    if (!entry)
        entry = addEntry();
    entry->setValue(rv);
}
