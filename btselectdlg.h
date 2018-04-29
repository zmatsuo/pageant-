#pragma once

#include "bt_agent_proxy.h"

#pragma warning(push)
#pragma warning(disable:4151)
#include <QDialog>
#pragma warning(pop)

namespace Ui {
class BtSelectDlg;
}

class BtSelectDlg : public QDialog ,bta_deviceinfo_listener
{
    Q_OBJECT

public:
	static BtSelectDlg *createInstance(QWidget *parent = 0);
    ~BtSelectDlg();
private:
    explicit BtSelectDlg(QWidget *parent = 0);

private slots:
    void on_buttonBox_accepted();
    void on_pushButton_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_treeView_clicked(const QModelIndex &index);
    void showDeviceList();

signals:
    void updateSignal();

private:
    Ui::BtSelectDlg *ui;
    static BtSelectDlg *instance;

    std::vector<DeviceInfoType> deviceInfos_;
    void showKeys();
    void update(const std::vector<DeviceInfoType> &deivceInfos);
};

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
