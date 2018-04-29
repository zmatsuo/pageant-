#pragma once

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QDialog>
#include "ui_aboutdlg.h"
#pragma warning(pop)

namespace Ui {
class AboutDlg;
}

class AboutDlg : public QDialog
{
 //   Q_OBJECT

public:
	static AboutDlg *createInstance(QWidget *parent = 0);
    ~AboutDlg();
    void helpRequest();
private:
    explicit AboutDlg(QWidget *parent = 0);

private slots:
    void on_buttonBox_helpRequested();

private:
    void versionDetail();
    Ui::AboutDlg *ui;
	static AboutDlg *instance;
};

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
