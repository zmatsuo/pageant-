#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include <QDialog>
#include "ui_aboutdlg.h"

namespace Ui {
class AboutDlg;
}

class AboutDlg : public QDialog
{
 //   Q_OBJECT

public:
    explicit AboutDlg(QWidget *parent = 0);
    ~AboutDlg();
    void helpRequest();

private slots:
    void on_buttonBox_helpRequested();

private:
    Ui::AboutDlg *ui;
};

#endif // ABOUTDLG_H
