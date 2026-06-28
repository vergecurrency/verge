#ifndef VERGE_QT_MESSAGEMODEL_H
#define VERGE_QT_MESSAGEMODEL_H

#include <boost/signals2/connection.hpp>

#include <uint256.h>
#include <vector>
#include <smsg/smessage.h>
#include <map>
#include <QSortFilterProxyModel>
#include <QAbstractTableModel>
#include <QStringList>
#include <QDateTime>
 class MessageTablePriv;
class InvoiceTableModel;
class InvoiceItemTableModel;
class ReceiptTableModel;
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
    QString status;
    QString message;
    QString funding_txid;
     MessageTableEntry() {}
    MessageTableEntry(std::vector<unsigned char> &chKey,
                      Type type,
                      const QString &label,
                      const QString &to_address,
                      const QString &from_address,
                      const QDateTime &sent_datetime,
                      const QDateTime &received_datetime,
                      const QString &status,
                      const QString &message,
                      const QString &funding_txid = QString()):
        chKey(chKey),
        type(type),
        label(label),
        to_address(to_address),
        from_address(from_address),
        sent_datetime(sent_datetime),
        received_datetime(received_datetime),
        status(status),
        message(message),
        funding_txid(funding_txid)
    {
    }
};
 /** Interface to Verge Secure Messaging (VISP) from Qt view code. */
class MessageModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit MessageModel(WalletModel *walletModel, QObject *parent = 0);
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
        Status = 1, /**< Delivery / receive status */
        SentDateTime = 2, /**< Time Sent */
        ReceivedDateTime = 3, /**< Time Received */
        Label = 4,   /**< User specified label */
        ToAddress = 5, /**< To Verge address */
        FromAddress = 6, /**< From Verge address */
        Message = 7, /**< Plaintext */
        TypeInt = 8, /**< Plaintext */
        Key = 9, /**< chKey */
        HTML = 10, /**< HTML Formatted Data */
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
        /** Delivery / receive status */
        StatusRole,
        /** Full Message */
        MessageRole,
        /** Short Message */
        ShortMessageRole,
        /** HTML Formatted */
        HTMLRole,
        /** Ambiguous bool */
        Ambiguous,
        /** Paid SMSG funding receipt data */
        ReceiptAvailableRole,
        ReceiptTxHashRole,
        ReceiptConfirmedRole
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
    StatusCode sendMessages(const QList<SendMessagesRecipient> &recipients, bool paidMessage = true, int retentionDays = 31);
    StatusCode sendMessages(const QList<SendMessagesRecipient> &recipients, const QString &addressFrom, bool paidMessage = true, int retentionDays = 31);
    
    QSortFilterProxyModel *proxyModel;
    
private:
    WalletModel *walletModel;
    OptionsModel *optionsModel;
    MessageTablePriv *priv;
    QStringList columns;
    boost::signals2::connection notifySecMsgInboxChanged;
    boost::signals2::connection notifySecMsgOutboxChanged;
    boost::signals2::connection notifySecMsgWalletUnlocked;
     void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
public Q_SLOTS:
    /* Check for new messages */
    void newMessage(const smsg::SecMsgStored& smsg);
    void newOutboxMessage(const smsg::SecMsgStored& smsg);
    
    void walletUnlocked();
    void reloadMessages();
    
    void setEncryptionStatus();
     friend class MessageTablePriv;
Q_SIGNALS:
    // Asynchronous error notification
    void error(const QString &title, const QString &message, bool modal);
};

#endif // VERGE_QT_MESSAGEMODEL_H
