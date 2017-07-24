#ifndef BLOCKBROWSER_H
#define BLOCKBROWSER_H

#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include <QWidget>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>

double getBlockHardness(int);
double getTxTotalValue(std::string);
double convertCoins(int64_t);
double getTxFees(std::string);
int getBlockTime(int);
int getBlocknBits(int);
int getBlockNonce(int);
int blocksInPastHours(int);
int getBlockHashrate(int);
std::string getInputs(std::string);
std::string getOutputs(std::string);
std::string getBlockHash(int);
std::string getBlockMerkle(int);
bool addnode(std::string);
const CBlockIndex* getBlockIndex(int);
int64_t getInputValue(CTransaction, CScript);


namespace Ui {
class BlockBrowser;
}
class ClientModel;

class BlockBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit BlockBrowser(QWidget *parent = 0);
    ~BlockBrowser();
    
    void setModel(ClientModel *model);
    
public slots:
    
    void blockClicked();
    void txClicked();
    void updateExplorer(bool);

private slots:

private:
    Ui::BlockBrowser *ui;
    ClientModel *model;
    
};

#endif // BLOCKBROWSER_H
