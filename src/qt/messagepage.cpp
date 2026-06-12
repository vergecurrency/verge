#include <qt/messagepage.h>
#include <qt/addressbookpage.h>
#include <qt/forms/ui_messagepage.h>
#include <qt/sendmessagesdialog.h>
#include <qt/messagemodel.h>
#include <qt/vergegui.h>
#include <qt/walletmodel.h>
#include <qt/csvmodelwriter.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <smsg/smessage.h>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QToolBar>
#include <QMenu>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTimer>
#include <QCheckBox>
#include <QSpinBox>
#include <QTextDocument>
#include <logging.h>
#include <sstream>
#define NUM_ITEMS 3

static QString BuildConversationHtml(const QString& body, bool outgoing)
{
    const QString align = outgoing ? QStringLiteral("right") : QStringLiteral("left");
    return QStringLiteral("<p align=\"%1\" style=\"color:#5CFF7A; font-family:'Consolas','Courier New',monospace; font-size:22px; line-height:1.45; margin:0;\">%2</p>")
        .arg(align, body);
}

static QString FormatStorageUsage(uint64_t bytes)
{
    static const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    size_t suffix = 0;
    while (size >= 1024.0 && suffix < 4) {
        size /= 1024.0;
        ++suffix;
    }

    std::ostringstream oss;
    if (suffix == 0) {
        oss << static_cast<uint64_t>(size);
    } else {
        oss.setf(std::ios::fixed);
        oss.precision(2);
        oss << size;
    }
    oss << ' ' << suffixes[suffix];
    return QString::fromStdString(oss.str());
}

