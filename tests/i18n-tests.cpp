// SPDX-License-Identifier: GPL-2.0-or-later
#include "../src/i18n.hpp"
#include <QCoreApplication>
#include <QSettings>
#include <QRegularExpression>
#include <QDebug>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	auto require = [](bool valid, const char *message) {
		if (!valid)
			qFatal("%s", message);
	};
	require(argc == 2, "Catalog directory required");
	const QString directory = QString::fromLocal8Bit(argv[1]);
	for (const auto &locale : {"es", "es-ES", "ES-mx", "es_AR"}) {
		require(I18n::languageFor(locale, {"en-US"}) == "es-ES", "Spanish OBS variant");
		require(I18n::languageFor("en-US", {locale}) == "es-ES", "Spanish system variant");
	}
	require(I18n::languageFor("fr-FR", {"de-DE", "en-US"}) == "en-US", "English fallback");
	require(I18n::languageFor("", {}) == "en-US", "Empty language fallback");
	require(I18n::languageFor("fr-FR", {"en-US", "es-ES"}) == "en-US", "Use the primary system UI language");
	require(I18n::languageFor("estonian", {"en-US"}) == "en-US", "Do not match language prefixes loosely");
	QSettings english(directory + "/en-US.ini", QSettings::IniFormat);
	QSettings spanish(directory + "/es-ES.ini", QSettings::IniFormat);
	require(english.allKeys() == spanish.allKeys(), "Catalog key parity");
	require(english.allKeys().size() > 80, "Complete catalogs");
	QRegularExpression placeholder("%[1-9][0-9]*");
	for (const auto &key : english.allKeys()) {
		const auto en = english.value(key).toString();
		const auto es = spanish.value(key).toString();
		require(!en.isEmpty() && !es.isEmpty(), "Empty translation");
		auto parameters = [&](const QString &value) {
			QStringList list;
			auto matches = placeholder.globalMatch(value);
			while (matches.hasNext())
				list.append(matches.next().captured());
			list.sort();
			return list;
		};
		require(parameters(en) == parameters(es), "Placeholder mismatch");
	}
	require(I18n::initialize("en-US", {"en-US"}, directory), "Load English catalog");
	require(I18n::text("Start") == "Start", "English button");
	require(I18n::initialize("en-US", {"es-MX"}, directory), "Load Spanish system catalog");
	require(I18n::text("Start") == QString::fromUtf8("Iniciar"), "Spanish system overrides English OBS");
	require(I18n::text("StreamHelp").contains('\n'), "Multiline translation");
	QTemporaryDir partial;
	QSettings base(partial.path() + "/en-US.ini", QSettings::IniFormat);
	base.setValue("Start", "Start");
	base.sync();
	require(I18n::initialize("es-ES", {}, partial.path()), "Missing Spanish catalog falls back");
	require(I18n::text("Start") == "Start", "Per-key English fallback");
	qInfo("Language selection, catalog parity, placeholders and fallback passed");
}
