// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronDiagnostics.h"
#include "BaronUpdates.h"
#include "PluginUi.h"
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QMutex>
#include <QMutexLocker>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QVBoxLayout>
#include <QApplication>
#include <functional>
#include <utility>
#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QtAndroid>
#endif
namespace {
class ReportThread : public QThread {
public:
    explicit ReportThread(std::function<void()> work) : m_work(std::move(work)) { }
protected:
    void run() override { m_work(); }
private:
    std::function<void()> m_work;
};
QString logDirectory;
QMutex logMutex;
bool started = false;
QByteArray tail(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    file.seek(qMax(qint64(0), file.size() - 32768));
    return file.read(32768);
}
}
void BaronDiagnostics::startSession() {
    QMutexLocker lock(&logMutex);
    if (started) return;
    started = true;
    logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/diagnostics";
    if (!QDir().mkpath(logDirectory)) { logDirectory.clear(); return; }
    const QString current = logDirectory + "/session.jsonl", previous = logDirectory + "/previous.jsonl";
    if (QFile::exists(current)) {
        QFile::remove(previous);
        QFile::rename(current, previous);
    }
}
void BaronDiagnostics::record(const char* stage, const QJsonObject& fields) {
    startSession();
    QMutexLocker lock(&logMutex);
    if (logDirectory.isEmpty()) return;
    auto data = fields;
    data["stage"] = QString::fromLatin1(stage);
    data["time"] = QString::number(QDateTime::currentMSecsSinceEpoch());
    QFile file(logDirectory + "/session.jsonl");
    if (file.size() > 256 * 1024) {
        const auto bytes = tail(file.fileName());
        QSaveFile compact(file.fileName());
        if (compact.open(QIODevice::WriteOnly)) { compact.write(bytes); compact.commit(); }
    }
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write(QJsonDocument(data).toJson(QJsonDocument::Compact));
        file.write("\n"); file.flush();
    }
}
QString BaronDiagnostics::localReport() {
    startSession();
    QMutexLocker lock(&logMutex);
    return QString::fromUtf8("\nPrevious session stages:\n" + tail(logDirectory + "/previous.jsonl")
        + "\nCurrent session stages:\n" + tail(logDirectory + "/session.jsonl"));
}
void BaronDiagnostics::show(QWidget* parent) {
    auto dialog = new QDialog(parent);
    dialog->setObjectName("crashDiagnosticsDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QCoreApplication::translate("BaronPanel", "Crash diagnostics"));
    auto layout = new QVBoxLayout(dialog);
    auto hint = new QLabel(QCoreApplication::translate("BaronPanel",
        "Read the previous Android crash without a computer. The report excludes prompts, images and tokens."), dialog);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto text = new QPlainTextEdit(dialog);
    text->setObjectName("crashDiagnosticsReport");
    text->setReadOnly(true); text->setAttribute(Qt::WA_InputMethodEnabled, false);
    text->setContextMenuPolicy(Qt::NoContextMenu); layout->addWidget(text);
    auto copy = new QPushButton(QCoreApplication::translate("BaronPanel", "Copy report"), dialog);
    auto save = new QPushButton(QCoreApplication::translate("BaronPanel", "Save report…"), dialog);
    layout->addWidget(copy); layout->addWidget(save);
    auto close = new QPushButton(QCoreApplication::translate("BaronPanel", "Close"), dialog);
    layout->addWidget(close); QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    QObject::connect(copy, &QPushButton::clicked, dialog, [text] { PluginUi::copyText(text->toPlainText()); });
    QObject::connect(save, &QPushButton::clicked, dialog, [dialog, text] {
        const auto path = QFileDialog::getSaveFileName(dialog, dialog->windowTitle(), "krita-baron-crash.txt", "Text (*.txt)");
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) { file.write(text->toPlainText().toUtf8()); file.commit(); }
    });
    dialog->resize(700, 560); dialog->show();
    const auto local = localReport();
    text->setPlainText("Baron " + BaronUpdates::version() + local);
#ifdef Q_OS_ANDROID
    copy->setEnabled(false); save->setEnabled(false);
    const QPointer<QPlainTextEdit> destination(text);
    const QPointer<QPushButton> copyButton(copy), saveButton(save);
    const auto activity = QtAndroid::androidActivity();
    auto thread = new ReportThread([destination, copyButton, saveButton, activity, local] {
        auto result = QAndroidJniObject::callStaticObjectMethod("org/baron/krita/CrashDiagnostics",
            "report", "(Landroid/content/Context;)Ljava/lang/String;", activity.object<jobject>());
        QAndroidJniEnvironment environment;
        QString report;
        if (environment->ExceptionCheck()) { environment->ExceptionClear(); report = "Android report unavailable."; }
        else if (result.isValid()) report = result.toString();
        QMetaObject::invokeMethod(qApp, [destination, copyButton, saveButton, report, local] {
            if (destination) destination->setPlainText(report + local);
            if (copyButton) copyButton->setEnabled(true);
            if (saveButton) saveButton->setEnabled(true);
        }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
#endif
}
