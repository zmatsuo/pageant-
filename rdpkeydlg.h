// rdpkeydlg.h

#ifndef RDPKEYDLG_H
#define RDPKEYDLG_H

#include <QDialog>

namespace Ui {
class RdpKeyDlg;
}

class RdpKeyDlg : public QDialog
{
    Q_OBJECT

public:
    explicit RdpKeyDlg(QWidget *parent = 0);
    ~RdpKeyDlg();

private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();

private:
    void showKeys();

    Ui::RdpKeyDlg *ui;
};

#endif // RDPKEYDLG_H
