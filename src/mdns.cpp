#include "i18n.hpp"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "mdns.hpp"
#include "mdns-socket.hpp"
#include <QDateTime>
#include <QNetworkDatagram>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <algorithm>

namespace {
const QHostAddress group(QStringLiteral("224.0.0.251"));
constexpr quint16 mdnsPort = 5353;
void u16(QByteArray &b, quint16 value)
{
	b.append(char(value >> 8));
	b.append(char(value));
}
void u32(QByteArray &b, quint32 value)
{
	u16(b, quint16(value >> 16));
	u16(b, quint16(value));
}
bool read16(const QByteArray &b, int &p, quint16 &value)
{
	if (p < 0 || p + 2 > b.size())
		return false;
	value = quint16((quint8(b[p]) << 8) | quint8(b[p + 1]));
	p += 2;
	return true;
}
bool readName(const QByteArray &b, int &p, QByteArray &result)
{
	int cursor = p, after = -1;
	result.clear();
	for (int hops = 0; hops < 128; ++hops) {
		if (cursor < 0 || cursor >= b.size())
			return false;
		const auto length = quint8(b[cursor++]);
		if ((length & 0xc0) == 0xc0) {
			if (cursor >= b.size())
				return false;
			if (after < 0)
				after = cursor + 1;
			cursor = ((length & 0x3f) << 8) | quint8(b[cursor]);
			continue;
		}
		if (length & 0xc0 || length > 63 || cursor + length > b.size())
			return false;
		if (!length) {
			p = after < 0 ? cursor : after;
			result = result.toLower();
			return true;
		}
		if (!result.isEmpty())
			result.append('.');
		result.append(b.mid(cursor, length));
		cursor += length;
		if (result.size() > 253)
			return false;
	}
	return false;
}
QByteArray wireName(const QByteArray &name)
{
	QByteArray result;
	for (const auto &label : name.split('.')) {
		result.append(char(label.size()));
		result.append(label);
	}
	result.append(char(0));
	return result;
}
QByteArray ipBytes(const QHostAddress &address)
{
	QByteArray result;
	u32(result, address.toIPv4Address());
	return result;
}
void addressRecord(QByteArray &b, const QByteArray &name, const QHostAddress &address, quint32 ttl, bool flush)
{
	b.append(wireName(name));
	u16(b, 1);
	u16(b, flush ? 0x8001 : 1);
	u32(b, ttl);
	u16(b, 4);
	b.append(ipBytes(address));
}
bool records(const QByteArray &b, int &p, int count, QList<MdnsWire::Record> &result)
{
	for (int i = 0; i < count; ++i) {
		MdnsWire::Record r;
		quint16 high, low, size;
		if (!readName(b, p, r.name) || !read16(b, p, r.type) || !read16(b, p, r.klass) || !read16(b, p, high) ||
		    !read16(b, p, low) || !read16(b, p, size) || p + size > b.size())
			return false;
		r.ttl = (quint32(high) << 16) | low;
		r.data = b.mid(p, size);
		p += size;
		result.append(r);
	}
	return true;
}
} // namespace

bool MdnsWire::validLabel(const QString &label)
{
	return QRegularExpression("^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$").match(label).hasMatch();
}
bool MdnsWire::parse(const QByteArray &packet, Message &message)
{
	message = {};
	if (packet.size() < 12 || packet.size() > 9000)
		return false;
	int p = 0;
	quint16 q, a, ns, ar;
	if (!read16(packet, p, message.id) || !read16(packet, p, message.flags) || !read16(packet, p, q) ||
	    !read16(packet, p, a) || !read16(packet, p, ns) || !read16(packet, p, ar) || (message.flags & 0x7800) ||
	    q > 64 || int(a) + ns + ar > 128)
		return false;
	for (int i = 0; i < q; ++i) {
		Question question;
		if (!readName(packet, p, question.name) || !read16(packet, p, question.type) ||
		    !read16(packet, p, question.klass))
			return false;
		message.questions.append(question);
	}
	return records(packet, p, a, message.answers) && records(packet, p, ns, message.authorities) &&
	       records(packet, p, ar, message.additional);
}
QByteArray MdnsWire::probe(const QByteArray &name, const QHostAddress &address)
{
	QByteArray b;
	u16(b, 0);
	u16(b, 0);
	u16(b, 1);
	u16(b, 0);
	u16(b, 1);
	u16(b, 0);
	b.append(wireName(name));
	u16(b, 255);
	u16(b, 0x8001);
	addressRecord(b, name, address, 120, false);
	return b;
}
QByteArray MdnsWire::answer(const QByteArray &name, const QHostAddress &address, quint32 ttl, quint16 id,
			    const QList<Question> &questions, bool flush)
{
	QByteArray b;
	u16(b, id);
	u16(b, 0x8400);
	u16(b, quint16(questions.size()));
	u16(b, 1);
	u16(b, 0);
	u16(b, 1);
	for (const auto &q : questions) {
		b.append(wireName(q.name));
		u16(b, q.type);
		u16(b, q.klass & 0x7fff);
	}
	addressRecord(b, name, address, ttl, flush);
	// NSEC: this hostname has an A record but no AAAA record (RFC 6762 §6.1).
	b.append(wireName(name));
	u16(b, 47);
	u16(b, flush ? 0x8001 : 1);
	u32(b, ttl);
	const QByteArray data = wireName(name) + QByteArray::fromHex("000140");
	u16(b, quint16(data.size()));
	b.append(data);
	return b;
}

