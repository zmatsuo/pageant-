#ifndef BTSELECTDLG_H
#define BTSELECTDLG_H

#include "bt_agent_proxy.h"

#pragma warning(push)
#pragma warning(disable:4151)
#include <QDialog>
#pragma warning(pop)

namespace Ui {
class BtSelectDlg;
}

class BtSelectDlg : public QDialog
{
    Q_OBJECT

public:
    explicit BtSelectDlg(QWidget *parent = 0);
    ~BtSelectDlg();

private slots:
    void on_pushButton_2_clicked();
    void on_pushButton_clicked();
    void on_buttonBox_accepted();

private:
    Ui::BtSelectDlg *ui;

    std::vector<DeviceInfoType> deviceInfos_;
    void update();
};

#endif // BTSELECTDLG_H
