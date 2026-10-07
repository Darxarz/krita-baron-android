// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>

class PromptLogic {
public:
    PromptLogic();
    QJsonValue call(const QString& function, const QJsonArray& arguments);
    QJsonArray segments(const QString& text);
    QString error() const { return m_error; }
private:
    QJSEngine m_engine;
    QJSValue m_functions;
    QString m_error;
};
