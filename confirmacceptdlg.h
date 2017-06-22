#ifndef CONFIRMACCEPTDLG_H
#define CONFIRMACCEPTDLG_H

#include <QDialog>
#include <QTimer>

#include "gui_stuff.h"

namespace Ui {
class ConfirmAcceptDlg;
}

class ConfirmAcceptDlg : public QDialog
{
	Q_OBJECT

public:
	explicit ConfirmAcceptDlg(QWidget *parent = 0, struct ConfirmAcceptDlgInfo *info = 0);
	~ConfirmAcceptDlg();

private slots:
	void on_buttonBox_accepted();
	void on_buttonBox_rejected();

private:
	void dispInfo();
	Ui::ConfirmAcceptDlg *ui;
	struct ConfirmAcceptDlgInfo *info_;
	int timerId_;
	int passSec_;
	
	void timerEvent(QTimerEvent *e);
};

#endif // CONFIRMACCEPTDLG_H

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
