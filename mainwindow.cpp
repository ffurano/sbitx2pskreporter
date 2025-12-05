#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "config.h"


void MainWindow::log(QString s) {
    QDateTime dt = QDateTime::currentDateTime();
    QString currentDateTime = dt.toString("yyyyMMdd-HHmmss");
    ui->plainTextEdit->appendPlainText(currentDateTime + " " + s);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    connect(&wrkthread, &pskrworker::log,
            this, &MainWindow::log);


    ui->setupUi(this);
    setFixedSize(size());
    on_pushButton_defaults_clicked();
}

MainWindow::~MainWindow()
{
    wrkthread.interrupt = true;
    wrkthread.wait();
    delete ui;
}

void MainWindow::on_pushButton_defaults_clicked()
{
    CFG->ProcessFile((char *)"/etc/sbitx2pskreporter.conf");

    Logger::get()->setLevel((Logger::Level)CFG->GetLong("loglevel"));
    mylog(Logger::Lvl1, "Restarting. loglevel is " << CFG->GetLong("loglevel"));

    std::string s = CFG->GetString("sbitx.telnethost", (char*)"localhost") + ':' + CFG->GetString("sbitx.telnetport", (char*)"4321");
    ui->lineEdit_telnethost->setText(s.c_str());
    ui->lineEdit_pskrepohost->setText(CFG->GetString("pskreporter.host", (char*)"empty").c_str());

    ui->plainTextEdit->appendPlainText("Starting...");

    wrkthread.sbitx_host = CFG->GetString("sbitx.telnethost", (char*)"localhost").c_str();
    wrkthread.sbitx_port = CFG->GetLong("sbitx.telnetport", 4321);

    wrkthread.myCall = CFG->GetString("myspot.callsign", (char*)"").c_str();
    wrkthread.myLocator = CFG->GetString("myspot.locator", (char*)"").c_str();
    wrkthread.mySw = CFG->GetString("myspot.sw", (char*)"sBitx").c_str();
    wrkthread.myAntinfo = CFG->GetString("myspot.antinfo", (char*)"").c_str();

    if (!wrkthread.myCall.length() || !wrkthread.myLocator.length() ||
        !wrkthread.mySw.length() || !wrkthread.myAntinfo.length()) {
      ui->plainTextEdit->appendPlainText("Incomplete configuration.");
      return;
    }
    wrkthread.connected = false;
    wrkthread.start();


}

void MainWindow::on_pushButton_about_clicked()
{
    ui->plainTextEdit->appendPlainText("By Fabrizio Furano, 2025");
}

