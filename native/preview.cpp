// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QJsonArray>
#include <QLocale>
#include <QPainter>
#include <QTimer>
class PreviewHost : public CanvasHost {
public:
    CanvasSnapshot capture(bool, QString*) override {
        QImage image(1024, 1024, QImage::Format_ARGB32);
        image.fill(Qt::white);
        return { image, {}, "preview-document" };
    }
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
    if (app.arguments().contains("--ru"))
        QLocale::setDefault(QLocale("ru_RU"));
#ifdef Q_OS_WIN
    if (QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf") >= 0)
        app.setFont(QFont("Segoe UI", 10));
#endif
    app.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(36, 38, 43));
    palette.setColor(QPalette::WindowText, QColor(236, 236, 242));
    palette.setColor(QPalette::Base, QColor(27, 29, 34));
    palette.setColor(QPalette::AlternateBase, QColor(43, 45, 51));
    palette.setColor(QPalette::Text, QColor(236, 236, 242));
    palette.setColor(QPalette::Button, QColor(48, 50, 58));
    palette.setColor(QPalette::ButtonText, QColor(236, 236, 242));
    palette.setColor(QPalette::Highlight, QColor(134, 108, 230));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    if (!app.arguments().contains("--light"))
        app.setPalette(palette);
    PreviewHost host;
    BaronPanel panel(&host);
    panel.setWindowTitle("Baron native panel — UI preview (not Krita)");
    panel.resize(520, 840);
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
    if (app.arguments().contains("--screenshot")) {
        panel.show();
        app.processEvents();
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
    panel.show();
    return app.exec();
}
