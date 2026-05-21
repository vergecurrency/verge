#include <qt/guiutil.h>
#include <qt/guiconstants.h>
#include <qt/vergeunits.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>
#include <qt/messagemodel.h>
#include <qt/addresstablemodel.h>
#include <ui_interface.h>
#include <base58.h>
#include <boost/bind/bind.hpp>
#include <smsg/db.h>
#include <key_io.h>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QFont>
#include <QColor>

#include <algorithm>
Q_DECLARE_METATYPE(std::vector<unsigned char>);
Q_DECLARE_METATYPE(smsg::SecMsgStored);

using smsg::MessageData;
using smsg::SecMsgDB;
using smsg::SecMsgStored;
using smsg::SecureMessage;
using smsg::SMSG_HDR_LEN;
using boost::placeholders::_1;
 QList<QString> ambiguous; /**< Specifies Ambiguous addresses */
 const QString MessageModel::Sent = "Sent";
const QString MessageModel::Received = "Received";
 struct MessageTableEntryLessThan
{
    bool operator()(const MessageTableEntry &a, const MessageTableEntry &b) const {return a.received_datetime < b.received_datetime;};
    bool operator()(const MessageTableEntry &a, const QDateTime         &b) const {return a.received_datetime < b;}
    bool operator()(const QDateTime         &a, const MessageTableEntry &b) const {return a < b.received_datetime;}
};
 // Private implementation
