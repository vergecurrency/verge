#include "uritests.h"
#include "../guiutil.h"
#include "../walletmodel.h"

#include <QUrl>

/*
struct SendCoinsRecipient
{
    QString address;
    QString label;
    qint64 amount;
};
*/

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("verge://fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr"));
    QVERIFY(rv.label == QString());

    // We currently don't implement the message parameter (ok, yea, we break spec...)
    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?req-message=Wikipedia Example Address"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("verge:fJGCsof8w19W8S2fPfQHYSz3EVkytxfFMr?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
