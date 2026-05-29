// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <qt/receivecoinsdialog.h>
#include <qt/forms/ui_receivecoinsdialog.h>

#include <qt/addressbookpage.h>
#include <qt/addresstablemodel.h>
#include <qt/vergeunits.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/guiconstants.h>
#include <qt/receiverequestdialog.h>
#include <qt/recentrequeststablemodel.h>
#include <qt/walletmodel.h>

#include <smsg/smessage.h>

#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h>
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif

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

bool ReceivingLabelExists(const AddressTableModel* model, const QString& label, const QString& exceptAddress = QString())
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
        if (!exceptAddress.isEmpty() && address == exceptAddress) {
            continue;
        }

        if (model->data(labelIndex, Qt::EditRole).toString().trimmed().compare(trimmedLabel, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

QString EncodeUriValue(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

QString FormatChatkeyURI(const QString& address, const QString& chatkey, const QString& label)
{
    QString uri = QStringLiteral("verge:%1?chatkey=%2").arg(address, EncodeUriValue(chatkey));
    if (!label.isEmpty()) {
        uri += QStringLiteral("&label=%1").arg(EncodeUriValue(label));
    }
    return uri;
}

QToolButton* CreateCopyButton(const PlatformStyle* platformStyle, const QString& text, const QString& clipboardText, QWidget* parent)
{
    QToolButton* button = new QToolButton(parent);
    button->setToolTip(text);
    button->setAutoRaise(true);
    button->setIcon(platformStyle->SingleColorIcon(":/icons/editcopy"));
    QObject::connect(button, &QToolButton::clicked, button, [clipboardText]() {
        GUIUtil::setClipboard(clipboardText);
    });
    return button;
}

QString BuildChatkeyQRCode(const QString& payload, const QString& caption, QPixmap& pixmap)
{
#ifdef USE_QRCODE
    if (payload.length() > MAX_URI_LENGTH) {
        return QObject::tr("QR payload is too long. Shorten the label and try again.");
    }

    QRcode* code = QRcode_encodeString(payload.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code) {
        return QObject::tr("Error encoding chatkey payload into QR code.");
    }

    QImage qrImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
    qrImage.fill(0xffffff);
    unsigned char* p = code->data;
    for (int y = 0; y < code->width; y++) {
        for (int x = 0; x < code->width; x++) {
            qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
            p++;
        }
    }
    QRcode_free(code);

    QImage qrAddrImage(QR_IMAGE_SIZE, QR_IMAGE_SIZE + 44, QImage::Format_RGB32);
    qrAddrImage.fill(0xffffff);
    QPainter painter(&qrAddrImage);
    painter.drawImage(0, 0, qrImage.scaled(QR_IMAGE_SIZE, QR_IMAGE_SIZE));
    QFont font = GUIUtil::fixedPitchFont();
    QRect paddedRect = qrAddrImage.rect();
    qreal fontSize = GUIUtil::calculateIdealFontSize(paddedRect.width() - 20, caption, font);
    font.setPointSizeF(fontSize);
    painter.setFont(font);
    paddedRect.setHeight(QR_IMAGE_SIZE + 36);
    painter.drawText(paddedRect, Qt::AlignBottom | Qt::AlignCenter, caption);
    painter.end();

    pixmap = QPixmap::fromImage(qrAddrImage);
    return QString();
#else
    Q_UNUSED(payload);
    Q_UNUSED(caption);
    Q_UNUSED(pixmap);
    return QObject::tr("QR code support is not available in this build.");
#endif
}

void ShowChatkeyDialog(QWidget* parent, const PlatformStyle* platformStyle, const QString& address, const QString& chatkey, const QString& label)
{
    const QString payload = FormatChatkeyURI(address, chatkey, label);

    QDialog dialog(parent);
    dialog.setObjectName("ReceiveRequestDialog");
    dialog.setWindowTitle(QObject::tr("Created chatkey"));
    dialog.setMinimumSize(640, 680);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    QRImageWidget* qrCode = new QRImageWidget(&dialog);
    qrCode->setObjectName("ReceiveRequestQRCode");
    qrCode->setMinimumSize(300, 340);
    qrCode->setAlignment(Qt::AlignCenter);
    qrCode->setWordWrap(true);

    QPixmap qrPixmap;
    const QString qrError = BuildChatkeyQRCode(payload, address, qrPixmap);
    if (!qrError.isEmpty()) {
        qrCode->setText(qrError);
    } else {
        qrCode->setPixmap(qrPixmap);
    }
    mainLayout->addWidget(qrCode);

    QTextEdit* details = new QTextEdit(&dialog);
    details->setObjectName("ReceiveRequestDetails");
    details->setReadOnly(true);
    details->setMinimumHeight(120);
    details->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    QString html;
    html += QStringLiteral("<b>%1</b><br>").arg(QObject::tr("Chatkey information"));
    html += QStringLiteral("<b>%1</b>: %2<br>").arg(QObject::tr("Label"), GUIUtil::HtmlEscape(label));
    html += QStringLiteral("<b>%1</b>: %2<br>").arg(QObject::tr("Address"), GUIUtil::HtmlEscape(address));
    html += QStringLiteral("<b>%1</b>: %2<br>").arg(QObject::tr("Chatkey"), GUIUtil::HtmlEscape(chatkey));
    html += QStringLiteral("<b>%1</b>: <a href=\"%2\">%3</a>")
        .arg(QObject::tr("QR payload"), GUIUtil::HtmlEscape(payload), GUIUtil::HtmlEscape(payload));
    details->setHtml(html);
    mainLayout->addWidget(details);

    QGridLayout* copyLayout = new QGridLayout();
    auto addCopyRow = [&](int row, const QString& title, const QString& value) {
        QLabel* labelWidget = new QLabel(title, &dialog);
        QLineEdit* valueWidget = new QLineEdit(value, &dialog);
        valueWidget->setReadOnly(true);
        valueWidget->setMinimumWidth(420);
        copyLayout->addWidget(labelWidget, row, 0);
        copyLayout->addWidget(valueWidget, row, 1);
        copyLayout->addWidget(CreateCopyButton(platformStyle, QObject::tr("Copy %1").arg(title.toLower()), value, &dialog), row, 2);
    };
    addCopyRow(0, QObject::tr("Address"), address);
    addCopyRow(1, QObject::tr("Chatkey"), chatkey);
    addCopyRow(2, QObject::tr("QR payload"), payload);
    mainLayout->addLayout(copyLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* copyQrPayloadButton = new QPushButton(QObject::tr("Copy QR payload"), &dialog);
    copyQrPayloadButton->setObjectName("ReceiveRequestActionButton");
    QObject::connect(copyQrPayloadButton, &QPushButton::clicked, copyQrPayloadButton, [payload]() {
        GUIUtil::setClipboard(payload);
    });
    buttonLayout->addWidget(copyQrPayloadButton);

    QPushButton* saveQrButton = new QPushButton(QObject::tr("Save Image..."), &dialog);
    saveQrButton->setObjectName("ReceiveRequestActionButton");
    saveQrButton->setEnabled(!qrPixmap.isNull());
    QObject::connect(saveQrButton, SIGNAL(clicked()), qrCode, SLOT(saveImage()));
    buttonLayout->addWidget(saveQrButton);

    buttonLayout->addStretch();
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    if (QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close)) {
        closeButton->setObjectName("DialogSecondaryButton");
    }
    QObject::connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    buttonLayout->addWidget(buttonBox);
    mainLayout->addLayout(buttonLayout);

    dialog.exec();
}
}

ReceiveCoinsDialog::ReceiveCoinsDialog(const PlatformStyle *_platformStyle, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveCoinsDialog),
    columnResizingFixer(0),
    model(0),
    editLabelButton(nullptr),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);
    setObjectName("ReceiveCoinsDialog");
    ui->frame2->setObjectName("ReceiveRequestCard");
    ui->frame->setObjectName("ReceiveHistoryCard");
    ui->label_5->setObjectName("ReceiveIntroText");
    ui->label_6->setObjectName("ReceiveSectionTitle");
    ui->reqLabel->setObjectName("ReceiveTextField");
    ui->reqAmount->setObjectName("ReceiveAmountField");
    ui->reqMessage->setObjectName("ReceiveTextField");
    ui->useStealth->setObjectName("ReceiveStealthToggle");
    ui->receiveButton->setObjectName("ReceivePrimaryButton");
    ui->createChatkeyButton->setObjectName("ReceivePrimaryButton");
    ui->clearButton->setObjectName("ReceiveSecondaryButton");
    ui->showRequestButton->setObjectName("ReceiveSecondaryButton");
    ui->removeRequestButton->setObjectName("ReceiveSecondaryButton");
    ui->recentRequestsView->setObjectName("ReceiveHistoryTable");
    ApplyCardShadow(ui->frame2);
    ApplyCardShadow(ui->frame);

    if (!_platformStyle->getImagesOnButtons()) {
        ui->clearButton->setIcon(QIcon());
        ui->receiveButton->setIcon(QIcon());
        ui->createChatkeyButton->setIcon(QIcon());
        ui->showRequestButton->setIcon(QIcon());
        ui->removeRequestButton->setIcon(QIcon());
    } else {
        ui->clearButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
        ui->receiveButton->setIcon(_platformStyle->SingleColorIcon(":/icons/receiving_addresses"));
        ui->createChatkeyButton->setIcon(_platformStyle->SingleColorIcon(":/icons/editcopy"));
        ui->showRequestButton->setIcon(_platformStyle->SingleColorIcon(":/icons/edit"));
        ui->removeRequestButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
    }

    // context menu actions
    QAction *copyURIAction = new QAction(tr("Copy URI"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyMessageAction = new QAction(tr("Copy message"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *editLabelAction = new QAction(tr("Edit label"), this);

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyURIAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyMessageAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addSeparator();
    contextMenu->addAction(editLabelAction);

    // context menu signals
    connect(ui->recentRequestsView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    connect(copyURIAction, SIGNAL(triggered()), this, SLOT(copyURI()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyMessageAction, SIGNAL(triggered()), this, SLOT(copyMessage()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(editLabelAction, SIGNAL(triggered()), this, SLOT(editSelectedLabel()));

    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    editLabelButton = new QPushButton(tr("Edit label"), this);
    editLabelButton->setObjectName("ReceiveSecondaryButton");
    editLabelButton->setEnabled(false);
    ui->horizontalLayout_2->insertWidget(0, editLabelButton);
    connect(editLabelButton, SIGNAL(clicked()), this, SLOT(editSelectedLabel()));
    connect(ui->useStealth, &QCheckBox::toggled, this, [this](bool checked) {
        ui->allowMessaging->setEnabled(!checked);
        if (checked) {
            ui->allowMessaging->setChecked(false);
        }
    });
    ui->allowMessaging->setEnabled(!ui->useStealth->isChecked());
}

void ReceiveCoinsDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if(_model && _model->getOptionsModel())
    {
        _model->getRecentRequestsTableModel()->sort(RecentRequestsTableModel::Date, Qt::DescendingOrder);
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();

        QTableView* tableView = ui->recentRequestsView;

        tableView->verticalHeader()->hide();
        tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tableView->setModel(_model->getRecentRequestsTableModel());
        tableView->setAlternatingRowColors(false);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSelectionMode(QAbstractItemView::ContiguousSelection);
        tableView->setShowGrid(false);
        tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tableView->setFrameShape(QFrame::NoFrame);
        tableView->setWordWrap(false);
        tableView->horizontalHeader()->setObjectName("ReceiveHistoryHeader");
        tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        tableView->verticalHeader()->setDefaultSectionSize(42);
        tableView->setColumnWidth(RecentRequestsTableModel::Date, DATE_COLUMN_WIDTH);
        tableView->setColumnWidth(RecentRequestsTableModel::Label, LABEL_COLUMN_WIDTH);
        tableView->setColumnWidth(RecentRequestsTableModel::Amount, AMOUNT_MINIMUM_COLUMN_WIDTH);

        connect(tableView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection, QItemSelection)), this,
            SLOT(recentRequestsView_selectionChanged(QItemSelection, QItemSelection)));
        // Last 2 columns are set by the columnResizingFixer, when the table geometry is ready.
        columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(tableView, AMOUNT_MINIMUM_COLUMN_WIDTH, DATE_COLUMN_WIDTH, this);

        if (model->wallet().getDefaultAddressType() == OutputType::STEALTH) {
            ui->useStealth->setCheckState(Qt::Checked);
        } else {
            ui->useStealth->setCheckState(Qt::Unchecked);
        }
    }
}

ReceiveCoinsDialog::~ReceiveCoinsDialog()
{
    delete ui;
}

void ReceiveCoinsDialog::clear()
{
    ui->reqAmount->clear();
    ui->reqLabel->setText("");
    ui->reqMessage->setText("");
    ui->allowMessaging->setChecked(false);
    updateDisplayUnit();
}

void ReceiveCoinsDialog::reject()
{
    clear();
}

void ReceiveCoinsDialog::accept()
{
    clear();
}

void ReceiveCoinsDialog::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        ui->reqAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }
}

void ReceiveCoinsDialog::on_receiveButton_clicked()
{
    if(!model || !model->getOptionsModel() || !model->getAddressTableModel() || !model->getRecentRequestsTableModel())
        return;

    QString address;
    QString label = ui->reqLabel->text();
    /* Generate new receiving address */
    OutputType address_type;
    if (ui->useStealth->isChecked()) {
        address_type = OutputType::STEALTH;
    } else {
        address_type = OutputType::LEGACY;
    }
    address = model->getAddressTableModel()->addRow(AddressTableModel::Receive, label, "", address_type);
    if (ui->allowMessaging->isChecked()) {
        const int rv = smsgModule.AddLocalAddress(address.toStdString());
        if (rv != smsg::SMSG_NO_ERROR) {
            QMessageBox::warning(this, tr("Enable Messaging"),
                tr("The address was created, but secure messaging could not be enabled for it: %1")
                    .arg(QString::fromLatin1(smsg::GetString(rv))),
                QMessageBox::Ok, QMessageBox::Ok);
        }
    }
    SendCoinsRecipient info(address, label,
        ui->reqAmount->value(), ui->reqMessage->text());
    ReceiveRequestDialog *dialog = new ReceiveRequestDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModel(model);
    dialog->setInfo(info);
    dialog->show();
    clear();

    /* Store request for later reference */
    model->getRecentRequestsTableModel()->addNewRequest(info);
}

void ReceiveCoinsDialog::on_createChatkeyButton_clicked()
{
    if (!model || !model->getAddressTableModel()) {
        return;
    }

    QString label = tr("MyChatkey1");
    while (true) {
        bool accepted = false;
        label = QInputDialog::getText(this,
            tr("Create Chatkey"),
            tr("Label:"),
            QLineEdit::Normal,
            label,
            &accepted).trimmed();
        if (!accepted) {
            return;
        }
        if (!ReceivingLabelExists(model->getAddressTableModel(), label)) {
            break;
        }
        QMessageBox::warning(this,
            tr("Create Chatkey"),
            tr("The label \"%1\" already exists. Choose a different label.").arg(label),
            QMessageBox::Ok,
            QMessageBox::Ok);
    }

    const QString address = model->getAddressTableModel()->addRow(
        AddressTableModel::Receive,
        label,
        QString(),
        OutputType::LEGACY);
    if (address.isEmpty()) {
        QMessageBox::warning(this,
            tr("Create Chatkey"),
            tr("Could not create a new address."),
            QMessageBox::Ok,
            QMessageBox::Ok);
        return;
    }

    int rv = smsgModule.AddLocalAddress(address.toStdString());
    if (rv != smsg::SMSG_NO_ERROR) {
        QMessageBox::warning(this,
            tr("Create Chatkey"),
            tr("The address was created, but secure messaging could not be enabled for it: %1")
                .arg(QString::fromLatin1(smsg::GetString(rv))),
            QMessageBox::Ok,
            QMessageBox::Ok);
        return;
    }

    std::string publicKey;
    std::string addressString = address.toStdString();
    rv = smsgModule.GetLocalPublicKey(addressString, publicKey);
    if (rv != smsg::SMSG_NO_ERROR) {
        QMessageBox::warning(this,
            tr("Create Chatkey"),
            tr("The address was created, but its chatkey could not be loaded: %1")
                .arg(QString::fromLatin1(smsg::GetString(rv))),
            QMessageBox::Ok,
            QMessageBox::Ok);
        return;
    }

    ShowChatkeyDialog(this, platformStyle, address, QString::fromStdString(publicKey), label);
}

void ReceiveCoinsDialog::on_recentRequestsView_doubleClicked(const QModelIndex &index)
{
    const RecentRequestsTableModel *submodel = model->getRecentRequestsTableModel();
    ReceiveRequestDialog *dialog = new ReceiveRequestDialog(this);
    dialog->setModel(model);
    dialog->setInfo(submodel->entry(index.row()).recipient);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void ReceiveCoinsDialog::recentRequestsView_selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Enable Show/Remove buttons only if anything is selected.
    bool enable = !ui->recentRequestsView->selectionModel()->selectedRows().isEmpty();
    ui->showRequestButton->setEnabled(enable);
    ui->removeRequestButton->setEnabled(enable);
    if (editLabelButton) {
        editLabelButton->setEnabled(enable);
    }
}

void ReceiveCoinsDialog::on_showRequestButton_clicked()
{
    if(!model || !model->getRecentRequestsTableModel() || !ui->recentRequestsView->selectionModel())
        return;
    QModelIndexList selection = ui->recentRequestsView->selectionModel()->selectedRows();

    for (const QModelIndex& index : selection) {
        on_recentRequestsView_doubleClicked(index);
    }
}

void ReceiveCoinsDialog::on_removeRequestButton_clicked()
{
    if(!model || !model->getRecentRequestsTableModel() || !ui->recentRequestsView->selectionModel())
        return;
    QModelIndexList selection = ui->recentRequestsView->selectionModel()->selectedRows();
    if(selection.empty())
        return;
    // correct for selection mode ContiguousSelection
    QModelIndex firstIndex = selection.at(0);
    model->getRecentRequestsTableModel()->removeRows(firstIndex.row(), selection.length(), firstIndex.parent());
}

// We override the virtual resizeEvent of the QWidget to adjust tables column
// sizes as the tables width is proportional to the dialogs width.
void ReceiveCoinsDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(RecentRequestsTableModel::Message);
}

void ReceiveCoinsDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Return)
    {
        // press return -> submit form
        if (ui->reqLabel->hasFocus() || ui->reqAmount->hasFocus() || ui->reqMessage->hasFocus())
        {
            event->ignore();
            on_receiveButton_clicked();
            return;
        }
    }

    this->QDialog::keyPressEvent(event);
}

