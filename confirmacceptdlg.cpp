#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "confirmacceptdlg.h"
#include "ui_confirmacceptdlg.h"
#pragma warning(pop)

#include "debug.h"

ConfirmAcceptDlg::ConfirmAcceptDlg(
        QWidget *parent,
        struct ConfirmAcceptDlgInfo *info) :
    QDialog(parent),
    ui(new Ui::ConfirmAcceptDlg)
{
    ui->setupUi(this);
    info_ = info;
    passSec_ = 0;

    timerId_ = startTimer(1000);	// 1sec

    ui->checkBox->setChecked(info_->dont_ask_again == 0 ? false : true);
	if (info_->dont_ask_again_available == 0) {
		ui->checkBox->setEnabled(false);
	}
    QString s = info->title;
    ui->label->setText(s);
    s = info->fingerprint;
    ui->label_2->setText(s);

	dispInfo();
}

ConfirmAcceptDlg::~ConfirmAcceptDlg()
{
    delete ui;
}

void ConfirmAcceptDlg::dispInfo()
{
    QString s;
    if (info_->timeout == 0) {
		s = QString("no timeout (%1)").arg(passSec_);
    } else {
		int leftTime = info_->timeout - passSec_;
		if (leftTime <= 0) {
			s = "timeout fire";
			reject();
		} else {
			s = QString("timeout left %1 sec").arg(leftTime);
		}
    }
    ui->label_3->setText(s);
}

void ConfirmAcceptDlg::timerEvent(QTimerEvent *e)
{
    (void)e;
    passSec_++;
	dispInfo();
}

void ConfirmAcceptDlg::on_buttonBox_accepted()
{
    dbgprintf("ok\n");
    info_->dont_ask_again = ui->checkBox->isChecked() ? 1 : 0;
}

void ConfirmAcceptDlg::on_buttonBox_rejected()
{
    dbgprintf("cancel\n");
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
