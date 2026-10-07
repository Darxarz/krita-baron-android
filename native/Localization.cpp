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
    Q_INIT_RESOURCE(plugin_icons);
    Q_INIT_RESOURCE(tags);
}
namespace {
class JsonTranslator : public QTranslator {
public:
    QJsonObject entries;
    QString translate(const char* context, const char* source, const char*, int) const override {
        const QByteArray name(context);
        if (name != "BaronPanel" && name != "OrchestrionClient" && name != "BaronDocker"
            && name != "BaronUpdates" && name != "GuidancePanel" && name != "JobQueue"
            && name != "ModelCatalog" && name != "InterfaceSettings" && name != "ToggleSwitch"
            && name != "InpaintWidget" && name != "AuthorCredits" && name != "UpscaleWidget")
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
    auto language = settings.value("language", "").toString();
    if (language.isEmpty()) language = QLocale().name().replace('_', '-').toLower();
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
