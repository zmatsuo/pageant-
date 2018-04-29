#pragma once

#pragma warning(push)
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
	static SettingDlg *createInstance(QWidget *parent = 0);
    ~SettingDlg();
private:
    explicit SettingDlg(QWidget *parent = 0);

private slots:
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
    void on_pushButton_5_clicked();
    void on_pushButton_15_clicked();
    void on_checkBox_13_clicked();
    void on_checkBoxSshRDPRelayClient_clicked();
    void on_checkBoxSshRDPRelayServer_clicked();

    void on_checkBox_3_clicked();

private:
    Ui::SettingDlg *ui;
	static SettingDlg *instance;

    void arrengePassphraseUI(bool enable);
    void showDetail(bool show);
    typedef struct {
		QWidget *tab;
		QString label;
    } invisiableTabsInfo_t;
    std::vector<invisiableTabsInfo_t> invisibleTabs_;
};

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
