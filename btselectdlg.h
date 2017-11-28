#ifndef BTSELECTDLG_H
#define BTSELECTDLG_H

#include <QDialog>

namespace Ui {
class BtSelectDlg;
}

class BtSelectDlg : public QDialog
{
    Q_OBJECT

public:
    explicit BtSelectDlg(QWidget *parent = 0);
    ~BtSelectDlg();

private:
    Ui::BtSelectDlg *ui;
};

#endif // BTSELECTDLG_H
