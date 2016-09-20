#include "showi2paddresses.h"
#include "ui_showi2paddresses.h"

ShowI2PAddresses::ShowI2PAddresses(const QString& caption, const QString& pub, const QString& priv, const QString& b32, const QString& configFileName, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShowI2PAddresses)
{
    ui->setupUi(this);
    this->setWindowTitle(caption);
    ui->pubText->setPlainText(pub);
    ui->privText->setText("<b>mydestination=</b>" + priv);
    ui->b32Line->setText(b32);
    ui->label->setText(ui->label->text() + "\n" + configFileName);

    QObject::connect(ui->privButton, SIGNAL(clicked()),
                     ui->privText, SLOT(selectAll()));
    QObject::connect(ui->privButton, SIGNAL(clicked()),
                     ui->privText, SLOT(copy()));

    QObject::connect(ui->pubButton, SIGNAL(clicked()),
                     ui->pubText, SLOT(selectAll()));
    QObject::connect(ui->pubButton, SIGNAL(clicked()),
                     ui->pubText, SLOT(copy()));

    QObject::connect(ui->b32Button, SIGNAL(clicked()),
                     ui->b32Line, SLOT(selectAll()));
    QObject::connect(ui->b32Button, SIGNAL(clicked()),
                     ui->b32Line, SLOT(copy()));
}

ShowI2PAddresses::~ShowI2PAddresses()
{
    delete ui;
}
