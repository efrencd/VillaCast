#include "i18n.hpp"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform.hpp"
#include "mdns.hpp"
#include "viewer.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include "plugin-support.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QHostInfo>
#include <QLabel>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTableWidget>
#include <QUrlQuery>
#include <QUuid>
#include <QLocale>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTextEdit>
#include <QTextDocument>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mediamtx", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "VillaCast: independent WebRTC streaming powered by MediaMTX";
}

namespace {

constexpr auto dockId = "obs-mediamtx.control";

class Panel final : public QWidget {
	QLineEdit *executable, *path, *url, *mdnsName, *ipUrl;
	QSpinBox *httpPort, *icePort, *viewerPort, *bitrate, *width, *height;
	QComboBox *encoder, *networkInterface, *protocol, *resolutionMode;
	QCheckBox *lan, *mdnsEnabled, *autoStart;
	QLabel *status, *mdnsStatus, *resolutionInfo;
	MdnsPublisher mdns;
	ViewerServer viewer;
	QTextEdit *logs;
	QPushButton *serverStart, *serverStop, *install, *upgrade;
	QProcess server;
	QProcess *package = new QProcess;
	QTimer timer;
	QTimer readinessTimer;
	QElapsedTimer readinessClock;
	QNetworkAccessManager readinessManager;
	QPointer<QNetworkReply> readinessReply;
	quint64 sessionGeneration = 0;
	bool sessionRequested = false, outputConnected = false;
	QString sessionError;
	QString directory;
	obs_output_t *output = nullptr;
	obs_service_t *service = nullptr;
	obs_encoder_t *video = nullptr, *audio = nullptr;
	bool stoppingServer = false;
	QString serverBuffer, packageBuffer;
	QLineEdit *playerTitle, *playerBackground;
	QComboBox *playerFit;
	QSpinBox *playerVolume, *monitorPort, *statsInterval;
	QCheckBox *playerSound, *playerAutoplay, *playerControls;
	QLabel *saveStatus;
	QTimer saveTimer;
	QCheckBox *playerLive, *playerStatus, *playerReconnect, *monitorEnabled;
	QTableWidget *clientsTable;
	QLabel *clientsStatus;
	QNetworkAccessManager clientsManager;
	QPointer<QNetworkReply> clientsReply;
	QTimer clientsTimer;
	QJsonArray apiClients, pendingApiClients;
	QString apiError, monitorPassword;
	struct HostNameEntry {
		QString name;
		qint64 expires = 0;
		bool pending = false;
	};
	QHash<QString, HostNameEntry> clientNames;
	int nameLookups = 0;
	QString clientNetworkName(QString address)
	{
		QHostAddress ip(address);
		if (ip.isNull()) {
			if (address.startsWith('['))
				address = address.mid(1, address.indexOf(']') - 1);
			else
				address = address.section(':', 0, -2);
			ip.setAddress(address);
		}
		if (ip.isNull())
			return "—";
		bool ipv4 = false;
		const auto v4 = ip.toIPv4Address(&ipv4);
		if (ipv4)
			ip.setAddress(v4);
		// Local readers belong to this PC, regardless of hosts-file loopback aliases.
		if (ip.isLoopback() || (ip.toIPv4Address() >> 24) == 127)
			return QHostInfo::localHostName();
		const auto key = ip.toString();
		const auto now = QDateTime::currentMSecsSinceEpoch();
		for (auto it = clientNames.begin(); it != clientNames.end();) {
			if (!it->pending && it->expires <= now)
				it = clientNames.erase(it);
			else
				++it;
		}
		if (clientNames.contains(key))
			return clientNames[key].name.isEmpty() ? QString("—") : clientNames[key].name;
		if (nameLookups >= 4 || clientNames.size() >= 256)
			return "—";
		clientNames.insert(key, {{}, 0, true});
		++nameLookups;
		QHostInfo::lookupHost(key, this, [this, key](const QHostInfo &info) {
			--nameLookups;
			const auto name = info.hostName();
			QHostAddress numeric(name);
			const bool found = info.error() == QHostInfo::NoError && !name.isEmpty() && numeric.isNull();
			clientNames[key] = {found ? name : QString(),
					    QDateTime::currentMSecsSinceEpoch() + (found ? 300000 : 60000), false};
			refreshClients();
		});
		return "—";
	}