QList<QNetworkInterface> MdnsPublisher::interfaces()
{
	QList<QNetworkInterface> result;
	for (const auto &iface : QNetworkInterface::allInterfaces()) {
		const auto flags = iface.flags();
		if (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::CanMulticast) &&
		    !flags.testFlag(QNetworkInterface::IsLoopBack) && !addressFor(iface).isNull())
			result.append(iface);
	}
	std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
		auto score = [](const auto &i) {
			return i.type() == QNetworkInterface::Ethernet ? 2
			       : i.type() == QNetworkInterface::Wifi   ? 1
								       : 0;
		};
		return score(a) > score(b);
	});
	return result;
}
QHostAddress MdnsPublisher::addressFor(const QNetworkInterface &iface)
{
	for (const auto &entry : iface.addressEntries()) {
		const auto ip = entry.ip();
		if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() &&
		    !ip.toString().startsWith("169.254."))
			return ip;
	}
	return {};
}
MdnsPublisher::MdnsPublisher(QObject *parent) : QObject(parent)
{
	networkTimer.setInterval(5000);
	announceTimer.setInterval(60000);
	probeTimer.setSingleShot(true);
	connect(&networkTimer, &QTimer::timeout, this, &MdnsPublisher::updateNetwork);
	connect(&probeTimer, &QTimer::timeout, this, &MdnsPublisher::probeStep);
	connect(&announceTimer, &QTimer::timeout, this, [this] {
		if (active)
			multicast(MdnsWire::answer(name, address));
	});
	connect(&socket, &QUdpSocket::readyRead, this, &MdnsPublisher::read);
}
MdnsPublisher::~MdnsPublisher()
{
	stop();
}
void MdnsPublisher::status(const QString &text)
{
	if (text == stateText)
		return;
	stateText = text;
	emit message(text);
	emit changed();
}
void MdnsPublisher::start(const QString &requestedLabel, int interfaceIndex)
{
	stop();
	label = requestedLabel.trimmed().toLower();
	if (!MdnsWire::validLabel(label)) {
		status(I18n::text("MdnsInvalid"));
		return;
	}
	requested = true;
	conflict = false;
	requestedInterface = interfaceIndex;
	name = label.toLatin1() + ".local";
	networkTimer.start();
	updateNetwork();
}
void MdnsPublisher::withdraw()
{
	++generation;
	if (active && socket.state() == QAbstractSocket::BoundState)
		multicast(MdnsWire::answer(name, address, 0));
	active = probing = false;
	probeTimer.stop();
	announceTimer.stop();
	socket.close();
	address.clear();
	network = {};
	lastDefense = lastMulticast = 0;
	emit changed();
}
void MdnsPublisher::stop()
{
	stateText = I18n::text("MdnsStopped");
	requested = false;
	networkTimer.stop();
	withdraw();
}
bool MdnsPublisher::multicast(const QByteArray &packet)
{
	return socket.writeDatagram(packet, group, mdnsPort) == packet.size();
}
void MdnsPublisher::updateNetwork()
{
	if (!requested || conflict)
		return;
	QNetworkInterface candidate;
	const auto available = interfaces();
	for (const auto &iface : available)
		if (!requestedInterface || iface.index() == requestedInterface) {
			candidate = iface;
			break;
		}
	const auto ip = addressFor(candidate);
	if (candidate.isValid() && candidate.index() == network.index() && address == ip &&
	    socket.state() == QAbstractSocket::BoundState)
		return;
	withdraw();
	if (!candidate.isValid() || ip.isNull()) {
		status(I18n::text("MdnsWaiting"));
		return;
	}
	if (!socket.bind(QHostAddress::AnyIPv4, mdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
		status(I18n::text("MdnsBindFailed") + socket.errorString());
		return;
	}
	// Qt's per-datagram IPv4 hop-limit metadata is unsupported by some Windows
	// providers. Set unicast TTL on the socket instead (multicast has its own option).
	if (!MdnsSocket::setIpv4Ttl(socket.socketDescriptor(), 255)) {
		withdraw();
		status(I18n::text("MdnsTtlFailed"));
		return;
	}
	network = candidate;
	address = ip;
	socket.setMulticastInterface(network);
	socket.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
	socket.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
	if (!socket.joinMulticastGroup(group, network)) {
		const auto error = socket.errorString();
		withdraw();
		status(I18n::text("MdnsJoinFailed") + error);
		return;
	}
	probing = true;
	probeCount = 0;
	status(I18n::text("MdnsChecking") + QString::fromLatin1(name) + I18n::text("OnInterface") +
	       network.humanReadableName());
	probeTimer.start(int(QRandomGenerator::global()->bounded(250u)));
}
void MdnsPublisher::probeStep()
{
	if (!probing)
		return;
	if (probeCount++ < 3) {
		if (!multicast(MdnsWire::probe(name, address))) {
			const auto error = socket.errorString();
			withdraw();
			status(I18n::text("MdnsSendFailed") + error);
			return;
		}
		probeTimer.start(250);
		return;
	}
	probing = false;
	active = true;
	multicast(MdnsWire::answer(name, address));
	announceTimer.start();
	const auto currentGeneration = generation;
	QTimer::singleShot(1000, this, [this, currentGeneration] {
		if (active && generation == currentGeneration)
			multicast(MdnsWire::answer(name, address));
	});
	status(I18n::text("MdnsAnnounced") + QString::fromLatin1(name) + " → " + address.toString() + " (" +
	       network.humanReadableName() + ")");
}
void MdnsPublisher::failConflict()
{
	withdraw();
	conflict = true;
	status(I18n::text("MdnsName") + QString::fromLatin1(name) + I18n::text("MdnsConflict"));
}
void MdnsPublisher::read()
{
	int processed = 0;
	while (socket.hasPendingDatagrams() && processed++ < 64) {
		const auto datagram = socket.receiveDatagram(9000);
		if (!requested || (!active && !probing))
			continue;
		if (datagram.interfaceIndex() && datagram.interfaceIndex() != uint(network.index()))
			continue;
		const auto sender = datagram.senderAddress();
		if (sender.protocol() != QAbstractSocket::IPv4Protocol)
			continue;
		MdnsWire::Message m;
		if (!MdnsWire::parse(datagram.data(), m))
			continue;
		// mDNS responses must originate at 5353; ignore off-link responses where
		// the platform exposes the received hop limit (RFC 6762 sections 6 and 11).
		if ((m.flags & 0x8000) &&
		    (datagram.senderPort() != mdnsPort || (datagram.hopLimit() >= 0 && datagram.hopLimit() != 255)))
			continue;
		const auto ownData = ipBytes(address);
		auto conflicting = [&](const QList<MdnsWire::Record> &records, bool tieBreak) {
			for (const auto &r : records) {
				if (r.name != name || r.type != 1 || (r.klass & 0x7fff) != 1 || r.data.size() != 4 ||
				    !r.ttl || r.data == ownData)
					continue;
				if (!tieBreak || r.data > ownData)
					return true;
			}
			return false;
		};
		if (m.flags & 0x8000) {
			if (!conflicting(m.answers, false) && !conflicting(m.additional, false))
				continue;
			if (probing) {
				failConflict();
				continue;
			}
			const auto now = QDateTime::currentMSecsSinceEpoch();
			if (lastDefense && now - lastDefense < 1000) {
				failConflict();
				continue;
			}
			lastDefense = now;
			multicast(MdnsWire::answer(name, address));
			continue;
		}
		if (probing) {
			if (conflicting(m.authorities, true))
				failConflict();
			continue;
		}
		bool relevant = false, unicast = datagram.senderPort() != mdnsPort;
		for (const auto &q : m.questions) {
			if (q.name == name && (q.klass & 0x7fff) == 1 &&
			    (q.type == 1 || q.type == 28 || q.type == 255)) {
				relevant = true;
				unicast |= (q.klass & 0x8000) != 0;
			}
		}
		if (!relevant)
			continue;
		bool known = false;
		for (const auto &r : m.answers)
			if (r.name == name && r.type == 1 && r.data == ownData && r.ttl >= 60)
				known = true;
		if (known && !unicast)
			continue;
		if (unicast) {
			const bool legacy = datagram.senderPort() != mdnsPort;
			const auto reply = MdnsWire::answer(name, address, legacy ? 10 : 120, legacy ? m.id : 0,
							    legacy ? m.questions : QList<MdnsWire::Question>{},
							    !legacy);
			socket.writeDatagram(reply, sender, datagram.senderPort());
		} else {
			const auto now = QDateTime::currentMSecsSinceEpoch();
			if (now - lastMulticast < 1000)
				continue;
			lastMulticast = now;
			const auto currentGeneration = generation;
			QTimer::singleShot(20 + int(QRandomGenerator::global()->bounded(100u)), this,
					   [this, currentGeneration] {
						   if (active && generation == currentGeneration)
							   multicast(MdnsWire::answer(name, address));
					   });
		}
	}
	if (socket.hasPendingDatagrams())
		QTimer::singleShot(0, this, &MdnsPublisher::read);
}
