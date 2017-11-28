#include "btselectdlg.h"
#include "ui_btselectdlg.h"

BtSelectDlg::BtSelectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BtSelectDlg)
{
    ui->setupUi(this);
}

BtSelectDlg::~BtSelectDlg()
{
    delete ui;
}
