// SPDX-License-Identifier: GPL-2.0-or-later
#include "viewer.hpp"
#include "i18n.hpp"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QUrl>
#include <cmath>

ViewerServer::ViewerServer(QObject *parent) : QTcpServer(parent)
{
	setMaxPendingConnections(32);
	connect(this, &QTcpServer::newConnection, this, [this] {
		while (hasPendingConnections()) {
			auto *client = nextPendingConnection();
			if (findChildren<QTcpSocket *>().size() > 64) {
				client->abort();
				client->deleteLater();
				continue;
			}
			connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
			QTimer::singleShot(5000, client, [client] {
				client->abort();
				client->deleteLater();
			});
			connect(client, &QTcpSocket::readyRead, this, [this, client] {
				if (client->property("answered").toBool())
					return;
				auto request = client->property("request").toByteArray() + client->readAll();
				if (request.size() > 12288) {
					client->abort();
					return;
				}
				client->setProperty("request", request);
				const auto end = request.indexOf("\r\n\r\n");
				if (end < 0) {
					if (request.size() > 8192)
						client->abort();
					return;
				}
				if (end > 8192) {
					client->abort();
					return;
				}
				const auto lines = request.left(end).split('\n');
				const auto first = lines.value(0).trimmed().split(' ');
				QHash<QByteArray, QByteArray> headers;
				for (int i = 1; i < lines.size(); ++i) {
					const auto colon = lines[i].indexOf(':');
					if (colon > 0) {
						const auto key = lines[i].left(colon).trimmed().toLower();
						if (headers.contains(key)) {
							client->abort();
							return;
						}
						headers.insert(key, lines[i].mid(colon + 1).trimmed());
					}
				}
				bool lengthOk = true;
				const auto length = headers.contains("content-length")
							    ? headers.value("content-length").toLongLong(&lengthOk)
							    : 0;
				if (!lengthOk || length < 0 || length > 4096 || headers.contains("transfer-encoding")) {
					client->abort();
					return;
				}
				if (request.size() - end - 4 < length)
					return;
				client->setProperty("answered", true);
				const auto target = first.value(1).split('?').first();
				int code = 404;
				QByteArray body = "Not found", type = "text/plain";
				if (first.size() == 3 && (first[0] == "GET" || first[0] == "HEAD") &&
				    (target == "/" || target == "/mystream/" || target == "/index.html")) {
					code = 200;
					type = "text/html";
					body = html;
					auto json = QJsonDocument(options).toJson(QJsonDocument::Compact);
					// Keep a title containing </script> as data, never executable HTML.
					json.replace("<", "\\u003c");
					json.replace(">", "\\u003e");
					json.replace("&", "\\u0026");
					body.replace("__PLAYER_OPTIONS__", json);
				} else if (first.size() == 3 && (first[0] == "GET" || first[0] == "HEAD") &&
					   target == "/favicon.png" && !favicon.isEmpty()) {
					code = 200;
					type = "image/png";
					body = favicon;
				} else if (first.size() == 3 && first[0] == "POST" &&
					   (target == "/telemetry/register" || target == "/telemetry")) {
					type = "application/json";
					body = "{}";
					const QUrl origin(QString::fromUtf8(headers.value("origin")));
					const QUrl expected("http://" + QString::fromUtf8(headers.value("host")));
					const bool sameOrigin = origin.isValid() && origin.scheme() == "http" &&
								origin.host() == expected.host() &&
								origin.port(80) == expected.port(80) &&
								origin.userInfo().isEmpty() && !origin.host().isEmpty();
					if (!sameOrigin || !options.value("monitor").toBool())
						code = 403;
					else if (headers.value("content-type").split(';').first().trimmed() !=
						 "application/json")
						code = 415;
					else {
						pruneReports();
						const auto doc = QJsonDocument::fromJson(request.mid(end + 4, length));
						if (!doc.isObject())
							code = 400;
						else if (target == "/telemetry/register") {
							if (reports.size() >= 1024)
								code = 429;
							else {
								ViewerReport report;
								report.id = QUuid::createUuid().toString(
									QUuid::WithoutBraces);
								report.token = QUuid::createUuid().toString(
									QUuid::WithoutBraces);
								report.address = client->peerAddress().toString();
								report.firstSeen = report.lastSeen =
									QDateTime::currentMSecsSinceEpoch();
								reports.insert(report.id, report);
								code = 200;
								body = QJsonDocument(
									       QJsonObject{{"id", report.id},
											   {"token", report.token}})
									       .toJson(QJsonDocument::Compact);
							}
						} else {
							const auto data = doc.object();
							auto it = reports.find(data.value("id").toString());
							if (it == reports.end() ||
							    data.value("token").toString() != it->token ||
							    it->address != client->peerAddress().toString())
								code = 401;
							else if (data.value("closed").toBool()) {
								reports.erase(it);
								code = 200;
							} else {
								QJsonObject metrics;
								for (const auto &key :
								     {"width", "height", "fps", "bitrate", "loss",
								      "jitter", "rtt", "dropped"}) {
									const auto value = data.value(key);
									if (value.isDouble() &&
									    std::isfinite(value.toDouble()) &&
									    value.toDouble() >= 0 &&
									    value.toDouble() <= 1e9)
										metrics.insert(key, value);
								}
								const auto state = data.value("state").toString();
								if (QStringList{"playing", "paused", "connecting",
										"connected", "disconnected", "failed",
										"closed", "new"}
									    .contains(state))
									metrics.insert("state", state);
								it->metrics = metrics;
								it->reported = true;
								it->lastSeen = QDateTime::currentMSecsSinceEpoch();
								code = 200;
							}
						}
					}
				}
				const QByteArray reason = code == 200   ? "OK"
							  : code == 400 ? "Bad Request"
							  : code == 401 ? "Unauthorized"
							  : code == 403 ? "Forbidden"
							  : code == 415 ? "Unsupported Media Type"
							  : code == 429 ? "Too Many Requests"
									: "Not Found";
				client->write(
					"HTTP/1.1 " + QByteArray::number(code) + " " + reason +
					"\r\nContent-Type: " + type +
					"; charset=utf-8\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: " +
					QByteArray::number(body.size()) + "\r\n\r\n");
				if (first.value(0) != "HEAD")
					client->write(body);
				client->disconnectFromHost();
			});
		}
	});
}
void ViewerServer::setOptions(const QJsonObject &value)
{
	options = value;
	if (!options.value("monitor").toBool())
		reports.clear();
}
void ViewerServer::pruneReports()
{
	const auto oldest = QDateTime::currentMSecsSinceEpoch() -
			    qMax(10000, options.value("statsInterval").toInt(2) * 3000 + 5000);
	for (auto it = reports.begin(); it != reports.end();)
		if (it->lastSeen < oldest)
			it = reports.erase(it);
		else
			++it;
}
QList<ViewerReport> ViewerServer::clientReports()
{
	pruneReports();
	QList<ViewerReport> result;
	for (const auto &report : reports)
		if (report.reported)
			result.append(report);
	return result;
}
bool ViewerServer::start(quint16 port, bool lan, quint16 webrtcPort, const QString &path, const QString &templateFile)
{
	stop();
	QFile file(templateFile);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	html = file.readAll();
	QFile icon(QFileInfo(templateFile).dir().filePath("favicon.png"));
	favicon = icon.open(QIODevice::ReadOnly) ? icon.readAll() : QByteArray();
	html.replace("__WEBRTC_PORT__", QByteArray::number(webrtcPort));
	html.replace("__STREAM_PATH__", path.toUtf8());
	html.replace("__LANG__", I18n::isSpanish() ? "es" : "en");
	return listen(lan ? QHostAddress::Any : QHostAddress::LocalHost, port);
}
void ViewerServer::stop()
{
	close();
	reports.clear();
	for (auto *client : findChildren<QTcpSocket *>()) {
		client->abort();
		client->deleteLater();
	}
}