class MessageViewDelegate : public QStyledItemDelegate
{
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};
 void MessageViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem optionCopy = option;
    initStyleOption(&optionCopy, index);
 QStyle *style = optionCopy.widget? optionCopy.widget->style() : QApplication::style();
 QTextDocument doc;
    const bool outgoing = index.data(MessageModel::TypeRole).toInt() != 1;
    doc.setHtml(BuildConversationHtml(index.data(MessageModel::HTMLRole).toString(), outgoing));
    optionCopy.state &= ~QStyle::State_Selected;
     /// Painting item without text
    optionCopy.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &optionCopy, painter);
     QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette.setColor(QPalette::Text, QColor(QStringLiteral("#5CFF7A")));
     QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionCopy);
    doc.setTextWidth( textRect.width() );
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}
 QSize MessageViewDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);
     QTextDocument doc;
    const bool outgoing = index.data(MessageModel::TypeRole).toInt() != 1;
    doc.setHtml(BuildConversationHtml(index.data(MessageModel::HTMLRole).toString(), outgoing));
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height() + 20);
}
MessagePage::MessagePage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessagePage),
    model(0),
    platformStyle(platformStyle),
    msgdelegate(new MessageViewDelegate()),
    flushButton(nullptr),
    addressBookButton(nullptr),
    storageLabel(nullptr),
    messageCountLabel(nullptr),
    receiptLink(nullptr),
    paidMessageCheckBox(nullptr),
    retentionDaysSpinBox(nullptr),
    paidFeeLabel(nullptr),
    storageRefreshTimer(nullptr)
{
    LogPrintf("GUI: MessagePage ctor begin\n");
    ui->setupUi(this);
   
    
#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->deleteButton->setIcon(QIcon());
#endif
    // Context menu actions
    replyAction           = new QAction(ui->sendButton->text(),            this);
    copyFromAddressAction = new QAction(ui->copyFromAddressButton->text(), this);
    copyToAddressAction   = new QAction(ui->copyToAddressButton->text(),   this);
    deleteAction          = new QAction(ui->deleteButton->text(),          this);
     // Build context menu
    contextMenu = new QMenu();
     contextMenu->addAction(replyAction);
    contextMenu->addAction(copyFromAddressAction);
    contextMenu->addAction(copyToAddressAction);
    contextMenu->addAction(deleteAction);
     connect(replyAction,           SIGNAL(triggered()), this, SLOT(on_sendButton_clicked()));
    connect(copyFromAddressAction, SIGNAL(triggered()), this, SLOT(on_copyFromAddressButton_clicked()));
    connect(copyToAddressAction,   SIGNAL(triggered()), this, SLOT(on_copyToAddressButton_clicked()));
    connect(deleteAction,          SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked()));
     connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
     // Show Messages
    ui->listConversation->setItemDelegate(msgdelegate);
    ui->listConversation->setIconSize(QSize(MESSAGE_DECORATION_SIZE, MESSAGE_DECORATION_SIZE));
    ui->listConversation->setMinimumHeight(NUM_ITEMS * (MESSAGE_DECORATION_SIZE + 2));
    ui->listConversation->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listConversation->setStyleSheet("QListView { background-color: #000000; color: #5CFF7A; border: 1px solid #11331A; }");
    receiptLink = new QLabel(this);
    receiptLink->setText(QStringLiteral("<a href=\"receipt\" style=\"color:#5CFF7A;\">receipt</a>"));
    receiptLink->setTextFormat(Qt::RichText);
    receiptLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    receiptLink->setOpenExternalLinks(false);
    receiptLink->setVisible(false);
    receiptLink->setToolTip(tr("Show paid v3 message receipt"));
    ui->horizontalLayout_3->insertWidget(ui->horizontalLayout_3->count() - 1, receiptLink);
    connect(receiptLink, SIGNAL(linkActivated(QString)), this, SLOT(showReceipt()));

    auto* plainMessageEdit = qobject_cast<QPlainTextEdit*>(ui->messageEdit);
    if (plainMessageEdit) {
        plainMessageEdit->setStyleSheet("QPlainTextEdit { background-color: #000000; color: #5CFF7A; border: 1px solid #11331A; font-family: 'Consolas', 'Courier New', monospace; font-size: 14pt; }");
    }

    flushButton = new QPushButton(tr("F&lush Storage"), this);
    flushButton->setToolTip(tr("Delete locally stored secure messages and queued message data"));
    flushButton->setStyleSheet("background-color: rgb(80, 0, 120); color: rgb(255, 255, 255);");
    ui->horizontalLayout->insertWidget(ui->horizontalLayout->count() - 1, flushButton);
    connect(flushButton, SIGNAL(clicked()), this, SLOT(on_flushButton_clicked()));

    storageLabel = new QLabel(this);
    storageLabel->setStyleSheet("color: rgb(92, 255, 122);");
    ui->horizontalLayout->insertWidget(ui->horizontalLayout->count() - 1, storageLabel);

    messageCountLabel = new QLabel(this);
    messageCountLabel->setStyleSheet("color: rgb(92, 255, 122);");
    ui->horizontalLayout->insertWidget(2, messageCountLabel);

    paidMessageCheckBox = new QCheckBox(tr("Paid v3"), this);
    paidMessageCheckBox->setToolTip(tr("Send as a paid v3 SMSG. Fee: 0.1 XVG."));
    paidMessageCheckBox->setStyleSheet("color: rgb(92, 255, 122);");
    ui->horizontalLayout->insertWidget(3, paidMessageCheckBox);

    retentionDaysSpinBox = new QSpinBox(this);
    retentionDaysSpinBox->setRange(1, 31);
    retentionDaysSpinBox->setValue(1);
    retentionDaysSpinBox->setSuffix(tr(" days"));
    retentionDaysSpinBox->setEnabled(false);
    retentionDaysSpinBox->setToolTip(tr("Paid v3 message retention."));
    ui->horizontalLayout->insertWidget(4, retentionDaysSpinBox);

    paidFeeLabel = new QLabel(tr("Fee: 0.1 XVG"), this);
    paidFeeLabel->setStyleSheet("color: rgb(92, 255, 122);");
    paidFeeLabel->setVisible(false);
    ui->horizontalLayout->insertWidget(5, paidFeeLabel);

    connect(ui->messageEdit, SIGNAL(textChanged()), this, SLOT(updateMessageCountdown()));
    connect(paidMessageCheckBox, SIGNAL(toggled(bool)), this, SLOT(updatePaidMessageControls()));

    addressBookButton = new QPushButton(tr("Address &Book"), this);
    addressBookButton->setToolTip(tr("Open local chat-enabled addresses and share chatkey QR payloads"));
    addressBookButton->setStyleSheet("background-color: rgb(80, 0, 120); color: rgb(255, 255, 255);");
    if (platformStyle && platformStyle->getImagesOnButtons()) {
        addressBookButton->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    }
    ui->horizontalLayout->insertWidget(1, addressBookButton);
    connect(addressBookButton, SIGNAL(clicked()), this, SLOT(on_addressBookButton_clicked()));

    refreshStorageUsage();
    updateMessageCountdown();
    LogPrintf("GUI: MessagePage ctor end\n");
}
 MessagePage::~MessagePage()
{
    delete ui;
}
void MessagePage::setModel(MessageModel *model)
{
    LogPrintf("GUI: MessagePage::setModel begin model=%p\n", model);
    this->model = model;
    if(!model)
        return;
    
    //if (model->proxyModel)
    //    delete model->proxyModel;
    model->proxyModel = new QSortFilterProxyModel(this);
    model->proxyModel->setSourceModel(model);
    model->proxyModel->setDynamicSortFilter(true);
    model->proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    model->proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    model->proxyModel->sort(MessageModel::ReceivedDateTime);
    model->proxyModel->setFilterRole(MessageModel::Ambiguous);
    model->proxyModel->setFilterFixedString("true");
     ui->tableView->setModel(model->proxyModel);
    ui->tableView->sortByColumn(MessageModel::ReceivedDateTime, Qt::DescendingOrder);
     ui->listConversation->setModel(model->proxyModel);
    ui->listConversation->setModelColumn(MessageModel::HTML);
     // Set column widths
     ui->tableView->horizontalHeader()->resizeSection(MessageModel::Type,             100);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Status,           140);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Label,            100);
    ui->tableView->horizontalHeader()->setSectionResizeMode(MessageModel::Label,     QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::FromAddress,      320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ToAddress,        320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::SentDateTime,     170);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ReceivedDateTime, 170);
     //ui->messageEdit->setMinimumHeight(100);
     // Hidden columns
    ui->tableView->setColumnHidden(MessageModel::Message, true);
     connect(ui->tableView->selectionModel(),        SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this, SLOT(selectionChanged()));
    connect(ui->tableView,                          SIGNAL(doubleClicked(QModelIndex)),                       this, SLOT(selectionChanged()));
    connect(ui->listConversation->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),  this, SLOT(itemSelectionChanged()));
    connect(ui->listConversation,                   SIGNAL(doubleClicked(QModelIndex)),                       this, SLOT(itemSelectionChanged()));
    //connect(ui->messageEdit,                        SIGNAL(textChanged()),                                    this, SLOT(messageTextChanged()));
     // Scroll to bottom
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(incomingMessage()));
    connect(model, SIGNAL(modelReset()), this, SLOT(handleModelReset()));
     selectionChanged();
    refreshStorageUsage();
    LogPrintf("GUI: MessagePage::setModel end rows=%d\n", model->proxyModel ? model->proxyModel->rowCount() : -1);
}
void MessagePage::on_sendButton_clicked()
{
    if(!model)
        return;
    if (model->getWalletModel()
        && model->getWalletModel()->getEncryptionStatus() == WalletModel::Locked) {
        QMessageBox::warning(this, tr("Send Secure Message"),
            tr("Wallet is locked. Unlock the wallet before sending encrypted messages."),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    SendMessagesRecipient recipient;
    recipient.address = replyToAddress;
    recipient.message = ui->messageEdit->toPlainText().trimmed();

    if (recipient.address.isEmpty() || recipient.message.isEmpty()) {
        return;
    }

    QList<SendMessagesRecipient> recipients;
    recipients.append(recipient);

    const QString fromAddress = replyFromAddress.isEmpty() ? QStringLiteral("anon") : replyFromAddress;
    const bool paidMessage = paidMessageCheckBox && paidMessageCheckBox->isChecked();
    const int retentionDays = retentionDaysSpinBox ? retentionDaysSpinBox->value() : 1;
    const MessageModel::StatusCode status = fromAddress == QLatin1String("anon")
        ? model->sendMessages(recipients, paidMessage, retentionDays)
        : model->sendMessages(recipients, fromAddress, paidMessage, retentionDays);

    if (status != MessageModel::OK) {
        QMessageBox::warning(this, tr("Send Secure Message"),
            tr("Send failed."),
            QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    ui->messageEdit->clear();
    ui->listConversation->scrollToBottom();
    refreshStorageUsage();
    updateMessageCountdown();
}
void MessagePage::on_newButton_clicked()
{
    if(!model)
        return;
     SendMessagesDialog dlg(SendMessagesDialog::Encrypted, SendMessagesDialog::Dialog, this);
    dlg.setModel(model);
    dlg.exec();
    refreshStorageUsage();
}

void MessagePage::on_addressBookButton_clicked()
{
    if (!model || !model->getWalletModel()) {
        return;
    }

    AddressBookPage dialog(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ChatAddressesTab, this);
    dialog.setWalletModel(model->getWalletModel());
    dialog.exec();
}

void MessagePage::on_flushButton_clicked()
{
    if (!model) {
        return;
    }

    if (QMessageBox::question(this,
            tr("Flush Secure Messages"),
            tr("This will permanently delete locally stored secure messages, queued messages, and bucket data on this wallet. Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    std::string error;
    const int rv = smsgModule.FlushMessageData(error);
    if (rv != smsg::SMSG_NO_ERROR) {
        QMessageBox::warning(this,
            tr("Flush Secure Messages"),
            error.empty() ? tr("Failed to flush local secure message storage.") : QString::fromStdString(error),
            QMessageBox::Ok,
            QMessageBox::Ok);
        return;
    }

    on_backButton_clicked();
    model->reloadMessages();
    refreshStorageUsage();
    QMessageBox::information(this,
        tr("Flush Secure Messages"),
        tr("Local secure message storage was flushed."),
        QMessageBox::Ok,
        QMessageBox::Ok);
}

void MessagePage::refreshStorageUsage()
{
    if (!storageLabel) {
        return;
    }
    const uint64_t usage = smsgModule.GetLocalStorageUsageBytes();
    storageLabel->setText(tr("SMSG Storage: %1 / %2")
        .arg(FormatStorageUsage(usage),
             FormatStorageUsage(smsg::SMSG_LOCAL_STORAGE_CAP_BYTES)));
}
 void MessagePage::on_copyFromAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::FromAddress, Qt::DisplayRole);
}
 void MessagePage::on_copyToAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::ToAddress, Qt::DisplayRole);
}
 void MessagePage::on_deleteButton_clicked()
{
    QListView *list = ui->listConversation;
     if(!list->selectionModel())
        return;
     QModelIndexList indexes = list->selectionModel()->selectedIndexes();
     if(!indexes.isEmpty())
    {
        list->model()->removeRow(indexes.at(0).row());
        indexes = list->selectionModel()->selectedIndexes();
         if(indexes.isEmpty())
            on_backButton_clicked();
    }
}
 void MessagePage::on_backButton_clicked()
{
    LogPrintf("GUI: MessagePage::on_backButton_clicked begin\n");
    model->proxyModel->setFilterRole(false);
    model->proxyModel->setFilterFixedString("");
    model->resetFilter();
    model->proxyModel->setFilterRole(MessageModel::Ambiguous);
    model->proxyModel->setFilterFixedString("true");
     ui->tableView->clearSelection();
    ui->listConversation->clearSelection();
    itemSelectionChanged();
    selectionChanged();
    ui->messageDetails->hide();
    ui->tableView->show();
    if (receiptLink) receiptLink->hide();
    ui->newButton->setEnabled(true);
    ui->newButton->setVisible(true);
    ui->sendButton->setEnabled(false);
    ui->sendButton->setVisible(false);
    ui->messageEdit->setVisible(false);
    if (messageCountLabel) messageCountLabel->setVisible(false);
    LogPrintf("GUI: MessagePage::on_backButton_clicked end\n");
}
 void MessagePage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;
    LogPrintf("GUI: MessagePage::selectionChanged hasSelection=%d tableVisible=%d\n",
        table->selectionModel()->hasSelection(),
        ui->tableView->isVisible());
     if(table->selectionModel()->hasSelection())
    {
        replyAction->setEnabled(true);
        copyFromAddressAction->setEnabled(true);
        copyToAddressAction->setEnabled(true);
        deleteAction->setEnabled(true);
         ui->copyFromAddressButton->setEnabled(true);
        ui->copyToAddressButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);
         ui->newButton->setEnabled(false);
        ui->newButton->setVisible(false);
        ui->sendButton->setEnabled(true);
        ui->sendButton->setVisible(true);
        ui->messageEdit->setVisible(true);
        if (messageCountLabel) messageCountLabel->setVisible(true);
         ui->tableView->hide();
         // Figure out which message was selected
        QModelIndexList labelColumn       = table->selectionModel()->selectedRows(MessageModel::Label);
        QModelIndexList addressFromColumn = table->selectionModel()->selectedRows(MessageModel::FromAddress);
        QModelIndexList addressToColumn   = table->selectionModel()->selectedRows(MessageModel::ToAddress);
        QModelIndexList typeColumn        = table->selectionModel()->selectedRows(MessageModel::Type);
         int type;
         for (const QModelIndex& index : typeColumn)
            type = (table->model()->data(index).toString() == MessageModel::Sent ? MessageTableEntry::Sent : MessageTableEntry::Received);
         for (const QModelIndex& index : labelColumn)
            ui->contactLabel->setText(table->model()->data(index).toString());
         for (const QModelIndex& index : addressFromColumn)
            if(type == MessageTableEntry::Sent)
                replyFromAddress = table->model()->data(index).toString();
            else
                replyToAddress = table->model()->data(index).toString();
         for (const QModelIndex& index : addressToColumn)
            if(type == MessageTableEntry::Sent)
                replyToAddress = table->model()->data(index).toString();
            else
                replyFromAddress = table->model()->data(index).toString();
         QString filter = (type == MessageTableEntry::Sent ? replyToAddress + replyFromAddress : replyToAddress + replyFromAddress);
        LogPrintf("GUI: MessagePage::selectionChanged open thread type=%d from=%s to=%s filter=%s\n",
            type,
            replyFromAddress.toStdString(),
            replyToAddress.toStdString(),
            filter.toStdString());
         model->proxyModel->setFilterRole(false);
        model->proxyModel->setFilterFixedString("");
        model->proxyModel->sort(MessageModel::ReceivedDateTime);
        model->proxyModel->setFilterRole(MessageModel::FilterAddressRole);
        model->proxyModel->setFilterFixedString(filter);
        ui->messageDetails->show();
        const QModelIndex firstConversationIndex = model->proxyModel->index(0, 0, QModelIndex());
        if (firstConversationIndex.isValid() && ui->listConversation->selectionModel()) {
            LogPrintf("GUI: MessagePage::selectionChanged selecting row=%d\n", firstConversationIndex.row());
            ui->listConversation->selectionModel()->setCurrentIndex(
                firstConversationIndex,
                QItemSelectionModel::ClearAndSelect);
        }
    }
    else
    {
        if (!ui->tableView->isVisible() && ui->listConversation->model() && ui->listConversation->model()->rowCount() > 0 && ui->listConversation->selectionModel()) {
            const QModelIndex firstConversationIndex = ui->listConversation->model()->index(0, 0, QModelIndex());
            if (firstConversationIndex.isValid()) {
                LogPrintf("GUI: MessagePage::selectionChanged reselection row=%d\n", firstConversationIndex.row());
                ui->listConversation->selectionModel()->setCurrentIndex(
                    firstConversationIndex,
                    QItemSelectionModel::ClearAndSelect);
                return;
            }
        }

        LogPrintf("GUI: MessagePage::selectionChanged clearing detail view\n");
        ui->newButton->setEnabled(true);
        ui->newButton->setVisible(true);
        ui->sendButton->setEnabled(false);
        ui->sendButton->setVisible(false);
        ui->copyFromAddressButton->setEnabled(false);
        ui->copyToAddressButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->messageEdit->hide();
        if (messageCountLabel) messageCountLabel->hide();
        ui->messageDetails->hide();
        if (receiptLink) receiptLink->hide();
        ui->messageEdit->clear();
        updateMessageCountdown();
    }
}
 void MessagePage::itemSelectionChanged()
{
    // Set button states based on selected tab and selection
    QListView *list = ui->listConversation;
    if(!list->selectionModel())
        return;
    LogPrintf("GUI: MessagePage::itemSelectionChanged hasSelection=%d listRows=%d tableVisible=%d\n",
        list->selectionModel()->hasSelection(),
        list->model() ? list->model()->rowCount() : -1,
        ui->tableView->isVisible());
     if(list->selectionModel()->hasSelection())
    {
        replyAction->setEnabled(true);
        copyFromAddressAction->setEnabled(true);
        copyToAddressAction->setEnabled(true);
        deleteAction->setEnabled(true);
         ui->copyFromAddressButton->setEnabled(true);
        ui->copyToAddressButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);
         ui->newButton->setEnabled(false);
        ui->newButton->setVisible(false);
        ui->sendButton->setEnabled(true);
        ui->sendButton->setVisible(true);
        ui->messageEdit->setVisible(true);
        if (messageCountLabel) messageCountLabel->setVisible(true);
        const QModelIndex current = list->selectionModel()->currentIndex();
        const bool hasReceipt = current.isValid() && current.data(MessageModel::ReceiptAvailableRole).toBool();
        if (receiptLink) receiptLink->setVisible(hasReceipt);
         ui->tableView->hide();
     }
    else
    {
        LogPrintf("GUI: MessagePage::itemSelectionChanged clearing detail view\n");
        ui->newButton->setEnabled(true);
        ui->newButton->setVisible(true);
        ui->sendButton->setEnabled(false);
        ui->sendButton->setVisible(false);
        ui->copyFromAddressButton->setEnabled(false);
        ui->copyToAddressButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->messageEdit->hide();
        if (messageCountLabel) messageCountLabel->hide();
        if (receiptLink) receiptLink->hide();
        ui->messageDetails->hide();
        ui->messageEdit->clear();
        updateMessageCountdown();
    }
}
void MessagePage::incomingMessage()
{
    ui->listConversation->scrollToBottom();
}

