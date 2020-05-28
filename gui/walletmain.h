#ifndef WALLETMAIN_H
#define WALLETMAIN_H

#include <QMainWindow>
#include "walletmanager.h"

namespace Ui {
class WalletMain;
}

class WalletMain : public QMainWindow
{
    Q_OBJECT

public:
    explicit WalletMain(QWidget *parent = 0);
    ~WalletMain(); 


public slots:
    void walletChange();
    void generalbuttonClicked();
    void saveloadButtonClicked();
    void recoverFromMnemonic();

private:
    void createWallet();
    Ui::WalletMain *ui;
    WalletManager mManager;
};

#endif // WALLETMAIN_H
