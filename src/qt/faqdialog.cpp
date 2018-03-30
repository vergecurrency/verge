#include "faqdialog.h"
#include "ui_faqdialog.h"
#include "clientmodel.h"

#include "version.h"

FaqDialog::FaqDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FaqDialog)
{
    ui->setupUi(this);
}

FaqDialog::~FaqDialog()
{
    delete ui;
}

void FaqDialog::on_buttonBox_accepted()
{
    close();
}
