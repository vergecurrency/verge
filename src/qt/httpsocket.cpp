#include "httpsocket.h"
#include <QtCore>

httpsocket::httpsocket(QObject *parent) :
    QObject(parent)
{
    response = "";
    useProxy = false;
}

void httpsocket::getUrl(QString url)
{
    if(this->useProxy==true){
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(this->proxyAddress);//"200.50.166.43"
        proxy.setPort(this->proxyPort);//8080
        //proxy.setUser("username");
        //proxy.setPassword("password");
        QNetworkProxy::setApplicationProxy(proxy);
    }

    qDebug()<< "Connecting...";
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect(manager,SIGNAL(finished(QNetworkReply*)),this,SLOT(replyFinished(QNetworkReply*)));

    manager->get(QNetworkRequest(QUrl(url)));
}

void httpsocket::replyFinished(QNetworkReply *reply)
{
    if(reply->error()==QNetworkReply::NoError){
        qDebug() << "Response";
        if(reply->isOpen()){
            QString response = reply->readAll();
            this->response = response;//the contents of response = response.
            reply->close();
            //qDebug()<<"Finished With Socket Data...";
        }
    }else{
        qDebug()<<"Error Occured";
        this->response = "Error";
        this->error = reply->errorString();
    }
    emit finished();
}