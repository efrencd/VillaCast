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

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mediamtx", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "MediaMTX server manager with an independent WebRTC output";
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
		    viewerPort->value() == icePort->value()) {
			log(I18n::text("DistinctPorts"));
			return false;
		}
		QTcpServer httpCheck, tcpCheck, viewerCheck;
		QUdpSocket udpCheck;
		const auto bind = QHostAddress::Any;
		if (!httpCheck.listen(bind, static_cast<quint16>(httpPort->value())) ||
		    !tcpCheck.listen(bind, static_cast<quint16>(icePort->value())) ||
		    !udpCheck.bind(bind, static_cast<quint16>(icePort->value())) ||
		    !viewerCheck.listen(bind, static_cast<quint16>(viewerPort->value()))) {
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
		obs_data_set_string(vs, "profile", "baseline");
		obs_data_set_string(vs, "preset", encoder->currentData().toString() == "obs_x264" ? "veryfast" : "p4");
		obs_data_set_string(vs, "tune",
				    encoder->currentData().toString() == "obs_x264" ? "zerolatency" : "ull");
		const auto encoderId = encoder->currentData().toString().toUtf8();
		video = obs_video_encoder_create(encoderId.constData(), "MediaMTX video", vs, nullptr);
		obs_data_release(vs);
		auto *as = obs_data_create();
		obs_data_set_int(as, "bitrate", 128);
		audio = obs_audio_encoder_create("ffmpeg_opus", "MediaMTX audio", as, 0, nullptr);
		obs_data_release(as);
		auto *ss = obs_data_create();
		const auto endpoint =
			QString("http://127.0.0.1:%1/%2/whip").arg(httpPort->value()).arg(path->text()).toUtf8();
		obs_data_set_string(ss, "server", endpoint.constData());
		service = obs_service_create("whip_custom", "MediaMTX local", ss, nullptr);
		obs_data_release(ss);
		output = obs_output_create("whip_output", "MediaMTX independent WebRTC", nullptr, nullptr);
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
		tabs->addTab(controls, I18n::text("StreamTab"));
		auto *configuration = new QWidget;
		auto *configurationScroll = new QScrollArea;
		configurationScroll->setWidgetResizable(true);
		configurationScroll->setWidget(configuration);
		tabs->addTab(configurationScroll, I18n::text("Settings"));
		auto *about = new QWidget;
		tabs->addTab(about, I18n::text("About"));
		auto *aboutLayout = new QVBoxLayout(about);
		auto *aboutTitle = new QLabel("OBS MediaMTX");
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
			QString("<a style=\"color:%1\" href=\"https://github.com/efrencd/OBS-MediaMTX\">GitHub · OBS-MediaMTX</a>")
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
		layout->addWidget(logs);
		auto s = settings();
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
		timer.stop();
		sessionRequested = false;
		++sessionGeneration;
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
	if (!obs_frontend_add_dock_by_id(dockId, "MediaMTX · WebRTC", panel)) {
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
