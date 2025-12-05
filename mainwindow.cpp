#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "config.h"


void MainWindow::log(QString s) {
    QDateTime dt = QDateTime::currentDateTime();
    QString currentDateTime = dt.toString("yyyyMMdd-HHmmss");

    Log(Logger::Lvl4, Logger::unregistered, Logger::unregisteredname, currentDateTime.toStdString() << " " << s.toStdString());
    ui->plainTextEdit->appendPlainText(currentDateTime + " " + s);


}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    connect(&wrkthread, &pskrworker::log,
            this, &MainWindow::log);

    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_defaults_clicked()
{
    CFG->ProcessFile((char *)"/etc/sbitx2pskreporter.conf");

    Logger::get()->setLevel((Logger::Level)CFG->GetLong("loglevel"));
    Log(Logger::Lvl1, Logger::unregistered, Logger::unregisteredname, "Restarting. loglevel is " << CFG->GetLong("loglevel"));
    ui->lineEdit_telnethost->setText(CFG->GetString("net.sbitx-telnet-hostport", "localhost:4321").c_str());
    ui->lineEdit_pskrepohost->setText(CFG->GetString("net.pskreporter-hostport", "empty").c_str());

    ui->plainTextEdit->appendPlainText("Starting...");

    QUrl url(QUrl::fromUserInput(ui->lineEdit_telnethost->text()));

    wrkthread.sbitx_host = url.host();
    wrkthread.sbitx_port = url.port();

    wrkthread.start();

    std::string fake( "{201845  35 +00 1678 ~  BI4IWL ON9DJ R-23}" );
    wrkthread.parseOneReport(fake, fake.length()-1, 0);
}

void MainWindow::on_pushButton_about_clicked()
{
    ui->plainTextEdit->appendPlainText("By Fabrizio Furano, 2025");
}

