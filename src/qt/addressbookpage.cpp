// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#include <qt/addressbookpage.h>
#include <qt/forms/ui_addressbookpage.h>

#include <qt/addresstablemodel.h>
#include <qt/vergegui.h>
#include <qt/csvmodelwriter.h>
#include <qt/editaddressdialog.h>
#include <qt/guiutil.h>
#include <qt/platformstyle.h>
#include <qt/receiverequestdialog.h>
#include <qt/walletmodel.h>

#include <smsg/smessage.h>

#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSortFilterProxyModel>

namespace {
void ApplyCardShadow(QWidget* widget)
{
    if (!widget) return;
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(28.0);
    effect->setOffset(0, 10);
    effect->setColor(QColor(88, 28, 140, 92));
    widget->setGraphicsEffect(effect);
}

QString GetLocalChatkey(const QString& address)
{
    std::string publicKey;
    if (smsgModule.GetLocalPublicKey(address.toStdString(), publicKey) != smsg::SMSG_NO_ERROR) {
        return QString();
    }
    return QString::fromStdString(publicKey);
}

QString FormatShareChatkey(const QString& address, const QString& chatkey)
{
    return QStringLiteral("%1 %2").arg(address, chatkey);
}
}

class AddressBookSortFilterProxyModel final : public QSortFilterProxyModel
{
    const QString m_type;
    const bool m_chatOnly;

public:
    AddressBookSortFilterProxyModel(const QString& type, bool chatOnly, QObject* parent)
        : QSortFilterProxyModel(parent)
        , m_type(type)
        , m_chatOnly(chatOnly)
    {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortCaseSensitivity(Qt::CaseInsensitive);
    }