QModelIndex ReceiveCoinsDialog::selectedRow()
{
    if(!model || !model->getRecentRequestsTableModel() || !ui->recentRequestsView->selectionModel())
        return QModelIndex();
    QModelIndexList selection = ui->recentRequestsView->selectionModel()->selectedRows();
    if(selection.empty())
        return QModelIndex();
    // correct for selection mode ContiguousSelection
    QModelIndex firstIndex = selection.at(0);
    return firstIndex;
}

// copy column of selected row to clipboard
void ReceiveCoinsDialog::copyColumnToClipboard(int column)
{
    QModelIndex firstIndex = selectedRow();
    if (!firstIndex.isValid()) {
        return;
    }
    GUIUtil::setClipboard(model->getRecentRequestsTableModel()->data(firstIndex.sibling(firstIndex.row(), column), Qt::EditRole).toString());
}

// context menu
void ReceiveCoinsDialog::showMenu(const QPoint &point)
{
    if (!selectedRow().isValid()) {
        return;
    }
    contextMenu->exec(QCursor::pos());
}

// context menu action: copy URI
void ReceiveCoinsDialog::copyURI()
{
    QModelIndex sel = selectedRow();
    if (!sel.isValid()) {
        return;
    }

    const RecentRequestsTableModel * const submodel = model->getRecentRequestsTableModel();
    const QString uri = GUIUtil::formatVERGEURI(submodel->entry(sel.row()).recipient);
    GUIUtil::setClipboard(uri);
}

