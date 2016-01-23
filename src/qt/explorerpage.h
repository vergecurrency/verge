#ifndef EXPLORERPAGE_H
#define EXPLORERPAGE_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class ExplorerPage;
}
class ClientModel;
class WalletModel;
class ExplorerViewDelegate;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Block explorer page widget */
class ExplorerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerPage(QWidget *parent = 0);
    ~ExplorerPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);

public slots:

signals:

private:
    Ui::ExplorerPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

    ExplorerViewDelegate *explorerdelegate;
    QNetworkAccessManager *nam;

private slots:
    void finished(QNetworkReply *reply);
    void DoHttpGet();
};

#endif // EXPLORERPAGE_H
