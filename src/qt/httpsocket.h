#ifndef HTTPSOCKET_H
#define HTTPSOCKET_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDebug>

#include <QNetworkProxy>

class httpsocket : public QObject
{
    Q_OBJECT
public:
    explicit httpsocket(QObject *parent = 0);
    QString response;
    QString error;
    QString proxyAddress;
    int proxyPort;
    bool useProxy;
signals:
    void finished();
public slots:
    void getUrl(QString url);
    void replyFinished(QNetworkReply *reply);
};

#endif // HTTPSOCKET_H