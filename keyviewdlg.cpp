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
#include "codeconvert.h"
#include "keystore.h"

#include "keyviewdlg.h"
#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "ui_keyviewdlg.h"
#pragma warning(pop)

static keyviewdlg *gWin;

keyviewdlg::keyviewdlg(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::keyviewdlg)
{
	ui->setupUi(this);
	if (!setting_get_bool("bt/enable", false)) {
		ui->pushButton_4->setVisible(false);
	}

	keylistUpdate();

	keystoreRegistListener(this);

	gWin = this;

	connect(this, &keyviewdlg::keylistUpdateSignal,
			this, &keyviewdlg::keylistUpdate,
			Qt::QueuedConnection);
}

keyviewdlg::~keyviewdlg()
{
	gWin = nullptr;

	keystoreUnRegistListener(this);
	delete ui;
}

#define FINGERPRINT_SHA256_COLUMN	3

void keyviewdlg::change()
{
	debug_printf("change() enter\n");
	debug_printf("KeystoreListener change!!\n");
	emit keylistUpdateSignal();
	debug_printf("change() leave\n");
}

void keyviewdlg::keylistUpdate()
{
	debug_printf("keylistUpdate() enter\n");
	std::vector<KeyListItem> k = keylist_update2();
	QStandardItemModel *model;

	if (k.size() == 0) {
		model = new QStandardItemModel(1, 1, this);
		QStandardItem *item = new QStandardItem(QString(u8"no key"));
		model->setItem(0, 0, item);
		ui->treeView->setHeaderHidden(true);
		ui->treeView->setModel(model);
		return;
	}
	model = new QStandardItemModel((int)k.size(), 5, this);
	int n = 0;
//	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("key"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("algorithm"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("size(bit)"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("MD5/hex fingerpinrt"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("SHA-256/base64 fingerprint"));
	assert(n-1 == FINGERPRINT_SHA256_COLUMN);
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("comment"));
	int colum = 0;
	for (const auto a : k) {
		QStandardItem *item;
		n = 0;
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
//		item = new QStandardItem(QString::fromStdString(a.name));
//		model->setItem(colum, n++, item);
		colum++;
	}

	ui->treeView->setHeaderHidden(false);
	ui->treeView->setModel(model);
	debug_printf("keylistUpdate() leave\n");
}

//////////////////////////////////////////////////////////////////////////////

void keyviewdlg::on_pushButtonAddKey_clicked()
{
	addFileCert();
	keylistUpdate();
}

// CAPI Cert
void keyviewdlg::on_pushButton_clicked()
{
	addCAPICert();
	keylistUpdate();
}

// PKCS Cert
void keyviewdlg::on_pushButton_2_clicked()
{
	addPKCSCert();
	keylistUpdate();
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
	const int count = indexes.count();
	if (count == 0) {
		message_box(L"鍵が選択されていません", L"pageant+", MB_OK);
	} else {
		const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
		std::vector<std::string> selectedArray;
		for (int i = 0; i < count; i++) {
			const QStandardItem *item = model->item(indexes[i].row(), FINGERPRINT_SHA256_COLUMN);
			selectedArray.push_back(item->text().toStdString());
		}

		std::string ok_keys;
		std::string ng_keys;
		std::string to_clipboard_str;
		for (const auto &fp_sha256 : selectedArray) {
			char *pubkey = pageant_get_pubkey(fp_sha256.c_str());
			if (pubkey != nullptr) {
				to_clipboard_str += pubkey;
				free(pubkey);
				ok_keys += fp_sha256;
				ok_keys += "\n";
			} else {
				ng_keys += fp_sha256;
				ng_keys + "\n";
			}
		}
		QApplication::clipboard()->setText(
			QString::fromStdString(to_clipboard_str));
		std::string msg;
		if (!ok_keys.empty()) {
			msg = ok_keys;
			msg += u8"OpenSSH形式の公開鍵をクリップボードにコピーしました";
		}
		if (!ng_keys.empty()) {
			msg += "\n";
			msg += ng_keys;
			msg += u8"公開鍵の取得に失敗しました";
		}
		std::wstring msg_w = mb_to_wc(msg);
		message_box(msg_w.c_str(), L"pageant+", MB_OK);
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
	keylistUpdate();
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
