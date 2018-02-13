#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <vector>

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QAbstractNativeEventFilter>
#pragma warning(pop)

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
    void on_pushButtonAddKey_clicked();
    void on_pushButtonAddBTKey_clicked();

private slots:
    void on_pushButton_close_clicked();
    void on_actionAboutDlg();
    void on_actionabout_triggered();
    void on_session(QAction *action);
    void on_actionhelp_triggered();
    void on_actionsetting_triggered();
	void on_viewKeys();
	
private:
    void createTrayIcon();
    void trayIconMenu();
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);
	void lockTerminal();

    Ui::MainWindow *ui;

    QSystemTrayIcon *trayIcon;
    std::vector<QAction *> sessionActionAry;

public:
    void on_pushButton_clicked();		// add CAPI
    void on_pushButton_2_clicked();		// add PKCS

    int passphraseDlg(struct PassphraseDlgInfo *info);
    int confirmAcceptDlg(struct ConfirmAcceptDlgInfo *info);
};

#endif // MAINWINDOW_H

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
