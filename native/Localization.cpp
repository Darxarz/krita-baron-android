// SPDX-License-Identifier: GPL-3.0-or-later
#include "Localization.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
static void initializeBaronResources() {
    Q_INIT_RESOURCE(translations);
}
namespace {
class JsonTranslator : public QTranslator {
public:
    QJsonObject entries;
    QString translate(const char* context, const char* source, const char*, int) const override {
        const QByteArray name(context);
        if (name != "BaronPanel" && name != "OrchestrionClient" && name != "BaronDocker")
            return {};
        return entries.value(QString::fromUtf8(source)).toString();
    }
};
}
void BaronLocalization::install() {
    static JsonTranslator* translator = nullptr;
    if (translator)
        return;
    initializeBaronResources();
    QSettings settings("BaronEdition", "Orchestrion");
    auto language
        = settings.value("language", QLocale().name().replace('_', '-').toLower()).toString();
    QFile file(":/baron/language/" + language + ".json");
    if (!file.exists())
        file.setFileName(":/baron/language/" + language.section('-', 0, 0) + ".json");
    if (!file.open(QIODevice::ReadOnly))
        return;
    translator = new JsonTranslator();
    translator->entries
        = QJsonDocument::fromJson(file.readAll()).object()["translations"].toObject();
    QCoreApplication::installTranslator(translator);
}
