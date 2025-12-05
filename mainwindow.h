#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include "pskrworker.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


    pskrworker wrkthread;
public slots:
    void log(QString s);

private slots:

    void on_pushButton_defaults_clicked();

    void on_pushButton_about_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
