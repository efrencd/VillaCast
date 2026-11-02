#include "i18n.hpp"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform.hpp"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QString Platform::findMediaMtx(const QString &configured)
{
	if (!configured.isEmpty() && QFileInfo(configured).isFile() && QFileInfo(configured).isExecutable())
		return configured;
	const QString onPath = QStandardPaths::findExecutable("mediamtx");
	if (!onPath.isEmpty())
		return onPath;
#ifdef Q_OS_WIN
	QDir packages(qEnvironmentVariable("LOCALAPPDATA") + "/Microsoft/WinGet/Packages");
	for (const auto &entry : packages.entryList({"bluenviron.mediamtx_*"}, QDir::Dirs)) {
		const QString candidate = packages.filePath(entry + "/mediamtx.exe");
		if (QFileInfo(candidate).isExecutable())
			return candidate;
	}
#else
	for (const auto &candidate : {"/opt/homebrew/bin/mediamtx", "/usr/local/bin/mediamtx", "/usr/bin/mediamtx"}) {
		if (QFileInfo(candidate).isExecutable())
			return QString::fromUtf8(candidate);
	}
#endif
	return {};
}

QString Platform::packageManager()
{
#ifdef Q_OS_WIN
	return QStandardPaths::findExecutable("winget.exe");
#else
	return {}; // Future adapters: Homebrew / distribution packages / signed release downloads.
#endif
}

QStringList Platform::packageArguments(bool update)
{
#ifdef Q_OS_WIN
	return {update ? "upgrade" : "install",
		"--id",
		"bluenviron.mediamtx",
		"--exact",
		"--source",
		"winget",
		"--accept-source-agreements",
		"--accept-package-agreements",
		"--disable-interactivity"};
#else
	Q_UNUSED(update)
	return {};
#endif
}

QString Platform::executableFilter()
{
#ifdef Q_OS_WIN
	return "MediaMTX (mediamtx.exe)";
#else
	return I18n::text("AllFiles");
#endif
}

QString Platform::installationHelp()
{
#ifdef Q_OS_WIN
	return I18n::text("WingetMissing");
#else
	return I18n::text("InstallUnsupported");
#endif
}
