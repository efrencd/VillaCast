// SPDX-License-Identifier: GPL-2.0-or-later
#include "../src/mdns.hpp"
#include "../src/i18n.hpp"
#include <QCoreApplication>
#include <QRandomGenerator>
#include <cstdio>
#include <cstdlib>

static void check(bool condition, const char *description)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", description);
		std::exit(1);
	}
}
int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	I18n::initialize("es-ES", {}, TEST_LOCALE_DIR);
	const auto arguments = app.arguments();
	if (arguments.contains("--serve")) {
		MdnsPublisher publisher;
		QObject::connect(&publisher, &MdnsPublisher::message, [](const QString &text) {
			printf("%s\n", text.toUtf8().constData());
			fflush(stdout);
		});
		const auto label = arguments.value(2, "obs-mdns-integration-test");
		publisher.start(label, arguments.value(3, "0").toInt());
		const int duration = qBound(5, arguments.value(4, "60").toInt(), 60);
		QTimer::singleShot(duration * 1000, &app, [&] {
			publisher.stop();
			app.quit();
		});
		return app.exec();
	}
	using namespace MdnsWire;
	check(validLabel("obs-aula-1") && validLabel("a") && validLabel(QString(63, 'a')), "valid hostname labels");
	for (const auto &label : {"", "-obs", "obs-", "obs.local", "OBS", "obs aula", "../obs"})
		check(!validLabel(label), "reject invalid labels");
	check(!validLabel(QString(64, 'a')), "reject long labels");
	const QByteArray host = "obs-test.local";
	const QHostAddress ip("192.168.1.42");
	Message m;
	check(parse(probe(host, ip), m), "parse probe");
	check(m.questions.size() == 1 && m.questions[0].type == 255 && m.questions[0].klass == 0x8001 &&
		      m.questions[0].name == host,
	      "probe ANY and QU");
	check(m.authorities.size() == 1 && m.authorities[0].data.toHex() == "c0a8012a" && m.authorities[0].klass == 1,
	      "probe authority IPv4 network order");
	check(parse(answer(host, ip), m), "parse answer");
	check(m.flags == 0x8400 && m.answers.size() == 1 && m.answers[0].ttl == 120 && m.answers[0].klass == 0x8001,
	      "authoritative cache-flush response");
	check(m.additional.size() == 1 && m.additional[0].type == 47, "NSEC denies nonexistent IPv6");
	check(parse(answer(host, ip, 0), m) && m.answers[0].ttl == 0, "goodbye TTL zero");
	const QList<Question> qs{{host, 1, 1}};
	check(parse(answer(host, ip, 10, 1234, qs, false), m) && m.id == 1234 && m.questions.size() == 1 &&
		      m.answers[0].klass == 1 && m.answers[0].ttl == 10,
	      "legacy unicast echoes ID and question");
	// A response compressed back to the question name.
	QByteArray compressed = QByteArray::fromHex(
		"000084000001000100000000086f62732d74657374056c6f63616c0000010001c00c00018001000000780004c0a8012a");
	check(parse(compressed, m) && m.answers[0].name == host, "compressed names");
	for (int size = 0; size < compressed.size(); ++size)
		check(!parse(compressed.left(size), m), "reject every truncated compressed packet");
	check(!parse(QByteArray::fromHex("000000000001000000000000c00c00010001"), m), "reject pointer cycle");
	check(!parse(QByteArray::fromHex("000000000001000000000000c0ff00010001"), m), "reject pointer out of bounds");
	check(!parse(QByteArray::fromHex("000000000041000000000000"), m), "bound question count");
	check(!parse(QByteArray(9001, char(0)), m), "bound packet size");
	for (int i = 0; i < 10000; ++i) {
		QByteArray packet(int(QRandomGenerator::global()->bounded(600u)), Qt::Uninitialized);
		for (auto &byte : packet)
			byte = char(QRandomGenerator::global()->generate());
		parse(packet, m);
	}
	printf("PASS: mDNS wire format, validation, compression, truncation, limits, 10000 malformed datagrams\n");
	return 0;
}
