#pragma once

#include <QDialog>
#include "gui_stuff.h"
#include "ui_passphrasedlg.h"

class passphrase : public QDialog
{
//	Q_OBJECT

public:
    passphrase(QWidget *parent = Q_NULLPTR, PassphraseDlgInfo *ptr = NULL);
    ~passphrase();

private:
    Ui::Dialog ui;
    PassphraseDlgInfo *infoPtr_;
private slots:
    void accept();
    void reject();
};
