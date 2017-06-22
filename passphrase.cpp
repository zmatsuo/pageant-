#include "passphrase.h"

passphrase::passphrase(QWidget *parent, PassphraseDlgInfo *ptr)
	: QDialog(parent)
{
    ui.setupUi(this);

    ui.lineEdit->setEchoMode(QLineEdit::Password);
    ui.lineEdit->setInputMethodHints(Qt::ImhHiddenText|Qt::ImhNoPredictiveText|Qt::ImhNoAutoUppercase);

    infoPtr_ = ptr;
}

passphrase::~passphrase()
{
}

void passphrase::accept()
{
	QString s = ui.lineEdit->text();
	bool c = ui.checkBox->isChecked();
	if (infoPtr_ != NULL) {
        std::string ss = s.toStdString();
        const char *s_const = ss.c_str();
        *infoPtr_->passphrase = _strdup(s_const);
	}
    infoPtr_->save = c == true ? 1 : 0;
    QDialog::accept();
}

void passphrase::reject()
{
	*infoPtr_->passphrase = NULL;
	QDialog::reject();
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
