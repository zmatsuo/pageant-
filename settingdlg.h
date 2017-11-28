#ifndef SETTINGDLG_H
#define SETTINGDLG_H

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QDialog>
#pragma warning(pop)

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
    void on_buttonBox_accepted();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_6_clicked();
    void on_pushButton_7_clicked();
    void on_pushButton_8_clicked();
    void on_pushButton_9_clicked();
    void on_pushButton_10_clicked();
    void on_pushButton_11_clicked();
    void on_checkBox_7_clicked();
    void on_pushButton_12_clicked();
    void on_pushButton_13_clicked();
    void on_pushButton_14_clicked();


    void on_checkBox_5_clicked();

private:
    Ui::SettingDlg *ui;

    void dispSockPath(const std::string &path);
    bool setStartup(int action);
    void dispSetting();
    void arrengePassphraseUI(bool enable);

    void showTab(bool show);
    typedef struct {
	QWidget *tab;
	QString label;
    } invisiableTabsInfo_t;
    std::vector<invisiableTabsInfo_t> invisibleTabs_;
};

#endif // SETTINGDLG_H
