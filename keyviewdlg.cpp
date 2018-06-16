/**
   keyviewdlg.cpp

   Copyright (c) 2018 zmatsuo

   This software is released under the MIT License.
   http://opensource.org/licenses/mit-license.php
*/
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
#include "debug.h"
#include "winhelp_.h"
#include "misc.h"
#include "misc_cpp.h"
#include "winmisc.h"
#include "setting.h"
#include "gui_stuff.h"
#include "pageant.h"
#include "ckey.h"
#include "codeconvert.h"
#include "keystore.h"
#include "winutils_qt.h"

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

keyviewdlg *keyviewdlg::instance = nullptr;

keyviewdlg::~keyviewdlg()
{
	gWin = nullptr;

	keystoreUnRegistListener(this);
	delete ui;
	instance = nullptr;
}

keyviewdlg *keyviewdlg::createInstance(QWidget *parent)
{
	if (instance != nullptr) {
		return instance;
	}
	instance = new keyviewdlg(parent);
	return instance;
}

#define FINGERPRINT_SHA256_COLUMN	3

void keyviewdlg::change()
{
	dbgprintf("change() enter\n");
	dbgprintf("KeystoreListener change!!\n");
	emit keylistUpdateSignal();
	dbgprintf("change() leave\n");
}

void keyviewdlg::keylistUpdate()
{
	dbgprintf("keylistUpdate() enter\n");
	std::vector<ckey> keys = keystore_get_keys();
	QStandardItemModel *model;

	if (keys.size() == 0) {
		model = new QStandardItemModel(1, 1, this);
		QStandardItem *item = new QStandardItem(QString(u8"no key"));
		model->setItem(0, 0, item);
		ui->treeView->setHeaderHidden(true);
		ui->treeView->setModel(model);
		return;
	}
	model = new QStandardItemModel((int)keys.size(), 7, this);
	int n = 0;
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("algorithm"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("size(bit)"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("MD5/hex fingerpinrt"));
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("SHA-256/base64 fingerprint"));
	assert(n-1 == FINGERPRINT_SHA256_COLUMN);
	model->setHeaderData(n++, Qt::Horizontal, QObject::tr("comment"));
	model->setHeaderData(n++, Qt::Horizontal, tr("expiration(sec)"));
	model->setHeaderData(n++, Qt::Horizontal, tr("confirmation"));
	int colum = 0;
	time_t now = time(NULL);
	for (const auto &key : keys) {
		QStandardItem *item;
		n = 0;
		item = new QStandardItem(QString::fromStdString(key.alg_name()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(std::to_string(key.bits())));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(key.fingerprint_md5()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(key.fingerprint_sha256()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(key.key_comment()));
		model->setItem(colum, n++, item);
		time_t expiration_time = key.expiration_time();
		std::string expiration_str = expiration_time == 0 ? "---" : std::to_string(expiration_time - now);
		item = new QStandardItem(QString::fromStdString(expiration_str));
		model->setItem(colum, n++, item);
		std::string confirmation =
			key.get_confirmation_required() ? "required" : "---";
		item = new QStandardItem(QString::fromStdString(confirmation));
		model->setItem(colum, n++, item);
		colum++;
	}

	ui->treeView->setHeaderHidden(false);
	ui->treeView->setModel(model);
	dbgprintf("keylistUpdate() leave\n");
}

//////////////////////////////////////////////////////////////////////////////

void keyviewdlg::on_pushButtonAddKey_clicked()
{
	addFileCert();
}

// CAPI Cert
void keyviewdlg::on_pushButton_clicked()
{
	addCAPICert();
}

// PKCS Cert
void keyviewdlg::on_pushButton_2_clicked()
{
	addPKCSCert();
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
		message_box(this, L"鍵が選択されていません", L"pageant+", MB_OK);
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
			ckey key;
			bool r = keystore_get(fp_sha256.c_str(), key);
			if (r) {
				to_clipboard_str += key.get_pubkey();
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
		std::wstring msg_w = utf8_to_wc(msg);
		message_box(this, msg_w.c_str(), L"pageant+", MB_OK);
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
		message_box(this, L"鍵が選択されていません", L"pageant+", MB_OK);
	} else {
		const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
		std::vector<std::string> selected;
		for (int i = 0; i < count; i++) {
			const QStandardItem *item = model->item(indexes[i].row(), FINGERPRINT_SHA256_COLUMN);
			selected.push_back(item->text().toStdString());
		}

		for (const auto &fingerprint : selected) {
			keystore_remove(fingerprint.c_str());
		}
	}
//	keylistUpdate();
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
