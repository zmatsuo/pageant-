// https://stackoverflow.com/questions/14356121/how-to-call-function-after-window-is-shown

#include <sstream>
#include <assert.h>

#include <QStandardItemModel>
#include <Qtimer>

#include "rdpkeydlg.h"
#include "ui_rdpkeydlg.h"

#include "rdp_ssh_relay.h"
#include "pageant.h"
#include "ckey.h"
#include "codeconvert.h"
#include "gui_stuff.h"
#include "setting.h"
#include "keystore.h"
//#define ENABLE_DEBUG_PRINT
#include "debug.h"
#include "winutils_qt.h"


// 鍵取得
static bool rdp_agent_proxy_main_get_key(
	const char *rdp_client,
	std::vector<ckey> &keys)
{
	int err = pageant_get_keylist(rdpSshRelaySendReceive, keys);
	if (err != 0) {
		return false;
	}
	
	// ファイル名をセット TODO:ここでやる?
	for(ckey &key : keys) {
		std::ostringstream oss;
		oss << "rdp://" << rdp_client << "/" << key.fingerprint_sha1();
		key.set_fname(oss.str().c_str());
	}

	ckey::dump_keys(keys);

	return true;
}

//////////////////////////////////////////////////////////////////////////////

RdpKeyDlg *RdpKeyDlg::instance = nullptr;

RdpKeyDlg::RdpKeyDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RdpKeyDlg)
{
    ui->setupUi(this);
}

RdpKeyDlg::~RdpKeyDlg()
{
    delete ui;
	instance = nullptr;
}

RdpKeyDlg *RdpKeyDlg::createInstance(QWidget *parent)
{
	if (instance != nullptr) {
		return instance;
	}
	instance = new RdpKeyDlg(parent);
	return instance;
}

static std::vector<ckey> keys_;

// refresh
void RdpKeyDlg::on_pushButton_clicked()
{
	std::vector<ckey> keys;
	std::wstring client = rdpSshRelayGetClientName();
	rdp_agent_proxy_main_get_key(wc_to_utf8(client).c_str(), keys);
	keys_ = keys;
	showKeys();
}

// import
void RdpKeyDlg::on_pushButton_2_clicked()
{
	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	if (selection == nullptr) {
		message_box(this, L"キー一覧を取得してください", L"pageant+", MB_OK);
		return;
	}
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(this, L"キーが選択されていません", L"pageant+", MB_OK);
		return;
	case 1:
        break;
	default:
		message_box(this, L"キーを1つだけ選択してください", L"pageant+", MB_OK);
		return;
	}

    const QModelIndex index = selection->currentIndex();
	auto key = keys_[index.row()];
	std::wstring key_fingerprint = 
		utf8_to_wc(key.fingerprint_sha256()) + L"\n" +
		utf8_to_wc(key.key_comment());

	if (!keystore_add(key)) {
		std::wstring msg = 
			L"追加失敗\n" +
			key_fingerprint;
		message_box(this, msg.c_str(), L"pageant+", MB_OK);
		return;
	}

	std::wstring msg = 
		L"追加しました\n" +
		key_fingerprint;
	message_box(this, msg.c_str(), L"pageant+", MB_OK);
}

#define FINGERPRINT_SHA256_COLUMN 3

void RdpKeyDlg::showKeys()
{
	std::vector<ckey> &k = keys_;

	QStandardItemModel *model = new QStandardItemModel((int)k.size(), 5, this);
	int n = 0;
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
		item = new QStandardItem(QString::fromStdString(a.alg_name()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(std::to_string(a.bits())));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.fingerprint_md5()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.fingerprint_sha256()));
		model->setItem(colum, n++, item);
		item = new QStandardItem(QString::fromStdString(a.key_comment()));
		model->setItem(colum, n++, item);
//		item = new QStandardItem(QString::fromStdString(a.name));
//		model->setItem(colum, n++, item);
		colum++;
	}
	ui->treeView->setHeaderHidden(false);
	ui->treeView->setModel(model);
}

void RdpKeyDlg::showEvent(QShowEvent* event)
{
	QWidget::showEvent( event );
	QTimer::singleShot(100, this, SLOT(on_pushButton_clicked()));
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
