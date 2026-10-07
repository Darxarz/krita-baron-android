// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
namespace CloudWorkflow {
QJsonObject prepare(const QJsonObject& input, const QJsonObject& resources, QString* error);
QJsonObject metadata(const QJsonObject& work);
}
