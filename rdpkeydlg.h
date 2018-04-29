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
	static RdpKeyDlg *createInstance(QWidget *parent = 0);
    ~RdpKeyDlg();
private:
    explicit RdpKeyDlg(QWidget *parent = 0);

private slots:
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();

private:
    void showKeys();
    void showEvent(QShowEvent* event);

    Ui::RdpKeyDlg *ui;
	static RdpKeyDlg *instance;
};

#endif // RDPKEYDLG_H

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
