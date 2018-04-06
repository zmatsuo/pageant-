#include <windows.h>
#include <sstream>
#include <iomanip>

#pragma warning(push)
//#pragma warning(disable:4127)
#include <QFile>
#include <QTextStream>
#pragma warning(pop)

#include "pageant+.h"
#include "aboutdlg.h"
#include "ui_aboutdlg.h"
#include "version.h"
#include "misc.h"
#include "puttymem.h"

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
	versionDetail();
}

AboutDlg::~AboutDlg()
{
    delete ui;
}

void AboutDlg::on_buttonBox_helpRequested()
{
	// not implimented
}

void AboutDlg::versionDetail()
{
	std::ostringstream ss;

	{
		char *buildinfo_text = buildinfo("\n");
		ss << buildinfo_text;
		sfree(buildinfo_text);
	}
	ss << "\n\n";

	ss << "Qt version: " << QT_VERSION_STR << "\n";
	ss << "Qt dll version: " << qVersion() << "\n";

	ss << "WINVER: 0x"
	   << std::hex << std::setw(4) << std::setfill('0')
	   << WINVER << "\n";
	ss << "_WIN32_WINNT: 0x" << std::setw(4) << _WIN32_WINNT << "\n";
	ss << "_WIN32_IE: 0x" << std::setw(4) << _WIN32_IE;

	QString text = QString::fromStdString(ss.str());
	ui->plainTextEdit->insertPlainText(text);
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
