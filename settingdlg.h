#ifndef SETTINGDLG_H
#define SETTINGDLG_H

#include <QDialog>

namespace Ui {
class SettingDlg;
}

class SettingDlg : public QDialog
{
    Q_OBJECT

public:
    explicit SettingDlg(QWidget *parent = 0);
    ~SettingDlg();

private slots:
    void on_lineEdit_textChanged(const QString &arg1);
    void on_checkBox_clicked(bool checked);
    void on_buttonBox_accepted();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_7_clicked();
    void on_checkBox_4_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_pushButton_10_clicked();

    void on_pushButton_11_clicked();

private:
    Ui::SettingDlg *ui;

    void dispSockPath(const std::string &path);
    void setStartup(int action);
    void dispSetting();
};

#endif // SETTINGDLG_H
