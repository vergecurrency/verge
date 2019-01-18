// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2018 The VERGE Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_OPENURIDIALOG_H
#define VERGE_QT_OPENURIDIALOG_H

#include <QDialog>

namespace Ui {
    class OpenURIDialog;
}

class OpenURIDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenURIDialog(QWidget *parent);
    ~OpenURIDialog();

    QString getURI();

protected Q_SLOTS:
    void accept();

private Q_SLOTS:
    void on_selectFileButton_clicked();

private:
    Ui::OpenURIDialog *ui;
};

#endif // VERGE_QT_OPENURIDIALOG_H
