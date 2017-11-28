#pragma once

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include <QDialog>
#pragma warning(pop)

#include "gui_stuff.h"

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "ui_passphrasedlg.h"
#pragma warning(pop)

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