class MessageTablePriv
{
public:
    QList<MessageTableEntry> cachedMessageTable;
    MessageModel *parent;
     MessageTablePriv(MessageModel *parent):
        parent(parent) {}
     void refreshMessageTable()
    {
        cachedMessageTable.clear();
        
        if (parent->getWalletModel()->getEncryptionStatus() == WalletModel::Locked)
        {
            // -- messages are stored encrypted, can't load them without the private keys
            return;
        };
         {
            LOCK(smsg::cs_smsgDB);
             SecMsgDB dbSmsg;
             if (!dbSmsg.Open("cr+"))
                //throw runtime_error("Could not open DB.");
                return;
             unsigned char chKey[18];
            std::vector<unsigned char> vchKey;
            vchKey.resize(18);
             SecMsgStored smsgStored;
            MessageData msg;
            QString label;
            QDateTime sent_datetime;
            QDateTime received_datetime;
             std::string sPrefix("im");
            leveldb::Iterator* it = dbSmsg.pdb->NewIterator(leveldb::ReadOptions());
            while (dbSmsg.NextSmesg(it, sPrefix, chKey, smsgStored))
            {
                uint32_t nPayload = smsgStored.vchMessage.size() - SMSG_HDR_LEN;
                if (smsgModule.Decrypt(false, smsgStored.addrTo, &smsgStored.vchMessage[0], &smsgStored.vchMessage[SMSG_HDR_LEN], nPayload, msg) == 0)
                {
                    label = parent->getWalletModel()->getAddressTableModel()->labelForAddress(QString::fromStdString(msg.sFromAddress));
                    sent_datetime = QDateTime::fromSecsSinceEpoch(msg.timestamp);
                    received_datetime = QDateTime::fromSecsSinceEpoch(smsgStored.timeReceived);
                    
                    memcpy(&vchKey[0], chKey, 18);
                     addMessageEntry(MessageTableEntry(vchKey,
                                                      MessageTableEntry::Received,
                                                      label,
                                                      QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                                      QString::fromStdString(msg.sFromAddress),
                                                      sent_datetime,
                                                      received_datetime,
                                                      (char*)&msg.vchMessage[0]),
                                    true);
                }
            };
             delete it;
             sPrefix = "sm";
            it = dbSmsg.pdb->NewIterator(leveldb::ReadOptions());
            while (dbSmsg.NextSmesg(it, sPrefix, chKey, smsgStored))
            {
                uint32_t nPayload = smsgStored.vchMessage.size() - SMSG_HDR_LEN;
                if (smsgModule.Decrypt(false, smsgStored.addrOutbox, &smsgStored.vchMessage[0], &smsgStored.vchMessage[SMSG_HDR_LEN], nPayload, msg) == 0)
                {
                    label = parent->getWalletModel()->getAddressTableModel()->labelForAddress(QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()));
                    sent_datetime = QDateTime::fromSecsSinceEpoch(msg.timestamp);
                    received_datetime = QDateTime::fromSecsSinceEpoch(smsgStored.timeReceived);
                    
                    memcpy(&vchKey[0], chKey, 18);
                     addMessageEntry(MessageTableEntry(vchKey,
                                                      MessageTableEntry::Sent,
                                                      label,
                                                      QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                                      QString::fromStdString(msg.sFromAddress),
                                                      sent_datetime,
                                                      received_datetime,
                                                      (char*)&msg.vchMessage[0]),
                                    true);
                }
            };
             delete it;
        }
    }
     void newMessage(const SecMsgStored& inboxHdr)
    {
        // we have to copy it, because it doesn't like constants going into Decrypt
        SecMsgStored smsgStored = inboxHdr;
        MessageData msg;
        QString label;
        QDateTime sent_datetime;
        QDateTime received_datetime;
         uint32_t nPayload = smsgStored.vchMessage.size() - SMSG_HDR_LEN;
        if (smsgModule.Decrypt(false, smsgStored.addrTo, &smsgStored.vchMessage[0], &smsgStored.vchMessage[SMSG_HDR_LEN], nPayload, msg) == 0)
        {
            label = parent->getWalletModel()->getAddressTableModel()->labelForAddress(QString::fromStdString(msg.sFromAddress));
            sent_datetime = QDateTime::fromSecsSinceEpoch(msg.timestamp);
            received_datetime = QDateTime::fromSecsSinceEpoch(smsgStored.timeReceived);
             std::string sPrefix("im");
            SecureMessage* psmsg = (SecureMessage*) &smsgStored.vchMessage[0];
            
            std::vector<unsigned char> vchKey;
            vchKey.resize(18);
            memcpy(&vchKey[0],  sPrefix.data(),  2);
            memcpy(&vchKey[2],  &psmsg->timestamp, 8);
            memcpy(&vchKey[10], &smsgStored.vchMessage[SMSG_HDR_LEN], 8);    // sample
             addMessageEntry(MessageTableEntry(vchKey,
                                              MessageTableEntry::Received,
                                              label,
                                              QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                              QString::fromStdString(msg.sFromAddress),
                                              sent_datetime,
                                              received_datetime,
                                              (char*)&msg.vchMessage[0]),
                            false);
        }
    }
     void newOutboxMessage(const SecMsgStored& outboxHdr)
    {
         SecMsgStored smsgStored = outboxHdr;
        MessageData msg;
        QString label;
        QDateTime sent_datetime;
        QDateTime received_datetime;
         uint32_t nPayload = smsgStored.vchMessage.size() - SMSG_HDR_LEN;
        if (smsgModule.Decrypt(false, smsgStored.addrOutbox, &smsgStored.vchMessage[0], &smsgStored.vchMessage[SMSG_HDR_LEN], nPayload, msg) == 0)
        {
            label = parent->getWalletModel()->getAddressTableModel()->labelForAddress(QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()));
            sent_datetime = QDateTime::fromSecsSinceEpoch(msg.timestamp);
            received_datetime = QDateTime::fromSecsSinceEpoch(smsgStored.timeReceived);
             std::string sPrefix("sm");
            SecureMessage* psmsg = (SecureMessage*) &smsgStored.vchMessage[0];
            std::vector<unsigned char> vchKey;
            vchKey.resize(18);
            memcpy(&vchKey[0],  sPrefix.data(),  2);
            memcpy(&vchKey[2],  &psmsg->timestamp, 8);
            memcpy(&vchKey[10], &smsgStored.vchMessage[SMSG_HDR_LEN], 8);    // sample
             addMessageEntry(MessageTableEntry(vchKey,
                                              MessageTableEntry::Sent,
                                              label,
                                              QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                              QString::fromStdString(msg.sFromAddress),
                                              sent_datetime,
                                              received_datetime,
                                              (char*)&msg.vchMessage[0]),
                            false);
        }
    }
     void walletUnlocked()
    {
        // -- wallet is unlocked, can get at the private keys now
        refreshMessageTable();

        parent->beginResetModel();
        parent->endResetModel();
        
        if (parent->proxyModel)
        {
            parent->proxyModel->setFilterRole(false);
            parent->proxyModel->setFilterFixedString("");
            parent->resetFilter();
            parent->proxyModel->setFilterRole(MessageModel::Ambiguous);
            parent->proxyModel->setFilterFixedString("true");
        }
        
        //invalidateFilter()
    }
    
    void setEncryptionStatus()
    {
        const int status = parent->getWalletModel()->getEncryptionStatus();
        if (status == WalletModel::Locked)
        {
            // -- Wallet is locked, clear secure message display.
            cachedMessageTable.clear();
            parent->beginResetModel();
            parent->endResetModel();
        };
    };
     MessageTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedMessageTable.size())
            return &cachedMessageTable[idx];
        else
            return 0;
    }
 private:
     void addMessageEntry(const MessageTableEntry & message, const bool & append)
    {
        if(append)
        {
            cachedMessageTable.append(message);
        } else
        {
            int index = std::lower_bound(cachedMessageTable.begin(), cachedMessageTable.end(), message.received_datetime, MessageTableEntryLessThan()) - cachedMessageTable.begin();
            parent->beginInsertRows(QModelIndex(), index, index);
            cachedMessageTable.insert(
                        index,
                        message);
            parent->endInsertRows();
        }
    }
 };
