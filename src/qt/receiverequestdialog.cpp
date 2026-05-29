// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/receiverequestdialog.h>
#include <qt/forms/ui_receiverequestdialog.h>

#include <qt/vergeunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <smsg/smessage.h>

#include <QClipboard>
#include <QDrag>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QTextDocument>
#include <QUrl>

#if defined(HAVE_CONFIG_H)
#include <config/verge-config.h> /* for USE_QRCODE */
#endif

#ifdef USE_QRCODE
#include <qrencode.h>
#endif


namespace {
QPixmap GetLabelPixmap(const QLabel* label)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return label->pixmap(Qt::ReturnByValue);
#else
    const QPixmap* pixmap = label->pixmap();
    return pixmap ? *pixmap : QPixmap();
#endif
}

QString EncodeUriValue(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

QString GetLocalChatkey(const QString& address)
{
    std::string publicKey;
    if (smsgModule.GetLocalPublicKey(address.toStdString(), publicKey) != smsg::SMSG_NO_ERROR) {
        return QString();
    }
    return QString::fromStdString(publicKey);
}

QString FormatChatkeyURI(const QString& address, const QString& chatkey, const QString& label)
{
    QString uri = QStringLiteral("verge:%1?chatkey=%2").arg(address, EncodeUriValue(chatkey));
    if (!label.isEmpty()) {
        uri += QStringLiteral("&label=%1").arg(EncodeUriValue(label));
    }
    return uri;
}

QString DialogURIForRecipient(const SendCoinsRecipient& info, const QString& chatkey)
{
    if (!chatkey.isEmpty()) {
        return FormatChatkeyURI(info.address, chatkey, info.label);
    }
    return GUIUtil::formatVERGEURI(info);
}
} // namespace

QRImageWidget::QRImageWidget(QWidget *parent):
    QLabel(parent), contextMenu(0)
{
    contextMenu = new QMenu(this);
    QAction *saveImageAction = new QAction(tr("&Save Image..."), this);
    connect(saveImageAction, SIGNAL(triggered()), this, SLOT(saveImage()));
    contextMenu->addAction(saveImageAction);
    QAction *copyImageAction = new QAction(tr("&Copy Image"), this);
    connect(copyImageAction, SIGNAL(triggered()), this, SLOT(copyImage()));
    contextMenu->addAction(copyImageAction);
}

QImage QRImageWidget::exportImage()
{
    const QPixmap pm = GetLabelPixmap(this);
    if (pm.isNull())
        return QImage();
    return pm.toImage();
}

void QRImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !GetLabelPixmap(this).isNull())
    {
        event->accept();
        QMimeData *mimeData = new QMimeData;
        mimeData->setImageData(exportImage());

        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->exec();
    } else {
        QLabel::mousePressEvent(event);
    }
}

void QRImageWidget::saveImage()
{
    if (GetLabelPixmap(this).isNull())
        return;
    QString fn = GUIUtil::getSaveFileName(this, tr("Save QR Code"), QString(), tr("PNG Image (*.png)"), nullptr);
    if (!fn.isEmpty())
    {
        exportImage().save(fn);
    }
}

void QRImageWidget::copyImage()
{
    if (GetLabelPixmap(this).isNull())
        return;
    QApplication::clipboard()->setImage(exportImage());
}

void QRImageWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (GetLabelPixmap(this).isNull())
        return;
    contextMenu->exec(event->globalPos());
}

ReceiveRequestDialog::ReceiveRequestDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ReceiveRequestDialog),
    model(0)
{
    ui->setupUi(this);
    setMinimumSize(560, 660);
    ui->lblQRCode->setObjectName("ReceiveRequestQRCode");
    ui->outUri->setObjectName("ReceiveRequestDetails");
    ui->outUri->setReadOnly(true);
    ui->outUri->document()->setDocumentMargin(14);
    ui->btnCopyURI->setObjectName("ReceiveRequestActionButton");
    ui->btnCopyAddress->setObjectName("ReceiveRequestActionButton");
    ui->btnSaveAs->setObjectName("ReceiveRequestActionButton");
    if (QPushButton* close_button = ui->buttonBox->button(QDialogButtonBox::Close)) {
        close_button->setObjectName("DialogSecondaryButton");
    }

#ifndef USE_QRCODE
    ui->btnSaveAs->setVisible(false);
    ui->lblQRCode->setVisible(false);
#endif

    connect(ui->btnSaveAs, SIGNAL(clicked()), ui->lblQRCode, SLOT(saveImage()));
}