// context menu action: copy label
void ReceiveCoinsDialog::copyLabel()
{
    copyColumnToClipboard(RecentRequestsTableModel::Label);
}

// context menu action: copy message
void ReceiveCoinsDialog::copyMessage()
{
    copyColumnToClipboard(RecentRequestsTableModel::Message);
}

// context menu action: copy amount
void ReceiveCoinsDialog::copyAmount()
{
    copyColumnToClipboard(RecentRequestsTableModel::Amount);
}

void ReceiveCoinsDialog::editSelectedLabel()
{
    QModelIndex sel = selectedRow();
    if (!sel.isValid() || !model || !model->getRecentRequestsTableModel() || !model->getAddressTableModel()) {
        return;
    }

    RecentRequestsTableModel* requestsModel = model->getRecentRequestsTableModel();
    const RecentRequestEntry& request = requestsModel->entry(sel.row());
    const QString currentLabel = request.recipient.label;

    bool accepted = false;
    const QString newLabel = QInputDialog::getText(this,
        tr("Edit label"),
        tr("Label:"),
        QLineEdit::Normal,
        currentLabel,
        &accepted).trimmed();

    if (!accepted) {
        return;
    }

    const int addressRow = model->getAddressTableModel()->lookupAddress(request.recipient.address);
    if (addressRow >= 0) {
        model->getAddressTableModel()->setData(
            model->getAddressTableModel()->index(addressRow, AddressTableModel::Label, QModelIndex()),
            newLabel,
            Qt::EditRole);
    }

    if (!requestsModel->updateLabel(sel.row(), newLabel)) {
        QMessageBox::warning(this,
            tr("Edit label"),
            tr("Could not update the receive request label."),
            QMessageBox::Ok,
            QMessageBox::Ok);
    }
}