	QJsonObject playerOptions() const
	{
		return {{"title",
			 playerTitle->text().trimmed().isEmpty() ? "VillaCast" : playerTitle->text().trimmed()},
			{"background", playerBackground->text().trimmed()},
			{"fit", playerFit->currentData().toString()},
			{"sound", playerSound->isChecked()},
			{"volume", playerVolume->value()},
			{"autoplay", playerAutoplay->isChecked()},
			{"controls", playerControls->isChecked()},
			{"preventPause", playerLive->isChecked()},
			{"showStatus", playerStatus->isChecked()},
			{"autoReconnect", playerReconnect->isChecked()},
			{"monitor", monitorEnabled->isChecked()},
			{"statsInterval", statsInterval->value()}};
	}
	bool applyPlayerOptions(bool notifyError = true)
	{
		if (!QRegularExpression("^#[0-9a-fA-F]{6}$").match(playerBackground->text().trimmed()).hasMatch()) {
			if (notifyError)
				log(I18n::text("InvalidBackground"));
			saveStatus->setText(I18n::text("InvalidBackground"));
			return false;
		}
		viewer.setOptions(playerOptions());
		save();
		clientsTimer.setInterval(statsInterval->value() * 1000);
		saveStatus->setText(I18n::text("AutoSaved"));
		return true;
	}
	void pollClients(int page = 0)
	{
		if (clientsReply || server.state() != QProcess::Running || stoppingServer ||
		    !monitorEnabled->isChecked())
			return;
		if (page == 0)
			pendingApiClients = {};
		const auto generation = sessionGeneration;
		QNetworkRequest request(
			QUrl(QString("http://127.0.0.1:%1/v3/webrtc/sessions/list?page=%2&itemsPerPage=256")
				     .arg(monitorPort->value())
				     .arg(page)));
		request.setRawHeader("Authorization",
				     "Basic " + ("obs-monitor:" + monitorPassword).toUtf8().toBase64());
		auto *reply = clientsManager.get(request);
		clientsReply = reply;
		QTimer::singleShot(1500, reply, [reply] {
			if (!reply->isFinished())
				reply->abort();
		});
		connect(reply, &QNetworkReply::finished, this, [this, reply, generation, page] {
			if (clientsReply == reply)
				clientsReply.clear();
			const auto doc = QJsonDocument::fromJson(reply->readAll());
			const bool valid = reply->error() == QNetworkReply::NoError && doc.isObject() &&
					   doc.object().value("items").isArray();
			reply->deleteLater();
			if (generation != sessionGeneration || stoppingServer)
				return;
			if (!valid) {
				apiError = I18n::text("MonitorUnavailable");
				apiClients = {};
				return;
			}
			const auto result = doc.object();
			for (const auto &item : result.value("items").toArray()) {
				const auto session = item.toObject();
				if (session.value("state").toString() == "read" &&
				    session.value("path").toString() == path->text())
					pendingApiClients.append(session);
			}
			if (page + 1 < qMin(16, result.value("pageCount").toInt())) {
				pollClients(page + 1);
				return;
			}
			apiClients = pendingApiClients;
			apiError.clear();
			refreshClients();
		});
	}
	void refreshClients()
	{
		if (server.state() != QProcess::Running || !monitorEnabled->isChecked()) {
			clientsTable->setRowCount(0);
			clientsStatus->setText(I18n::text(monitorEnabled->isChecked() ? "Stopped" : "MonitorDisabled"));
			return;
		}
		QHash<QString, ViewerReport> reports;
		for (const auto &report : viewer.clientReports())
			reports.insert(report.id, report);
		struct Row {
			QString id, address, created, state;
			QJsonObject metrics;
		};
		QList<Row> rows;
		for (const auto &entry : apiClients) {
			const auto session = entry.toObject();
			const auto client = QUrlQuery(session.value("query").toString()).queryItemValue("obsClient");
			Row row{session.value("id").toString(),
				session.value("remoteAddr").toString(),
				session.value("created").toString(),
				session.value("peerConnectionEstablished").toBool() ? "connected" : "connecting",
				{}};
			if (reports.contains(client)) {
				const auto report = reports.take(client);
				row.id = client;
				row.address = report.address;
				row.metrics = report.metrics;
				row.state = row.metrics.value("state").toString(row.state);
			}
			rows.append(row);
		}
		for (const auto &report : reports)
			rows.append(
				{report.id, report.address,
				 QDateTime::fromMSecsSinceEpoch(report.firstSeen).toUTC().toString(Qt::ISODateWithMs),
				 report.metrics.value("state").toString("connecting"), report.metrics});
		std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.id < b.id; });
		clientsTable->setRowCount(rows.size());
		for (int i = 0; i < rows.size(); ++i) {
			const auto &row = rows[i];
			const auto created = QDateTime::fromString(row.created, Qt::ISODate);
			const auto seconds = qMax<qint64>(0, created.secsTo(QDateTime::currentDateTimeUtc()));
			QStringList values{
				QString::number(i + 1),
				clientNetworkName(row.address),
				row.id.left(8),
				row.address,
				created.isValid() ? created.toLocalTime().toString("yyyy-MM-dd HH:mm:ss") : "—",
				created.isValid() ? QString("%1:%2:%3")
							    .arg(seconds / 3600, 2, 10, QChar('0'))
							    .arg(seconds / 60 % 60, 2, 10, QChar('0'))
							    .arg(seconds % 60, 2, 10, QChar('0'))
						  : "—",
				I18n::text(("ClientState_" + row.state).toUtf8().constData()),
				row.metrics.contains("width") ? QString("%1 × %2")
									.arg(row.metrics.value("width").toInt())
									.arg(row.metrics.value("height").toInt())
							      : "—"};
			for (const auto &key : {"fps", "bitrate", "loss", "jitter", "rtt", "dropped"})
				values.append(row.metrics.contains(key)
						      ? QString::number(row.metrics.value(key).toDouble(), 'f',
									QByteArray(key) == "dropped" ? 0 : 1)
						      : "—");
			for (int c = 0; c < values.size(); ++c) {
				auto *item = clientsTable->item(i, c);
				if (!item) {
					item = new QTableWidgetItem;
					clientsTable->setItem(i, c, item);
				}
				item->setText(values[c]);
			}
		}
		clientsStatus->setText(apiError.isEmpty() ? I18n::text("ClientCount").arg(rows.size()) : apiError);
	}

	void log(const QString &line)
	{
		static const QRegularExpression timestamp(
			"^(\\d{4}[/-]\\d{2}[/-]\\d{2}) (\\d{2}:\\d{2}:\\d{2})(?:\\s+)(.*)$");
		const auto match = timestamp.match(line);
		const auto now = QDateTime::currentDateTime();
		const auto date = match.hasMatch() ? match.captured(1) : now.toString("yyyy-MM-dd");
		const auto time = match.hasMatch() ? match.captured(2) : now.toString("HH:mm:ss");
		const auto message = match.hasMatch() ? match.captured(3) : line;
		logs->append(
			QString("<span style=\"color:#94bfff;font-family:monospace\">%1</span> <span style=\"color:#64d9c2;font-family:monospace\">%2</span> %3")
				.arg(date.toHtmlEscaped(), time.toHtmlEscaped(), message.toHtmlEscaped()));
		blog(LOG_INFO, "[obs-mediamtx] %s", line.toUtf8().constData());
	}
	QSettings settings() const { return QSettings(directory + "/settings.ini", QSettings::IniFormat); }
	void save()
	{
		auto s = settings();
		s.setValue("executable", executable->text());
		s.setValue("path", path->text());
		s.setValue("http", httpPort->value());
		s.setValue("ice", icePort->value());
		s.setValue("viewer", viewerPort->value());
		s.setValue("lan", lan->isChecked());
		s.setValue("bitrate", bitrate->value());
		s.setValue("width", width->value());
		s.setValue("height", height->value());
		s.setValue("encoder", encoder->currentData());
		s.setValue("protocol", protocol->currentData());
		s.setValue("resolutionMode", resolutionMode->currentData());
		s.setValue("autoStart", autoStart->isChecked());
		s.setValue("mdns", mdnsEnabled->isChecked());
		s.setValue("mdnsName", mdnsName->text().trimmed().toLower());
		s.setValue("networkInterface", networkInterface->currentData());
		s.setValue("playerOptions", QJsonDocument(playerOptions()).toJson(QJsonDocument::Compact));
		s.setValue("monitorPort", monitorPort->value());
	}
	QString findExecutable() { return Platform::findMediaMtx(executable->text().trimmed()); }
	QSize effectiveResolution() const
	{
		if (resolutionMode->currentData().toString() == "custom")
			return {width->value(), height->value()};
		obs_video_info info{};
		if (!obs_get_video_info(&info))
			return {};
		return {int(info.output_width), int(info.output_height)};
	}
	void refresh()
	{
		const bool running = server.state() != QProcess::NotRunning;
		const bool busy = package->state() != QProcess::NotRunning;
		const bool active = output != nullptr;
		const bool idle = !running && !busy && !active && !sessionRequested && !stoppingServer;
		serverStart->setEnabled(idle);
		serverStop->setEnabled((running || active || sessionRequested) && !stoppingServer);
		protocol->setEnabled(idle);
		install->setEnabled(!running && !active && !busy);
		upgrade->setEnabled(!running && !active && !busy);
		for (QWidget *w :
		     QList<QWidget *>{static_cast<QWidget *>(executable), path, httpPort, icePort, viewerPort, lan})
			w->setEnabled(!running && !active && !busy);
		mdnsEnabled->setEnabled(lan->isChecked() && !running && !active && !busy);
		mdnsName->setEnabled(lan->isChecked() && mdnsEnabled->isChecked() && !running && !active && !busy);
		networkInterface->setEnabled(lan->isChecked() && !running && !active && !busy);
		for (QWidget *w : QList<QWidget *>{static_cast<QWidget *>(bitrate), encoder, resolutionMode})
			w->setEnabled(idle);
		const bool custom = resolutionMode->currentData().toString() == "custom";
		width->setEnabled(idle && custom);
		height->setEnabled(idle && custom);
		const auto size = effectiveResolution();
		resolutionInfo->setText(QString("%1 × %2 · %3")
						.arg(size.width())
						.arg(size.height())
						.arg(custom ? I18n::text("Custom") : I18n::text("ObsOutput")));
		QString state = busy ? I18n::text("PackageBusy") : I18n::text("Stopped");
		if (stoppingServer)
			state = I18n::text("Stopping");
		else if (outputConnected)
			state = I18n::text("Streaming");
		else if (active)
			state = I18n::text("Connecting");
		else if (sessionRequested)
			state = I18n::text("Starting");
		else if (!sessionError.isEmpty())
			state = I18n::text("ErrorPrefix") + sessionError;
		status->setText(state);
		QString host = "127.0.0.1";
		if (lan->isChecked()) {
			const int selected = networkInterface->currentData().toInt();
			for (const auto &iface : MdnsPublisher::interfaces()) {
				if (!selected || iface.index() == selected) {
					host = MdnsPublisher::addressFor(iface).toString();
					break;
				}
			}
		}
		ipUrl->setText(QString("http://%1:%2/").arg(host).arg(viewerPort->value()));
		const auto advertised = mdns.hostName();
		url->setText(
			QString("http://%1:%2/").arg(advertised.isEmpty() ? host : advertised).arg(viewerPort->value()));
		mdnsStatus->setText(lan->isChecked() && mdnsEnabled->isChecked() ? mdns.description()
										 : I18n::text("MdnsDisabled"));
		monitorEnabled->setEnabled(idle);
		monitorPort->setEnabled(idle && monitorEnabled->isChecked());
		statsInterval->setEnabled(idle && monitorEnabled->isChecked());
		refreshClients();
	}
	void runPackage(bool update)
	{
		if (server.state() != QProcess::NotRunning || output || package->state() != QProcess::NotRunning)
			return;
		const QString winget = Platform::packageManager();
		if (winget.isEmpty()) {
			log(Platform::installationHelp());
			return;
		}
		QMessageBox consent(QMessageBox::Question, update ? I18n::text("Update") : I18n::text("Install"),
				    I18n::text("PackageConsent"), QMessageBox::Yes | QMessageBox::No, this);
		consent.button(QMessageBox::Yes)->setText(I18n::text("Yes"));
		consent.button(QMessageBox::No)->setText(I18n::text("No"));
		consent.setDefaultButton(QMessageBox::No);
		const auto response = consent.exec();
		if (response != QMessageBox::Yes)
			return;
		save();
		packageBuffer.clear();
		package->setProgram(winget);
		package->setArguments(Platform::packageArguments(update));
		log(update ? I18n::text("Updating") : I18n::text("Installing"));
		package->start();
		refresh();
	}
	bool writeConfig()
	{
		if (lan->isChecked() && mdnsEnabled->isChecked() &&
		    !MdnsWire::validLabel(mdnsName->text().trimmed().toLower())) {
			log(I18n::text("InvalidName"));
			return false;
		}
		if (!QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(path->text()).hasMatch()) {
			log(I18n::text("InvalidPath"));
			return false;
		}
		if (httpPort->value() == icePort->value() || viewerPort->value() == httpPort->value() ||
		    viewerPort->value() == icePort->value() ||
		    (monitorEnabled->isChecked() &&
		     (monitorPort->value() == httpPort->value() || monitorPort->value() == icePort->value() ||
		      monitorPort->value() == viewerPort->value()))) {
			log(I18n::text("DistinctPorts"));
			return false;
		}
		QTcpServer httpCheck, tcpCheck, viewerCheck, apiCheck;
		QUdpSocket udpCheck;
		const auto bind = QHostAddress::Any;
		if (!httpCheck.listen(bind, static_cast<quint16>(httpPort->value())) ||
		    !tcpCheck.listen(bind, static_cast<quint16>(icePort->value())) ||
		    !udpCheck.bind(bind, static_cast<quint16>(icePort->value())) ||
		    !viewerCheck.listen(bind, static_cast<quint16>(viewerPort->value())) ||
		    (monitorEnabled->isChecked() &&
		     !apiCheck.listen(QHostAddress::LocalHost, static_cast<quint16>(monitorPort->value())))) {
			log(I18n::text("PortBusy"));
			return false;
		}
		QString config =
			QString("logLevel: info\nlogDestinations: [stdout]\nrtsp: false\nrtmp: false\nhls: false\nsrt: false\n"
				"webrtc: true\nwebrtcAddress: '%1:%2'\nwebrtcLocalUDPAddress: '%1:%3'\nwebrtcLocalTCPAddress: '%1:%3'\n"
				"webrtcAdditionalHosts: [127.0.0.1]\nauthMethod: internal\nauthInternalUsers:\n"
				"  - user: any\n    pass:\n    ips: [127.0.0.1, '::1']\n    permissions:\n"
				"      - action: publish\n        path: %4\n"
				"  - user: any\n    pass:\n    ips: %5\n    permissions:\n"
				"      - action: read\n        path: %4\npaths:\n  %4:\n    source: publisher\n")
				.arg(lan->isChecked() ? "" : "127.0.0.1")
				.arg(httpPort->value())
				.arg(icePort->value())
				.arg(path->text())
				.arg(lan->isChecked()
					     ? "[127.0.0.1, '::1', 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 'fe80::/10', 'fc00::/7']"
					     : "[127.0.0.1, '::1']");
		// Disable newer protocols only on versions that support them. Minimum supported server: 1.21.
		config += "moq: false\nplayback: false\n";
		if (monitorEnabled->isChecked()) {
			monitorPassword = QUuid::createUuid().toString(QUuid::WithoutBraces);
			config.replace(
				"paths:\n",
				QString("  - user: obs-monitor\n    pass: '%1'\n    ips: [127.0.0.1]\n    permissions:\n      - action: api\npaths:\n")
					.arg(monitorPassword));
			config += QString("api: true\napiAddress: '127.0.0.1:%1'\napiAllowOrigins: []\n")
					  .arg(monitorPort->value());
		} else
			config += "api: false\n";
		QSaveFile file(directory + "/mediamtx.yml");
		if (!file.open(QIODevice::WriteOnly) || file.write(config.toUtf8()) < 0 || !file.commit()) {
			log(I18n::text("ConfigSaveFailed"));
			return false;
		}
		return true;
	}
	void startServer()
	{
		if (server.state() != QProcess::NotRunning || package->state() != QProcess::NotRunning || output ||
		    sessionRequested)
			return;
		if (!applyPlayerOptions())
			return;
		apiClients = {};
		pendingApiClients = {};
		apiError.clear();
		sessionError.clear();
		if (protocol->currentData().toString() != "webrtc") {
			log(I18n::text("ProtocolUnavailable"));
			return;
		}
		const QString exe = findExecutable();
		if (exe.isEmpty()) {
			failSession(I18n::text("MediaMtxMissing"));
			return;
		}
		executable->setText(exe);
		if (!writeConfig()) {
			sessionError = I18n::text("PrepareFailed");
			refresh();
			return;
		}
		char *viewerFile = obs_module_file("viewer.html");
		const bool viewerReady = viewerFile &&
					 viewer.start(static_cast<quint16>(viewerPort->value()), lan->isChecked(),
						      static_cast<quint16>(httpPort->value()), path->text(),
						      QString::fromUtf8(viewerFile));
		bfree(viewerFile);
		if (!viewerReady) {
			failSession(I18n::text("ViewerFailed"));
			return;
		}
		save();
		stoppingServer = false;
		sessionRequested = true;
		++sessionGeneration;
		serverBuffer.clear();
		server.setProgram(exe);
		server.setArguments({directory + "/mediamtx.yml"});
		server.setWorkingDirectory(directory);
		log(I18n::text("StartingWith") + directory + "/mediamtx.yml");
		server.start();
		refresh();
	}
	void terminateServer()
	{
		if (server.state() == QProcess::NotRunning) {
			stoppingServer = false;
			refresh();
			return;
		}
		server.terminate();
		QTimer::singleShot(2000, this, [this] {
			if (stoppingServer && server.state() != QProcess::NotRunning)
				server.kill();
		});
	}
	void stopServer()
	{
		sessionRequested = false;
		++sessionGeneration;
		clientsTimer.stop();
		apiClients = {};
		pendingApiClients = {};
		if (clientsReply)
			clientsReply->abort();
		readinessTimer.stop();
		if (readinessReply)
			readinessReply->abort();
		stoppingServer = true;
		if (output)
			obs_output_stop(output);
		else
			terminateServer();
		refresh();
	}
	void failSession(const QString &error)
	{
		sessionError = error;
		log(error);
		stopServer();
	}
	void probeReadiness()
	{
		if (!sessionRequested || stoppingServer || server.state() != QProcess::Running)
			return;
		if (readinessClock.elapsed() > 10000) {
			failSession(I18n::text("ServerTimeout"));
			return;
		}
		if (readinessReply)
			return;
		const auto generation = sessionGeneration;
		auto *reply = readinessManager.get(QNetworkRequest(
			QUrl(QString("http://127.0.0.1:%1/%2/").arg(httpPort->value()).arg(path->text()))));
		readinessReply = reply;
		QTimer::singleShot(1000, reply, [reply] {
			if (!reply->isFinished())
				reply->abort();
		});
		connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
			const bool ready = reply->error() == QNetworkReply::NoError &&
					   reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200;
			if (readinessReply == reply)
				readinessReply.clear();
			reply->deleteLater();
			if (generation != sessionGeneration || !sessionRequested || stoppingServer)
				return;
			if (ready) {
				readinessTimer.stop();
				log(I18n::text("ServerReady"));
				startOutput();
			}
		});
	}
	static void onOutputStart(void *data, calldata_t *)
	{
		auto *self = static_cast<Panel *>(data);
		QMetaObject::invokeMethod(
			self,
			[self] {
				if (!self->output || !self->sessionRequested || self->stoppingServer)
					return;
				self->outputConnected = true;
				self->log(I18n::text("OutputConnected"));
				self->refresh();
			},
			Qt::QueuedConnection);
	}
	static void onOutputStop(void *data, calldata_t *cd)
	{
		auto *self = static_cast<Panel *>(data);
		const int code = static_cast<int>(calldata_int(cd, "code"));
		QMetaObject::invokeMethod(
			self,
			[self, code] {
				QString error;
				if (self->output)
					error = QString::fromUtf8(obs_output_get_last_error(self->output));
				self->log(QString(I18n::text("OutputStopped")).arg(code).arg(error));
				self->releaseOutput();
				if (self->sessionRequested) {
					self->sessionError = I18n::text("OutputInterrupted");
					self->stopServer();
				} else if (self->stoppingServer)
					self->terminateServer();
				self->refresh();
			},
			Qt::QueuedConnection);
	}
	void releaseOutput()
	{
		outputConnected = false;
		if (output) {
			auto *signalHandler = obs_output_get_signal_handler(output);
			signal_handler_disconnect(signalHandler, "start", onOutputStart, this);
			signal_handler_disconnect(signalHandler, "stop", onOutputStop, this);
			obs_output_release(output);
			output = nullptr;
		}
		obs_encoder_release(video);
		video = nullptr;
		obs_encoder_release(audio);
		audio = nullptr;
		obs_service_release(service);
		service = nullptr;
	}
	void startOutput()
	{
		if (output || server.state() != QProcess::Running || !sessionRequested || stoppingServer)
			return;
		if (!obs_get_video() || !obs_get_audio()) {
			failSession(I18n::text("AvUnavailable"));
			return;
		}
		const auto size = effectiveResolution();
		if (!size.isValid() || size.width() % 2 || size.height() % 2) {
			failSession(I18n::text("InvalidResolution"));
			return;
		}
		save();
		auto *vs = obs_data_create();
		obs_data_set_int(vs, "bitrate", bitrate->value());
		obs_data_set_string(vs, "rate_control", "CBR");
		obs_data_set_int(vs, "keyint_sec", 2);
		obs_data_set_int(vs, "bf", 0);
		obs_data_set_bool(vs, "lookahead", false);
		// WebRTC receivers joining later need SPS/PPS with every keyframe.
		obs_data_set_bool(vs, "repeat_headers", true);
		obs_data_set_string(vs, "profile", "baseline");
		obs_data_set_string(vs, "preset", encoder->currentData().toString() == "obs_x264" ? "veryfast" : "p4");
		obs_data_set_string(vs, "tune",
				    encoder->currentData().toString() == "obs_x264" ? "zerolatency" : "ull");
		const auto encoderId = encoder->currentData().toString().toUtf8();
		video = obs_video_encoder_create(encoderId.constData(), "VillaCast video", vs, nullptr);
		obs_data_release(vs);
		auto *as = obs_data_create();
		obs_data_set_int(as, "bitrate", 128);
		audio = obs_audio_encoder_create("ffmpeg_opus", "VillaCast audio", as, 0, nullptr);
		obs_data_release(as);
		auto *ss = obs_data_create();
		const auto endpoint =
			QString("http://127.0.0.1:%1/%2/whip").arg(httpPort->value()).arg(path->text()).toUtf8();
		obs_data_set_string(ss, "server", endpoint.constData());
		service = obs_service_create("whip_custom", "VillaCast local", ss, nullptr);
		obs_data_release(ss);
		output = obs_output_create("whip_output", "VillaCast independent WebRTC", nullptr, nullptr);
		if (!video || !audio || !service || !output) {
			releaseOutput();
			failSession(I18n::text("OutputCreateFailed"));
			return;
		}
		obs_encoder_set_video(video, obs_get_video());
		if (resolutionMode->currentData().toString() == "custom") {
			obs_encoder_set_scaled_size(video, static_cast<uint32_t>(size.width()),
						    static_cast<uint32_t>(size.height()));
			obs_encoder_set_gpu_scale_type(video, OBS_SCALE_BICUBIC);
		}
		log(QString(I18n::text("StreamResolution"))
			    .arg(size.width())
			    .arg(size.height())
			    .arg(resolutionMode->currentData().toString() == "custom" ? I18n::text("CustomLog")
										      : I18n::text("ObsOutputLog")));
		obs_encoder_set_audio(audio, obs_get_audio());
		obs_output_set_video_encoder(output, video);
		obs_output_set_audio_encoder(output, audio, 0);
		obs_output_set_service(output, service);
		auto *signalHandler = obs_output_get_signal_handler(output);
		signal_handler_connect(signalHandler, "start", onOutputStart, this);
		signal_handler_connect(signalHandler, "stop", onOutputStop, this);
		if (!obs_output_start(output)) {
			const auto error = QString::fromUtf8(obs_output_get_last_error(output));
			releaseOutput();
			failSession(I18n::text("WebRtcFailed") + error);
		}
		refresh();
	}
	void readLogs(QProcess &process, QString &buffer)
	{
		buffer += QString::fromUtf8(process.readAllStandardOutput()).replace('\r', '\n');
		int newline;
		while ((newline = buffer.indexOf('\n')) >= 0) {
			const QString line = buffer.left(newline).trimmed();
			buffer.remove(0, newline + 1);
			if (!line.isEmpty())
				log(line);
		}
		if (buffer.size() > 16384) {
			log(buffer);
			buffer.clear();
		}
	}