ReceiveRequestDialog::~ReceiveRequestDialog()
{
    delete ui;
}

void ReceiveRequestDialog::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model)
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(update()));

    // update the display unit if necessary
    update();
}

void ReceiveRequestDialog::setInfo(const SendCoinsRecipient &_info)
{
    this->info = _info;
    update();
}

void ReceiveRequestDialog::update()
{
    if(!model)
        return;
    QString target = info.label;
    if(target.isEmpty())
        target = info.address;
    const QString chatkey = GetLocalChatkey(info.address);
    const bool isChatkey = !chatkey.isEmpty();
    setWindowTitle(isChatkey ? tr("Share chatkey for %1").arg(target) : tr("Request payment to %1").arg(target));

    QString uri = DialogURIForRecipient(info, chatkey);
    ui->btnSaveAs->setEnabled(false);
    ui->btnCopyAddress->setText(isChatkey ? tr("Copy Chatkey") : tr("Copy Address"));
    QString html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b>" + (isChatkey ? tr("Chatkey information") : tr("Payment information")) + "</b><br>";
    html += "<b>"+tr("URI")+"</b>: ";
    html += "<a href=\""+uri+"\">" + GUIUtil::HtmlEscape(uri) + "</a><br>";
    if (isChatkey) {
        html += "<b>"+tr("Chatkey")+"</b>: " + GUIUtil::HtmlEscape(chatkey) + "<br>";
    } else {
        html += "<b>"+tr("Address")+"</b>: " + GUIUtil::HtmlEscape(info.address) + "<br>";
    }
    if(info.amount && !isChatkey)
        html += "<b>"+tr("Amount")+"</b>: " + VERGEUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), info.amount) + "<br>";
    if(!info.label.isEmpty())
        html += "<b>"+tr("Label")+"</b>: " + GUIUtil::HtmlEscape(info.label) + "<br>";
    if(!info.message.isEmpty() && !isChatkey)
        html += "<b>"+tr("Message")+"</b>: " + GUIUtil::HtmlEscape(info.message) + "<br>";
    if(model->isMultiwallet()) {
        html += "<b>"+tr("Wallet")+"</b>: " + GUIUtil::HtmlEscape(model->getWalletName()) + "<br>";
    }
    ui->outUri->setText(html);

#ifdef USE_QRCODE
    ui->lblQRCode->setText("");
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            ui->lblQRCode->setText(tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code)
            {
                ui->lblQRCode->setText(tr("Error encoding URI into QR Code."));
                return;
            }
            QImage qrImage = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
            qrImage.fill(0xffffff);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
                    p++;
                }
            }
            QRcode_free(code);

            QImage qrAddrImage = QImage(QR_IMAGE_SIZE, QR_IMAGE_SIZE+20, QImage::Format_RGB32);
            qrAddrImage.fill(0xffffff);
            QPainter painter(&qrAddrImage);
            painter.drawImage(0, 0, qrImage.scaled(QR_IMAGE_SIZE, QR_IMAGE_SIZE));
            QFont font = GUIUtil::fixedPitchFont();
            QRect paddedRect = qrAddrImage.rect();

            // calculate ideal font size
            const QString qrCaption = isChatkey ? chatkey : info.address;
            qreal font_size = GUIUtil::calculateIdealFontSize(paddedRect.width() - 20, qrCaption, font);
            font.setPointSizeF(font_size);

            painter.setFont(font);
            paddedRect.setHeight(QR_IMAGE_SIZE+12);
            painter.drawText(paddedRect, Qt::AlignBottom|Qt::AlignCenter, qrCaption);
            painter.end();

            ui->lblQRCode->setPixmap(QPixmap::fromImage(qrAddrImage));
            ui->btnSaveAs->setEnabled(true);
        }
    }
#endif
}

void ReceiveRequestDialog::on_btnCopyURI_clicked()
{
    GUIUtil::setClipboard(DialogURIForRecipient(info, GetLocalChatkey(info.address)));
}

void ReceiveRequestDialog::on_btnCopyAddress_clicked()
{
    const QString chatkey = GetLocalChatkey(info.address);
    GUIUtil::setClipboard(chatkey.isEmpty() ? info.address : chatkey);
}
