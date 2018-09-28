#include "walletmain.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    WalletMain w;
    w.show();

    return a.exec();
}
