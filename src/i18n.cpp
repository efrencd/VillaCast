// SPDX-License-Identifier: GPL-2.0-or-later
#include "i18n.hpp"
#include <QHash>
#include <QSettings>

namespace {
QHash<QString, QString> messages;
bool useSpanish = false;
bool spanish(const QString &language)
{
	const auto normalized = language.trimmed().toLower().replace('_', '-');
	return normalized == "es" || normalized.startsWith("es-");
}
void load(const QString &filename)
{
	QSettings catalog(filename, QSettings::IniFormat);
	for (const auto &key : catalog.allKeys())
		messages.insert(key, catalog.value(key).toString());
}
} // namespace
QString I18n::languageFor(const QString &obsLanguage, const QStringList &systemLanguages)
{
	if (spanish(obsLanguage))
		return "es-ES";
	if (!systemLanguages.isEmpty() && spanish(systemLanguages.first()))
		return "es-ES";
	return "en-US";
}
bool I18n::initialize(const QString &obsLanguage, const QStringList &systemLanguages, const QString &directory)
{
	messages.clear();
	load(directory + "/en-US.ini");
	const bool loaded = !messages.isEmpty();
	useSpanish = languageFor(obsLanguage, systemLanguages) == "es-ES";
	if (useSpanish)
		load(directory + "/es-ES.ini");
	return loaded;
}
QString I18n::text(const char *key)
{
	return messages.value(QString::fromUtf8(key), QString::fromUtf8(key));
}
bool I18n::isSpanish()
{
	return useSpanish;
}
