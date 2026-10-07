// SPDX-License-Identifier: GPL-3.0-or-later
#include "PromptActions.h"
#include "PluginUi.h"
#include "TagModel.h"
#include "OrchestrionClient.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputMethod>
#include <QJsonDocument>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSettings>
#include <QScopedValueRollback>
#include <QStringListModel>
#include <QLocale>
#include <QTextCursor>
#include <QVBoxLayout>
#include <algorithm>

PromptActions::PromptActions(PromptEditor* editor) : QObject(editor), m_editor(editor) {
    QFile file(":/baron/prompt/messages.json");
    if (file.open(QIODevice::ReadOnly)) m_messages = QJsonDocument::fromJson(file.readAll()).object();
    m_refresh.setSingleShot(true); m_refresh.setInterval(120);
    connect(&m_refresh, &QTimer::timeout, this, &PromptActions::refresh);
    connect(editor, &QPlainTextEdit::textChanged, this, [this] {
        if (m_editor->m_inserting) { m_refresh.start(); return; }
        close(); m_refresh.start();
    });
    connect(editor, &QPlainTextEdit::selectionChanged, this, [this] {
        if (m_editor->textCursor().hasSelection()) close();
    });
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged, this, &PromptActions::positionBar);
    connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged, this, &PromptActions::positionBar);
    qApp->installEventFilter(this);
    m_disabled = new QScrollArea(editor);
    m_disabled->setObjectName("promptDisabledFragments");
    m_disabled->setFocusPolicy(Qt::NoFocus);
    m_disabled->setWidgetResizable(true); m_disabled->setFrameShape(QFrame::NoFrame);
    m_disabled->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_disabled->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    QScroller::grabGesture(m_disabled->viewport(), QScroller::TouchGesture);
    m_disabled->hide();
    m_refresh.start();
}
PromptActions::~PromptActions() { if (qApp) qApp->removeEventFilter(this); delete m_bar; }
void PromptActions::setTarget(PromptEditor* target) { m_target = target; }
void PromptActions::setClient(OrchestrionClient* client) { m_client = client; }
QString PromptActions::message(const QString& key) const {
    auto lang = QSettings("BaronEdition", "Orchestrion").value("language", "").toString();
    if (lang.isEmpty()) lang = QLocale().name().replace('_', '-').toLower();
    const auto local = m_messages[lang].toObject().isEmpty() ? lang.section('-', 0, 0) : lang;
    auto find = [&key](QJsonObject object) {
        QJsonValue value = object;
        for (const auto& part : key.split('.')) value = value.toObject()[part];
        return value.toString();
    };
    const auto translated = find(m_messages[local].toObject()["promptEditor"].toObject());
    return translated.isEmpty() ? find(m_messages["en"].toObject()["promptEditor"].toObject()) : translated;
}
void PromptActions::setModel(const QString& model, const QString& family) { m_model = model; m_family = family; }
QJsonObject PromptActions::options() {
    QJsonObject dictionary;
    QJsonArray triggers;
    for (const auto& value : m_segments) {
        const auto key = m_logic.call("normalizeKey", {value.toObject()["text"]}).toString();
        const auto entry = TagModel::shared()->lookup(key);
        if (!entry.isEmpty()) dictionary[key] = entry;
        const auto segment = value.toObject();
        if (segment["kind"] == "lora") {
            for (const auto& word : m_editor->m_triggers.value(segment["base"].toString()).split(','))
                if (!word.trimmed().isEmpty()) triggers.append(word.trimmed().toLower().replace('_', ' '));
        }
    }
    return {{"family", m_logic.call("promptOrderFamily", {m_model, m_family})}, {"dictionary", dictionary}, {"triggerWords", triggers}};
}
void PromptActions::close() {
    m_index = -1; m_selected.clear(); m_multi = false; m_more = false;
    if (m_bar) m_bar->hide();
    m_refresh.start();
}
void PromptActions::refresh() {
    if (m_editor->m_composing || m_editor->m_pastePending) { m_refresh.start(); return; }
    m_segments = m_logic.segments(m_editor->toPlainText());
    if (m_index >= m_segments.size()) close();
    updateBubbles();
    if (m_bar && m_bar->isVisible()) buildBar();
}
void PromptActions::open(int position) {
    if (m_editor->isReadOnly() || m_editor->m_composing || m_editor->m_pastePending
        || m_editor->textCursor().hasSelection()) return;
    m_segments = m_logic.segments(m_editor->toPlainText());
    const int index = m_logic.call("segmentAt", {m_segments, position}).toInt(-1);
    if (index < 0 || index >= m_segments.size()) { close(); return; }
    m_editor->hideCompletion(); m_editor->m_completionTimer->stop();
    m_index = index;
    if (m_multi) {
        if (m_selected.contains(index)) m_selected.remove(index); else m_selected.insert(index);
        if (m_selected.isEmpty()) m_selected.insert(index);
    } else m_selected = {index};
    m_note.clear();
    buildBar(); updateBubbles();
}
void PromptActions::positionBar() {
    if (!m_bar || !m_bar->isVisible()) return;
    auto window = m_editor->window();
    const auto origin = m_editor->mapTo(window, QPoint());
    int bottom = window->height() - 4;
    const auto keyboard = QGuiApplication::inputMethod()->keyboardRectangle();
    if (QGuiApplication::inputMethod()->isVisible() && !keyboard.isEmpty())
        bottom = qMin(bottom, keyboard.topLeft().toPoint().y() - 4);
    const int availableHeight = qMax(48, bottom - 4);
    m_bar->setFixedWidth(qMin(430, qMax(220, window->width() - 8)));
    m_bar->setMaximumHeight(qMin(350, availableHeight));
    m_bar->adjustSize();
    const int above = origin.y() - m_bar->height() - 4;
    const int y = above >= 4 ? above : qMin(origin.y() + m_editor->height() + 4, bottom - m_bar->height());
    m_bar->move(qBound(4, origin.x(), qMax(4, window->width() - m_bar->width() - 4)), qMax(4, y));
    m_bar->raise();
}
void PromptActions::buildBar() {
    if (m_index < 0 || m_index >= m_segments.size()) return;
    if (!m_bar || m_bar->parentWidget() != m_editor->window()) {
        delete m_bar;
        m_bar = new QFrame(m_editor->window());
        m_bar->setObjectName("promptActionBar");
        m_bar->setWindowFlags(Qt::Widget); m_bar->setAttribute(Qt::WA_ShowWithoutActivating);
        m_bar->setFocusPolicy(Qt::NoFocus); m_bar->setFrameShape(QFrame::StyledPanel);
        m_bar->setAutoFillBackground(true);
    }
    m_bar->hide();
    delete m_bar->layout();
    for (auto child : m_bar->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) delete child;
    auto box = new QVBoxLayout(m_bar); box->setContentsMargins(5, 5, 5, 5); box->setSpacing(4);
    auto row = new QHBoxLayout; row->setSpacing(2); box->addLayout(row);
    const auto seg = m_segments[m_index].toObject();
    const auto weight = m_logic.call("getSegmentWeight", {seg["text"]});
    auto button = [this](QLayout* layout, const QString& glyph, const QString& action,
        const QString& label, const QJsonValue& argument = QJsonValue(), bool enabled = true) {
        auto b = new QPushButton(glyph, m_bar);
        b->setObjectName("promptAction_" + action + (argument.isUndefined() || argument.isNull() ? QString() : "_" + argument.toVariant().toString()));
        b->setAccessibleName(message(label)); b->setToolTip(message(label));
        b->setFocusPolicy(Qt::NoFocus); b->setMinimumHeight(38); b->setEnabled(enabled);
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        layout->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, action, argument] {
            const auto epoch = m_editor->m_inputEpoch;
            QTimer::singleShot(0, this, [this, action, argument, epoch] {
                if (epoch == m_editor->m_inputEpoch && !m_editor->m_composing) perform(action, argument);
            });
        });
        return b;
    };
    auto grip = button(row, m_multi ? QString::number(m_selected.size()) : QString(), "multiStart", "drag");
    if (!m_multi) PluginUi::setIcon(grip, "prompt-grip");
    grip->setObjectName("promptFragmentGrip"); grip->installEventFilter(this);
    button(row, "−", "weight", "weightDown", -.1, !weight.isNull());
    auto badge = new QLabel(m_multi || weight.isNull() ? "·" : m_logic.call("formatWeight", {weight}).toString(), m_bar);
    badge->setAlignment(Qt::AlignCenter); row->addWidget(badge);
    button(row, "+", "weight", "weightUp", .1, !weight.isNull());
    button(row, "A↔", "translate", "translate", {}, !m_busy && seg["kind"] != "keyword" && seg["kind"] != "lora" && seg["kind"] != "extra" && m_client && m_client->backend() == OrchestrionClient::orchestrion);
    auto copy = button(row, "", "copy", "copy"); PluginUi::setIcon(copy, "copy-image");
    auto paste = button(row, "", "paste", "paste"); PluginUi::setIcon(paste, "prompt-paste");
    button(row, "×", "delete", "delete");
    auto more = button(row, "", "toggleMore", "more"); PluginUi::setIcon(more, m_more ? "prompt-collapse" : "more");
    auto issues = m_logic.call("lintPrompt", {m_editor->toPlainText(), m_segments}).toArray();
    QStringList warnings;
    if (m_index < issues.size()) for (const auto& value : issues[m_index].toArray()) {
        const auto issue = value.toObject();
        warnings.append(message("issue." + issue["code"].toString()).replace("{{char}}", issue["char"].toString())
            .replace("{{weight}}", QString::number(issue["weight"].toDouble())));
    }
    if (seg["kind"] == "lora" && !m_editor->m_loraModel->stringList().contains(seg["base"].toString(), Qt::CaseInsensitive)) warnings.append(message("issue.loraNotFound"));
    const auto dict = TagModel::shared()->lookup(m_logic.call("normalizeKey", {seg["text"]}).toString());
    if (!dict.isEmpty()) warnings.append(message("inDictionary").replace("{{category}}", message("category." + dict["category"].toString()))
        .replace("{{count}}", QString::number(qint64(dict["count"].toDouble()))));
    if (!m_note.isEmpty()) warnings.append(m_note);
    if (!warnings.isEmpty()) {
        auto note = new QLabel(warnings.join("\n"), m_bar); note->setWordWrap(true); note->setTextFormat(Qt::PlainText); box->addWidget(note);
    }
    if (m_more || m_multi) {
        auto scroll = new QScrollArea(m_bar); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
        scroll->setFocusPolicy(Qt::NoFocus); scroll->setMinimumHeight(120); scroll->setMaximumHeight(240);
        auto content = new QWidget(scroll); auto grid = new QGridLayout(content); grid->setContentsMargins(0, 0, 0, 0); grid->setSpacing(3);
        grid->setColumnStretch(0, 1); grid->setColumnStretch(1, 1);
        int cell = 0;
        auto add = [&](const QString& action, const QString& key, const QJsonValue& argument = QJsonValue(), bool enabled = true) {
            auto layout = new QHBoxLayout; grid->addLayout(layout, cell / 2, cell % 2); ++cell;
            const auto label = message(key);
            button(layout, content->fontMetrics().elidedText(label, Qt::ElideRight, 192), action, key, argument, enabled);
        };
        if (!weight.isNull()) {
            const QList<double> presets = seg["kind"] == "lora" ? QList<double>{.5,.8,1} : QList<double>{.8,1,1.2,1.4};
            for (double value : presets) add("setWeight", "presets", value);
            for (auto b : content->findChildren<QPushButton*>()) if (b->objectName().startsWith("promptAction_setWeight")) b->setText(b->objectName().section('_', -1));
        }
        add("duplicate", "duplicate"); add("cut", "cut");
        add("move", "moveLeft", -1, m_index > 0); add("move", "moveRight", 1, m_index + 1 < m_segments.size());
        add("logical", "logicalPlace"); add("organize", "organizeMenu");
        add("groupStep", "groupEarlier", -1); add("groupStep", "groupLater", 1);
        add("moveEdge", "moveStart", -1); add("moveEdge", "moveEnd", 1);
        add("moveOther", m_editor->m_negative ? "toPositive" : "toNegative", {}, bool(m_target));
        add("park", "disable"); add("underscores", "underscores"); add("breakAfter", "breakAfter");
        add("splitTags", "splitTags", {}, !m_busy && m_client && m_client->backend() == OrchestrionClient::orchestrion);
        add("multiStart", "selectMany"); add("selectAll", "selectAllHint"); add("multiDone", "done");
        const auto key = m_logic.call("normalizeKey", {seg["text"]}).toString();
        if (seg["kind"] != "keyword" && seg["kind"] != "lora" && seg["kind"] != "extra") {
            if (dict.isEmpty()) {
                const auto suggestion = m_logic.call("typoForCandidates", {key, TagModel::shared()->typoCandidates(key)}).toObject()["tag"].toString();
                if (!suggestion.isEmpty()) {
                    auto title = new QLabel(message("didYouMean"), content); grid->addWidget(title, (cell + 1) / 2, 0, 1, 2); cell = ((cell + 1) / 2 + 1) * 2;
                    add("replaceBase", "didYouMean", suggestion);
                    content->findChildren<QPushButton*>().last()->setText(content->fontMetrics().elidedText(suggestion, Qt::ElideRight, 192));
                }
            }
            const auto similar = TagModel::shared()->similar(key);
            if (!similar.isEmpty()) {
                auto title = new QLabel(message("similar"), content); grid->addWidget(title, (cell + 1) / 2, 0, 1, 2); cell = ((cell + 1) / 2 + 1) * 2;
                for (const auto& tag : similar) {
                    add("replaceBase", "similar", tag);
                    content->findChildren<QPushButton*>().last()->setText(content->fontMetrics().elidedText(tag, Qt::ElideRight, 192));
                }
            }
        }
        scroll->setWidget(content); box->addWidget(scroll);
    }
    m_bar->show(); positionBar();
}
void PromptActions::updateBubbles() {
    QList<QTextEdit::ExtraSelection> selections;
    if (m_editor->m_composing) return;
    for (int i = 0; i < m_segments.size() && i < 2000; ++i) {
        const auto segment = m_segments[i].toObject();
        const auto kind = segment["kind"].toString();
        QColor color = kind == "lora" ? QColor(167, 118, 228) : kind == "weighted" ? QColor(222, 175, 94) : m_editor->palette().color(QPalette::Highlight);
        color.setAlpha(m_selected.contains(i) ? 95 : 25);
        QTextEdit::ExtraSelection selection; selection.cursor = QTextCursor(m_editor->document());
        selection.cursor.setPosition(segment["start"].toInt());
        selection.cursor.setPosition(segment["end"].toInt(), QTextCursor::KeepAnchor);
        selection.format.setBackground(color); selections.append(selection);
    }
    m_editor->setExtraSelections(selections);
}
void PromptActions::commit(const QString& text, int caret, int index) {
    if (text == m_editor->toPlainText() || m_editor->isReadOnly() || m_editor->m_composing) return;
    const auto multi = m_multi; const auto selected = m_selected; const auto more = m_more;
    const QScopedValueRollback<bool> inserting(m_editor->m_inserting, true);
    m_editor->resetInputState();
    auto cursor = m_editor->textCursor(); cursor.beginEditBlock();
    cursor.select(QTextCursor::Document); cursor.insertText(text); cursor.endEditBlock();
    cursor.setPosition(qBound(0, caret, text.size())); m_editor->setTextCursor(cursor);
    m_editor->m_completionTimer->stop(); m_editor->hideCompletion();
    m_segments = m_logic.segments(text); m_index = index;
    if (index >= 0) { m_multi = multi; m_more = more; m_selected = multi ? selected : QSet<int>{index}; buildBar(); }
    refresh();
}
void PromptActions::perform(const QString& action, const QJsonValue& argument) {
    if (m_editor->m_composing || m_editor->isReadOnly()) return;
    const auto text = m_editor->toPlainText();
    m_segments = m_logic.segments(text);
    if (m_index < 0 || m_index >= m_segments.size()) return;
    const auto seg = m_segments[m_index].toObject();
    QList<int> indices = m_selected.values(); std::sort(indices.begin(), indices.end());
    QStringList pieces; for (int i : indices) if (i < m_segments.size()) pieces.append(m_segments[i].toObject()["text"].toString());
    const auto joined = pieces.join(", ");
    if (action == "toggleMore") { m_more = !m_more; buildBar(); return; }
    if (action == "multiStart" || action == "selectAll") {
        m_multi = true;
        if (action == "selectAll") for (int i = 0; i < m_segments.size(); ++i) m_selected.insert(i);
        buildBar(); updateBubbles(); return;
    }
    if (action == "multiDone") { close(); return; }
    if (action == "copy" || action == "cut") {
        PluginUi::copyText(joined); if (action == "copy") { m_note = message("copied"); buildBar(); return; }
    }
    if (action == "paste") {
        m_editor->m_actionPasteIndex = m_index;
        m_editor->queueClipboardPaste(); return;
    }
    if (action == "translate" || action == "splitTags") { translate(action == "translate" ? "translate" : "tags"); return; }
    if (action == "organize") { organize(); return; }
    if (action == "moveOther" && m_target) {
        const auto result = m_logic.call("appendSegment", {m_target->toPlainText(), joined}).toObject();
        m_target->m_actions->commit(result["text"].toString(), result["end"].toInt(), -1);
    }
    if (action == "park") {
        int n = 0;
        for (int i : indices) m_parked.append(QJsonObject{{"text", m_segments[i].toObject()["text"]}, {"at", i - n++}});
        while (m_parked.size() > 40) m_parked.removeFirst();
    }
    if (action == "delete" || action == "cut" || action == "park" || (action == "moveOther" && m_target)) {
        QString out = text; int caret = 0;
        for (auto it = indices.crbegin(); it != indices.crend(); ++it) {
            const auto result = m_logic.call("deleteSegment", {out, m_logic.segments(out), *it}).toObject();
            out = result["text"].toString(); caret = result["caret"].toInt();
        }
        commit(out, caret); close();
        if (action == "park") { restoreDisabled(m_parked); emit m_editor->contextActionsChanged(); }
        return;
    }
    if (action == "weight" || action == "setWeight") {
        QString out = text;
        for (auto it = indices.crbegin(); it != indices.crend(); ++it) {
            const auto s = m_segments[*it].toObject();
            const auto replacement = m_logic.call(action == "weight" ? "adjustSegmentWeight" : "setSegmentWeight", {s["text"], argument});
            if (replacement.isString()) out = m_logic.call("replaceRange", {out, s["start"], s["end"], replacement}).toString();
        }
        const auto after = m_logic.segments(out);
        commit(out, after[m_index].toObject()["end"].toInt(), m_index); return;
    }
    QJsonValue result;
    if (action == "duplicate") result = m_logic.call("duplicateSegment", {text, m_segments, m_index});
    else if (action == "move" || action == "moveEdge" || action == "moveTo") result = m_logic.call("moveSegment", {text, m_segments, m_index,
        action == "moveTo" ? argument.toInt() : action == "move" ? m_index + argument.toInt() : argument.toInt() < 0 ? 0 : m_segments.size() - 1});
    else if (action == "logical") {
        QString out = text; QJsonObject last;
        for (const auto& piece : pieces) {
            const auto now = m_logic.segments(out); int i = -1;
            for (int k = 0; k < now.size(); ++k) if (now[k].toObject()["text"] == piece) { i = k; break; }
            if (i < 0) continue;
            const auto r = m_logic.call("moveSegmentLogically", {out, now, i, options()}).toObject();
            if (!r.isEmpty()) { out = r["text"].toString(); last = r; }
        }
        if (out != text) { commit(out, last["end"].toInt(), m_multi ? -1 : last["index"].toInt()); return; }
        m_note = message("logicalAlready"); buildBar(); return;
    } else if (action == "groupStep") {
        const auto target = m_logic.call("categoryStepTarget", {text, m_segments, m_index, argument, options()});
        if (!target.isNull()) result = m_logic.call("moveSegment", {text, m_segments, m_index, target});
    } else if (action == "underscores" || action == "replaceBase") {
        const auto replacement = action == "underscores" ? m_logic.call("toggleUnderscores", {seg["text"]}) : m_logic.call("replaceBase", {seg["text"], argument});
        if (replacement.isString()) result = m_logic.call("replaceSegment", {text, m_segments, m_index, replacement});
    } else if (action == "breakAfter") {
        commit(m_logic.call("replaceRange", {text, seg["end"], seg["end"], " BREAK"}).toString(), seg["end"].toInt() + 6, m_index); return;
    }
    if (result.isObject()) {
        const auto r = result.toObject(); commit(r["text"].toString(), r["end"].toInt(), r["index"].toInt(m_index));
    }
}
void PromptActions::pasteAfter(int index, const QString& text) {
    const auto current = m_editor->toPlainText();
    const auto segments = m_logic.segments(current);
    if (index < 0 || index >= segments.size()) return;
    const auto result = m_logic.call("insertAfterSegment", {current, segments, index, text}).toObject();
    if (!result.isEmpty()) commit(result["text"].toString(), result["end"].toInt(), index);
}
void PromptActions::restoreDisabled(const QJsonArray& entries) {
    ++m_epoch;
    m_parked = entries;
    auto content = new QWidget; auto layout = new QHBoxLayout(content); layout->setContentsMargins(2, 0, 2, 0);
    for (int i = 0; i < m_parked.size(); ++i) {
        auto text = m_parked[i].toObject()["text"].toString();
        auto restore = new QPushButton(text.left(28), content); restore->setToolTip(text);
        restore->setFocusPolicy(Qt::NoFocus); restore->setMinimumHeight(34); layout->addWidget(restore);
        connect(restore, &QPushButton::clicked, this, [this, i] { QTimer::singleShot(0, this, [this, i] { this->restore(i); }); });
        auto discard = new QPushButton("×", content); discard->setFocusPolicy(Qt::NoFocus); discard->setFixedWidth(30); layout->addWidget(discard);
        connect(discard, &QPushButton::clicked, this, [this, i] { QTimer::singleShot(0, this, [this, i] {
            if (i >= m_parked.size()) return;
            m_parked.removeAt(i); restoreDisabled(m_parked); emit m_editor->contextActionsChanged();
        }); });
    }
    layout->addStretch(); delete m_disabled->takeWidget(); m_disabled->setWidget(content);
    m_disabled->setVisible(!m_parked.isEmpty()); m_editor->setViewportMargins(0, 0, 0, m_parked.isEmpty() ? 0 : 43);
    m_disabled->setGeometry(0, m_editor->height() - 43, m_editor->width(), 43);
}
void PromptActions::restore(int index) {
    if (m_editor->m_composing || index < 0 || index >= m_parked.size()) return;
    const auto entry = m_parked[index].toObject();
    const auto text = m_editor->toPlainText(); const auto segments = m_logic.segments(text);
    const int at = qBound(0, entry["at"].toInt(), segments.size());
    auto result = m_logic.call("insertAfterSegment", {text, segments, at - 1, entry["text"]}).toObject();
    if (at == 0 && !segments.isEmpty()) {
        const QString prefix = entry["text"].toString() + ", ";
        result = QJsonObject{{"text", QString(prefix + text)}, {"end", prefix.size()}};
    }
    commit(result["text"].toString(), result["end"].toInt());
    m_parked.removeAt(index); restoreDisabled(m_parked); emit m_editor->contextActionsChanged();
}
void PromptActions::organize() {
    close();
    auto dialog = new QDialog(m_editor); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setWindowTitle(message("organize.title"));
    dialog->setMinimumWidth(qMin(600, m_editor->window()->width() - 20));
    auto layout = new QVBoxLayout(dialog); auto family = new QComboBox(dialog);
    family->addItems({"Illustrious / NoobAI", "Pony", "Anima", "Flux / Krea / Qwen"});
    const QStringList ids{"illustrious", "pony", "anima", "natural"};
    family->setCurrentIndex(qMax(0, ids.indexOf(options()["family"].toString()))); layout->addWidget(family);
    auto dedupe = new QCheckBox(message("organize.dedupe").replace("{{count}}", ""), dialog); layout->addWidget(dedupe);
    auto before = new QLabel(message("organize.before"), dialog); layout->addWidget(before);
    auto sourcePreview = new QPlainTextEdit(dialog); sourcePreview->setReadOnly(true); sourcePreview->setMaximumHeight(130);
    sourcePreview->setPlainText(m_editor->toPlainText()); layout->addWidget(sourcePreview);
    layout->addWidget(new QLabel(message("organize.after"), dialog));
    auto preview = new QPlainTextEdit(dialog); preview->setReadOnly(true); preview->setMinimumHeight(180); layout->addWidget(preview);
    const auto original = m_editor->toPlainText(); const auto epoch = m_editor->m_inputEpoch;
    const auto baseOptions = options();
    const auto labels = QSharedPointer<QJsonArray>::create();
    auto update = [this, family, dedupe, preview, original, ids, baseOptions, labels] {
        auto opts = baseOptions; opts["family"] = ids[family->currentIndex()]; opts["labels"] = *labels;
        auto text = m_logic.call("organizePrompt", {original, opts}).toObject()["text"].toString(original);
        if (!m_logic.call("isPermutationOfSegments", {original, text}).toBool()) text = original;
        if (dedupe->isChecked()) text = m_logic.call("removeDuplicates", {text}).toObject()["text"].toString(text);
        preview->setPlainText(text);
    };
    connect(family, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, update);
    connect(dedupe, &QCheckBox::toggled, dialog, update); update();
    auto ask = new QPushButton(message("organize.askAi"), dialog); ask->hide(); layout->addWidget(ask);
    auto status = new QLabel(dialog); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    if (m_client && m_client->backend() == OrchestrionClient::orchestrion && m_client->signedIn()) {
        auto detector = new OrchestrionClient(dialog); detector->copyConnection(*m_client);
        connect(detector, &OrchestrionClient::error, dialog, [detector] { detector->deleteLater(); });
        const QPointer<QPushButton> button(ask);
        detector->promptOrganizerCapabilities([button, detector](const QJsonObject& data) {
            if (button && data["ai"].toBool()) button->show(); detector->deleteLater();
        });
    }
    connect(ask, &QPushButton::clicked, dialog, [this, ask, dialog, family, status, original, ids, labels, update, baseOptions] {
        if (!m_client) return;
        auto opts = baseOptions; const auto selectedFamily = ids[family->currentIndex()]; opts["family"] = selectedFamily;
        const auto result = m_logic.call("organizePrompt", {original, opts}).toObject();
        const auto segments = result["segments"].toArray(); const auto unknown = result["unknown"].toArray();
        QJsonArray requested; QList<int> indices;
        for (const auto& value : unknown) {
            if (requested.size() == 60) break;
            const int i = value.toInt(); indices.append(i); requested.append(segments[i].toObject()["text"]);
        }
        if (requested.isEmpty()) { status->setText(message("logicalAlready")); return; }
        ask->setEnabled(false); status->setText(message("organize.askingAi"));
        auto client = new OrchestrionClient(dialog); client->copyConnection(*m_client);
        connect(client, &OrchestrionClient::error, dialog, [this, ask, status, client] {
            ask->setEnabled(true); status->setText(message("organize.aiFailed")); client->deleteLater();
        });
        const QPointer<QDialog> owner(dialog);
        client->promptOrganizerLabels(requested, selectedFamily, [this, owner, client, family, selectedFamily, ids,
            ask, status, indices, segments, labels, update](const QJsonObject& data) {
            client->deleteLater(); if (!owner) return; ask->setEnabled(true);
            const auto returned = data["labels"].toArray();
            if (!data["success"].toBool() || returned.size() != indices.size() || ids[family->currentIndex()] != selectedFamily) {
                status->setText(message("organize.aiFailed")); return;
            }
            QJsonArray complete; for (int i = 0; i < segments.size(); ++i) complete.append(QJsonValue());
            for (int i = 0; i < indices.size(); ++i) complete[indices[i]] = returned[i];
            *labels = complete; update(); status->setText(message("organize.aiDone").replace("{{count}}", QString::number(returned.size())));
        });
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this, dialog, preview, original, epoch] {
        if (epoch == m_editor->m_inputEpoch && m_editor->toPlainText() == original) commit(preview->toPlainText(), 0);
        dialog->accept();
    });
    dialog->open();
}
void PromptActions::translate(const QString& mode) {
    if (m_busy || !m_client || m_client->backend() != OrchestrionClient::orchestrion) return;
    const auto original = m_editor->toPlainText(); const auto segment = m_segments[m_index].toObject();
    const auto source = segment["base"].toString(); const int index = m_index;
    const auto epoch = m_editor->m_inputEpoch; const auto context = m_epoch;
    auto client = new OrchestrionClient(this); client->copyConnection(*m_client);
    m_busy = true; m_note = message("translating"); buildBar();
    connect(client, &OrchestrionClient::error, this, [this, client](const QString& issue) {
        m_busy = false; m_note = issue; client->deleteLater(); if (m_index >= 0) buildBar();
    });
    client->translatePrompt(source, mode, [this, client, original, segment, index, epoch, context](const QJsonObject& data) {
        m_busy = false; client->deleteLater();
        if (epoch != m_editor->m_inputEpoch || context != m_epoch || original != m_editor->toPlainText()) return;
        if (!data["success"].toBool() || data["warning"].toBool() || !data["warning"].toString().isEmpty() || !data["translatedText"].isString()) {
            m_note = message("textChanged"); buildBar(); return;
        }
        auto translated = data["translatedText"].toString().trimmed();
        if (translated.endsWith("[/OUT]")) translated.chop(6);
        const auto next = m_logic.call("replaceBase", {segment["text"], translated.trimmed()});
        const auto r = m_logic.call("replaceSegment", {original, m_segments, index, next}).toObject();
        commit(r["text"].toString(), r["end"].toInt(), index); m_note.clear(); buildBar();
    }, m_model);
}
bool PromptActions::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::Resize && object == m_editor) {
        m_disabled->setGeometry(0, m_editor->height() - 43, m_editor->width(), 43); positionBar();
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouse = static_cast<QMouseEvent*>(event);
        if (object == m_editor->viewport()) { m_press = mouse->pos(); m_pressed.start(); }
        else if (auto widget = qobject_cast<QWidget*>(object)) {
            if (m_bar && m_bar->isAncestorOf(widget) && widget->objectName() == "promptFragmentGrip") { m_press = mouse->globalPos(); m_drag = true; return true; }
            if (m_bar && m_bar->isVisible() && widget != m_editor && !m_editor->isAncestorOf(widget)
                && widget != m_bar && !m_bar->isAncestorOf(widget)) close();
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto mouse = static_cast<QMouseEvent*>(event);
        if (m_drag && object->objectName() == "promptFragmentGrip") {
            m_drag = false;
            const auto local = m_editor->viewport()->mapFromGlobal(mouse->globalPos());
            if ((mouse->globalPos() - m_press).manhattanLength() > 8 && m_editor->viewport()->rect().contains(local)) {
                const int target = m_logic.call("segmentAt", {m_segments, m_editor->cursorForPosition(local).position()}).toInt(-1);
                if (target >= 0) perform("moveTo", target);
            } else perform("multiStart");
            return true;
        }
        if (object == m_editor->viewport() && m_pressed.isValid() && m_pressed.elapsed() < 350
            && (mouse->pos() - m_press).manhattanLength() < 8 && !m_editor->textCursor().hasSelection()) {
            const auto position = m_editor->cursorForPosition(mouse->pos()).position();
            const auto epoch = m_editor->m_inputEpoch;
            QTimer::singleShot(0, this, [this, position, epoch] { if (epoch == m_editor->m_inputEpoch) open(position); });
        }
    }
    return QObject::eventFilter(object, event);
}
