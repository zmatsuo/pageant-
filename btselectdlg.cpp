
#include <winsock2.h>
#include <thread>
#include <sstream>

#pragma warning(push)
#pragma warning(disable:4127)
#include <QStandardItemModel>
#pragma warning(pop)

#include "bt_agent_proxy_main.h"
#include "bt_agent_proxy.h"
#include "gui_stuff.h"
#include "debug.h"
#include "bt_agent_proxy_main2.h"
#include "pageant.h"
#include "codeconvert.h"
#include "setting.h"

#pragma warning(push)
#pragma warning(disable:4127)
#pragma warning(disable:4251)
#include "btselectdlg.h"
#include "ui_btselectdlg.h"
#pragma warning(pop)

BtSelectDlg::BtSelectDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BtSelectDlg)
{
    ui->setupUi(this);
}

BtSelectDlg::~BtSelectDlg()
{
    delete ui;
}

void BtSelectDlg::update()
{
    int n = 0;
    const int colum = 3;
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
		item = new QStandardItem(deviceInfo.connected ? QString("connent") : QString("unconnent"));
		item->setEditable(false);
		model->setItem(n, 2, item);
		n++;
    }

    model->setHeaderData(0, Qt::Horizontal, QObject::tr("no"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("device name"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("connect"));
    ui->treeView->setModel(model);
}

// refresh
void BtSelectDlg::on_pushButton_2_clicked()
{
    bt_agent_proxy_t *hBta = bt_agent_proxy_main_get_handle();
    if (hBta == nullptr) {
		printf("bt?\n");
		return;
    }

    // デバイス一覧を取得
    std::vector<DeviceInfoType> deviceInfos;
    bta_deviceinfo(hBta, deviceInfos_);
    if (deviceInfos.size() == 0) {
		auto start = std::chrono::system_clock::now();
		while(1) {
			auto now = std::chrono::system_clock::now();
			auto elapse =
				std::chrono::duration_cast<std::chrono::seconds>(
					now - start);
			if (elapse >= (std::chrono::seconds)10) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(10));

			bta_deviceinfo(hBta, deviceInfos);
			if (deviceInfos.size() != 0) {
				break;
			}
		}
		if (deviceInfos.size() == 0) {
			// 見つからなかった
			return;
		}
    }

    deviceInfos = deviceInfos_;
    update();
}

void BtSelectDlg::on_pushButton_clicked()
{
	dbgprintf("click1\n");

	const QItemSelectionModel *selection = ui->treeView->selectionModel();
	const QModelIndexList &indexes = selection->selectedRows();
	switch (indexes.count()) {
	case 0:
		message_box(L"デバイスが選択されていません", L"pageant+", MB_OK);
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
		dbgprintf("%ls\n", deviceInfo.deviceName);
		bool r = bt_agent_proxy_main_connect(deviceInfo);
		if (!r) {
			dbgprintf("connect error\n");
			return;
		}


		std::vector<ckey> keys;
		r = bt_agent_proxy_main_get_key(
			deviceInfo.deviceName.c_str(), keys);

		for (const ckey &key : keys) {
			ssh2_userkey *key_st;
			key.get_raw_key(&key_st);
			if (!pageant_add_ssh2_key(key_st)) {
				printf("err\n");
			}
		}

		for (const ckey &key : keys) {
			std::wostringstream oss;
			oss << L"次回起動時に読み込みますか?\n"
				<< key.key_comment().c_str();
			int r2 = message_boxW(oss.str().c_str(), L"pageant+", MB_YESNO, 0);
			if (r2 == IDYES) {
				std::wstring fn = utf8_to_wc(key.key_comment());
				setting_add_keyfile(fn.c_str());
			}
		}

		break;
	}
	default:
		message_box(L"1つだけ選択してください", L"pageant+", MB_OK);
		break;
	}
}

void BtSelectDlg::on_buttonBox_accepted()
{
	printf("click ok\n");
	accept();
}


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:
