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
    ui->status->setText("无钱包");
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
        ui->saveOrLoadButton->setText("保存到文件");
        if( inst->isLocked() ){
            statusText.append(",已经锁定");
            ui->generalButton->setText("解锁");
        }
        else {
            statusText.append(",已经解锁");
            ui->generalButton->setText("锁定");
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
        QString text = QInputDialog::getText(this, tr("请输入密码"),
                                                 tr("输入密码以解锁钱包"), QLineEdit::Password,
                                                 "", &ok);
        if( !ok){
            return;
        }
        if( inst->unlock(text) ){
            walletChange();
        }
        else{
            ui->status->setText("密码错误，解锁失败");
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
        filename = QFileDialog::getSaveFileName(this, "选择文件");
        if( ! filename.isNull()) {
            inst->saveToFile(filename);
        }
    }
    else {
        filename = QFileDialog::getOpenFileName(this, "选择文件");
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
    mbox.setText("第一步：请记录助记词");
    QString information = "助记词是终极秘密，请用纸笔记录，保存到安全位置。\n\n";
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
    QString text = QInputDialog::getMultiLineText(this, "第二步，确保您记住了助记词",
                                             "请输入助记词,可任意分行",
                                             "", &ok);
    if (ok) {
        if( text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") != mnemonic) {
            ui->status->setText("助记词输入错误");
            return;
        }
    }
    else {
        return;
    }

    // step 3, name
    password = QInputDialog::getText(this, "第三步，请设定密码",
                                         "请输入密码用于解锁钱包，请使用高强度密码",
                                         QLineEdit::Password,
                                         "", &ok);
    if( !ok ) {
        return;
    }


    mManager.createWallet(mnemonic, password, "新钱包");
}


void WalletMain::recoverFromMnemonic()
{
    QString mnemonic;
    // step 2: test user has remembered mnemonic
    QInputDialog dlg;
    // mbox.exec();
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, "第一步，确保您记住了助记词",
                                             "请输入助记词,可任意分行",
                                             "", &ok);
    if (ok) {
        mnemonic = text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") ;
        //if( text.split(QRegExp("\\s+"), QString::SkipEmptyParts).join(" ") != mnemonic) {
        //    ui->status->setText("助记词输入错误");
        //    return;
        // }
        if( !mnemonic_check(mnemonic.toUtf8().constData())) {
            ui->status->setText("非法助记词");
            return;
        }
    }
    else {
        return;
    }

    // step 3, name
    QString password = QInputDialog::getText(this, "第二步，请设定密码",
                                         "请输入密码用于解锁钱包，请使用高强度密码",
                                         QLineEdit::Password,
                                         "", &ok);
    if( !ok ) {
        return;
    }


    mManager.createWallet(mnemonic, password, "新钱包");
}
