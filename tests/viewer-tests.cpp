// SPDX-License-Identifier: GPL-2.0-or-later
#include "../src/viewer.hpp"
#include "../src/i18n.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QDebug>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	auto check = [](bool valid, const char *message) {
		if (!valid)
			qFatal("%s", message);
	};
	check(argc == 3, "Template and locale directory required");
	I18n::initialize("en-US", {}, QString::fromLocal8Bit(argv[2]));
	ViewerServer server;
	server.setOptions({{"title", "</script><script>alert(1)</script>"}, {"monitor", true}, {"statsInterval", 2}});
	check(server.start(0, false, 18889, "mystream", QString::fromLocal8Bit(argv[1])), "Start viewer");
	const auto base = QString("http://127.0.0.1:%1").arg(server.serverPort());
	QNetworkAccessManager network;
	struct Response {
		int code;
		QByteArray body;
	};
	auto request = [&](const QString &path, const QByteArray &body = QByteArray(),
			   const QString &origin = QString()) {
		QNetworkRequest request(QUrl(base + path));
		request.setRawHeader("Origin", (origin.isNull() ? base : origin).toUtf8());
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		auto *reply = body.isNull() ? network.get(request) : network.post(request, body);
		QEventLoop loop;
		QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		QTimer::singleShot(3000, reply, [reply] {
			if (!reply->isFinished())
				reply->abort();
		});
		loop.exec();
		Response result{reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->readAll()};
		reply->deleteLater();
		return result;
	};
	const auto page = request("/");
	check(page.code == 200, "GET viewer");
	const auto favicon = request("/favicon.png");
	check(favicon.code == 200 && favicon.body.startsWith(QByteArray::fromHex("89504e470d0a1a0a")),
	      "Serve bundled PNG icon");
	check(!page.body.contains("__PLAYER_OPTIONS__") && !page.body.contains("__LANG__"),
	      "Options and language substituted");
	check(!page.body.contains("</script><script>alert(1)") && page.body.contains("\\u003c/script\\u003e"),
	      "Title cannot escape script data");
	check(request("/telemetry/register", "{}", "http://other.example").code == 403,
	      "Cross-origin reports rejected");
	const auto registered = request("/telemetry/register", "{}");
	check(registered.code == 200, "Register player");
	const auto identity = QJsonDocument::fromJson(registered.body).object();
	check(!identity.value("id").toString().isEmpty(), "Player identity");
	check(server.clientReports().isEmpty(), "Unreported registrations are not clients");
	auto report = identity;
	report.insert("state", "playing");
	report.insert("fps", 30);
	report.insert("width", 1280);
	report.insert("height", 720);
	report.insert("loss", 0.25);
	auto posted = request("/telemetry", QJsonDocument(report).toJson());
	check(posted.code == 200, "Report accepted");
	const auto clients = server.clientReports();
	check(clients.size() == 1 && clients[0].metrics.value("fps").toInt() == 30, "Receiver metrics retained");
	report.insert("token", "wrong");
	check(request("/telemetry", QJsonDocument(report).toJson()).code == 401,
	      "Cannot overwrite another player's metrics");
	report = identity;
	report.insert("fps", -10);
	report.insert("state", "<script>");
	check(request("/telemetry", QJsonDocument(report).toJson()).code == 200, "Sanitize metrics");
	check(!server.clientReports()[0].metrics.contains("fps") &&
		      !server.clientReports()[0].metrics.contains("state"),
	      "Invalid metrics and states omitted");
	report = identity;
	report.insert("closed", true);
	check(request("/telemetry", QJsonDocument(report).toJson()).code == 200 && server.clientReports().isEmpty(),
	      "Closing player removes its report");
	server.setOptions({{"monitor", false}});
	check(request("/telemetry/register", "{}").code == 403, "Monitoring can be disabled");
	server.stop();
	check(server.clientReports().isEmpty(), "Stop clears reports");
	qInfo("Viewer configuration escaping, report authentication, origin checks and lifecycle passed");
}
