
#include <thread>
#include <sstream>
#include <assert.h>

#pragma warning(push)
#pragma warning(disable:4127)
#include <QStandardItemModel>
#pragma warning(pop)

//#define ENABLE_DEBUG_PRINT
#include "bt_agent_proxy_main.h"
#include "bt_agent_proxy.h"
#include "gui_stuff.h"
#include "debug.h"
#include "pageant.h"
#include "codeconvert.h"
#include "setting.h"
#include "ckey.h"
#include "keystore.h"
#include "winutils_qt.h"

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "btselectdlg.h"
#include "ui_btselectdlg.h"
#pragma warning(pop)

// TODO:メンバ変数にすると何故かエラーが出るのでとりあえずここに置く
static std::vector<ckey> keys_;
BtSelectDlg *BtSelectDlg::instance = nullptr;

BtSelectDlg::BtSelectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BtSelectDlg)
{
    ui->setupUi(this);

	connect(this, &BtSelectDlg::updateSignal,
			this, &BtSelectDlg::showDeviceList,
			Qt::QueuedConnection);

    bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
    if (hBta == nullptr) {
		return;
	}

	bta_regist_deviceinfo_listener(hBta, this);

    // デバイス一覧を取得
    std::vector<DeviceInfoType> deviceInfos;
    bta_deviceinfo(hBta, deviceInfos);
    deviceInfos_ = deviceInfos;
	showDeviceList();
}

BtSelectDlg::~BtSelectDlg()
{
    bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
    if (hBta != nullptr) {
		bta_unregist_deviceinfo_listener(hBta, this);
    }
    delete ui;
	instance = nullptr;
}

BtSelectDlg *BtSelectDlg::createInstance(QWidget *parent)
{
	if (instance != nullptr) {
		return instance;
	}
	instance = new BtSelectDlg(parent);
	return instance;
}

