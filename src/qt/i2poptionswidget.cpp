#include "i2poptionswidget.h"
#include "ui_i2poptionswidget.h"

#include "optionsmodel.h"
#include "monitoreddatamapper.h"
#include "showi2paddresses.h"
//#include "i2p.h"
#include "util.h"
#include "clientmodel.h"


I2POptionsWidget::I2POptionsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::I2POptionsWidget),
    clientModel(0)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonCurrentI2PAddress,  SIGNAL(clicked()), this, SLOT(ShowCurrentI2PAddress()));
    QObject::connect(ui->pushButtonGenerateI2PAddress, SIGNAL(clicked()), this, SLOT(GenerateNewI2PAddress()));

    QObject::connect(ui->checkBoxAllowZeroHop         , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->checkBoxInboundAllowZeroHop  , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->checkBoxUseI2POnly           , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->lineEditSAMHost              , SIGNAL(textChanged(QString)), this, SIGNAL(settingsChanged()));
    QObject::connect(ui->lineEditTunnelName           , SIGNAL(textChanged(QString)), this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundBackupQuality  , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundIPRestriction  , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundLength         , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundLengthVariance , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundQuantity       , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundBackupQuantity, SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundIPRestriction , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundLength        , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundLengthVariance, SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundPriority      , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundQuantity      , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxSAMPort               , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
}

I2POptionsWidget::~I2POptionsWidget()
{
    delete ui;
}

void I2POptionsWidget::setMapper(MonitoredDataMapper& mapper)
{
    mapper.addMapping(ui->checkBoxUseI2POnly           , OptionsModel::I2PUseI2POnly);
    mapper.addMapping(ui->lineEditSAMHost              , OptionsModel::I2PSAMHost);
    mapper.addMapping(ui->spinBoxSAMPort               , OptionsModel::I2PSAMPort);
    mapper.addMapping(ui->lineEditTunnelName           , OptionsModel::I2PSessionName);
    mapper.addMapping(ui->spinBoxInboundQuantity       , OptionsModel::I2PInboundQuantity);
    mapper.addMapping(ui->spinBoxInboundLength         , OptionsModel::I2PInboundLength);
    mapper.addMapping(ui->spinBoxInboundLengthVariance , OptionsModel::I2PInboundLengthVariance);
    mapper.addMapping(ui->spinBoxInboundBackupQuality  , OptionsModel::I2PInboundBackupQuantity);
    mapper.addMapping(ui->checkBoxInboundAllowZeroHop  , OptionsModel::I2PInboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxInboundIPRestriction  , OptionsModel::I2PInboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundQuantity      , OptionsModel::I2POutboundQuantity);
    mapper.addMapping(ui->spinBoxOutboundLength        , OptionsModel::I2POutboundLength);
    mapper.addMapping(ui->spinBoxOutboundLengthVariance, OptionsModel::I2POutboundLengthVariance);
    mapper.addMapping(ui->spinBoxOutboundBackupQuantity, OptionsModel::I2POutboundBackupQuantity);
    mapper.addMapping(ui->checkBoxAllowZeroHop         , OptionsModel::I2POutboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxOutboundIPRestriction , OptionsModel::I2POutboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundPriority      , OptionsModel::I2POutboundIPRestriction);
}

void I2POptionsWidget::setModel(ClientModel* model)
{
    clientModel = model;
}

void I2POptionsWidget::ShowCurrentI2PAddress()
{
    if (clientModel)
    {
        const QString pub = clientModel->getPublicI2PKey();
        const QString priv = clientModel->getPrivateI2PKey();
        const QString b32 = clientModel->getB32Address(pub);
        const QString configFile = QString::fromStdString(GetConfigFile().string());

        ShowI2PAddresses i2pCurrDialog("Your current I2P-address", pub, priv, b32, configFile, this);
        i2pCurrDialog.exec();
    }
}

void I2POptionsWidget::GenerateNewI2PAddress()
{
    if (clientModel)
    {
        QString pub, priv;
        clientModel->generateI2PDestination(pub, priv);
        const QString b32 = clientModel->getB32Address(pub);
        const QString configFile = QString::fromStdString(GetConfigFile().string());

        ShowI2PAddresses i2pCurrDialog("Generated I2P address", pub, priv, b32, configFile, this);
        i2pCurrDialog.exec();
    }
}