void MessagePage::handleModelReset()
{
    LogPrintf("GUI: MessagePage::handleModelReset rows=%d tableVisible=%d\n",
        model && model->proxyModel ? model->proxyModel->rowCount() : -1,
        ui->tableView->isVisible());

    ui->tableView->clearSelection();
    ui->listConversation->clearSelection();
    selectionChanged();
    itemSelectionChanged();
    refreshStorageUsage();
}
 void MessagePage::messageTextChanged()
{
    updateMessageCountdown();
 }

void MessagePage::updateMessageCountdown()
{
    auto* plainMessageEdit = qobject_cast<QPlainTextEdit*>(ui->messageEdit);
    if (!plainMessageEdit || !messageCountLabel) {
        return;
    }

    const int maxBytes = paidMessageCheckBox && paidMessageCheckBox->isChecked()
        ? static_cast<int>(smsg::SMSG_MAX_MSG_BYTES_PAID)
        : static_cast<int>(smsg::SMSG_MAX_MSG_BYTES);
    QString text = plainMessageEdit->toPlainText();
    QByteArray utf8 = text.toUtf8();
    if (utf8.size() > maxBytes) {
        utf8.truncate(maxBytes);
        const QString truncated = QString::fromUtf8(utf8);
        if (truncated != text) {
            plainMessageEdit->blockSignals(true);
            plainMessageEdit->setPlainText(truncated);
            QTextCursor cursor = plainMessageEdit->textCursor();
            cursor.movePosition(QTextCursor::End);
            plainMessageEdit->setTextCursor(cursor);
            plainMessageEdit->blockSignals(false);
            text = truncated;
            utf8 = text.toUtf8();
        }
    }

    const int remaining = maxBytes - utf8.size();
    messageCountLabel->setText(tr("Characters left: %1").arg(remaining));
}

