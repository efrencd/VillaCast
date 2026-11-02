// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QTcpServer>

class ViewerServer final : public QTcpServer {
public:
	explicit ViewerServer(QObject *parent = nullptr);
	bool start(quint16 port, bool lan, quint16 webrtcPort, const QString &path, const QString &templateFile);
	void stop();

private:
	QByteArray html;
};
