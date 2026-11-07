// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QImage>
#include <QJsonObject>
#include <functional>
class OrchestrionClient;
class QNetworkAccessManager;

using KadanceImages = QList<QPair<QString, QImage>>;
QDialog* showKadanceDialog(QWidget* parent, OrchestrionClient* connection, const QJsonObject& context,
    const KadanceImages& images, std::function<QString()> currentPrompt,
    std::function<bool(const QString&)> applyPrompt, QNetworkAccessManager* transport = nullptr);
