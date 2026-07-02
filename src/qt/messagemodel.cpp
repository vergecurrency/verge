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
#include <validation.h>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QFont>
#include <QColor>
#include <logging.h>

#include <algorithm>
Q_DECLARE_METATYPE(std::vector<unsigned char>);
Q_DECLARE_METATYPE(smsg::SecMsgStored);

using smsg::MessageData;
using smsg::SecMsgDB;
using smsg::SecMsgStored;
using smsg::SecureMessage;
using smsg::SMSG_HDR_LEN;
using boost::placeholders::_1;

static bool HasQueuedOutboxEntry(SecMsgDB &dbSmsg, const uint8_t *outboxKey)
{
    uint8_t queueKey[30];
    memcpy(queueKey, outboxKey, 30);
    memcpy(queueKey, "qm", 2);
    return dbSmsg.ExistsSmesg(queueKey);
}

static QString GetPaidFundingTxid(const SecMsgStored& smsgStored)
{
    if (smsgStored.vchMessage.size() <= SMSG_HDR_LEN) {
        return QString();
    }

    const SecureMessage* psmsg = reinterpret_cast<const SecureMessage*>(&smsgStored.vchMessage[0]);
    const uint32_t nPayload = smsgStored.vchMessage.size() - SMSG_HDR_LEN;
    if (!psmsg->IsPaidVersion()) {
        return QString();
    }

    uint256 txid;
    if (!smsg::GetFundingTxid(&smsgStored.vchMessage[SMSG_HDR_LEN], nPayload, txid)) {
        return QString();
    }
    return QString::fromStdString(txid.ToString());
}

static bool IsFundingTxConfirmed(const QString& txHash)
{
    if (txHash.isEmpty()) {
        return false;
    }

    uint256 txid;
    txid.SetHex(txHash.toStdString());
    if (smsgModule.IsPaidFundingTxConfirmed(txid)) {
        return true;
    }

    CTransactionRef tx;
    uint256 hashBlock;
    {
        LOCK(cs_main);
        if (!GetTransaction(txid, tx, Params().GetConsensus(), hashBlock, true)) {
            return false;
        }
        if (hashBlock.IsNull()) {
            return false;
        }
        const auto mi = mapBlockIndex.find(hashBlock);
        return mi != mapBlockIndex.end() && mi->second && chainActive.Contains(mi->second);
    }
}

const QString MessageModel::Sent = "Sent";
const QString MessageModel::Received = "Received";
 struct MessageTableEntryLessThan
{
    bool operator()(const MessageTableEntry &a, const MessageTableEntry &b) const {return a.received_datetime < b.received_datetime;};
    bool operator()(const MessageTableEntry &a, const QDateTime         &b) const {return a.received_datetime < b;}
    bool operator()(const QDateTime         &a, const MessageTableEntry &b) const {return a < b.received_datetime;}
};

static QString ConversationFilterAddress(const MessageTableEntry& entry)
{
    return entry.type == MessageTableEntry::Sent
        ? entry.to_address + entry.from_address
        : entry.from_address + entry.to_address;
}

static QDateTime ConversationTimestamp(const MessageTableEntry& entry)
{
    return entry.sent_datetime.isValid() ? entry.sent_datetime : entry.received_datetime;
}

static bool IsLaterConversationEntry(const MessageTableEntry& candidate, const MessageTableEntry& current)
{
    const QDateTime candidateTimestamp = ConversationTimestamp(candidate);
    const QDateTime currentTimestamp = ConversationTimestamp(current);

    if (candidateTimestamp != currentTimestamp) {
        return candidateTimestamp > currentTimestamp;
    }
    if (candidate.received_datetime != current.received_datetime) {
        return candidate.received_datetime > current.received_datetime;
    }
    return candidate.chKey > current.chKey;
}

