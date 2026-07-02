#ifndef VERGE_QT_MESSAGEPAGE_H
#define VERGE_QT_MESSAGEPAGE_H
 #include <QWidget>
 namespace Ui {
    class MessagePage;
}
class MessageModel;
class PlatformStyle;
//class OptionsModel;
 QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
class QMenu;
class QModelIndex;
class MessageViewDelegate;
class QPushButton;
class QLabel;
class QTimer;
class QPlainTextEdit;
class QSpinBox;
QT_END_NAMESPACE
 /** Widget that shows a list of sending or receiving addresses.
  */
class MessagePage : public QWidget
{
    Q_OBJECT
 public:
     explicit MessagePage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~MessagePage();
     void setModel(MessageModel *model);
 private:
    void setupTextActions();
 public Q_SLOTS:
    void exportClicked();
 private:
    Ui::MessagePage *ui;
    MessageModel *model;
    const PlatformStyle *platformStyle;
    
    QMenu *contextMenu;
    QAction *replyAction;
    QAction *copyFromAddressAction;
    QAction *copyToAddressAction;
    QAction *deleteAction;
    QString replyFromAddress;
    QString replyToAddress;
    MessageViewDelegate *msgdelegate;
    QPushButton *flushButton;
    QPushButton *addressBookButton;
    QLabel *storageLabel;
    QLabel *messageCountLabel;
    QLabel *receiptLink;
    QSpinBox *retentionDaysSpinBox;
    QLabel *paidFeeLabel;
    QTimer *storageRefreshTimer;
 private Q_SLOTS:
    void on_sendButton_clicked();
    void on_newButton_clicked();
    void on_addressBookButton_clicked();
    void on_flushButton_clicked();
    void refreshStorageUsage();
    void on_copyFromAddressButton_clicked();
    void on_copyToAddressButton_clicked();
    void on_deleteButton_clicked();
    void on_backButton_clicked();
    void messageTextChanged();
    void updateMessageCountdown();
    void showReceipt();
    void selectionChanged();
    void itemSelectionChanged();
    void incomingMessage();
    void handleModelReset();
    /** Spawn contextual menu (right mouse menu) for address book entry */
    void contextualMenu(const QPoint &point);
 Q_SIGNALS:
};

#endif // VERGE_QT_MESSAGEPAGE_H
