// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QTcpServer>
#include <QJsonObject>
#include <QHash>
#include <QList>

struct ViewerReport {
	QString id, token, address;
	qint64 firstSeen = 0, lastSeen = 0;
	bool reported = false;
	QJsonObject metrics;
};

class ViewerServer final : public QTcpServer {
public:
	explicit ViewerServer(QObject *parent = nullptr);
	bool start(quint16 port, bool lan, quint16 webrtcPort, const QString &path, const QString &templateFile);
	void stop();
	void setOptions(const QJsonObject &value);
	QList<ViewerReport> clientReports();

private:
	QByteArray html;
	QJsonObject options;
	QHash<QString, ViewerReport> reports;
	void pruneReports();
};
