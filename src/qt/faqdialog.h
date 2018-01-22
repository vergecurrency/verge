#ifndef FAQDIALOG_H
#define FAQDIALOG_H

#include <QDialog>

namespace Ui {
    class FaqDialog;
}

/** "FAQ" dialog box */
class FaqDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FaqDialog(QWidget *parent = 0);
    ~FaqDialog();
private:
    Ui::FaqDialog *ui;

private slots:
    void on_buttonBox_accepted();
};

#endif // FAQDIALOG_H
