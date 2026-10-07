// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include "UpscaleWidget.h"
#include "GuidancePanel.h"
#include "PluginUi.h"
#include "PromptActions.h"
#include "ModelCatalog.h"
#include "Localization.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSettings>
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QToolButton>
#include <QSlider>
#include <QCheckBox>
class PreviewHost : public CanvasHost {
public:
    QString documentId() const override { return "baron-ui-preview"; }
    bool selected=false;
    QMap<QString, QByteArray> annotations;
    QByteArray annotation(const QString& key) const override { return annotations.value(key); }
    void setAnnotation(const QString& key, const QByteArray& data) override { annotations[key] = data; }
    CanvasSnapshot capture(bool, QString*) override {
        QImage image(1024, 1024, QImage::Format_ARGB32);
        image.fill(Qt::white);
        return { image, {}, "preview-document" };
    }
    QRect imageBounds(bool mask) const override { return selected&&mask?QRect(80,80,512,512):QRect(0,0,1024,1024); }
    bool apply(const QString&, const QImage& image, const QImage& mask, const QString&,
        QString* error) override {
        const auto path = QFileDialog::getSaveFileName(
            nullptr, "Save preview result", "result.png", "PNG (*.png)");
        if (path.isEmpty()) {
            *error = "Save cancelled";
            return false;
        }
        auto result = image.convertToFormat(QImage::Format_ARGB32);
        if (!mask.isNull()) {
            for (int y = 0; y < result.height(); ++y)
                for (int x = 0; x < result.width(); ++x) {
                    const auto pixel = result.pixel(x, y);
                    result.setPixel(x, y,
                        qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), qGray(mask.pixel(x, y))));
                }
        }
        return result.save(path);
    }
};
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir styleDemoSettings;
    if (app.arguments().contains("--styles-demo") || app.arguments().contains("--catalog-preview") || app.arguments().contains("--inpaint-preview")) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, styleDemoSettings.path());
        QSettings settings("BaronEdition", "Orchestrion");
        settings.setValue("styleId", "demo-generate");
        settings.setValue("stylePresets", QJsonDocument(QJsonArray {
            QJsonObject { { "id", "demo-generate" }, { "name", "Illustration" },
                { "checkpoints", QJsonArray { "qwen-demo" } }, { "linked_edit_style", "demo-edit" },
                { "style_prompt", "best quality, {prompt}" }, { "sampler", "Default - DPM++ 2M" } },
            QJsonObject { { "id", "demo-edit" }, { "name", "General Editing" },
                { "checkpoints", QJsonArray { "qwen-demo" } } } }).toJson());
    }
    app.setFont(QFont("DejaVu Sans", 10));
    if (app.arguments().contains("--ru"))
        QLocale::setDefault(QLocale("ru_RU"));
