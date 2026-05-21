// Copyright (c) 2026 The Verge Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_QT_SENDMESSAGESENTRY_H
#define VERGE_QT_SENDMESSAGESENTRY_H

#include <QFrame>

class MessageModel;
class SendMessagesRecipient;

namespace Ui {
class SendMessagesEntry;
}

class SendMessagesEntry : public QFrame
{
    Q_OBJECT

public:
    explicit SendMessagesEntry(QWidget* parent = 0);
    ~SendMessagesEntry();

    void setModel(MessageModel* model);
    bool validate();
    SendMessagesRecipient getValue();
    bool isClear();
    void setValue(const SendMessagesRecipient& value);
    void loadRow(int row);
    void setRemoveEnabled(bool enabled);
    QWidget* setupTabChain(QWidget* prev);
    void setFocus();

public Q_SLOTS:
    void clear();

Q_SIGNALS:
    void removeEntry(SendMessagesEntry* entry);

private Q_SLOTS:
    void deleteClicked();
    void on_pasteButton_clicked();
    void on_addressBookButton_clicked();
    void on_sendTo_textChanged(const QString& address);

private:
    bool updateLabel(const QString& address);

    Ui::SendMessagesEntry* ui;
    MessageModel* model;
};

#endif // VERGE_QT_SENDMESSAGESENTRY_H