    QVariant data(const QModelIndex& index, int role) const
    {
        if (m_chatOnly && index.column() == AddressTableModel::Address &&
            (role == Qt::DisplayRole || role == Qt::EditRole)) {
            const QModelIndex sourceIndex = mapToSource(index);
            const QString address = sourceModel()->data(sourceIndex, Qt::EditRole).toString();
            const QString chatkey = GetLocalChatkey(address);
            if (!chatkey.isEmpty()) {
                return FormatShareChatkey(address, chatkey);
            }
        }
        return QSortFilterProxyModel::data(index, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (m_chatOnly && orientation == Qt::Horizontal && role == Qt::DisplayRole &&
            section == AddressTableModel::Address) {
            return tr("Chatkeys");
        }
        return QSortFilterProxyModel::headerData(section, orientation, role);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const
    {
        auto model = sourceModel();
        auto label = model->index(row, AddressTableModel::Label, parent);

        if (model->data(label, AddressTableModel::TypeRole).toString() != m_type) {
            return false;
        }

        auto address = model->index(row, AddressTableModel::Address, parent);
        const QString addressString = model->data(address).toString();
        QString chatkeyString;
        if (m_chatOnly) {
            chatkeyString = GetLocalChatkey(addressString);
            if (chatkeyString.isEmpty()) {
                return false;
            }
        }

        const QRegularExpression re = filterRegularExpression();
        const QString combinedChatkey = chatkeyString.isEmpty() ? QString() : FormatShareChatkey(addressString, chatkeyString);
        if (!re.match(addressString).hasMatch() &&
            !re.match(combinedChatkey).hasMatch() &&
            !re.match(model->data(label).toString()).hasMatch()) {
            return false;
        }

        return true;
    }
};

AddressBookPage::AddressBookPage(const PlatformStyle *platformStyle, Mode _mode, Tabs _tab, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddressBookPage),
    model(0),
    walletModel(0),
    mode(_mode),
    tab(_tab)
{
    ui->setupUi(this);
    GUIUtil::EnableThemedDialogChrome(this);
    setObjectName("AddressBookPage");
    setMinimumSize(880, 520);
    ui->labelExplanation->setObjectName("AddressBookIntroText");
    ui->searchLineEdit->setObjectName("AddressBookSearchField");
    ui->tableView->setObjectName("AddressBookTable");
    ui->tableView->horizontalHeader()->setObjectName("AddressBookTableHeader");
    ui->newAddress->setObjectName("AddressBookPrimaryButton");
    ui->copyAddress->setObjectName("AddressBookSecondaryButton");
    ui->deleteAddress->setObjectName("AddressBookDangerButton");
    ui->exportButton->setObjectName("AddressBookSecondaryButton");
    ui->closeButton->setObjectName("DialogSecondaryButton");
    ApplyCardShadow(ui->tableView);

    if (!platformStyle->getImagesOnButtons()) {
        ui->newAddress->setIcon(QIcon());
        ui->copyAddress->setIcon(QIcon());
        ui->deleteAddress->setIcon(QIcon());
        ui->exportButton->setIcon(QIcon());
    } else {
        ui->newAddress->setIcon(platformStyle->SingleColorIcon(":/icons/add"));
        ui->copyAddress->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
        ui->deleteAddress->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
        ui->exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }

    switch(mode)
    {
    case ForSelection:
        switch(tab)
        {
        case SendingTab: setWindowTitle(tr("Choose the address to send coins to")); break;
        case ReceivingTab: setWindowTitle(tr("Choose the address to receive coins with")); break;
        case ChatAddressesTab: setWindowTitle(tr("Choose the address to send messages from")); break;
        }
        connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tableView->setFocus();
        ui->closeButton->setText(tr("C&hoose"));
        ui->exportButton->hide();
        break;
    case ForEditing:
        switch(tab)
        {
        case SendingTab: setWindowTitle(tr("Sending addresses")); break;
        case ReceivingTab: setWindowTitle(tr("Receiving addresses")); break;
        case ChatAddressesTab: setWindowTitle(tr("Chat addresses")); break;
        }
        break;
    }
    switch(tab)
    {
    case SendingTab:
        ui->labelExplanation->setText(tr("These are your VERGE addresses for sending payments. Always check the amount and the receiving address before sending coins."));
        ui->deleteAddress->setVisible(true);
        ui->newAddress->setVisible(true);
        break;
    case ReceivingTab:
        ui->labelExplanation->setText(tr("These are your VERGE addresses for receiving payments. It is recommended to use a new receiving address for each transaction."));
        ui->deleteAddress->setVisible(false);
        ui->newAddress->setVisible(false);
        break;
    case ChatAddressesTab:
        ui->labelExplanation->setText(tr("These are your local chat-enabled addresses. Double-click a chatkey to share it."));
        ui->deleteAddress->setVisible(false);
        ui->newAddress->setVisible(false);
        ui->copyAddress->setText(tr("Copy Chatkey"));
        break;
    }

    // Context menu actions
    QAction *copyAddressAction = new QAction(tr("&Copy Address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showChatkeyAction = new QAction(tr("Show Chatkey QR"), this);
    deleteAction = new QAction(ui->deleteAddress->text(), this);
    if (tab == ChatAddressesTab) {
        copyAddressAction->setText(tr("&Copy Chatkey"));
    }

    // Build context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if (tab == ChatAddressesTab) {
        contextMenu->addAction(showChatkeyAction);
    }
    if(tab == SendingTab)
        contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyAddress_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(editAction, SIGNAL(triggered()), this, SLOT(onEditAction()));
    connect(showChatkeyAction, SIGNAL(triggered()), this, SLOT(showSelectedChatkey()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteAddress_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    if (mode == ForEditing && tab == ChatAddressesTab) {
        connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(showSelectedChatkey()));
    }

    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(accept()));
}

AddressBookPage::~AddressBookPage()
{
    delete ui;
}

void AddressBookPage::setModel(AddressTableModel *_model)
{
    this->model = _model;
    if(!_model)
        return;

    auto type = tab == SendingTab ? AddressTableModel::Send : AddressTableModel::Receive;
    proxyModel = new AddressBookSortFilterProxyModel(type, tab == ChatAddressesTab, this);
    proxyModel->setSourceModel(_model);

    connect(ui->searchLineEdit, SIGNAL(textChanged(QString)), proxyModel, SLOT(setFilterWildcard(QString)));

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);
    ui->tableView->setAlternatingRowColors(false);
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setShowGrid(false);
    ui->tableView->setWordWrap(false);

    // Set column widths
#if QT_VERSION < 0x050000
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setResizeMode(AddressTableModel::Address, QHeaderView::ResizeToContents);
#else
    ui->tableView->horizontalHeader()->setSectionResizeMode(AddressTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(AddressTableModel::Address, QHeaderView::ResizeToContents);
#endif
    ui->tableView->horizontalHeader()->setHighlightSections(false);
    ui->tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
        this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(selectNewAddress(QModelIndex,int,int)));

    selectionChanged();
}

void AddressBookPage::setWalletModel(WalletModel *_walletModel)
{
    walletModel = _walletModel;
    setModel(_walletModel ? _walletModel->getAddressTableModel() : nullptr);
}

void AddressBookPage::on_copyAddress_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}

void AddressBookPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}

