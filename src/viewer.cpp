// SPDX-License-Identifier: GPL-2.0-or-later
#include "viewer.hpp"
#include "i18n.hpp"
#include <QFile>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>

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
				if (request.size() > 8192) {
					client->abort();
					return;
				}
				client->setProperty("request", request);
				if (!request.contains("\r\n\r\n"))
					return;
				client->setProperty("answered", true);
				const auto first = request.left(request.indexOf("\r\n")).split(' ');
				const bool get = first.size() == 3 && (first[0] == "GET" || first[0] == "HEAD");
				const auto target = first.size() == 3 ? first[1].split('?').first() : QByteArray();
				const bool valid = get &&
						   (target == "/" || target == "/mystream/" || target == "/index.html");
				const QByteArray body = valid ? html : QByteArray("Not found");
				QByteArray response = valid ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n";
				response +=
					"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: " +
					QByteArray::number(body.size()) + "\r\n\r\n";
				if (first.size() == 3 && first[0] != "HEAD")
					response += body;
				client->write(response);
				client->disconnectFromHost();
			});
		}
	});
}
bool ViewerServer::start(quint16 port, bool lan, quint16 webrtcPort, const QString &path, const QString &templateFile)
{
	stop();
	QFile file(templateFile);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	html = file.readAll();
	html.replace("__WEBRTC_PORT__", QByteArray::number(webrtcPort));
	html.replace("__STREAM_PATH__", path.toUtf8());
	html.replace("__LANG__", I18n::isSpanish() ? "es" : "en");
	return listen(lan ? QHostAddress::Any : QHostAddress::LocalHost, port);
}
void ViewerServer::stop()
{
	close();
	for (auto *client : findChildren<QTcpSocket *>()) {
		client->abort();
		client->deleteLater();
	}
}