#ifdef Q_OS_WIN
    if (QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf") >= 0)
        app.setFont(QFont("Segoe UI", 10));
#endif
    app.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(64, 64, 64));
    palette.setColor(QPalette::WindowText, QColor(200, 200, 200));
    palette.setColor(QPalette::Base, QColor(56, 56, 56));
    palette.setColor(QPalette::AlternateBase, QColor(64, 64, 64));
    palette.setColor(QPalette::Text, QColor(200, 200, 200));
    palette.setColor(QPalette::Button, QColor(72, 72, 72));
    palette.setColor(QPalette::ButtonText, QColor(200, 200, 200));
    palette.setColor(QPalette::Dark, QColor(44, 44, 44));
    palette.setColor(QPalette::Light, QColor(100, 100, 100));
    palette.setColor(QPalette::Highlight, QColor(77, 119, 151));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    if (!app.arguments().contains("--light"))
        app.setPalette(palette);
    if (app.arguments().contains("--catalog-preview")) {
        BaronLocalization::install();
        ModelCatalog catalog; catalog.resize(1280,850);
        QJsonArray models;
        const QStringList names {"Fantasy portrait", "Watercolor ink", "Soft cinematic light", "Character identity", "Painterly illustration", "Detailed fabrics"};
        for(int i=0;i<names.size();++i)models.append(QJsonObject{{"name",QString(QString(i%2 ? "Styles/" : "Characters/")+names[i]+".safetensors")},
            {"title",names[i]},{"kind",i==0 ? "checkpoint" : "lora"},{"family",i%2 ? "SDXL" : "Qwen"},
            {"triggers",QJsonArray{names[i].toLower()}},{"tags",QJsonArray{i%2 ? "illustration" : "character", "fantasy"}}});
        catalog.setModels(models);
        for(int i=0;i<catalog.gallery()->count();++i) {
            QImage image(160,160,QImage::Format_RGB32); QPainter painter(&image);
            QLinearGradient gradient(0,0,160,160); gradient.setColorAt(0,QColor::fromHsv(i*43,110,180)); gradient.setColorAt(1,QColor::fromHsv(i*43+30,180,70));
            painter.fillRect(image.rect(),gradient); painter.setPen(QPen(QColor(255,255,255,160),2));
            painter.drawEllipse(55,25,50,50); painter.drawRoundedRect(38,82,84,65,28,28); painter.end();
            catalog.gallery()->item(i)->setIcon(QIcon(QPixmap::fromImage(image)));
        }
        catalog.setMetadataProvider([](const QString&,const QString&,ModelCatalog::MetadataCallback done,bool) {
            done({{"creator", "Example artist"},{"versionName","v2"},{"modelDescription","<p>Use strength 0.6–0.8 for a gentle effect. Keep the trigger word at the beginning of your prompt.</p>"},
                {"recommendedSettings",QJsonObject{{"strength",0.7},{"steps",28}}},{"images",QJsonArray{QJsonObject{{"meta",QJsonObject{{"sampler","DPM++ 2M"},{"steps",28},{"cfgScale",5.5}}}}}}},{});
        });
        catalog.show();
        QTimer::singleShot(250,&catalog,[&] {
            if(app.arguments().contains("--model-details")) {
                QTimer::singleShot(100,&catalog,[&] {
                    auto dialog=catalog.findChild<QDialog*>("modelInformationDialog");
                    if(dialog) { dialog->grab().save(app.arguments().last()); dialog->accept(); }
                    app.quit();
                });
                catalog.showInformation(catalog.gallery()->item(1)->data(Qt::UserRole).toJsonObject());
            } else app.exit(catalog.grab().save(app.arguments().last()) ? 0 : 1);
        });
        return app.exec();
    }
    if (app.arguments().contains("--completion")) {
        QWidget completionHost;
        completionHost.resize(380, 420);
        PromptEditor editor(&completionHost, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(8, 8, 364, 150);
        completionHost.show(); completionHost.activateWindow(); editor.setFocus();
        app.processEvents();
        editor.setPlainText("portrait, blue ey"); editor.moveCursor(QTextCursor::End);
        QTimer::singleShot(150, &app, [&] {
            app.exit(completionHost.grab().save(app.arguments().last()) ? 0 : 1);
        });
        return app.exec();
    }
    PreviewHost host;host.selected=app.arguments().contains("--inpaint-preview");
    BaronPanel panel(&host);
    panel.documentChanged();
    panel.setWindowTitle("Baron native panel — UI preview (not Krita)");
    panel.resize(456, 900);
    if (app.arguments().contains("--upscale")) {
        panel.findChild<OrchestrionClient*>()->modelsReady({
            { "items", QJsonArray { QJsonObject { { "name", "demo.safetensors" },
                { "title", "XL Illustration" }, { "architecture", "sdxl" }, { "family", "sdxl" }, { "kind", "checkpoint" } } } },
            { "resources", QJsonObject { { "upscalers", QJsonArray { "4x_NMKD-Superscale-SP_178000_G.pth", "OmniSR_X4_DIV2K.safetensors" } },
                { "resources", QJsonObject { { "controlnet-blur-sdxl", "tile.pth" } } } } } });
        for (auto action : panel.findChild<QToolButton*>("workspaceSelect")->menu()->actions())
            if (action->data() == "upscale") action->trigger();
    }
    if (app.arguments().contains("--narrow"))
        panel.resize(320, 900);
    if (app.arguments().contains("--demo-catalog")) {
        panel.findChild<OrchestrionClient*>()->modelsReady({ { "items",
            QJsonArray { QJsonObject { { "name", "qwen-demo" }, { "title", "Qwen Image Edit 2.1" },
                             { "family", "Qwen" }, { "kind", "checkpoint" } },
                QJsonObject { { "name", "krea-demo" }, { "title", "Krea 2 Turbo" },
                    { "family", "Krea 2" }, { "kind", "diffusion_model" } },
                QJsonObject { { "name", "identity-demo" }, { "title", "Identity reference" },
                    { "family", "Krea 2" }, { "kind", "lora" } },
                QJsonObject { { "name", "style-demo" }, { "title", "Illustration style" },
                    { "family", "Krea 2" }, { "kind", "lora" } } } } });
    }
    if (app.arguments().contains("--controls")) {
        panel.findChild<QToolButton*>("addControlLayer")->menu()->actions().first()->trigger();
        if (app.arguments().contains("--control-map")) {
            auto mode = panel.findChild<QComboBox*>("controlMode");
            mode->setCurrentIndex(mode->findData("depth"));
        }
        if (app.arguments().contains("--qwen"))
            panel.findChild<GuidancePanel*>()->setArchitecture("qwen2");
        panel.findChild<QPlainTextEdit*>("positivePrompt")
            ->setPlainText(
                "Change the clothes while preserving identity, <lora:identity-demo:0.8>");
        if (app.arguments().contains("--control-options"))
            panel.findChild<QToolButton*>("controlAdvanced")->setChecked(true);
    }
    if (app.arguments().contains("--region"))
        panel.findChild<QToolButton*>("addRegion")->click();
    if (app.arguments().contains("--history")) {
        auto history = panel.findChild<QListWidget*>("resultHistory");
        history->clear();
        auto heading = new QListWidgetItem("19:48 - 43% - character reference", history);
        heading->setFlags(Qt::NoItemFlags);
        heading->setData(Qt::UserRole, QVariantMap { { "header", true } });
        heading->setSizeHint(QSize(9999, panel.fontMetrics().lineSpacing() + 4));
        heading->setTextAlignment(Qt::AlignLeft);
        for (int i = 0; i < 8; ++i) {
            if (i == 4) {
                auto next = new QListWidgetItem("19:52 - another reference", history);
                next->setFlags(Qt::NoItemFlags);
                next->setData(Qt::UserRole, QVariantMap { { "header", true } });
                next->setSizeHint(QSize(9999, panel.fontMetrics().lineSpacing() + 4));
                next->setTextAlignment(Qt::AlignLeft);
            }
            QImage result(256, 256, QImage::Format_ARGB32);
            result.fill(Qt::transparent);
            QPainter painter(&result);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor::fromHsv(20 + i * 32, 140, 200));
            painter.drawEllipse(80, 10, 96, 96);
            painter.drawRoundedRect(50, 100, 156, 145, 25, 25);
            painter.end();
            auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(result)), "", history);
            item->setSizeHint(QSize(history->iconSize().width() + 8, history->iconSize().height() + 8));
            item->setData(Qt::UserRole, QVariantMap { { "image", result }, { "target", "preview-document" },
                { "key", QString::number(i) } });
        }
        if (history->count() > 1) history->setCurrentItem(history->item(1));
    }
    if (app.arguments().contains("--screenshot")) {
        if (app.arguments().contains("--prompt-actions")) {
            panel.resize(650, 950); panel.show();
            auto editor = panel.findChild<PromptEditor*>("positivePrompt");
            editor->setPlainText("masterpiece, (soft light:1.2), <lora:Illustration:0.8>, long hair, forest, blue eyes");
            editor->setFocus();
            QTimer::singleShot(150, &panel, [editor] {
                auto actions = editor->findChild<PromptActions*>(); actions->open(22); actions->perform("toggleMore");
                qInfo() << "Prompt actions" << actions->isOpen() << "selection" << editor->textCursor().hasSelection()
                        << "visible" << editor->isVisible() << "bar" << editor->window()->findChild<QFrame*>("promptActionBar");
            });
            QTimer::singleShot(450, &panel, [&app, &panel] {
                auto bar = panel.findChild<QFrame*>("promptActionBar");
                if (bar) qInfo() << "Bar geometry" << bar->geometry() << "visible" << bar->isVisible() << "parent" << bar->parentWidget();
                app.exit(panel.grab().save(app.arguments().last()) ? 0 : 1);
            });
            return app.exec();
        }
        panel.show();
        app.processEvents();
        if (app.arguments().contains("--settings")) {
            auto dialog = panel.findChild<QDialog*>("pluginSettingsDialog");
            dialog->findChild<QListWidget*>("settingsCategories")->setCurrentRow(
                app.arguments().contains("--interface") ? 3 : app.arguments().contains("--connection") ? 0 : app.arguments().contains("--about") ? 5 : 1);
            dialog->resize(1280, 900);
            dialog->show();
            QTimer::singleShot(250, &panel, [&app, dialog] {
                app.exit(dialog->grab().save(app.arguments().last()) ? 0 : 1);
            });
            return app.exec();
        }
        if (app.arguments().contains("--queue")) {
            auto menu = panel.findChild<QToolButton*>("generationSettings")->menu();
            menu->popup(panel.mapToGlobal(QPoint(20, 380)));
            app.processEvents();
            return menu->grab().save(app.arguments().last()) ? 0 : 1;
        }
        if (!app.arguments().contains("--gallery"))
            return panel.grab().save(app.arguments().last()) ? 0 : 1;
        QTimer::singleShot(
            50, &panel, [&panel] { panel.findChild<QPushButton*>("openGalleryButton")->click(); });
        QTimer::singleShot(200, &panel, [&app] {
            auto dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (dialog) {
                dialog->grab().save(app.arguments().last());
                dialog->accept();
            }
            app.quit();
        });
        return app.exec();
    }
    if(app.arguments().contains("--inpaint-preview")) {
        auto model=panel.findChild<QComboBox*>("checkpointSelect");
        model->addItem("Illustration","sdxl-demo");model->setItemData(model->count()-1,QJsonObject{{"architecture","sdxl"}},Qt::UserRole+1);model->setCurrentIndex(model->count()-1);
        for(auto action:panel.findChild<QToolButton*>("generationMode")->menu()->actions())
            if(action->objectName()=="inpaintMode_custom")action->trigger();
        panel.findChild<QSlider*>("denoiseSlider")->setValue(35);
        panel.findChild<QCheckBox*>("inpaintFocus")->setChecked(true);
        panel.findChild<QCheckBox*>("inpaintSeamless")->setChecked(false);
        panel.findChild<QComboBox*>("inpaintFill")->setCurrentIndex(0);
        panel.resize(620,900);panel.show();
        QTimer::singleShot(300,&panel,[&app,&panel]{app.exit(panel.grab().save(app.arguments().last())?0:1);});return app.exec();
    }
    panel.show();
    return app.exec();
}
