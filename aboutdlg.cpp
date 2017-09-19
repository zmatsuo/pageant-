#include <QFile>
#include <QTextStream>

#include "pageant+.h"
#include "aboutdlg.h"
#include "ui_aboutdlg.h"
#include "version.h"

static QString getText(const char *file)
{
    QString text;
    QFile f(file);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream in(&f);
		in.setCodec("UTF-8");
		text = in.readAll();
    }
	return text;
}

AboutDlg::AboutDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDlg)
{
    ui->setupUi(this);

    ui->labelTitle->setText(APP_NAME);
    const QString s = QString(ver) + " " + commitid;
    ui->labelVersion->setText(s);

	ui->textEdit_2->setText(getText("://license.txt"));
	ui->textEdit->setText(getText("://license_putty.txt"));
	ui->label_6->setText(getText("://reference.txt"));

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

AboutDlg::~AboutDlg()
{
    delete ui;
}

void AboutDlg::on_buttonBox_helpRequested()
{
	// not implimented
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
