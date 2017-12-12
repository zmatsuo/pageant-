#undef UNICODE
//#define STRICT

#include <assert.h>

#include <windows.h>

#pragma warning(push)
#pragma warning(disable:4127)
#include <QDir>
#include <QStandardPaths>
#include <QClipboard>
#include <QStandardItemModel>
#pragma warning(pop)

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
#include "ckey.h"

#include "keyviewdlg.h"
#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "ui_keyviewdlg.h"
#pragma warning(pop)

keyviewdlg::keyviewdlg(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::keyviewdlg)
{
	ui->setupUi(this);
	if (!setting_get_bool("bt/enable", false)) {
		ui->pushButton_4->setVisible(false);
	}

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

	model = new QStandardItemModel((int)k.size(), 5, this);
	int n = 0;
//	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("key"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("algorithm"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("size(bit)"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("MD5/hex fingerpinrt"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("SHA-256/base64 fingerprint"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("comment"));
	int colum = 0;
	for (const auto a : k) {
		QStandardItem *item;
		n = 0;
		// item = new QStandardItem(QString::fromStdString(k[i].name));
		// model->setItem(i, 0, item);
		item = new QStandardItem(QString::fromStdString(a.algorithm));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(std::to_string(a.size)));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.md5));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.sha256));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.comment));
		model->setItem(colum, n++, item);
		colum++;
	}

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

void keyviewdlg::on_pushButton_4_clicked()
{
	addBtCert();
}

// pubkey to clipboard
void keyviewdlg::on_pushButton_3_clicked()
{
	dbgprintf("pubkey to clipboard\n");

	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(L"鍵が選択されていません", L"pageant+", MB_OK);
		break;
	case 1:
	{
		const QModelIndex &index = selection->currentIndex();
		const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
//		QStandardItem *item = model->itemFromIndex(index);
		const QStandardItem *item = model->item(index.row(), 0);
		if (item == nullptr)
			return;
		QString qs = item->text();

		std::string s = qs.toStdString();
#if 0
		std::string::size_type p = s.find_last_of(' ');
		s = s.substr(0, p);
#endif
		std::string::size_type p = s.find_first_of(' ');
		if (p != std::string::npos) {
			p++;
			p = s.find_first_of(' ', p);
			if (p != std::string::npos) {
				p++;
				p = s.find_first_of(' ', p);
			}
			s = s.substr(0, p);
		}

		char *pubkey = pageant_get_pubkey(s.c_str());
		if (pubkey != nullptr) {
			QApplication::clipboard()->setText(QString(pubkey));
			free(pubkey);
			message_box(L"OpenSSH形式の公開鍵をクリップボードにコピーしました", L"pageant+", MB_OK);
		} else {
			message_box(L"公開鍵の取得に失敗しました", L"pageant+", MB_OK);
		}

		break;
	}
	default:
		message_box(L"1つだけ選択してください", L"pageant+", MB_OK);
		break;
	}
}

// remove key
void keyviewdlg::on_pushButtonRemoveKey_clicked()
{
	dbgprintf("remove\n");

	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	const QModelIndexList &indexes = selection->selectedRows();
	const int count = indexes.count();
	if (count == 0) {
		message_box(L"鍵が選択されていません", L"pageant+", MB_OK);
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
	dbgprintf("on_help_triggerd()\n");
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