static QString LabelOrChatkeyForAddress(WalletModel* walletModel, const QString& address)
{
    if (walletModel && walletModel->getAddressTableModel()) {
        const QString label = walletModel->getAddressTableModel()->labelForAddress(address).trimmed();
        if (!label.isEmpty()) {
            return label;
        }
    }

    QString resolvedAddress = address;
    QString pubkey;
    CBitcoinAddress addressParsed(address.toStdString());
    if (addressParsed.IsValid()) {
        CKeyID destinationAddress;
        CPubKey destinationKey;
        addressParsed.GetKeyID(destinationAddress);
        if (smsgModule.GetStoredKey(destinationAddress, destinationKey) == 0
            || smsgModule.GetLocalKey(destinationAddress, destinationKey) == 0) {
            resolvedAddress = QString::fromStdString(CBitcoinAddress(destinationAddress).ToString());
            pubkey = QString::fromStdString(EncodeBase58(destinationKey.Raw()));
        }
    }

    return pubkey.isEmpty() ? resolvedAddress : resolvedAddress + QStringLiteral("-") + pubkey;
}

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
             unsigned char chKey[30];
            std::vector<unsigned char> vchKey;
            vchKey.resize(30);
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
                    
                    memcpy(&vchKey[0], chKey, 30);
                    addMessageEntry(MessageTableEntry(vchKey,
                                                     MessageTableEntry::Received,
                                                     label,
                                                     QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                                     QString::fromStdString(msg.sFromAddress),
                                                     sent_datetime,
                                                     received_datetime,
                                                     QObject::tr("Received locally"),
                                                     (char*)&msg.vchMessage[0],
                                                     GetPaidFundingTxid(smsgStored)),
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
                    const QString status = HasQueuedOutboxEntry(dbSmsg, chKey)
                        ? QObject::tr("Queued")
                        : QObject::tr("Stored locally");
                    
                    memcpy(&vchKey[0], chKey, 30);
                     addMessageEntry(MessageTableEntry(vchKey,
                                                      MessageTableEntry::Sent,
                                                      label,
                                                      QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                                      QString::fromStdString(msg.sFromAddress),
                                                      sent_datetime,
                                                      received_datetime,
                                                      status,
                                                      (char*)&msg.vchMessage[0],
                                                      GetPaidFundingTxid(smsgStored)),
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
            vchKey.resize(30);
            memcpy(&vchKey[0],  sPrefix.data(),  2);
            memcpy(&vchKey[2],  &psmsg->timestamp, 8);
            memcpy(&vchKey[10], &smsgStored.vchMessage[SMSG_HDR_LEN], 20);
            addMessageEntry(MessageTableEntry(vchKey,
                                              MessageTableEntry::Received,
                                              label,
                                              QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                              QString::fromStdString(msg.sFromAddress),
                                              sent_datetime,
                                              received_datetime,
                                              QObject::tr("Received locally"),
                                              (char*)&msg.vchMessage[0],
                                              GetPaidFundingTxid(smsgStored)),
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
            QString status = QObject::tr("Stored locally");
             std::string sPrefix("sm");
            SecureMessage* psmsg = (SecureMessage*) &smsgStored.vchMessage[0];
            std::vector<unsigned char> vchKey;
            vchKey.resize(30);
            memcpy(&vchKey[0],  sPrefix.data(),  2);
            memcpy(&vchKey[2],  &psmsg->timestamp, 8);
            memcpy(&vchKey[10], &smsgStored.vchMessage[SMSG_HDR_LEN], 20);
            {
                LOCK(smsg::cs_smsgDB);
                SecMsgDB dbSmsg;
                if (dbSmsg.Open("cr+")) {
                    status = HasQueuedOutboxEntry(dbSmsg, &vchKey[0])
                        ? QObject::tr("Queued")
                        : QObject::tr("Stored locally");
                }
            }
             addMessageEntry(MessageTableEntry(vchKey,
                                              MessageTableEntry::Sent,
                                              label,
                                              QString::fromStdString(CBitcoinAddress(smsgStored.addrTo).ToString()),
                                              QString::fromStdString(msg.sFromAddress),
                                              sent_datetime,
                                              received_datetime,
                                              status,
                                              (char*)&msg.vchMessage[0],
                                              GetPaidFundingTxid(smsgStored)),
                            false);
        }
    }
    void walletUnlocked()
    {
        // -- wallet is unlocked, can get at the private keys now
        parent->beginResetModel();
        refreshMessageTable();
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
            parent->beginResetModel();
            cachedMessageTable.clear();
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
    columns << tr("Type") << tr("Status") << tr("Sent Date Time") << tr("Received Date Time") << tr("Label") << tr("To") << tr("From") << tr("Message");
    
    proxyModel = NULL;
    
    optionsModel = walletModel->getOptionsModel();
    priv = new MessageTablePriv(this);
    priv->refreshMessageTable();
     subscribeToCoreSignals();
}
MessageModel::~MessageModel()
{
    unsubscribeFromCoreSignals();

    proxyModel = 0;
    delete priv;
    priv = 0;
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
 MessageModel::StatusCode MessageModel::sendMessages(const QList<SendMessagesRecipient> &recipients, const QString &addressFrom, bool paidMessage, int retentionDays)
{
    paidMessage = true;
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
        LogPrint(BCLog::SMSG, "SMSG UI send request: from=%s to=%s bytes=%u has_pubkey=%u paid=%u retention=%d\n",
            addFrom,
            sendTo,
            static_cast<unsigned int>(message.size()),
            pubkey.empty() ? 0 : 1,
            paidMessage ? 1 : 0,
            retentionDays);
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
        if (smsgModule.Send(addressFrom, addressTo, message, smsgOut, sError, paidMessage, retentionDays) != 0)
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
 MessageModel::StatusCode MessageModel::sendMessages(const QList<SendMessagesRecipient> &recipients, bool paidMessage, int retentionDays)
{
    return sendMessages(recipients, "anon", paidMessage, retentionDays);
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
    const QString filter_address = ConversationFilterAddress(*rec);
    QString messageLabel = rec->label;
    if (rec->type == MessageTableEntry::Sent && walletModel && walletModel->getAddressTableModel()) {
        messageLabel = walletModel->getAddressTableModel()->labelForAddress(rec->from_address).trimmed();
    }
    if (messageLabel.isEmpty()) {
        messageLabel = rec->from_address;
    }
    const QString html = QStringLiteral(
        "<span style=\"color:#FF9A2F; font-size:12px;\">%1</span><br>"
        "<span style=\"color:#FFFFFF;\">%2</span><br>"
        "<span style=\"color:#5CFF7A;\">%3</span>")
        .arg(rec->received_datetime.toString(),
             messageLabel,
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
            case Status:           return rec->status;
            case Label:	           return (rec->label.isEmpty() ? tr("(no label)") : rec->label);
            case ToAddress:	       return LabelOrChatkeyForAddress(walletModel, rec->to_address);
            case FromAddress:      return LabelOrChatkeyForAddress(walletModel, rec->from_address);
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
    case StatusRole:        return rec->status;
    case MessageRole:       return rec->message;
    case ShortMessageRole:  return rec->message; // TODO: Short message
    case HTMLRole:          return html;
    case ReceiptAvailableRole: return !rec->funding_txid.isEmpty();
    case ReceiptTxHashRole: return rec->funding_txid;
    case ReceiptConfirmedRole: return IsFundingTxConfirmed(rec->funding_txid);
    case Ambiguous:
        for (const MessageTableEntry& entry : priv->cachedMessageTable) {
            if (ConversationFilterAddress(entry) != filter_address) {
                continue;
            }
            if (IsLaterConversationEntry(entry, *rec)) {
                return false;
            }
        }
        return QStringLiteral("true");
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
}
 void MessageModel::newMessage(const SecMsgStored &smsg)
{
    if (!priv)
        return;
    priv->newMessage(smsg);
}
void MessageModel::newOutboxMessage(const SecMsgStored &smsgOutbox)
{
    if (!priv)
        return;
    priv->newOutboxMessage(smsgOutbox);
}
void MessageModel::walletUnlocked()
{
    if (!priv)
        return;
    priv->walletUnlocked();
}
void MessageModel::reloadMessages()
{
    if (!priv)
        return;

    beginResetModel();
    priv->refreshMessageTable();
    endResetModel();
}
void MessageModel::setEncryptionStatus()
{
    if (!priv)
        return;
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
    notifySecMsgInboxChanged = smsg::NotifySecMsgInboxChanged.connect(boost::bind(NotifySecMsgInbox, this, _1));
    notifySecMsgOutboxChanged = smsg::NotifySecMsgOutboxChanged.connect(boost::bind(NotifySecMsgOutbox, this, _1));
    notifySecMsgWalletUnlocked = smsg::NotifySecMsgWalletUnlocked.connect(boost::bind(NotifySecMsgWallet, this));
    
    connect(walletModel, SIGNAL(encryptionStatusChanged()), this, SLOT(setEncryptionStatus()));
}
void MessageModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals
    notifySecMsgInboxChanged.disconnect();
    notifySecMsgOutboxChanged.disconnect();
    notifySecMsgWalletUnlocked.disconnect();
}
