// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QStringList>

namespace I18n {
QString languageFor(const QString &obsLanguage, const QStringList &systemLanguages);
bool initialize(const QString &obsLanguage, const QStringList &systemLanguages, const QString &directory);
QString text(const char *key);
bool isSpanish();
} // namespace I18n
