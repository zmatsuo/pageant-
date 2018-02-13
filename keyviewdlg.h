#pragma once

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QDialog>
#pragma warning(pop)

#include "keystore.h"

namespace Ui {
class keyviewdlg;
}

class keyviewdlg : public QDialog, KeystoreListener
{
	Q_OBJECT

public:
	explicit keyviewdlg(QWidget *parent = 0);
	~keyviewdlg();

private slots:
	void keylistUpdate();
    void on_buttonBox_accepted();
	void on_pushButtonRemoveKey_clicked();
	void on_pushButtonAddKey_clicked();
	void on_actionhelp_triggered();
	void on_pushButton_clicked();
	void on_pushButton_2_clicked();
	void on_pushButton_3_clicked();
	void on_pushButton_4_clicked();

signals:
	void keylistUpdateSignal();

private:
	Ui::keyviewdlg *ui;
	void change();
};

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