public:
	Panel()
	{
		char *configDir = obs_module_config_path("");
		directory = QDir(QString::fromUtf8(configDir)).absolutePath();
		bfree(configDir);
		QDir().mkpath(directory);
		auto *root = new QVBoxLayout(this);
		status = new QLabel;
		root->addWidget(status);
		auto *tabs = new QTabWidget;
		root->addWidget(tabs);
		auto *controls = new QWidget;
		auto *controlsScroll = new QScrollArea;
		controlsScroll->setWidgetResizable(true);
		controlsScroll->setWidget(controls);
		tabs->addTab(controlsScroll, I18n::text("StreamTab"));
		auto *configuration = new QWidget;
		auto *configurationScroll = new QScrollArea;
		configurationScroll->setWidgetResizable(true);
		configurationScroll->setWidget(configuration);
		tabs->addTab(configurationScroll, I18n::text("Settings"));
		auto *clientsTab = new QWidget;
		tabs->addTab(clientsTab, I18n::text("Clients"));
		auto *clientsLayout = new QVBoxLayout(clientsTab);
		clientsStatus = new QLabel;
		clientsLayout->addWidget(clientsStatus);
		auto *clientsHelp = new QLabel(I18n::text("ClientsHelp"));
		clientsHelp->setWordWrap(true);
		clientsLayout->addWidget(clientsHelp);
		clientsTable = new QTableWidget(0, 14);
		clientsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
		clientsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
		QStringList headings;
		for (const auto &key :
		     {"RowNumber", "NetworkName", "Client", "Address", "ConnectedSince", "ConnectionTime", "State",
		      "Resolution", "FPS", "VideoKbps", "PacketLoss", "Jitter", "Rtt", "DroppedFrames"})
			headings.append(I18n::text(key));
		clientsTable->setHorizontalHeaderLabels(headings);
		clientsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
		clientsTable->verticalHeader()->hide();
		clientsLayout->addWidget(clientsTable);
		auto *logTab = new QWidget;
		tabs->addTab(logTab, "LOG");
		auto *logLayout = new QVBoxLayout(logTab);
		auto *about = new QWidget;
		tabs->addTab(about, I18n::text("About"));
		auto *aboutLayout = new QVBoxLayout(about);
		auto *aboutTitle = new QLabel("VillaCast");
		auto titleFont = aboutTitle->font();
		titleFont.setBold(true);
		aboutTitle->setFont(titleFont);
		aboutLayout->addWidget(aboutTitle);
		aboutLayout->addWidget(new QLabel(I18n::text("AboutVersion").arg(QString::fromUtf8(PLUGIN_VERSION))));
		aboutLayout->addWidget(new QLabel(I18n::text("AboutAuthor").arg("Efren C.D.")));
		auto *origin = new QLabel(I18n::text("AboutOrigin"));
		origin->setWordWrap(true);
		aboutLayout->addWidget(origin);
		const auto linkColor = palette().color(QPalette::Window).lightness() < 128 ? "#94bfff" : "#0645ad";
		auto *projectLink = new QLabel(
			QString("<a style=\"color:%1\" href=\"https://github.com/efrencd/VillaCast\">GitHub · VillaCast</a>")
				.arg(linkColor));
		projectLink->setTextFormat(Qt::RichText);
		projectLink->setOpenExternalLinks(true);
		projectLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
		aboutLayout->addWidget(projectLink);
		aboutLayout->addStretch();
		auto *layout = new QVBoxLayout(controls);
		autoStart = new QCheckBox(I18n::text("AutoStart"));
		autoStart->setToolTip(I18n::text("AutoStartHelp"));
		layout->addWidget(autoStart);
		auto *form = new QFormLayout(configuration);
		form->setRowWrapPolicy(QFormLayout::WrapLongRows);
		executable = new QLineEdit;
		form->addRow("MediaMTX", executable);
		auto *browse = new QPushButton(I18n::text("Browse"));
		form->addRow(browse);
		path = new QLineEdit("mystream");
		form->addRow(I18n::text("StreamName"), path);
		auto spin = [form](const QString &label, int min, int max, int value) {
			auto *s = new QSpinBox;
			s->setRange(min, max);
			s->setValue(value);
			form->addRow(label, s);
			return s;
		};
		httpPort = spin(I18n::text("WebPort"), 1024, 65535, 8889);
		icePort = spin(I18n::text("IcePort"), 1024, 65535, 8189);
		viewerPort = spin(I18n::text("PlayerPort"), 1024, 65535, 8891);
		lan = new QCheckBox(I18n::text("AllowLan"));
		lan->setChecked(true);
		form->addRow(lan);
		mdnsEnabled = new QCheckBox(I18n::text("Advertise"));
		form->addRow(mdnsEnabled);
		mdnsName = new QLineEdit;
		mdnsName->setMaxLength(63);
		form->addRow(I18n::text("HostName"), mdnsName);
		networkInterface = new QComboBox;
		networkInterface->addItem(I18n::text("AutoInterface"), 0);
		for (const auto &iface : MdnsPublisher::interfaces())
			networkInterface->addItem(iface.humanReadableName() + " · " +
							  MdnsPublisher::addressFor(iface).toString(),
						  iface.index());
		form->addRow(I18n::text("LinkInterface"), networkInterface);
		encoder = new QComboBox;
		encoder->addItem("x264 (CPU)", "obs_x264");
		encoder->addItem("NVIDIA NVENC H.264", "obs_nvenc_h264_tex");
		form->addRow(I18n::text("Encoder"), encoder);
		bitrate = spin(I18n::text("Bitrate"), 250, 50000, 4000);
		width = spin(I18n::text("Width"), 160, 7680, 1280);
		height = spin(I18n::text("Height"), 90, 4320, 720);
		width->setSingleStep(2);
		height->setSingleStep(2);
		resolutionMode = new QComboBox;
		resolutionMode->addItem(I18n::text("AutoResolution"), "obs");
		resolutionMode->addItem(I18n::text("Custom"), "custom");
		form->insertRow(form->rowCount() - 2, I18n::text("Resolution"), resolutionMode);
		auto *playerGroup = new QGroupBox(I18n::text("WebPlayer"));
		auto *playerForm = new QFormLayout(playerGroup);
		playerForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
		playerTitle = new QLineEdit("VillaCast");
		playerTitle->setMaxLength(120);
		playerForm->addRow(I18n::text("PlayerTitle"), playerTitle);
		playerBackground = new QLineEdit("#111111");
		playerBackground->setMaxLength(7);
		playerForm->addRow(I18n::text("PlayerBackground"), playerBackground);
		playerFit = new QComboBox;
		playerFit->addItem(I18n::text("FitContain"), "contain");
		playerFit->addItem(I18n::text("FitCover"), "cover");
		playerForm->addRow(I18n::text("PlayerFit"), playerFit);
		auto check = [playerForm](const char *key, bool value) {
			auto *box = new QCheckBox(I18n::text(key));
			box->setChecked(value);
			playerForm->addRow(box);
			return box;
		};
		playerSound = check("PlayerSound", false);
		playerVolume = new QSpinBox;
		playerVolume->setRange(0, 100);
		playerVolume->setValue(100);
		playerVolume->setSuffix(" %");
		playerForm->addRow(I18n::text("PlayerVolume"), playerVolume);
		playerAutoplay = check("PlayerAutoplay", true);
		playerControls = check("PlayerControls", true);
		playerLive = check("PlayerLive", true);
		playerStatus = check("PlayerStatus", true);
		playerReconnect = check("PlayerReconnect", true);
		auto *playerHelp = new QLabel(I18n::text("PlayerHelp"));
		playerHelp->setWordWrap(true);
		playerForm->addRow(playerHelp);
		form->addRow(playerGroup);
		auto *monitorGroup = new QGroupBox(I18n::text("Clients"));
		auto *monitorForm = new QFormLayout(monitorGroup);
		monitorForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
		monitorEnabled = new QCheckBox(I18n::text("MonitorEnabled"));
		monitorEnabled->setChecked(true);
		monitorForm->addRow(monitorEnabled);
		monitorPort = new QSpinBox;
		monitorPort->setRange(1024, 65535);
		monitorPort->setValue(9997);
		monitorForm->addRow(I18n::text("MonitorPort"), monitorPort);
		statsInterval = new QSpinBox;
		statsInterval->setRange(1, 30);
		statsInterval->setValue(2);
		statsInterval->setSuffix(" s");
		monitorForm->addRow(I18n::text("StatsInterval"), statsInterval);
		form->addRow(monitorGroup);
		saveStatus = new QLabel(I18n::text("AutoSaveHelp"));
		saveStatus->setWordWrap(true);
		form->addRow(saveStatus);
		resolutionInfo = new QLabel;
		layout->addWidget(resolutionInfo);
		protocol = new QComboBox;
		protocol->addItem("WebRTC", "webrtc");
		auto *protocolForm = new QFormLayout;
		protocolForm->addRow(I18n::text("StreamType"), protocol);
		layout->insertLayout(0, protocolForm);
		auto *note = new QLabel(I18n::text("StreamHelp"));
		note->setWordWrap(true);
		layout->addWidget(note);
		auto row = [layout](QPushButton *a, QPushButton *b) {
			auto *r = new QHBoxLayout;
			r->addWidget(a);
			r->addWidget(b);
			layout->addLayout(r);
		};
		install = new QPushButton(I18n::text("Install"));
		upgrade = new QPushButton(I18n::text("Update"));
		row(install, upgrade);
		serverStart = new QPushButton(I18n::text("Start"));
		serverStop = new QPushButton(I18n::text("Stop"));
		serverStop->setStyleSheet("QPushButton:enabled {background-color:#2563eb;color:white;}"
					  "QPushButton:enabled:hover {background-color:#3474f5;}"
					  "QPushButton:enabled:pressed {background-color:#1d4ed8;}");
		row(serverStart, serverStop);
		url = new QLineEdit;
		url->setReadOnly(true);
		layout->addWidget(url);
		mdnsStatus = new QLabel;
		mdnsStatus->setWordWrap(true);
		layout->addWidget(mdnsStatus);
		ipUrl = new QLineEdit;
		ipUrl->setReadOnly(true);
		ipUrl->setToolTip(I18n::text("IpLinkHelp"));
		layout->addWidget(ipUrl);
		auto *open = new QPushButton(I18n::text("OpenVideo"));
		auto *copy = new QPushButton(I18n::text("CopyLink"));
		row(open, copy);
		auto *folder = new QPushButton(I18n::text("OpenConfig"));
		layout->addWidget(folder);
		logs = new QTextEdit;
		logs->setReadOnly(true);
		logs->document()->setMaximumBlockCount(1500);
		logLayout->addWidget(logs);
		layout->addStretch();
		auto s = settings();
		const auto savedPlayer = QJsonDocument::fromJson(s.value("playerOptions").toByteArray()).object();
		const auto savedTitle = savedPlayer.value("title").toString("VillaCast");
		playerTitle->setText(savedTitle == "OBS MediaMTX" ? "VillaCast" : savedTitle);
		const auto background = savedPlayer.value("background").toString("#111111");
		playerBackground->setText(
			QRegularExpression("^#[0-9a-fA-F]{6}$").match(background).hasMatch() ? background : "#111111");
		playerFit->setCurrentIndex(savedPlayer.value("fit").toString() == "cover" ? 1 : 0);
		playerSound->setChecked(savedPlayer.value("sound").toBool(false));
		playerVolume->setValue(savedPlayer.value("volume").toInt(100));
		playerAutoplay->setChecked(savedPlayer.value("autoplay").toBool(true));
		playerControls->setChecked(savedPlayer.value("controls").toBool(true));
		playerLive->setChecked(savedPlayer.value("preventPause").toBool(true));
		playerStatus->setChecked(savedPlayer.value("showStatus").toBool(true));
		playerReconnect->setChecked(savedPlayer.value("autoReconnect").toBool(true));
		monitorEnabled->setChecked(savedPlayer.value("monitor").toBool(true));
		statsInterval->setValue(savedPlayer.value("statsInterval").toInt(2));
		monitorPort->setValue(s.value("monitorPort", 9997).toInt());
		connect(monitorEnabled, &QCheckBox::toggled, this, [this] { refresh(); });
		autoStart->setChecked(s.value("autoStart", false).toBool());
		connect(autoStart, &QCheckBox::toggled, this, [this] { save(); });
		executable->setText(s.value("executable").toString());
		path->setText(s.value("path", "mystream").toString());
		httpPort->setValue(s.value("http", 8889).toInt());
		icePort->setValue(s.value("ice", 8189).toInt());
		viewerPort->setValue(s.value("viewer", 8891).toInt());
		lan->setChecked(s.value("lan", true).toBool());
		mdnsEnabled->setChecked(s.value("mdns", true).toBool());
		auto defaultName = QHostInfo::localHostName().section('.', 0, 0).toLower();
		defaultName.replace(QRegularExpression("[^a-z0-9-]"), "-");
		defaultName = "obs-" + defaultName.left(55);
		if (!MdnsWire::validLabel(defaultName))
			defaultName = "obs-video";
		mdnsName->setText(s.value("mdnsName", defaultName).toString());
		const int savedInterface = s.value("networkInterface", 0).toInt();
		const int networkIdx = networkInterface->findData(savedInterface);
		if (networkIdx >= 0)
			networkInterface->setCurrentIndex(networkIdx);
		else if (savedInterface) {
			networkInterface->addItem(I18n::text("InterfaceMissing"), savedInterface);
			networkInterface->setCurrentIndex(networkInterface->count() - 1);
		}
		bitrate->setValue(s.value("bitrate", 4000).toInt());
		width->setValue(s.value("width", 1280).toInt());
		height->setValue(s.value("height", 720).toInt());
		const int idx = encoder->findData(s.value("encoder", "obs_x264"));
		if (idx >= 0)
			encoder->setCurrentIndex(idx);
		resolutionMode->setCurrentIndex(s.value("resolutionMode", "obs").toString() == "custom" ? 1 : 0);
		connect(resolutionMode, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
		connect(width, &QSpinBox::valueChanged, this, [this] { refresh(); });
		connect(height, &QSpinBox::valueChanged, this, [this] { refresh(); });
		executable->setText(findExecutable());
		saveTimer.setSingleShot(true);
		saveTimer.setInterval(500);
		connect(&saveTimer, &QTimer::timeout, this, [this] { applyPlayerOptions(false); });
		auto scheduleSave = [this] {
			saveTimer.start();
		};
		for (auto *field : {executable, path, mdnsName, playerTitle, playerBackground})
			connect(field, &QLineEdit::textChanged, this, scheduleSave);
		for (auto *field :
		     {httpPort, icePort, viewerPort, bitrate, width, height, playerVolume, monitorPort, statsInterval})
			connect(field, &QSpinBox::valueChanged, this, scheduleSave);
		for (auto *field : {encoder, networkInterface, protocol, resolutionMode, playerFit})
			connect(field, &QComboBox::currentIndexChanged, this, scheduleSave);
		for (auto *field : {lan, mdnsEnabled, playerSound, playerAutoplay, playerControls, playerLive,
				    playerStatus, playerReconnect, monitorEnabled})
			connect(field, &QCheckBox::toggled, this, [this] { applyPlayerOptions(false); });
		connect(browse, &QPushButton::clicked, this, [this] {
			if (server.state() != QProcess::NotRunning || package->state() != QProcess::NotRunning)
				return;
			const auto f = QFileDialog::getOpenFileName(this, "MediaMTX", {}, Platform::executableFilter());
			if (!f.isEmpty()) {
				executable->setText(f);
				save();
			}
		});
		connect(install, &QPushButton::clicked, this, [this] { runPackage(false); });
		connect(upgrade, &QPushButton::clicked, this, [this] { runPackage(true); });
		connect(serverStart, &QPushButton::clicked, this, [this] { startServer(); });
		connect(serverStop, &QPushButton::clicked, this, [this] { stopServer(); });
		connect(open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl(url->text())); });
		connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(url->text()); });
		connect(folder, &QPushButton::clicked, this,
			[this] { QDesktopServices::openUrl(QUrl::fromLocalFile(directory)); });
		server.setProcessChannelMode(QProcess::MergedChannels);
		connect(&mdns, &MdnsPublisher::message, this, [this](const QString &text) { log(text); });
		connect(&mdns, &MdnsPublisher::changed, this, [this] { refresh(); });
		connect(&server, &QProcess::started, this, [this] {
			if (!sessionRequested || stoppingServer) {
				terminateServer();
				return;
			}
			if (lan->isChecked() && mdnsEnabled->isChecked())
				mdns.start(mdnsName->text(), networkInterface->currentData().toInt());
			if (monitorEnabled->isChecked()) {
				clientsTimer.start(statsInterval->value() * 1000);
				pollClients();
			}
			readinessClock.start();
			readinessTimer.start();
			probeReadiness();
		});
		package->setProcessChannelMode(QProcess::MergedChannels);
		connect(&server, &QProcess::readyReadStandardOutput, this, [this] { readLogs(server, serverBuffer); });
		connect(package, &QProcess::readyReadStandardOutput, this,
			[this] { readLogs(*package, packageBuffer); });
		connect(&server, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
			if (error == QProcess::FailedToStart) {
				clientsTimer.stop();
				apiClients = {};
				pendingApiClients = {};
				if (clientsReply)
					clientsReply->abort();
				viewer.stop();
				mdns.stop();
				sessionRequested = false;
				readinessTimer.stop();
				++sessionGeneration;
				if (readinessReply)
					readinessReply->abort();
				if (!stoppingServer)
					sessionError = I18n::text("ServerPrefix") + server.errorString();
				stoppingServer = false;
			}
			if (!stoppingServer)
				log(I18n::text("ServerPrefix") + server.errorString());
			refresh();
		});
		connect(package, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
			log("WinGet: " + package->errorString());
			refresh();
		});
		connect(&server, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
			[this](int code, QProcess::ExitStatus) {
				if (sessionRequested)
					sessionError = I18n::text("ServerUnexpectedStop");
				sessionRequested = false;
				++sessionGeneration;
				readinessTimer.stop();
				clientsTimer.stop();
				apiClients = {};
				pendingApiClients = {};
				if (clientsReply)
					clientsReply->abort();
				if (readinessReply)
					readinessReply->abort();
				viewer.stop();
				mdns.stop();
				readLogs(server, serverBuffer);
				log(stoppingServer ? I18n::text("ServerStopped")
						   : QString(I18n::text("ServerStoppedCode")).arg(code));
				stoppingServer = false;
				if (output)
					obs_output_stop(output);
				refresh();
			});
		connect(package, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
			[this](int code, QProcess::ExitStatus) {
				readLogs(*package, packageBuffer);
				log(QString(I18n::text("PackageExit")).arg(code));
				executable->clear();
				executable->setText(findExecutable());
				save();
				refresh();
			});
		connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
		connect(&clientsTimer, &QTimer::timeout, this, [this] { pollClients(); });
		readinessTimer.setInterval(250);
		connect(&readinessTimer, &QTimer::timeout, this, [this] { probeReadiness(); });
		timer.start(1000);
		log(I18n::text("Requirements"));
		refresh();
	}
	void onFrontendReady()
	{
		if (autoStart->isChecked())
			QTimer::singleShot(0, this, [this] {
				if (!autoStart->isChecked())
					return;
				log(I18n::text("AutoStarting"));
				startServer();
			});
	}
	~Panel() override
	{
		saveTimer.stop();
		timer.stop();
		clientsTimer.stop();
		sessionRequested = false;
		++sessionGeneration;
		if (clientsReply)
			clientsReply->abort();
		readinessTimer.stop();
		if (readinessReply)
			readinessReply->abort();
		disconnect(&mdns, nullptr, this, nullptr);
		mdns.stop();
		viewer.stop();
		save();
		if (output) {
			obs_output_force_stop(output);
			releaseOutput();
		}
		if (server.state() != QProcess::NotRunning) {
			server.kill();
			server.waitForFinished(3000);
		}
		// Do not kill an installation mid-flight. Closing OBS is handled by the frontend callback.
		disconnect(package, nullptr, this, nullptr);
		if (package->state() == QProcess::NotRunning)
			delete package;
		else
			connect(package, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), package,
				&QObject::deleteLater);
	}
};
QPointer<Panel> panel;
void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && panel) {
		panel->onFrontendReady();
	} else if (event == OBS_FRONTEND_EVENT_EXIT && panel) {
		delete panel;
		panel = nullptr;
	}
}
} // namespace

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	return true;
}
void obs_module_post_load(void)
{
	// OBS restores its saved dock layout after module post-load, before FINISHED_LOADING.
	// The stable dock id must already exist when that restoration takes place.
	if (panel)
		return;
	char *catalog = obs_module_file("locale/en-US.ini");
	const auto catalogDirectory = QFileInfo(QString::fromUtf8(catalog ? catalog : "")).absolutePath();
	bfree(catalog);
	if (!I18n::initialize(QString::fromUtf8(obs_get_locale()), QLocale::system().uiLanguages(), catalogDirectory)) {
		blog(LOG_ERROR, "[obs-mediamtx] Missing translation catalog: %s",
		     catalogDirectory.toUtf8().constData());
		return;
	}
	panel = new Panel;
	if (!obs_frontend_add_dock_by_id(dockId, "VillaCast", panel)) {
		delete panel;
		panel = nullptr;
	}
}
void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	if (panel) {
		delete panel;
		panel = nullptr;
	}
}
