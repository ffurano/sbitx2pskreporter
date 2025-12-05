#include "mainwindow.h"

#include <QApplication>
#include "config.h"


int main(int argc, char *argv[])
{

    // Load the config file
    CFG->ProcessFile((char *)"/etc/sbitx2pskreporter.conf");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
