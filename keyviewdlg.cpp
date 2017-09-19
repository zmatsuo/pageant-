#undef UNICODE
//#define STRICT

#include <assert.h>

#include <windows.h>

#include <QDir>
#include <QStandardPaths>
#include <QClipboard>
#include <QStandardItemModel>

#include "pageant+.h"
#include "winpgnt.h"
#include "debug.h"
#include "winhelp_.h"
#include "misc.h"
#include "misc_cpp.h"
#include "winmisc.h"
#include "filename.h"
#include "setting.h"
#include "gui_stuff.h"
#include "pageant.h"
#include "winutils.h"
#include "ssh.h"

#include "keyviewdlg.h"
#include "ui_keyviewdlg.h"

#ifdef PUTTY_CAC
extern "C" {
#include "cert/cert_common.h"
}
#endif

void add_keyfile(const Filename *fn);

keyviewdlg::keyviewdlg(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::keyviewdlg)
{
	ui->setupUi(this);

	keylist_update();
}

keyviewdlg::~keyviewdlg()
{
	delete ui;
}

void keyviewdlg::keylist_update()
{
	std::vector<KeyListItem> k = keylist_update2();
	QStandardItemModel *model;

	const int colum = 1;
	model = new QStandardItemModel((int)k.size(), colum, this);
	for (int i=0; i<(int)k.size(); i++) {
		QStandardItem *item;
#if 0
		item = new QStandardItem(QString("%0").arg(i));
		model->setItem(i, 0, item);
#endif
		item = new QStandardItem(QString::fromStdString(k[i].name));
		model->setItem(i, 0, item);
#if 0
		item = new QStandardItem(QString("comment"));
		model->setItem(i, 2, item);
#endif
	}


#if 0
	model->setHeaderData(0, Qt::Horizontal, QObject::tr("no"));
#endif
	model->setHeaderData(0, Qt::Horizontal, QObject::tr("key"));
#if 0
	model->setHeaderData(2, Qt::Horizontal, QObject::tr("test"));
#endif
	ui->treeView->setModel(model);
}

//////////////////////////////////////////////////////////////////////////////

void keyviewdlg::on_pushButtonAddKey_clicked()
{
	addFileCert();
	keylist_update();
}

// CAPI Cert
void keyviewdlg::on_pushButton_clicked()
{
	addCAPICert();
	keylist_update();
}

// PKCS Cert
void keyviewdlg::on_pushButton_2_clicked()
{
	addPKCSCert();
	keylist_update();
}

// pubkey to clipboard
void keyviewdlg::on_pushButton_3_clicked()
{
	debug_printf("pubkey to clipboard\n");

	const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
	QItemSelectionModel *selection = ui->treeView->selectionModel();
	QModelIndexList indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_boxW(L"鍵が選択されていません", L"pagent+", MB_OK, 0);
		break;
	case 1:
	{
		const QModelIndex index = selection->currentIndex();
		QStandardItem *item = model->itemFromIndex(index);
		if (item == nullptr)
			return;
		QString qs = item->text();

		std::string s = qs.toStdString();
		std::string::size_type p = s.find_last_of(' ');
		s = s.substr(0, p);

		char *pubkey = pageant_get_pubkey(s.c_str());
		QApplication::clipboard()->setText(QString(pubkey));
		free(pubkey);

		message_boxW(L"OpenSSH形式の公開鍵をクリップボードにコピーしました", L"pagent+", MB_OK, 0);

		break;
	}
	default:
		message_boxW(L"1つだけ選択してください", L"pagent+", MB_OK, 0);
		break;
	}
}

// remove key
void keyviewdlg::on_pushButtonRemoveKey_clicked()
{
	debug_printf("remove\n");

	QItemSelectionModel *selection = ui->treeView->selectionModel();
	QModelIndexList indexes = selection->selectedRows();
	const int count = indexes.count();
	if (count == 0) {
		message_boxW(L"鍵が選択されていません", L"pagent+", MB_OK, 0);
	} else {
		std::vector<int> selectedArray;
		for (int i = 0; i < count; i++) {
			selectedArray.push_back(indexes[i].row());
		}
		std::sort(selectedArray.begin(), selectedArray.end());

		pageant_delete_key2((int)selectedArray.size(), &selectedArray[0]);
	}
	keylist_update();
}

//////////////////////////////////////////////////////////////////////////////

void keyviewdlg::on_actionhelp_triggered()
{
	debug_printf("on_help_triggerd()\n");
	launch_help_id(NULL, WINHELP_CTXID_errors_hostkey_absent);
}

void keyviewdlg::on_buttonBox_accepted()
{
	close();
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