MessageModel::MessageModel(WalletModel *walletModel, QObject *parent) :
    QAbstractTableModel(parent), walletModel(walletModel), optionsModel(0), priv(0)
{
    columns << tr("Type") << tr("Sent Date Time") << tr("Received Date Time") << tr("Label") << tr("To Address") << tr("From Address") << tr("Message");
    
    proxyModel = NULL;
    
    optionsModel = walletModel->getOptionsModel();
     priv = new MessageTablePriv(this);
    priv->refreshMessageTable();
     subscribeToCoreSignals();
}
 MessageModel::~MessageModel()
{
    if (proxyModel)
        delete proxyModel;
    
    delete priv;
    unsubscribeFromCoreSignals();
}
bool MessageModel::getAddressOrPubkey(QString &address, QString &pubkey) const
{
    CBitcoinAddress addressParsed(address.toStdString());
     if(addressParsed.IsValid()) {
        CKeyID destinationAddress;
        CPubKey destinationKey;
         addressParsed.GetKeyID(destinationAddress);
         if (smsgModule.GetStoredKey(destinationAddress, destinationKey) != 0
            && smsgModule.GetLocalKey(destinationAddress, destinationKey) != 0)
            return false;
         address = QString::fromStdString(CBitcoinAddress(destinationAddress).ToString());
        pubkey = EncodeBase58(destinationKey.Raw()).c_str();
        return true;
    }
    return false;
}
 WalletModel *MessageModel::getWalletModel()
{
    return walletModel;
}
 OptionsModel *MessageModel::getOptionsModel()
{
    return optionsModel;
}
 MessageModel::StatusCode MessageModel::sendMessages(const QList<SendMessagesRecipient> &recipients, const QString &addressFrom)
{
     QSet<QString> setAddress;
     if(recipients.empty())
        return OK;
     // Pre-check input data for validity
    for (const SendMessagesRecipient &rcp : recipients)
    {
        if(!walletModel->validateAddress(rcp.address))
            return InvalidAddress;
        if (rcp.message.trimmed().isEmpty())
            return InvalidMessage;
        if (setAddress.contains(rcp.address))
            return DuplicateAddress;
        setAddress.insert(rcp.address);
    }

    for (const SendMessagesRecipient &rcp : recipients)
    {
        std::string sendTo  = rcp.address.toStdString();
        std::string pubkey  = rcp.pubkey.toStdString();
        std::string message = rcp.message.toStdString();
        std::string addFrom = addressFrom.toStdString();
        if (!pubkey.empty()) {
            smsgModule.AddAddress(sendTo, pubkey);
        }
        
        std::string sError;
        CKeyID addressFrom;
        CKeyID addressTo;
        if (addFrom != "anon") {
            CBitcoinAddress fromAddress(addFrom);
            if (!fromAddress.IsValid() || !fromAddress.GetKeyID(addressFrom)) {
                return InvalidAddress;
            }
        }
        CBitcoinAddress toAddress(sendTo);
        if (!toAddress.IsValid() || !toAddress.GetKeyID(addressTo)) {
            return InvalidAddress;
        }
        smsg::SecureMessage smsgOut;
        if (smsgModule.Send(addressFrom, addressTo, message, smsgOut, sError) != 0)
        {
            QMessageBox::warning(NULL, tr("Send Secure Message"),
                tr("Send failed: %1.").arg(sError.c_str()),
                QMessageBox::Ok, QMessageBox::Ok);
            
            return FailedErrorShown;
        };
         // Add addresses / update labels that we've sent to the address book
        std::string strAddress = rcp.address.toStdString();
        CTxDestination dest = CBitcoinAddress(strAddress).Get();
        std::string strLabel = rcp.label.toStdString();
        if (!strLabel.empty()) {
            std::string currentLabel;
            isminetype isMine = ISMINE_NO;
            std::string purpose;
            walletModel->wallet().getAddress(dest, &currentLabel, &isMine, &purpose);
            if (currentLabel != strLabel) {
                walletModel->wallet().setAddressBook(dest, strLabel, purpose.empty() ? "send" : purpose);
            }
        }
    }
    return OK;
}
 MessageModel::StatusCode MessageModel::sendMessages(const QList<SendMessagesRecipient> &recipients)
{
    return sendMessages(recipients, "anon");
}
 int MessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->cachedMessageTable.size();
}
 int MessageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}
QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();
     MessageTableEntry *rec = static_cast<MessageTableEntry*>(index.internalPointer());
    const QString filter_address = rec->type == MessageTableEntry::Sent
        ? rec->to_address + rec->from_address
        : rec->from_address + rec->to_address;
    const QString html = QStringLiteral("%1<br>%2<br>%3")
        .arg(rec->received_datetime.toString(),
             rec->label.isEmpty() ? rec->from_address : rec->label,
             rec->message);
     switch(role)
    {
    /*
    case Qt::DecorationRole:
        switch(index.column())
        {
            return txStatusDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;*/
    case Qt::DisplayRole:
        switch(index.column())
        {
            case Label:	           return (rec->label.isEmpty() ? tr("(no label)") : rec->label);
            case ToAddress:	       return rec->to_address;
            case FromAddress:      return rec->from_address;
            case SentDateTime:     return rec->sent_datetime;
            case ReceivedDateTime: return rec->received_datetime;
            case Message:          return rec->message;
            case TypeInt:          return rec->type;
            case HTML:             return html;
            case Type:
                switch(rec->type)
                {
                    case MessageTableEntry::Sent:     return Sent;
                    case MessageTableEntry::Received: return Received;
                    default: break;
                }
            case Key:               return QVariant::fromValue(rec->chKey);
        }
        break;
    case KeyRole:           return QVariant::fromValue(rec->chKey);
    case TypeRole:          return rec->type;
    case SentDateRole:      return rec->sent_datetime;
    case ReceivedDateRole:  return rec->received_datetime;
    case FromAddressRole:   return rec->from_address;
    case ToAddressRole:     return rec->to_address;
    case FilterAddressRole: return filter_address;
    case LabelRole:         return rec->label;
    case MessageRole:       return rec->message;
    case ShortMessageRole:  return rec->message; // TODO: Short message
    case HTMLRole:          return html;
    case Ambiguous:
        int it;
         for (it = 0; it<ambiguous.length(); it++) {
            if(ambiguous[it] == filter_address)
                return false;
        }
        ambiguous.append(filter_address);
         return QStringLiteral("true");
        break;
    }
     return QVariant();
}
 QVariant MessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return (orientation == Qt::Horizontal && role == Qt::DisplayRole ? columns[section] : QVariant());
}
 Qt::ItemFlags MessageModel::flags(const QModelIndex & index) const
{
    if(index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
     return Qt::NoItemFlags;
}
 QModelIndex MessageModel::index(int row, int column, const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    MessageTableEntry *data = priv->index(row);
    return (data ? createIndex(row, column, priv->index(row)) : QModelIndex());
}
 bool MessageModel::removeRows(int row, int count, const QModelIndex & parent)
{
    MessageTableEntry *rec = priv->index(row);
    if(count != 1 || !rec)
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove receiving addresses.
        return false;
     {
        LOCK(smsg::cs_smsgDB);
        SecMsgDB dbSmsg;
         if (!dbSmsg.Open("cr+"))
            //throw runtime_error("Could not open DB.");
            return false;
         dbSmsg.EraseSmesg(&rec->chKey[0]);
    }
     beginRemoveRows(parent, row, row);
    priv->cachedMessageTable.removeAt(row);
    endRemoveRows();
     return true;
}
 void MessageModel::resetFilter()
{
    ambiguous.clear();
}
 void MessageModel::newMessage(const SecMsgStored &smsg)
{
    priv->newMessage(smsg);
}
 void MessageModel::newOutboxMessage(const SecMsgStored &smsgOutbox)
{
    priv->newOutboxMessage(smsgOutbox);
}
 void MessageModel::walletUnlocked()
{
    priv->walletUnlocked();
}
 void MessageModel::setEncryptionStatus()
{
    priv->setEncryptionStatus();
}
static void NotifySecMsgInbox(MessageModel *messageModel, SecMsgStored inboxHdr)
{
    // Too noisy: OutputDebugStringF("NotifySecMsgInboxChanged %s\n", message);
    QMetaObject::invokeMethod(messageModel, "newMessage", Qt::QueuedConnection,
                              Q_ARG(smsg::SecMsgStored, inboxHdr));
}
 static void NotifySecMsgOutbox(MessageModel *messageModel, SecMsgStored outboxHdr)
{
    QMetaObject::invokeMethod(messageModel, "newOutboxMessage", Qt::QueuedConnection,
                              Q_ARG(smsg::SecMsgStored, outboxHdr));
}
 static void NotifySecMsgWallet(MessageModel *messageModel)
{
    QMetaObject::invokeMethod(messageModel, "walletUnlocked", Qt::QueuedConnection);
}
 void MessageModel::subscribeToCoreSignals()
{
    qRegisterMetaType<smsg::SecMsgStored>("smsg::SecMsgStored");
    qRegisterMetaType<smsg::SecMsgStored>("SecMsgStored");
     // Connect signals
    smsg::NotifySecMsgInboxChanged.connect(boost::bind(NotifySecMsgInbox, this, _1));
    smsg::NotifySecMsgOutboxChanged.connect(boost::bind(NotifySecMsgOutbox, this, _1));
    smsg::NotifySecMsgWalletUnlocked.connect(boost::bind(NotifySecMsgWallet, this));
    
    connect(walletModel, SIGNAL(encryptionStatusChanged()), this, SLOT(setEncryptionStatus()));
}
 void MessageModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals
    smsg::NotifySecMsgInboxChanged.disconnect(boost::bind(NotifySecMsgInbox, this, _1));
    smsg::NotifySecMsgOutboxChanged.disconnect(boost::bind(NotifySecMsgOutbox, this, _1));
    smsg::NotifySecMsgWalletUnlocked.disconnect(boost::bind(NotifySecMsgWallet, this));
    
    disconnect(walletModel, SIGNAL(encryptionStatusChanged()), this, SLOT(setEncryptionStatus()));
}
