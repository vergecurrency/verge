#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include "uint256.h"

#include <vector>
#include "allocators.h" /* for SecureString */
#include "smessage.h"
#include <map>
#include <QSortFilterProxyModel>
#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>


class MessageTablePriv;
class InvoiceTableModel;
class InvoiceItemTableModel;
class ReceiptTableModel;
class CWallet;
class WalletModel;
class OptionsModel;

class SendMessagesRecipient
{
public:
    QString address;
    QString label;
    QString pubkey;
    QString message;
};

struct MessageTableEntry
{
    enum Type {
        Sent,
        Received
    };
    
    std::vector<unsigned char> chKey;
    Type type;
    QString label;
    QString to_address;
    QString from_address;
    QDateTime sent_datetime;
    QDateTime received_datetime;
    QString message;

    MessageTableEntry() {}
    MessageTableEntry(std::vector<unsigned char> &chKey,
                      Type type,
                      const QString &label,
                      const QString &to_address,
                      const QString &from_address,
                      const QDateTime &sent_datetime,
                      const QDateTime &received_datetime,
                      const QString &message):
        chKey(chKey),
        type(type),
        label(label),
        to_address(to_address),
        from_address(from_address),
        sent_datetime(sent_datetime),
        received_datetime(received_datetime),
        message(message)
    {
    }
};

/** Interface to Verge Wraith Secure Messaging from Qt view code. */
class MessageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MessageModel(CWallet *wallet, WalletModel *walletModel, QObject *parent = 0);
    ~MessageModel();

    enum StatusCode // Returned by sendMessages
    {
        OK,
        InvalidAddress,
        InvalidMessage,
        DuplicateAddress,
        MessageCreationFailed, // Error returned when DB is still locked
        MessageCommitFailed,
        Aborted,
        FailedErrorShown
    };

    enum ColumnIndex {
        Type = 0,   /**< Sent/Received */
        SentDateTime = 1, /**< Time Sent */
        ReceivedDateTime = 2, /**< Time Received */
        Label = 3,   /**< User specified label */
        ToAddress = 4, /**< To Bitcoin address */
        FromAddress = 5, /**< From Bitcoin address */
        Message = 6, /**< Plaintext */
        TypeInt = 7, /**< Plaintext */
        Key = 8, /**< chKey */
        HTML = 9, /**< HTML Formatted Data */
    };

    /** Roles to get specific information from a message row.
        These are independent of column.
    */
    enum RoleIndex {
        /** Type of message */
        TypeRole = Qt::UserRole,
        /** Date and time this message was sent */
        /** message key */
        KeyRole,
        SentDateRole,
        /** Date and time this message was received */
        ReceivedDateRole,
        /** From Address of message */
        FromAddressRole,
        /** To Address of message */
        ToAddressRole,
        /** Filter address related to message */
        FilterAddressRole,
        /** Label of address related to message */
        LabelRole,
        /** Full Message */
        MessageRole,
        /** Short Message */
        ShortMessageRole,
        /** HTML Formatted */
        HTMLRole,
        /** Ambiguous bool */
        Ambiguous
    };

    static const QString Sent; /**< Specifies sent message */
    static const QString Received; /**< Specifies sent message */

    //QList<QString> ambiguous; /**< Specifies Ambiguous addresses */

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent) const;
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex & index) const;
    /*@}*/

    /* Look up row index of a message in the model.
       Return -1 if not found.
     */
    int lookupMessage(const QString &message) const;

    WalletModel *getWalletModel();
    OptionsModel *getOptionsModel();

    void resetFilter();

    bool getAddressOrPubkey( QString &Address,  QString &Pubkey) const;

    // Send messages to a list of recipients
    StatusCode sendMessages(const QList<SendMessagesRecipient> &recipients);
    StatusCode sendMessages(const QList<SendMessagesRecipient> &recipients, const QString &addressFrom);
    
    QSortFilterProxyModel *proxyModel;
    
private:
    CWallet *wallet;
    WalletModel *walletModel;
    OptionsModel *optionsModel;
    MessageTablePriv *priv;
    QStringList columns;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

public slots:

    /* Check for new messages */
    void newMessage(const SecMsgStored& smsg);
    void newOutboxMessage(const SecMsgStored& smsg);
    
    void walletUnlocked();
    
    void setEncryptionStatus(int status);

    friend class MessageTablePriv;

signals:
    // Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);
};

#endif // MESSAGEMODEL_H