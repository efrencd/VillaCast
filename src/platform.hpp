// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QStringList>

// Platform boundary: the dock and OBS output do not depend on WinGet or Windows paths.
namespace Platform {
QString findMediaMtx(const QString &configured);
QString packageManager();
QStringList packageArguments(bool update);
QString executableFilter();
QString installationHelp();
} // namespace Platform
