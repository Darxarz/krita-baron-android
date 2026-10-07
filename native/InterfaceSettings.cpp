// SPDX-License-Identifier: GPL-3.0-or-later
#include "InterfaceSettings.h"
#include "PluginUi.h"
#include "TagModel.h"
#include "OrientationLayouts.h"
#include <QPushButton>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QToolButton>

ToggleSwitch::ToggleSwitch(QWidget* parent, const QString& on, const QString& off)
    : QAbstractButton(parent), m_on(on), m_off(off) {
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto animation = new QPropertyAnimation(this, "position", this);
    animation->setDuration(120);
    connect(this, &QAbstractButton::toggled, this, [this, animation](bool active) {
        animation->stop();
        animation->setStartValue(m_position);
        animation->setEndValue(active ? 1.0 : 0.0);
        animation->start();
    });
}
QSize ToggleSwitch::sizeHint() const {
    const int radius = fontMetrics().height() / 2 + 2;
    return QSize(qMax(fontMetrics().horizontalAdvance(m_on), fontMetrics().horizontalAdvance(m_off))
            + 12 + 4 * radius, 2 * radius + 4);
}
void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const qreal radius = fontMetrics().height() / 2 + 2;
    const QRectF track(width() - 4 * radius, (height() - 2 * radius) / 2., 4 * radius, 2 * radius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(group, isChecked() ? QPalette::Highlight : QPalette::Dark));
    painter.drawRoundedRect(track, radius, radius);
    painter.setBrush(palette().color(group, isChecked() ? QPalette::Text : QPalette::Light));
    const auto center = track.left() + radius + 2 * radius * m_position;
    painter.drawEllipse(QPointF(center, track.center().y()), radius - 2, radius - 2);
    painter.setPen(palette().color(group, QPalette::WindowText));
    painter.drawText(QRectF(0, 0, track.left() - 10, height()), Qt::AlignRight | Qt::AlignVCenter,
        isChecked() ? m_on : m_off);
    if (hasFocus()) {
        painter.setPen(QPen(palette().color(group, QPalette::Highlight), 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}
InterfaceSettings::InterfaceSettings(QWidget* parent) : QWidget(parent) {
    connect(this, &InterfaceSettings::changed, this, [](const QString& key, const QVariant& value) {
        if (key == "orientation_layouts_enabled") OrientationLayouts::applyEnabled(value.toBool());
    });
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 8, 16, 0);
    auto title = new QLabel(tr("Interface Settings"), this);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.2);
    title->setFont(font);
    outer->addWidget(title);
    outer->addSpacing(12);
    auto scroll = new QScrollArea(this);
    scroll->setObjectName("interfaceSettingsScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_content = new QWidget(scroll);
    m_rows = new QVBoxLayout(m_content);
    m_rows->setContentsMargins(8, 4, 12, 0);
    m_rows->setSpacing(12);
    scroll->setWidget(m_content);
    outer->addWidget(scroll);
    build();
}
void InterfaceSettings::save(const QString& key, const QVariant& value) {
    QSettings settings("BaronEdition", "Orchestrion");
    if (key == "language" && value.toString().isEmpty()) {
        settings.remove(key);
    } else {
        settings.setValue(key, value);
    }
    emit changed(key, value);
}
QWidget* InterfaceSettings::row(const QString& title, const QString& description, QWidget* control) {
    auto widget = new QWidget(m_content);
    auto layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, title.isEmpty() ? 0 : 4, 0, 0);
    if (!title.isEmpty()) {
        auto label = new QLabel("<b>" + title.toHtmlEscaped() + "</b><br>" + description.toHtmlEscaped(), widget);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        label->setBuddy(control);
        layout->addWidget(label, 1);
        control->setAccessibleName(title);
        control->setAccessibleDescription(description);
    } else layout->addStretch();
    layout->addWidget(control);
    m_rows->addWidget(widget);
    return widget;
}
QComboBox* InterfaceSettings::combo(const QString& key, const QString& title,
    const QString& description, const QList<QPair<QString, QString>>& choices, const QString& defaultValue) {
    auto control = new QComboBox(m_content);
    control->setObjectName(key);
    control->setMinimumWidth(240);
    control->setMaximumWidth(340);
    for (const auto& choice : choices) control->addItem(choice.first, choice.second);
    const auto value = QSettings("BaronEdition", "Orchestrion").value(key, defaultValue);
    control->setCurrentIndex(qMax(0, control->findData(value)));
    row(title, description, control);
    m_keys.append(key);
    connect(control, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this, control, key] { save(key, control->currentData()); });
    return control;
}
ToggleSwitch* InterfaceSettings::toggle(const QString& key, const QString& title,
    const QString& description, bool defaultValue, bool showHide) {
    auto control = new ToggleSwitch(m_content, showHide ? tr("Show") : tr("On"), showHide ? tr("Hide") : tr("Off"));
    control->setObjectName(key);
    control->setChecked(QSettings("BaronEdition", "Orchestrion").value(key, defaultValue).toBool());
    control->setPosition(control->isChecked() ? 1 : 0);
    row(title, description, control);
    m_keys.append(key);
    connect(control, &QAbstractButton::toggled, this, [this, key](bool value) { save(key, value); });
    return control;
}
void InterfaceSettings::spin(const QString& key, const QString& title, const QString& description,
    int low, int high, int defaultValue) {
    auto control = new QSpinBox(m_content);
    control->setObjectName(key);
    control->setMinimumWidth(100);
    control->setRange(low, high);
    control->setValue(QSettings("BaronEdition", "Orchestrion").value(key, defaultValue).toInt());
    row(title, description, control);
    m_keys.append(key);
    connect(control, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this, key](int value) { save(key, value); });
}
void InterfaceSettings::build() {
    QSettings settings("BaronEdition", "Orchestrion");
    if (!settings.contains("show_negative_prompt"))
        settings.setValue("show_negative_prompt", settings.value("showNegative", true));
    QList<QPair<QString, QString>> languages { { tr("System"), "" }, { "English", "en" } };
    for (const auto& name : QDir(":/baron/language").entryList({ "*.json" }, QDir::Files)) {
        QFile file(":/baron/language/" + name);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        if (data["id"] != "en") languages.append({ data["name"].toString(), data["id"].toString() });
    }
    combo("language", tr("Language"), tr("UI language used by the plugin - requires restart!"), languages, "");
    m_translation = combo("prompt_translation", tr("Prompt Translation"),
        tr("Translate text prompts from the selected language to English"), { { tr("Disabled"), "" } }, "");
    m_translation->setEnabled(false);
    m_translation->setToolTip(tr("This connection does not offer prompt translation."));
    const int legacyLines = qMax(1, (settings.value("promptHeight", fontMetrics().lineSpacing() * 2 + 10).toInt() - 10)
        / qMax(1, fontMetrics().lineSpacing()));
    spin("prompt_line_count", tr("Prompt Line Count"), tr("Size of the text editor for image descriptions"), 1, 40, legacyLines);
    toggle("show_negative_prompt", tr("Negative Prompt"), tr("Show text editor to describe things to avoid"), false, true);
    toggle("show_steps", tr("Show Steps"), tr("Display the number of steps to be evaluated in the weights box."), false);
    spin("recent_styles_count", tr("Recent Styles"), tr("Number of most recently used styles to show at the top of the style list"), 0, 10, 4);
    auto tools = new QWidget(m_content);
    auto toolLayout = new QHBoxLayout(tools);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    auto enabled = new QLabel(tools);
    enabled->setObjectName("tagCompletionStatus");
    toolLayout->addWidget(enabled);
    for (const auto& entry : QList<QPair<QString, QString>> {
             { "reload-preset", tr("Look for new tag files") },
             { "document-open", tr("Open folder where custom tag files can be placed") } }) {
        auto button = new QToolButton(tools);
        PluginUi::setIcon(button, entry.first);
        button->setToolTip(entry.second);
        button->setAccessibleName(entry.second);
        toolLayout->addWidget(button);
        if (entry.first == "reload-preset") connect(button, &QToolButton::clicked, this, [this] {
            rebuildTags();
            TagModel::shared()->reload(QSettings("BaronEdition", "Orchestrion")
                .value("tagFiles", QStringList { "Danbooru", "e621" }).toStringList(), true);
            emit changed("tagFiles", QSettings("BaronEdition", "Orchestrion").value("tagFiles"));
        });
        else connect(button, &QToolButton::clicked, this, [this] {
            const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags";
            QDir().mkpath(folder);
#ifdef Q_OS_ANDROID
            const auto files = QFileDialog::getOpenFileNames(this, tr("Import tag files"), {}, "CSV (*.csv)");
            for (const auto& path : files) {
                QFile source(path);
                if (!source.open(QIODevice::ReadOnly) || source.size() > 8 * 1024 * 1024) continue;
                const QString target = folder + "/" + QFileInfo(path).fileName();
                if (QFile::exists(target)) {
                    QMessageBox::information(this, tr("Tag Auto-Completion"), tr("A tag file with this name already exists."));
                    continue;
                }
                QFile output(target);
                if (output.open(QIODevice::WriteOnly)) output.write(source.readAll());
            }
            rebuildTags();
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
        });
    }
    row(tr("Tag Auto-Completion"), tr("Enable text completion for tags from the selected files"), tools);
    m_tags = new QWidget(m_content);
    m_tags->setObjectName("tagDatasetList");
    new QHBoxLayout(m_tags);
    m_tags->layout()->setContentsMargins(8, 0, 0, 0);
    m_rows->addWidget(m_tags);
    rebuildTags();
    m_keys.append("tagFiles");
    combo("generation_finished_action", tr("Finished Generation"),
        tr("Action to take when an image generation job finishes"),
        { { tr("Do Nothing"), "none" }, { tr("Preview"), "preview" }, { tr("Apply"), "apply" } }, "preview");
    combo("history_click_behavior", tr("Result interaction"),
        tr("Click an empty area of the history to hide the preview"),
        { { tr("Toggle preview; double-click to apply"), "toggle_preview" },
            { tr("Apply with a second click"), "apply_on_second_click" } }, "toggle_preview");
    const QList<QPair<QString, QString>> applyChoices { { tr("Modify active layer"), "replace" },
        { tr("New layer on top"), "layer" }, { tr("New layer above active"), "layer_active" } };
    const QList<QPair<QString, QString>> regionChoices { { tr("Do not update regions"), "none" },
        { tr("Modify region layers"), "replace" }, { tr("Layer group"), "layer_group" },
        { tr("Layer group + mask"), "transparency_mask" }, { tr("Layer group (don't hide)"), "no_hide" } };
    combo("apply_behavior", tr("Apply Behavior"),
        tr("Choose how result images are applied to the canvas (generation workspaces)"), applyChoices, "layer");
    combo("apply_region_behavior", {}, {}, regionChoices, "layer_group");
    const auto unavailable = tr("Live mode is not available in this Android version yet.");
    auto live = combo("apply_behavior_live", tr("Apply Behavior (Live)"),
        tr("Choose how result images are applied to the canvas in Live mode"), applyChoices, "replace");
    auto liveRegion = combo("apply_region_behavior_live", {}, {}, regionChoices, "replace");
    auto liveSeed = toggle("new_seed_after_apply", tr("Live: New Seed after Apply"),
        tr("Pick a new seed after copying the result to the canvas in Live mode"), false);
    for (auto control : QList<QWidget*> { live, liveRegion, liveSeed }) {
        control->setEnabled(false);
        control->setToolTip(unavailable);
    }
    m_format = combo("save_image_format", tr("Save Image Format"), tr("File format for saved images from thumbnails."),
        { { tr("PNG (fast)"), "png" }, { "PNG", "png_small" }, { "WebP", "webp" },
            { tr("WebP (lossless)"), "webp_lossless" }, { "JPEG", "jpeg" } }, "png_small");
    m_metadata = toggle("save_image_metadata", tr("Save Image Metadata"),
        tr("When saving generated images from thumbnails, include metadata in the PNG"), false);
    auto updateMetadata = [this] { m_metadata->setEnabled(m_format->currentData().toString().startsWith("png")); };
    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateMetadata);
    updateMetadata();
    toggle("debug_dump_workflow", tr("Dump Workflow"), tr("Write latest ComfyUI prompt to the log folder for test & debug"), false);
    spin("thumbnailSize", tr("History thumbnail size"), tr("Size of result thumbnails in the history panel"), 64, 256, 96);
    toggle("orientation_layouts_enabled", tr("Layouts by Orientation"),
        tr("Remember panels and toolbars separately for portrait and landscape. Restore automatically when rotating the tablet."), true);
    auto forgetLayouts = new QPushButton(tr("Forget Saved Layouts"), m_content);
    forgetLayouts->setObjectName("forgetOrientationLayouts");
    row({}, {}, forgetLayouts);
    connect(forgetLayouts, &QPushButton::clicked, this, &OrientationLayouts::forgetSaved);
    m_rows->addStretch();
}
void InterfaceSettings::rebuildTags() {
    auto layout = m_tags->layout();
    while (auto item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QSet<QString> files { "Danbooru", "Danbooru NSFW", "e621", "e621 NSFW" };
    const QDir folder(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags");
    for (const auto& file : folder.entryList({ "*.csv" }, QDir::Files)) files.insert(QFileInfo(file).completeBaseName());
    auto names = files.values();
    names.sort(Qt::CaseInsensitive);
    const auto selected = QSettings("BaronEdition", "Orchestrion").value("tagFiles", QStringList { "Danbooru", "e621" }).toStringList();
    for (const auto& file : names) {
        auto checkbox = new QCheckBox(file, m_tags);
        checkbox->setObjectName("tagDataset" + file);
        checkbox->setChecked(selected.contains(file));
        layout->addWidget(checkbox);
        connect(checkbox, &QCheckBox::toggled, this, [this, file](bool enabled) {
            auto files = QSettings("BaronEdition", "Orchestrion").value("tagFiles", QStringList { "Danbooru", "e621" }).toStringList();
            files.removeAll(file);
            if (enabled) files.append(file);
            save("tagFiles", files);
            findChild<QLabel*>("tagCompletionStatus")->setText(files.isEmpty() ? tr("Disabled") : tr("Enabled"));
        });
    }
    static_cast<QHBoxLayout*>(layout)->addStretch();
    findChild<QLabel*>("tagCompletionStatus")->setText(selected.isEmpty() ? tr("Disabled") : tr("Enabled"));
}
void InterfaceSettings::setTranslationLanguages(const QJsonArray& languages) {
    const QSignalBlocker blocker(m_translation);
    m_translation->clear();
    m_translation->addItem(tr("Disabled"), "");
    for (const auto& value : languages) {
        const auto language = value.toObject();
        m_translation->addItem(language["name"].toString(), language["code"].toString());
    }
    m_translation->setCurrentIndex(qMax(0, m_translation->findData(QSettings("BaronEdition", "Orchestrion").value("prompt_translation", ""))));
    m_translation->setEnabled(!languages.isEmpty());
}
void InterfaceSettings::reset() {
    QSettings settings("BaronEdition", "Orchestrion");
    for (const auto& key : m_keys) settings.remove(key);
    settings.remove("showNegative");
    settings.remove("promptHeight");
    settings.setValue("show_negative_prompt", false);
    settings.setValue("tagFiles", QStringList());
    while (auto item = m_rows->takeAt(0)) { delete item->widget(); delete item; }
    const auto keys = m_keys;
    m_keys.clear();
    build();
    for (const auto& key : keys) {
        if (auto spin = findChild<QSpinBox*>(key)) emit changed(key, spin->value());
        else if (auto combo = findChild<QComboBox*>(key)) emit changed(key, combo->currentData());
        else if (auto toggle = findChild<ToggleSwitch*>(key)) emit changed(key, toggle->isChecked());
        else emit changed(key, settings.value(key));
    }
}
