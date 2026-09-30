// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QByteArray>
#include <QUrl>
namespace BaronCredentials {
QByteArray load(const QUrl& root);
bool save(const QUrl& root, const QByteArray& token);
void clear();
}