void MessagePage::updatePaidMessageControls()
{
    const bool paidMessage = paidMessageCheckBox && paidMessageCheckBox->isChecked();
    if (retentionDaysSpinBox) {
        retentionDaysSpinBox->setEnabled(paidMessage);
    }
    if (paidFeeLabel) {
        paidFeeLabel->setVisible(paidMessage);
    }
    updateMessageCountdown();
}

void MessagePage::showReceipt()
{
    if (!model || !ui->listConversation->selectionModel()) {
        return;
    }

    const QModelIndex current = ui->listConversation->selectionModel()->currentIndex();
    if (!current.isValid() || !current.data(MessageModel::ReceiptAvailableRole).toBool()) {
        return;
    }

    const QString from = current.data(MessageModel::FromAddressRole).toString();
    const QString to = current.data(MessageModel::ToAddressRole).toString();
    const QString txHash = current.data(MessageModel::ReceiptTxHashRole).toString();
    const bool confirmed = current.data(MessageModel::ReceiptConfirmedRole).toBool();
    const QString status = confirmed
        ? QStringLiteral("<span style=\"color:#00AA00; font-weight:bold;\">Confirmed</span>")
        : QStringLiteral("<span style=\"color:#CC0000; font-weight:bold;\">Not confirmed</span>");

    QMessageBox box(this);
    box.setWindowTitle(tr("SMSG Receipt"));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<table>"
        "<tr><td><b>%1</b></td><td>%2</td></tr>"
        "<tr><td><b>%3</b></td><td>%4</td></tr>"
        "<tr><td><b>%5</b></td><td><code>%6</code></td></tr>"
        "<tr><td><b>%7</b></td><td>%8</td></tr>"
        "</table>")
        .arg(tr("From:").toHtmlEscaped(),
             from.toHtmlEscaped(),
             tr("To:").toHtmlEscaped(),
             to.toHtmlEscaped(),
             tr("Tx hash:").toHtmlEscaped(),
             txHash.toHtmlEscaped(),
             tr("Status:").toHtmlEscaped(),
             status));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}
 void MessagePage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Messages"), QString(),
            tr("Comma separated file (*.csv)"),
            nullptr);
     if (filename.isNull()) return;
     CSVModelWriter writer(filename);
     // name, column, role
    writer.setModel(model->proxyModel);
    writer.addColumn("Type",             MessageModel::Type,             Qt::DisplayRole);
    writer.addColumn("Label",            MessageModel::Label,            Qt::DisplayRole);
    writer.addColumn("FromAddress",      MessageModel::FromAddress,      Qt::DisplayRole);
    writer.addColumn("ToAddress",        MessageModel::ToAddress,        Qt::DisplayRole);
    writer.addColumn("SentDateTime",     MessageModel::SentDateTime,     Qt::DisplayRole);
    writer.addColumn("ReceivedDateTime", MessageModel::ReceivedDateTime, Qt::DisplayRole);
    writer.addColumn("Message",          MessageModel::Message,          Qt::DisplayRole);
     if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}
 void MessagePage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}
