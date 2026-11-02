// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QHostAddress>
#include <QNetworkInterface>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>

namespace MdnsWire {
struct Question {
	QByteArray name;
	quint16 type;
	quint16 klass;
};
struct Record {
	QByteArray name;
	quint16 type;
	quint16 klass;
	quint32 ttl;
	QByteArray data;
};
struct Message {
	quint16 id = 0, flags = 0;
	QList<Question> questions;
	QList<Record> answers, authorities, additional;
};
bool parse(const QByteArray &packet, Message &message);
QByteArray probe(const QByteArray &name, const QHostAddress &address);
QByteArray answer(const QByteArray &name, const QHostAddress &address, quint32 ttl = 120, quint16 id = 0,
		  const QList<Question> &questions = {}, bool flush = true);
bool validLabel(const QString &label);
} // namespace MdnsWire

// IPv4 mDNS hostname responder. No system DNS/hosts changes and no background service.
// Publishes only on one selected LAN interface and exists only while MediaMTX is running.
class MdnsPublisher final : public QObject {
	Q_OBJECT
public:
	explicit MdnsPublisher(QObject *parent = nullptr);
	~MdnsPublisher() override;
	void start(const QString &label, int interfaceIndex);
	void stop();
	QString hostName() const { return active ? QString::fromLatin1(name) : QString(); }
	QString description() const { return stateText; }
	static QList<QNetworkInterface> interfaces();
	static QHostAddress addressFor(const QNetworkInterface &iface);
signals:
	void changed();
	void message(const QString &text);

private:
	QUdpSocket socket;
	QTimer networkTimer, probeTimer, announceTimer;
	QString label, stateText;
	QByteArray name;
	QHostAddress address;
	QNetworkInterface network;
	int requestedInterface = 0, probeCount = 0;
	quint64 generation = 0;
	bool requested = false, active = false, probing = false, conflict = false;
	qint64 lastDefense = 0, lastMulticast = 0;
	void updateNetwork();
	void read();
	void probeStep();
	void withdraw();
	void failConflict();
	void status(const QString &text);
	bool multicast(const QByteArray &packet);
};
