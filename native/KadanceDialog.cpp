// SPDX-License-Identifier: GPL-3.0-or-later
#include "KadanceDialog.h"
#include "OrchestrionClient.h"
#include "PluginUi.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QCheckBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString kd(const char* text) { return QCoreApplication::translate("BaronPanel", text); }

class KadanceDialog : public QDialog {
public:
    KadanceDialog(QWidget* parent, OrchestrionClient* connection, QJsonObject context,
        KadanceImages images, std::function<QString()> current, std::function<bool(const QString&)> apply, QNetworkAccessManager* transport)
        : QDialog(parent), m_context(context), m_images(images), m_current(current), m_apply(apply) {
        setObjectName("kadanceDialog");
        setWindowTitle(kd("Kadance · Prompt assistant"));
        setAttribute(Qt::WA_DeleteOnClose);
        resize(700, 680);
        m_client = new OrchestrionClient(this, transport);
        m_client->copyConnection(*connection);
        auto layout = new QVBoxLayout(this);
        auto modelLabel = new QLabel(this);
        modelLabel->setTextFormat(Qt::PlainText);
        modelLabel->setText(context["model"].toString() + " · " + context["architecture"].toString() + " · " + context["mode"].toString());
        modelLabel->setWordWrap(true); layout->addWidget(modelLabel);
        if (!images.isEmpty()) {
            auto previews = new QHBoxLayout;
            for (int i = 0; i < qMin(4, images.size()); ++i) {
                auto column = new QVBoxLayout;
                auto preview = new QLabel(this);
                preview->setPixmap(QPixmap::fromImage(images[i].second.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                column->addWidget(preview);
                auto label = new QLabel(QString("%1 · ").arg(i + 1) + (images[i].first == "canvas" ? kd("Canvas") : kd("Reference")), this);
                column->addWidget(label); previews->addLayout(column);
            }
            previews->addStretch(); layout->addLayout(previews);
        }
        auto description = new QLabel(kd("Describe what to change and what to preserve. Kadance proposes a prompt; you decide when to insert it."), this);
        description->setWordWrap(true); layout->addWidget(description);
        m_instruction = new PromptEditor(this, false, PromptEditor::CompletionDisplay::Embedded);
        m_instruction->setObjectName("kadanceInstruction"); m_instruction->setMaximumHeight(120);
        m_instruction->setPlaceholderText(kd("For example: keep image 1's pose and scene, but match the character's fur colors and markings to image 2."));
        layout->addWidget(m_instruction);
        auto presets = new QHBoxLayout;
        for (const auto& pair : QList<QPair<QString, QString>> {
            { kd("Improve prompt"), kd("Improve my current prompt for the selected model, preserving my intent.") },
            { kd("Match reference"), kd("Correct the existing character in image 1 to match the appearance, fur colors and markings in image 2. Keep image 1's pose, composition, background and lighting.") } }) {
            auto button = new QPushButton(pair.first, this); presets->addWidget(button);
            connect(button, &QPushButton::clicked, this, [this, pair] { m_instruction->replacePromptText(pair.second); });
        }
        layout->addLayout(presets);
        m_include = new QCheckBox(kd("Include the edit canvas and configured reference layers"), this);
        m_include->setChecked(true); layout->addWidget(m_include);
        auto note = new QLabel(kd("Images are reduced and sent to Orchestrion's AI provider only when you press Ask Kadance. In Edit mode the canvas is image 1; reference layers follow in order."), this);
        note->setWordWrap(true); layout->addWidget(note);
        m_ask = new QPushButton(kd("Ask Kadance"), this); m_ask->setObjectName("kadanceAsk"); layout->addWidget(m_ask);
        m_status = new QLabel(this); m_status->setWordWrap(true); m_status->setTextFormat(Qt::PlainText);
        m_status->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_status);
        m_result = new PromptEditor(this, false, PromptEditor::CompletionDisplay::Embedded);
        m_result->setObjectName("kadanceSuggestion");
        m_result->setPlaceholderText(kd("The proposed prompt will appear here. You can edit it before inserting."));
        layout->addWidget(m_result, 1);
        auto actions = new QHBoxLayout;
        m_use = new QPushButton(kd("Use this prompt"), this); m_use->setObjectName("kadanceApply"); m_use->setEnabled(false); actions->addWidget(m_use);
        m_undo = new QPushButton(kd("Undo insertion"), this); m_undo->setEnabled(false); actions->addWidget(m_undo);
        auto close = new QPushButton(kd("Close"), this); actions->addWidget(close); layout->addLayout(actions);
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_ask, &QPushButton::clicked, this, [this] { review(); });
        connect(m_client, &OrchestrionClient::error, this, [this](const QString& message) { m_status->setText(message); m_ask->setEnabled(true); });
        connect(m_client, &OrchestrionClient::promptAssisted, this, [this](const QJsonObject& data) {
            m_ask->setEnabled(true);
            const auto prompt = data["prompt"].toString();
            if (prompt.trimmed().isEmpty() || prompt.size() > 32000 || !data["tips"].isArray() || !data["summary"].isString()) {
                m_status->setText(kd("Kadance returned an invalid answer. Please try again.")); return;
            }
            m_result->replacePromptText(prompt);
            QStringList notes { data["summary"].toString() };
            for (const auto& tip : data["tips"].toArray()) notes.append(tip.toString());
            m_status->setText(notes.join('\n')); m_use->setEnabled(true);
        });
        connect(m_use, &QPushButton::clicked, this, [this] {
            if (m_current() != m_original || !m_apply(m_result->toPlainText())) {
                m_status->setText(kd("The prompt changed while Kadance was working. Ask again before inserting.")); m_use->setEnabled(false); return;
            }
            m_last = m_result->toPlainText(); m_undoText = m_original; m_use->setEnabled(false); m_undo->setEnabled(true);
        });
        connect(m_undo, &QPushButton::clicked, this, [this] {
            if (m_current() == m_last) m_apply(m_undoText);
            m_undo->setEnabled(false);
        });
    }
private:
    void review() {
        if (m_client->backend() != OrchestrionClient::orchestrion || !m_client->signedIn()) {
            m_status->setText(kd("Sign in to Orchestrion in the connection settings to use Kadance.")); return;
        }
        if (m_include->isChecked() && m_images.size() > 4) {
            m_status->setText(kd("Kadance can review at most four images, including the canvas.")); return;
        }
        m_original = m_current();
        m_ask->setEnabled(false); m_use->setEnabled(false);
        m_status->setText(kd("Kadance is reviewing your request…"));
        auto data = m_context;
        data["prompt"] = m_original; data["instruction"] = m_instruction->toPlainText();
        const auto images = m_include->isChecked() ? m_images : KadanceImages();
        BackgroundWork::run(this, [images] {
            QJsonArray result;
            for (int i = 0; i < images.size(); ++i) {
                const auto image = images[i].second.scaled(QSize(1024, 1024), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QByteArray jpeg; QBuffer buffer(&jpeg); buffer.open(QIODevice::WriteOnly);
                if (!image.save(&buffer, "JPEG", 80) || jpeg.size() > 768 * 1024) return QJsonArray { QJsonObject { { "error", true } } };
                result.append(QJsonObject { { "role", images[i].first }, { "label", QString("Image %1").arg(i + 1) },
                    { "data", QString("data:image/jpeg;base64," + jpeg.toBase64()) } });
            }
            return result;
        }, [this, data](const QJsonArray& images) {
            if (!images.isEmpty() && images[0].toObject()["error"].toBool()) {
                m_status->setText(kd("An assistant image is too large. Try a smaller reference.")); m_ask->setEnabled(true); return;
            }
            auto request = data; request["images"] = images; m_client->assistPrompt(request);
        });
    }
    OrchestrionClient* m_client;
    PromptEditor *m_instruction, *m_result;
    QCheckBox* m_include;
    QPushButton *m_ask, *m_use, *m_undo;
    QLabel* m_status;
    QJsonObject m_context;
    KadanceImages m_images;
    QString m_original, m_last, m_undoText;
    std::function<QString()> m_current;
    std::function<bool(const QString&)> m_apply;
};
}

QDialog* showKadanceDialog(QWidget* parent, OrchestrionClient* connection, const QJsonObject& context,
    const KadanceImages& images, std::function<QString()> currentPrompt, std::function<bool(const QString&)> applyPrompt, QNetworkAccessManager* transport) {
    auto dialog = new KadanceDialog(parent, connection, context, images, currentPrompt, applyPrompt, transport);
#ifdef Q_OS_ANDROID
    dialog->showMaximized();
#else
    dialog->show();
#endif
    return dialog;
}
