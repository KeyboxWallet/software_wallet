#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

#include "walletmain.h"
#include "ui_walletmain.h"

extern "C" {
    #include "bip39.h"
}

WalletMain::WalletMain(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WalletMain),
    mManager(parent)
{
    ui->setupUi(this);
    ui->status->setText(tr("NO WALLET"));
    connect(&mManager, SIGNAL(walletChanged()), this, SLOT(walletChange()));
    connect(ui->generalButton, SIGNAL(clicked()), this, SLOT(generalbuttonClicked()));
    connect(ui->saveOrLoadButton, &QPushButton::clicked, this, &WalletMain::saveloadButtonClicked);
    connect(ui->mnemonicRecoveryButton, &QPushButton::clicked, this, &WalletMain::recoverFromMnemonic);
}

WalletMain::~WalletMain()
{
    delete ui;
}

void WalletMain::walletChange()
{
    WalletInstance *inst = mManager.getWalletInstance();
    if( inst != NULL) {
        auto statusText = inst->getName();
        ui->saveOrLoadButton->setText(tr("Save to file"));
        if( inst->isLocked() ){
            statusText.append(tr(",locked"));
            ui->generalButton->setText(tr("unlock"));
        }
        else {
            statusText.append(tr(",unlocked"));
            ui->generalButton->setText(tr("lock"));
        }
        ui->status->setText(statusText);
        ui->mnemonicRecoveryButton->hide();
    }
}

void WalletMain::generalbuttonClicked()
{
    WalletInstance *inst = mManager.getWalletInstance();
    if( !inst ){
        createWallet();
        return;
    }
    if( inst->isLocked()){
        bool ok;
        QString text = QInputDialog::getText(this, tr("Please input password"),
                                                 tr("input password to lock wallet"), QLineEdit::Password,
                                                 "", &ok);
        if( !ok){
            return;
        }
        if( inst->unlock(text) ){
            walletChange();
        }
        else{
            ui->status->setText(tr("Password error, unlock failed."));
        }
    }
    else{
        inst->lock();
         walletChange();
    }

}

void WalletMain::saveloadButtonClicked()
{
    //QFileDialog dlg;
    WalletInstance *inst = mManager.getWalletInstance();
    QString filename;
    if( inst ){ // save
        filename = QFileDialog::getSaveFileName(this, tr("select file"));
        if( ! filename.isNull()) {
            inst->saveToFile(filename);
        }
    }
    else {
        filename = QFileDialog::getOpenFileName(this, tr("select file"));
        qDebug() << filename;
        mManager.loadWallet(filename);
        //inst = WalletInstance::loadFromFile(filename);
        //walletChange();
    }
}

void WalletMain::createWallet()
{
    QString mnemonic = WalletInstance::generateMnemonic();
    QString password; //  = "abcdefg";

    // UI
    // step 1: save mnemonic
    QMessageBox mbox;
    mbox.setText(tr("Step 1,") + tr("please save your mnemonic words"));
    QString information = tr("mnemonic words are ultimate secret, please write down and keep it safe.")+ "\n\n";
    mbox.setInformativeText(information);
    mbox.setDetailedText(mnemonic);

    mbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    mbox.setDefaultButton(QMessageBox::Ok);
    int r = mbox.exec();

    if( r != QMessageBox::Ok ) {
        return;
    }
    // step 2: test user has remembered mnemonic
    QInputDialog dlg;
    // mbox.exec();
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, tr("Step 2,") + tr("make sure you remembered mnemonic words"),
                                             tr("Please input mnemonic words here, can split by new line between any words"),
                                             "", &ok);
    if (ok) {
        if( text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") != mnemonic) {
            ui->status->setText(tr("invalid mnemonic words"));
            return;
        }
    }
    else {
        return;
    }

    // step 3, name
    password = QInputDialog::getText(this, tr("Step 3,") + tr("please set password"),
                                         tr("password is used to unlock the wallet, make sure to use a strong one"),
                                         QLineEdit::Password,
                                         "", &ok);
    if( !ok ) {
        return;
    }


    mManager.createWallet(mnemonic, password, tr("new wallet"));
}


void WalletMain::recoverFromMnemonic()
{
    QString mnemonic;
    // step 2: test user has remembered mnemonic
    QInputDialog dlg;
    // mbox.exec();
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, tr("Step 1,") + tr("make sure you remembered mnemonic words"),
                                             tr("Please input mnemonic words here, can split by new line between any words"),
                                             "", &ok);
    if (ok) {
        mnemonic = text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") ;
        //if( text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") != mnemonic) {
        //    ui->status->setText("助记词输入错误");
        //    return;
        // }
        if( !mnemonic_check(mnemonic.toUtf8().constData())) {
           // ui->status->setText("非法助记词");
            ui->status->setText(tr("invalid mnemonic words"));
            return;
        }
    }
    else {
        return;
    }

    // step 3, name
    QString password = QInputDialog::getText(this, tr("Step 2,") + tr("please set password"),
                                         tr("password is used to unlock the wallet, make sure to use a strong one"),
                                         QLineEdit::Password,
                                         "", &ok);
    if( !ok ) {
        return;
    }


    mManager.createWallet(mnemonic, password, tr("Recover wallet"));
}
