#include <sstream>
#include <assert.h>

#include <QStandardItemModel>

#include "rdpkeydlg.h"
#include "ui_rdpkeydlg.h"

#define ENABLE_DEBUG_PRINT
#include "rdp_ssh_relay.h"
#include "pageant.h"
#include "ckey.h"
#include "codeconvert.h"
#include "gui_stuff.h"
#include "setting.h"
#include "keystore.h"


//////////////////////////////////////////////////////////////////////////////
static void rdp_agent_query_synchronous_fn(void *in, size_t inlen, void **out, size_t *outlen)
{
    std::vector<uint8_t> send((uint8_t *)in, ((uint8_t *)in) + inlen);
	std::vector<uint8_t> receive;
	bool r = rdpSshRelaySendReceive(send, receive);
	if (r == false || receive.size() == 0) {
#define SSH_AGENT_FAILURE                    5		// TODO ヘッダへ
		uint8_t *p = (uint8_t *)malloc(5);
		p[0] = 0x00;
		p[1] = 0x00;
		p[2] = 0x00;
		p[3] = 0x01;
		p[4] = SSH_AGENT_FAILURE;
		*out = p;
		*outlen = 5;
		return;
	}
	*outlen = receive.size();
	void *p = malloc(receive.size());
	memcpy(p, &receive[0], receive.size());
	*out = p;
	return;
}

// 鍵取得
bool rdp_agent_proxy_main_get_key(
	const char *rdp_client,
	std::vector<ckey> &keys)
{
	keys.clear();

	// rdp問い合わせする
	pagent_set_destination(rdp_agent_query_synchronous_fn);
	int length;
	void *p = pageant_get_keylist2(&length);
	pagent_set_destination(nullptr);
	if (p == nullptr) {
		// 応答なし
		dbgprintf("rdp 応答なし\n");
		return false;
	}
	std::vector<uint8_t> from_bt((uint8_t *)p, ((uint8_t *)p) + length);
	sfree(p);
	p = &from_bt[0];


	debug_memdump(p, length, 1);

	// 鍵を抽出
	const char *fail_reason = nullptr;
	bool r = parse_public_keys(p, length, keys, &fail_reason);
	if (r == false) {
		dbgprintf("err %s\n", fail_reason);
		return false;
	}

	// ファイル名をセット TODO:ここでやる?
	for(ckey &key : keys) {
		std::ostringstream oss;
		oss << "rdp://" << rdp_client << "/" << key.fingerprint_sha1();
		key.set_fname(oss.str().c_str());
	}

	for(const ckey &key : keys) {
		dbgprintf("key\n");
		dbgprintf(" '%s'\n", key.fingerprint().c_str());
		dbgprintf(" md5 %s\n", key.fingerprint_md5().c_str());
		dbgprintf(" sha256 %s\n", key.fingerprint_sha256().c_str());
		dbgprintf(" alg %s %d\n", key.alg_name().c_str(), key.bits());
		dbgprintf(" comment %s\n", key.key_comment().c_str());
		dbgprintf(" comment2 %s\n", key.key_comment2().c_str());
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////

RdpKeyDlg::RdpKeyDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RdpKeyDlg)
{
    ui->setupUi(this);
}

RdpKeyDlg::~RdpKeyDlg()
{
    delete ui;
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
		message_box(L"キー一覧を取得してください", L"pageant+", MB_OK);
		return;
	}
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(L"キーが選択されていません", L"pageant+", MB_OK);
		return;
	case 1:
        break;
	default:
		message_box(L"キーを1つだけ選択してください", L"pageant+", MB_OK);
		return;
	}

    const QModelIndex index = selection->currentIndex();
	const auto &key = keys_[index.row()];

	ssh2_userkey *key_st;
	key.get_raw_key(&key_st);
	if (!pageant_add_ssh2_key(key_st)) {
		std::wostringstream oss;
		oss << L"追加失敗\n"
			<< utf8_to_wc(key.fingerprint_sha256()) << "\n"
			<< utf8_to_wc(key.key_comment());
		message_box(oss.str().c_str(), L"pageant+", MB_OK);
		return;
	}
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

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
