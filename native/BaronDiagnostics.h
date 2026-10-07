// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QString>
class QWidget;
namespace BaronDiagnostics {
void startSession();
void record(const char* stage, const QJsonObject& fields = {});
QString localReport();
void show(QWidget* parent);
}