void AddressBookPage::onEditAction()
{
    if(!model)
        return;

    if(!ui->tableView->selectionModel())
        return;
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if(indexes.isEmpty())
        return;

    EditAddressDialog dlg(
        tab == SendingTab ?
        EditAddressDialog::EditSendingAddress :
        EditAddressDialog::EditReceivingAddress, this);
    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
}

void AddressBookPage::showSelectedChatkey()
{
    if (tab != ChatAddressesTab || !walletModel || !ui->tableView->selectionModel()) {
        return;
    }

    const QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        return;
    }

    const QModelIndex sourceIndex = proxyModel->mapToSource(indexes.at(0));
    const QString label = model->data(model->index(sourceIndex.row(), AddressTableModel::Label, QModelIndex()), Qt::EditRole).toString();
    const QString address = model->data(model->index(sourceIndex.row(), AddressTableModel::Address, QModelIndex()), Qt::EditRole).toString();
    SendCoinsRecipient info(address, label, 0, QString());

    ReceiveRequestDialog* dialog = new ReceiveRequestDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModel(walletModel);
    dialog->setInfo(info, ReceiveRequestDialog::ChatkeyRequest);
    dialog->show();
}

void AddressBookPage::on_newAddress_clicked()
{
    if(!model)
        return;

    if (tab == ReceivingTab) {
        return;
    }

    EditAddressDialog dlg(EditAddressDialog::NewSendingAddress, this);
    dlg.setModel(model);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

void AddressBookPage::on_deleteAddress_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void AddressBookPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            ui->deleteAddress->setEnabled(true);
            ui->deleteAddress->setVisible(true);
            deleteAction->setEnabled(true);
            break;
        case ReceivingTab:
        case ChatAddressesTab:
            // Deleting receiving addresses, however, is not allowed
            ui->deleteAddress->setEnabled(false);
            ui->deleteAddress->setVisible(false);
            deleteAction->setEnabled(false);
            break;
        }
        ui->copyAddress->setEnabled(true);
    }
    else
    {
        ui->deleteAddress->setEnabled(false);
        ui->copyAddress->setEnabled(false);
    }
}

void AddressBookPage::done(int retval)
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel() || !table->model())
        return;

    // Figure out which address was selected, and return it
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    for (const QModelIndex& index : indexes) {
        const QModelIndex sourceIndex = proxyModel->mapToSource(index);
        returnValue = model->data(model->index(sourceIndex.row(), AddressTableModel::Address, QModelIndex()), Qt::EditRole).toString();
    }

    if(returnValue.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

void AddressBookPage::on_exportButton_clicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Export Address List"), QString(),
        tr("Comma separated file (*.csv)"), nullptr);

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
    writer.addColumn(tab == ChatAddressesTab ? "Chatkeys" : "Address", AddressTableModel::Address, Qt::EditRole);

    if(!writer.write()) {
        QMessageBox::critical(this, tr("Exporting Failed"),
            tr("There was an error trying to save the address list to %1. Please try again.").arg(filename));
    }
}

void AddressBookPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void AddressBookPage::selectNewAddress(const QModelIndex &parent, int begin, int /*end*/)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}
