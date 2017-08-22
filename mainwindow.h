#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QStandardItemModel>
#include <QAbstractNativeEventFilter>

#include "gui_stuff.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:
    int signal_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info);
    int signal_passphraseDlg(struct PassphraseDlgInfo *info);

public slots:
    int slot_confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info);
    int slot_passphraseDlg(struct PassphraseDlgInfo *info);
    void trayClicked(QSystemTrayIcon::ActivationReason e);

private slots:
    void on_pushButton_close_clicked();
    void on_pushButtonRemoveKey_clicked();
    void on_pushButtonAddKey_clicked();
    void on_actionAboutDlg();
    void on_actionabout_triggered();
    void on_session(QAction *action);
    void on_actionquit_triggered();
    void on_actionhelp_triggered();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_actionsetting_triggered();
    void on_actionsetting2_triggered();
    void on_pushButton_3_clicked();
	
private:
    void createTrayIcon();
	void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void createTrayIconMenu();

    Ui::MainWindow *ui;

    QSystemTrayIcon *trayIcon;
    bool quitGurad_;
    std::vector<QAction *> sessionActionAry;

public:
    int passphraseDlg(struct PassphraseDlgInfo *info);
    int confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info);
    void keylist_update();
    void add_keyfile(const QString &filename);

private:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);
};

#endif // MAINWINDOW_H

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