void BtSelectDlg::showDeviceList()
{
    int n = 0;
    const int colum = 4;
    QStandardItemModel *model =
		new QStandardItemModel((int)deviceInfos_.size(), colum, this);
    for (const auto &deviceInfo : deviceInfos_) {
		QStandardItem *item;
		item = new QStandardItem(QString("%0").arg(n));
		item->setEditable(false);		// editTriggers
		model->setItem(n, 0, item);
		item = new QStandardItem(QString::fromStdWString(deviceInfo.deviceName));
		item->setEditable(false);
		model->setItem(n, 1, item);
		item = new QStandardItem(deviceInfo.connected ? QString("connent") : QString("unconnent(paired)"));
		item->setEditable(false);
		model->setItem(n, 2, item);
		item = new QStandardItem(deviceInfo.handled ? QString("yes") : QString("no"));
		item->setEditable(false);
		model->setItem(n, 3, item);
		n++;
    }

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("no"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("device name"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("connect"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("handled"));
    ui->treeView->setModel(model);

	keys_.clear();
	showKeys();
}

#define FINGERPRINT_SHA256_COLUMN 3

void BtSelectDlg::showKeys()
{
	std::vector<ckey> &k = keys_;

	if (k.size() == 0) {
		QStandardItemModel *model = new QStandardItemModel(1, 1, this);
		const char *s;
		const QItemSelectionModel *selection = ui->treeView->selectionModel();
		int row;
		if (selection == nullptr) {
			row = -1;
		} else {
			const QModelIndex index = selection->currentIndex();
			row = index.row();
		}
		if (row < 0) {
			s = u8"デバイスを選択してください";
		} else {
			if (!deviceInfos_[row].connected) {
				s = u8"接続していません(ペアリングされています)";
			} else {
				s = u8"鍵がありません/アプリが起動していません/未対応デバイス";
			}
		}
		QStandardItem *item = new QStandardItem(QString(s));
		model->setItem(0, 0, item);
		ui->treeView_2->setHeaderHidden(true);
		ui->treeView_2->setModel(model);
		return;
	}
	
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
	ui->treeView_2->setHeaderHidden(false);
	ui->treeView_2->setModel(model);
}

void BtSelectDlg::on_treeView_clicked(const QModelIndex &index)
{
	keys_.clear();
	showKeys();

	const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
	QStandardItem *item = model->itemFromIndex(index);
	if (item == nullptr)
		return;
	QString qs = item->text();

	// 指定デバイスは接続済みか?
	const auto &selectedDeviceInfo = deviceInfos_[index.row()];
	dbgprintf("%ls %s\n",
			  selectedDeviceInfo.deviceName.c_str(),
			  (selectedDeviceInfo.connected ? "connected" : "unconnected"));
	if (!selectedDeviceInfo.connected) {
		return;
	}

	if (!selectedDeviceInfo.handled) {
		return;
	}

	
	// 接続済みなら、キーを取りに行く
	setCursor(Qt::WaitCursor);
	// QApplication::setOverrideCursor(Qt::WaitCursor);
	// QApplication::processEvents();
	std::vector<ckey> &keys = keys_;
    bool r = bt_agent_proxy_main_get_key(
		selectedDeviceInfo.deviceName.c_str(), keys);
	setCursor(Qt::ArrowCursor);
	// QApplication::restoreOverrideCursor();
	// QApplication::processEvents();
	dbgprintf("キー取得 %d(%s)\n", r, r == false ? "失敗" : "成功");
	showKeys();
}

// connect
void BtSelectDlg::on_pushButton_clicked()
{
	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	if (selection == nullptr) {
		message_box(this, L"デバイスが見つかりません", L"pageant+", MB_OK);
		return;
	}
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(this, L"デバイスが選択されていません", L"pageant+", MB_OK);
		break;
	case 1:
	{
		const QModelIndex index = selection->currentIndex();
		const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
		QStandardItem *item = model->itemFromIndex(index);
		if (item == nullptr)
			return;
		QString qs = item->text();

		const auto &deviceInfo = deviceInfos_[index.row()];
		dbgprintf("%ls\n", deviceInfo.deviceName.c_str());
		setCursor(Qt::WaitCursor);
		// QApplication::setOverrideCursor(Qt::WaitCursor);
		// QApplication::processEvents();
		bool r = bt_agent_proxy_main_connect(deviceInfo);
		dbgprintf("bt_agent_proxy_main_connect() %d\n", r);
		setCursor(Qt::ArrowCursor);
		// QApplication::restoreOverrideCursor();
		// QApplication::processEvents();
		if (!r) {
			dbgprintf("connect error\n");
			return;
		}
		break;
	}
	default:
		message_box(this, L"1つだけ選択してください", L"pageant+", MB_OK);
		break;
	}
}

// disconnect
void BtSelectDlg::on_pushButton_4_clicked()
{
	keys_.clear();
	showKeys();

	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	if (selection == nullptr) {
		message_box(this, L"デバイスが見つかりません\nまず検索", L"pageant+", MB_OK);
		return;
	}
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(this, L"デバイスが選択されていません", L"pageant+", MB_OK);
		return;
	case 1:
	{
		const QModelIndex index = selection->currentIndex();
		const QStandardItemModel *model = (QStandardItemModel *)ui->treeView->model();
		QStandardItem *item = model->itemFromIndex(index);
		if (item == nullptr)
			return;
		QString qs = item->text();

		const auto &deviceInfo = deviceInfos_[index.row()];
		dbgprintf("%ls\n", deviceInfo.deviceName.c_str());
		setCursor(Qt::WaitCursor);
		bool r = bt_agent_proxy_main_disconnect(deviceInfo);
		setCursor(Qt::ArrowCursor);
		if (!r) {
			dbgprintf("disconnect error\n");
			return;
		}
		break;
	}
	default:
		message_box(this, L"1つだけ選択してください", L"pageant+", MB_OK);
		break;
	}
}

// 取込
void BtSelectDlg::on_pushButton_5_clicked()
{
	const QItemSelectionModel *selection = ui->treeView_2->selectionModel();
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

	if (!keystore_add(key)) {
//		key = keys_[index.row()];
		std::wostringstream oss;
		oss << L"追加失敗\n"
			<< utf8_to_wc(key.fingerprint_sha256()) << "\n"
			<< utf8_to_wc(key.key_comment());
		message_box(this, oss.str().c_str(), L"pageant+", MB_OK);
		return;
	}

	if (setting_get_bool("key/enable_loading_when_startup", false)) {
		std::wostringstream oss;
		oss << L"次回起動時に読み込みますか?\n"
			<< utf8_to_wc(key.key_comment().c_str());
		int r2 = message_box(this, oss.str().c_str(), L"pageant+", MB_YESNO, 0);
		if (r2 == IDYES) {
			std::wstring fn = utf8_to_wc(key.key_comment());
			setting_add_keyfile(fn.c_str());
		}
	}
}

void BtSelectDlg::on_buttonBox_accepted()
{
	dbgprintf("click ok\n");
	accept();
}

void BtSelectDlg::update(const std::vector<DeviceInfoType> &deivceInfos)
{
	dbgprintf("update() enter\n");
	deviceInfos_ = deivceInfos;
	emit updateSignal();
	dbgprintf("update() leave\n");
}

// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:


