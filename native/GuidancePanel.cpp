// SPDX-License-Identifier: GPL-3.0-or-later
#include "GuidancePanel.h"
#include "BaronPanel.h"
#include "PluginUi.h"
#include <QBuffer>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QMenu>
#include <QResizeEvent>
#include <QSettings>
#include <QSlider>
#include <QToolButton>
#include <QUuid>
#include <QWidgetAction>
#include <algorithm>

namespace {
class LayerChoices : public QComboBox {
public:
    static QString tr(const char* source) {
        return QCoreApplication::translate("GuidancePanel", source);
    }
    LayerChoices(CanvasHost* host, QWidget* parent, bool visible)
        : QComboBox(parent)
        , m_host(host)
        , m_visible(visible) {
        refresh();
    }
    void showPopup() override {
        refresh();
        QComboBox::showPopup();
    }

private:
    void refresh() {
        const auto selected = currentData().toString(), title = currentText();
        QList<QPair<QString, QString>> special;
        for (int i = 0; i < count(); ++i)
            if (itemData(i) == "file" || itemData(i) == "selection")
                special.append({ itemText(i), itemData(i).toString() });
        const QSignalBlocker blocker(this);
        clear();
        for (const auto& item : special)
            addItem(item.first, item.second);
        if (m_visible)
            addItem(tr("Visible"), "visible");
        if (m_host)
            for (auto value : m_host->layers()) {
                const auto layer = value.toObject();
                addItem(layer["name"].toString(), layer["id"].toString());
                if (selected.isEmpty() && layer["active"].toBool())
                    setCurrentIndex(count() - 1);
            }
        if (!selected.isEmpty()) {
            auto index = findData(selected);
            if (index < 0) {
                addItem(title, selected);
                index = count() - 1;
            }
            setCurrentIndex(index);
        }
    }
    CanvasHost* m_host;
    bool m_visible;
};
QComboBox* layerChoices(CanvasHost* host, QWidget* parent, bool visible = false) {
    return new LayerChoices(host, parent, visible);
}
}

class ControlRow : public QWidget {
public:
    static QString tr(const char* source) {
        return QCoreApplication::translate("GuidancePanel", source);
    }
    ControlRow(CanvasHost* host, const QString& type, const QString& title, QWidget* parent)
        : QWidget(parent)
        , mode(type)
        , title(title)
        , id(QUuid::createUuid().toString()) {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        auto row = new QHBoxLayout;
        row->setSpacing(4);
        auto modeSelect = new QComboBox(this);
        modeSelect->setObjectName("controlMode");
        const QList<QPair<QString, QString>> modes { { "reference", tr("Reference") },
            { "style", tr("Style") }, { "composition", tr("Composition") }, { "face", tr("Face") },
            { "scribble", tr("Scribble") }, { "line_art", tr("Line Art") },
            { "soft_edge", tr("Soft Edge") }, { "canny_edge", tr("Canny Edge") },
            { "depth", tr("Depth") }, { "normal", tr("Normal") }, { "pose", tr("Pose") },
            { "segmentation", tr("Segment") }, { "blur", tr("Unblur") },
            { "stencil", tr("Stencil") }, { "hands", tr("Hands") } };
        for (const auto& entry : modes)
            modeSelect->addItem(
                PluginUi::icon("control-" + entry.first, this), entry.second, entry.first);
        modeSelect->setCurrentIndex(modeSelect->findData(mode));
        modeSelect->setStyleSheet(
            "QComboBox { border: none; background: transparent; padding: 1px 12px 1px 2px; }");
        modeSelect->setMinimumContentsLength(7);
        modeSelect->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        row->addWidget(modeSelect);
        layer = layerChoices(host, this, true);
        layer->setObjectName("controlLayer");
        row->addWidget(layer, 1);
        generate = new QToolButton(this);
        generate->setObjectName("controlFromImage");
        generate->setAutoRaise(true);
        PluginUi::setIcon(generate, "control-generate");
        generate->setText(tr("From Image"));
        generate->setToolTip(tr("Generate control layer from current image"));
        generate->setAccessibleName(generate->toolTip());
        row->addWidget(generate);
        generateRegions = new QToolButton(this);
        generateRegions->setAutoRaise(true);
        generateRegions->setObjectName("controlFromRegions");
        generateRegions->setText(tr("From Regions"));
        generateRegions->setToolTip(tr("Generate segmentation control layer from current regions"));
        PluginUi::setIcon(generateRegions, "region-prompt");
        row->addWidget(generateRegions);
        preset = new QSlider(Qt::Horizontal, this);
        preset->setObjectName("controlPreset");
        preset->setRange(0, 4);
        preset->setValue(2);
        preset->setPageStep(2);
        preset->setTickInterval(2);
        preset->setTickPosition(QSlider::TicksBothSides);
        preset->setMinimumWidth(40);
        preset->setToolTip(tr("Control strength: how much the layer affects the image"));
        row->addWidget(preset, 1);
        warning = new QLabel(this);
        warning->hide();
        row->addWidget(warning, 3);
        extend = new QToolButton(this);
        extend->setObjectName("controlAdvanced");
        extend->setAutoRaise(true);
        PluginUi::setIcon(extend, "more");
        extend->setCheckable(true);
        extend->setToolTip(tr("Show/hide advanced settings"));
        row->addWidget(extend);
        remove = new QToolButton(this);
        remove->setAutoRaise(true);
        PluginUi::setIcon(remove, "remove");
        row->addWidget(remove);
        layout->addLayout(row);
        extended = new QWidget(this);
        extended->setObjectName("controlOptions");
        extended->hide();
        auto form = new QFormLayout(extended);
        form->setContentsMargins(18, 2, 4, 6);
        custom = new QCheckBox(tr("Use custom values"), extended);
        custom->setObjectName("controlCustom");
        auto actionRow = new QHBoxLayout;
        actionRow->addWidget(custom, 1);
        generateText = new QToolButton(extended);
        generateText->setObjectName("controlFromImageText");
        generateRegionsText = new QToolButton(extended);
        generateRegionsText->setObjectName("controlFromRegionsText");
        for (const auto& pair : { qMakePair(generateText, generate), qMakePair(generateRegionsText, generateRegions) }) {
            pair.first->setText(pair.second->text());
            pair.first->setIcon(pair.second->icon());
            pair.first->setToolTip(pair.second->toolTip());
            pair.first->setAutoRaise(true);
            pair.first->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            actionRow->addWidget(pair.first);
            QObject::connect(pair.first, &QToolButton::clicked, pair.second, &QToolButton::click);
        }
        form->addRow(actionRow);
        auto source = new QPushButton(tr("Reference images"), extended);
        form->addRow(source);
        QObject::connect(source, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getOpenFileName(
                this, tr("Reference images"), {}, tr("Images (*.png *.jpg *.jpeg *.webp)"));
            if (path.isEmpty())
                return;
            QImageReader reader(path);
            const auto size = reader.size();
            if (!size.isValid() || qint64(size.width()) * size.height() > 67108864)
                return;
            if (qint64(size.width()) * size.height() > 16777216)
                reader.setScaledSize(size.scaled(4096, 4096, Qt::KeepAspectRatio));
            const auto image = reader.read();
            if (image.isNull())
                return;
            layer->addItem(QFileInfo(path).fileName(), "file");
            layer->setCurrentIndex(layer->count() - 1);
            external = image;
        });
        QObject::connect(layer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            if (layer->currentData() != "file")
                external = {};
        });
        strength = new QDoubleSpinBox(this);
        strength->setObjectName("controlStrength");
        strength->setRange(0, 2);
        strength->setSingleStep(.1);
        strength->setValue(1);
        strength->hide();
        strengthSlider = new QSlider(Qt::Horizontal, extended);
        strengthSlider->setObjectName("controlStrengthSlider");
        strengthSlider->setRange(0, 75);
        strengthSlider->setValue(50);
        strengthSlider->setPageStep(10);
        strengthLabel = new QLabel("1.00", extended);
        auto strengthRow = new QHBoxLayout;
        strengthRow->addWidget(strengthSlider, 1);
        strengthRow->addWidget(strengthLabel);
        form->addRow(tr("Strength") + ":", strengthRow);
        auto range = new QHBoxLayout;
        start = new QDoubleSpinBox(this);
        end = new QDoubleSpinBox(this);
        for (auto spin : { start, end }) {
            spin->setRange(0, 1);
            spin->setSingleStep(.05);
            spin->hide();
        }
        end->setValue(1);
        interval = new IntervalSlider(extended);
        interval->setObjectName("controlRange");
        lowLabel = new QLabel("0.00", extended);
        highLabel = new QLabel("1.00", extended);
        range->addWidget(lowLabel);
        range->addWidget(interval, 1);
        range->addWidget(highLabel);
        form->addRow(tr("Range") + ":", range);
        region = new QComboBox(this);
        region->addItem(tr("Whole image"), "");
        form->addRow(tr("Region"), region);
        layout->addWidget(extended);
        QObject::connect(extend, &QToolButton::toggled, extended, &QWidget::setVisible);
        QObject::connect(preset, &QSlider::valueChanged, this, [this] { if (!custom->isChecked()) applyPreset(); });
        QObject::connect(custom, &QCheckBox::toggled, this, [this] {
            preset->setEnabled(!custom->isChecked());
            strengthSlider->setEnabled(custom->isChecked());
            interval->setEnabled(custom->isChecked());
            if (!custom->isChecked()) applyPreset();
        });
        QObject::connect(strengthSlider, &QSlider::valueChanged, this, [this](int value) { strength->setValue(value / 50.0); });
        QObject::connect(strength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
            const QSignalBlocker blocker(strengthSlider);
            strengthSlider->setValue(qRound(value * 50));
            strengthLabel->setText(QString::number(value, 'f', 2));
        });
        QObject::connect(interval, &IntervalSlider::intervalChanged, this, [this](int low, int high) {
            start->setValue(low / 20.0); end->setValue(high / 20.0);
        });
        auto updateRange = [this] {
            const QSignalBlocker blocker(interval);
            interval->setInterval(qRound(start->value() * 20), qRound(end->value() * 20));
            lowLabel->setText(QString::number(start->value(), 'f', 2));
            highLabel->setText(QString::number(end->value(), 'f', 2));
        };
        for (auto spin : { start, end }) QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateRange);
        auto update = [this, modeSelect] {
            mode = modeSelect->currentData().toString();
            this->title = modeSelect->currentText();
            updateArchitecture();
            if (!custom->isChecked()) applyPreset();
        };
        QObject::connect(modeSelect, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [update](int) { update(); });
        update();
        if (type == "reference") { strength->setValue(1); start->setValue(0); end->setValue(1); }
        strengthSlider->setEnabled(false);
        interval->setEnabled(false);
    }
    void applyPreset() {
        QFile file(":/baron/presets/control.json");
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto all = QJsonDocument::fromJson(file.readAll()).object();
        const auto presets = all.value(all.contains(mode) ? mode : "default").toObject();
        const auto values = presets.value(presets.contains(architecture) ? architecture : "all").toArray();
        if (values.isEmpty()) return;
        const double position = preset->value() / 4.0 * (values.size() - 1);
        const int index = qMin(int(position), values.size() - 1);
        const auto lower = values[index].toObject(), upper = values[qMin(index + 1, values.size() - 1)].toObject();
        auto interpolate = [&](const char* key) { return lower[key].toDouble() + (upper[key].toDouble() - lower[key].toDouble()) * (position - index); };
        strength->setValue(int(interpolate("strength") * 50) / 50.0);
        start->setValue(interpolate("start"));
        end->setValue(interpolate("end"));
    }
    void updateArchitecture() {
        const bool reference = QStringList { "reference", "style", "composition", "face" }.contains(mode);
        const bool instruction = architecture == "qwen2" || architecture == "krea2" || architecture.startsWith("flux2_")
            || architecture == "flux_k" || architecture == "qwen_l" || architecture.startsWith("qwen_e");
        supported = !instruction || reference
            || ((architecture == "qwen2" || architecture.startsWith("flux2_"))
                && QStringList { "scribble", "line_art", "canny_edge", "depth", "pose" }.contains(mode));
        preset->setVisible(supported && !instruction);
        extend->setVisible(supported && !instruction);
        layer->setVisible(supported);
        warning->setVisible(!supported);
        warning->setText(supported ? QString() : tr("Not supported for") + " " + architecture);
        const bool small = width() < 420;
        const bool canGenerate = supported && QStringList { "scribble", "line_art", "soft_edge", "canny_edge",
            "depth", "normal", "pose", "segmentation", "hands" }.contains(mode);
        generate->setVisible(canGenerate);
        generateText->setVisible(canGenerate && small);
        generateRegions->setVisible(supported && mode == "segmentation");
        generateRegionsText->setVisible(supported && mode == "segmentation" && small);
        if (instruction || !supported) { extend->setChecked(false); extended->hide(); }
    }
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        updateArchitecture();
    }
    QString mode, title, id, architecture;
    bool supported = true;
    QComboBox *layer, *region;
    QDoubleSpinBox *strength, *start, *end;
    QToolButton *remove, *generate, *generateRegions, *extend, *generateText, *generateRegionsText;
    QSlider *preset, *strengthSlider;
    QCheckBox* custom;
    IntervalSlider* interval;
    QLabel *strengthLabel, *lowLabel, *highLabel, *warning;
    QWidget* extended;
    QImage external;
};
class RegionRow : public QWidget {
public:
    static QString tr(const char* source) {
        return QCoreApplication::translate("GuidancePanel", source);
    }
    RegionRow(CanvasHost* host, QWidget* parent)
        : QWidget(parent)
        , id(QUuid::createUuid().toString()) {
        auto form = new QVBoxLayout(this);
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(0);
        auto heading = new QHBoxLayout;
        heading->setSpacing(2);
        select = new QToolButton(this);
        select->setAutoRaise(true);
        select->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        PluginUi::setIcon(select, "region");
        heading->addWidget(select, 1);
        auto link = new QToolButton(this);
        link->setAutoRaise(true);
        PluginUi::setIcon(link, "link");
        auto menu = new QMenu(link);
        auto action = new QWidgetAction(menu);
        layer = layerChoices(host, this);
        layer->insertItem(0, tr("Current selection"), "selection");
        action->setDefaultWidget(layer);
        menu->addAction(action);
        link->setMenu(menu);
        link->setPopupMode(QToolButton::InstantPopup);
        heading->addWidget(link);
        remove = new QToolButton(this);
        remove->setAutoRaise(true);
        PluginUi::setIcon(remove, "remove");
        heading->addWidget(remove);
        form->addLayout(heading);
        prompt = new PromptEditor(this);
        prompt->setObjectName("regionPrompt");
        prompt->setFixedHeight(fontMetrics().lineSpacing()
            * QSettings("BaronEdition", "Orchestrion").value("prompt_line_count", 2).toInt() + 10);
        prompt->setPlaceholderText(tr("Describe the content you want to see, or leave empty."));
        prompt->setFrameShape(QFrame::NoFrame);
        prompt->hide();
        form->addWidget(prompt);
    }
    QToolButton* select;
    QString id;
    QComboBox* layer;
    QPlainTextEdit* prompt;
    QToolButton* remove;
};

GuidancePanel::GuidancePanel(CanvasHost* host, QWidget* parent)
    : QWidget(parent)
    , m_host(host) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto toolbar = new QHBoxLayout;
    auto control = new QToolButton(this);
    control->setObjectName("addControlLayer");
    control->setToolTip(tr("Add Control Layer"));
    control->setAutoRaise(true);
    PluginUi::setIcon(control, "control-add");
    control->setPopupMode(QToolButton::DelayedPopup);
    auto menu = new QMenu(control);
    control->setMenu(menu);
    const QList<QPair<QString, QString>> modes { { "reference", tr("Reference") },
        { "style", tr("Style") }, { "composition", tr("Composition") }, { "face", tr("Face") },
        { "scribble", tr("Scribble") }, { "line_art", tr("Line Art") },
        { "soft_edge", tr("Soft Edge") }, { "canny_edge", tr("Canny Edge") },
        { "depth", tr("Depth") }, { "normal", tr("Normal") }, { "pose", tr("Pose") },
        { "segmentation", tr("Segment") }, { "blur", tr("Unblur") },
        { "stencil", tr("Stencil") }, { "hands", tr("Hands") } };
    for (const auto& mode : modes) {
        auto action = menu->addAction(PluginUi::icon("control-" + mode.first, this), mode.second);
        connect(action, &QAction::triggered, this,
            [this, mode] { addControl(mode.first, mode.second); });
    }
    connect(control, &QToolButton::clicked, this, [this, modes] {
        const bool edit = m_architecture == "flux_k" || m_architecture == "qwen_l"
            || m_architecture.startsWith("qwen_e");
        const auto selected = edit ? QString("reference") : m_lastControlMode;
        for (const auto& mode : modes)
            if (mode.first == selected) { addControl(mode.first, mode.second); return; }
    });
    toolbar->addWidget(control, 1);
    auto region = new QToolButton(this);
    region->setObjectName("addRegion");
    PluginUi::setIcon(region, "region-add");
    region->setAutoRaise(true);
    region->setToolTip(tr("Add region"));

    toolbar->addWidget(region);
    connect(region, &QToolButton::clicked, this, &GuidancePanel::addRegion);
    layout->addLayout(toolbar);
    m_regionsLayout = new QVBoxLayout;
    m_controlsLayout = new QVBoxLayout;
    layout->addLayout(m_regionsLayout);
    layout->addLayout(m_controlsLayout);
}
void GuidancePanel::addControl(const QString& mode, const QString& title) {
    if (m_controls.size() >= 16)
        return;
    auto row = new ControlRow(m_host, mode, title, this);
    row->architecture = m_architecture;
    row->updateArchitecture();
    if (mode != "reference") row->applyPreset();
    m_controls.append(row);
    m_lastControlMode = mode;
    m_controlsLayout->addWidget(row);
    connect(row->generate, &QToolButton::clicked, this, [this, row] {
        QString issue;
        if (!m_host)
            return;
        const auto canvas = m_host->capture(false, &issue);
        if (canvas.image.isNull()) {
            emit error(issue);
            return;
        }
        const auto image = canvas.image;
        m_pendingControl = row;
        emit controlRequested(QJsonObject { { "mode", "generate" },
            { "operation", "control_image" }, { "control_mode", row->mode },
            { "width", image.width() }, { "height", image.height() },
            { "image", QString::fromLatin1(BaronPanel::png(image).toBase64()) },
            { "target", canvas.id }, { "control_id", row->id } });
    });
    connect(row->generateRegions, &QToolButton::clicked, this, [this, row] {
        if (!m_host) return;
        QString issue;
        const auto canvas = m_host->capture(false, &issue);
        if (canvas.image.isNull()) { emit error(issue); return; }
        QImage result(canvas.image.size(), QImage::Format_ARGB32);
        result.fill(Qt::white);
        QPainter painter(&result);
        const QList<QColor> colors { Qt::white, Qt::red, Qt::green, Qt::blue, Qt::yellow,
            Qt::magenta, Qt::cyan, QColor(80, 80, 80), QColor(160, 80, 0),
            QColor(80, 160, 0), QColor(0, 80, 160), QColor(0, 160, 80) };
        int count = 0;
        const auto layers = m_host->layers();
        for (int i = layers.size() - 1; i >= 0; --i) {
            const auto layer = layers[i].toObject();
            const bool linked = std::any_of(m_regions.cbegin(), m_regions.cend(), [&](const RegionRow* region) {
                return region->layer->currentData().toString() == layer["id"].toString();
            });
            if (!linked) continue;
            const auto image = m_host->layerImage(layer["id"].toString(), canvas.bounds, &issue);
            if (image.isNull()) { emit error(issue); return; }
            QImage colored(image.size(), QImage::Format_ARGB32);
            const auto color = colors[count++ % colors.size()];
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    colored.setPixel(x, y, qRgba(color.red(), color.green(), color.blue(), qAlpha(image.pixel(x, y))));
            painter.drawImage(QPoint(), colored);
        }
        painter.end();
        if (!count) { emit error(tr("Text prompt regions have not been set up.")); return; }
        emit controlMapGenerated(canvas.id, row->id, result);
    });
    updateRegions();
    row->region->setCurrentIndex(qMax(0, row->region->findData(m_activeRegion)));
    connect(row->remove, &QToolButton::clicked, this, [this, row] {
        m_controls.removeOne(row);
        row->deleteLater();
        emit changed();
    });
    for (auto combo : { row->layer, row->region })
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            activateRegion(m_activeRegion);
            emit changed();
        });
    for (auto spin : { row->strength, row->start, row->end })
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this] { emit changed(); });
    connect(row->findChild<QComboBox*>("controlMode"),
        QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, row] {
            m_lastControlMode = row->mode;
            emit changed();
        });
    connect(row->preset, &QSlider::valueChanged, this, [this] { emit changed(); });
    connect(row->custom, &QCheckBox::toggled, this, [this] { emit changed(); });
    activateRegion(m_activeRegion);
    emit changed();
}
void GuidancePanel::addRegion() {
    if (m_regions.size() >= 8)
        return;
    auto row = new RegionRow(m_host, this);
    m_regions.append(row);
    m_regionsLayout->addWidget(row);
    connect(row->remove, &QToolButton::clicked, this, [this, row] {
        m_regions.removeOne(row);
        row->deleteLater();
        if (m_activeRegion == row->id)
            activateRegion({});
        updateRegions();
        emit changed();
    });
    updateRegions();
    connect(row->layer, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this] { emit changed(); });
    connect(row->prompt, &QPlainTextEdit::textChanged, this, [this] { emit changed(); });
    connect(static_cast<PromptEditor*>(row->prompt), &PromptEditor::activated, this,
        &GuidancePanel::activated);
    connect(row->select, &QToolButton::clicked, this, [this, row] { activateRegion(row->id); });
    activateRegion(row->id);
    emit changed();
}
void GuidancePanel::setControlLayer(const QString& id, const QString& controlId) {
    QPointer<ControlRow> selected = m_pendingControl;
    if (!controlId.isEmpty()) {
        selected.clear();
        for (auto row : m_controls)
            if (row->id == controlId)
                selected = row;
    }
    if (!selected || id.isEmpty())
        return;
    selected->external = QImage();
    selected->layer->clear();
    for (auto value : m_host->layers()) {
        const auto layer = value.toObject();
        selected->layer->addItem(layer["name"].toString(), layer["id"].toString());
    }
    selected->layer->setCurrentIndex(selected->layer->findData(id));
    m_pendingControl.clear();
    emit changed();
}
QJsonObject GuidancePanel::state() const {
    QJsonArray controls, regions;
    for (auto row : m_controls)
        controls.append(QJsonObject { { "id", row->id }, { "mode", row->mode },
            { "preset_value", row->preset->value() }, { "use_custom_strength", row->custom->isChecked() },
            { "title", row->title }, { "layer", row->layer->currentData().toString() },
            { "layer_title", row->layer->currentText() },
            { "region", row->region->currentData().toString() },
            { "strength", row->strength->value() }, { "start", row->start->value() },
            { "end", row->end->value() },
            { "external",
                row->external.isNull()
                    ? QString()
                    : QString::fromLatin1(BaronPanel::png(row->external).toBase64()) } });
    for (auto row : m_regions)
        regions.append(
            QJsonObject { { "id", row->id }, { "layer", row->layer->currentData().toString() },
                { "layer_title", row->layer->currentText() },
                { "prompt", row->prompt->toPlainText() } });
    return { { "controls", controls }, { "regions", regions },
        { "active_region", m_activeRegion } };
}
void GuidancePanel::restoreState(const QJsonObject& state) {
    const QSignalBlocker blocker(this);
    for (auto row : m_controls) {
        m_controlsLayout->removeWidget(row);
        delete row;
    }
    for (auto row : m_regions) {
        m_regionsLayout->removeWidget(row);
        delete row;
    }
    m_controls.clear();
    m_regions.clear();
    m_pendingControl.clear();
    activateRegion({});
    auto select = [](QComboBox* combo, const QJsonObject& data) {
        const auto id = data["layer"].toString();
        auto index = combo->findData(id);
        if (index < 0 && !id.isEmpty()) {
            combo->addItem(data["layer_title"].toString(), id);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    };
    for (auto value : state["regions"].toArray()) {
        if (m_regions.size() >= 8)
            break;
        const auto data = value.toObject();
        addRegion();
        auto row = m_regions.last();
        if (!data["id"].toString().isEmpty())
            row->id = data["id"].toString();
        select(row->layer, data);
        row->prompt->setPlainText(data["prompt"].toString().left(65536));
    }
    for (auto value : state["controls"].toArray()) {
        if (m_controls.size() >= 16)
            break;
        const auto data = value.toObject();
        const auto mode = data["mode"].toString();
        if (!QStringList { "reference", "style", "composition", "face", "scribble", "line_art",
                "soft_edge", "canny_edge", "depth", "normal", "pose", "segmentation", "blur",
                "stencil", "hands" }
                .contains(mode))
            continue;
        addControl(mode, data["title"].toString().left(128));
        auto row = m_controls.last();
        if (!data["id"].toString().isEmpty())
            row->id = data["id"].toString();
        select(row->layer, data);
        row->strength->setValue(data["strength"].toDouble(1));
        row->start->setValue(data["start"].toDouble());
        row->end->setValue(data["end"].toDouble(1));
        row->custom->setChecked(data["use_custom_strength"].toBool(true));
        const QSignalBlocker presetBlocker(row->preset);
        row->preset->setValue(data["preset_value"].toInt(2));
        row->region->setCurrentIndex(qMax(0, row->region->findData(data["region"].toString())));
        const auto encoded = data["external"].toString();
        if (encoded.size() <= 8 * 1024 * 1024 && !encoded.isEmpty()) {
            QBuffer buffer;
            buffer.setData(QByteArray::fromBase64(encoded.toLatin1()));
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            const auto size = reader.size();
            if (size.isValid() && qint64(size.width()) * size.height() <= 16777216)
                row->external = reader.read();
        }
    }
    updateRegions();
    const auto selected = state["active_region"].toString();
    const bool exists = std::any_of(m_regions.cbegin(), m_regions.cend(),
        [&selected](const RegionRow* row) { return row->id == selected; });
    activateRegion(exists ? selected : QString());
}
void GuidancePanel::updateRegions() {
    for (auto control : m_controls) {
        const auto selected = control->region->currentData();
        control->region->clear();
        control->region->addItem(tr("Whole image"), "");
        for (int i = 0; i < m_regions.size(); ++i)
            control->region->addItem(tr("Region %1").arg(i), m_regions[i]->id);
        const auto index = control->region->findData(selected);
        control->region->setCurrentIndex(qMax(0, index));
    }
}
void GuidancePanel::setArchitecture(const QString& family) {
    QString arch = family.toLower();
    if (arch == "illustrious" || arch == "illustrious xl") arch = "illu";
    else if (arch == "pony") arch = "sdxl";
    else if (arch == "sd1.5" || arch == "sd 1.5") arch = "sd15";
    else if (arch == "qwen21" || arch == "qwen-image21" || arch == "qwen image 2.1") arch = "qwen2";
    else if (arch == "krea 2") arch = "krea2";
    else if (arch == "flux 2 klein 9b") arch = "flux2_9b";
    else if (arch == "flux 2 klein 4b") arch = "flux2_4b";
    if (arch == m_architecture) return;
    m_architecture = arch;
    for (auto row : m_controls) {
        row->architecture = arch;
        row->updateArchitecture();
    }
}
void GuidancePanel::setControlBusy(const QString& id, bool busy) {
    for (auto row : m_controls) if (row->id == id) {
        row->generate->setEnabled(!busy);
        row->generateRegions->setEnabled(!busy);
        row->generateText->setEnabled(!busy);
        row->generateRegionsText->setEnabled(!busy);
        row->layer->setEnabled(!busy);
    }
}
QJsonObject GuidancePanel::input(const QRect& bounds, const QImage& selection, QString* error) {
    QJsonArray controls, regions;
    for (auto row : m_controls) {
        if (!row->supported) continue;
        const bool reference
            = QStringList { "reference", "style", "composition", "face" }.contains(row->mode);
        const auto image = row->external.isNull() && m_host
            ? m_host->layerImage(
                  row->layer->currentData().toString(), reference ? QRect() : bounds, error)
            : row->external;
        if (image.isNull()) {
            if (error->isEmpty())
                *error = tr("Choose an image or a Krita layer for each control.");
            return {};
        }
        if (row->start->value() > row->end->value()) {
            *error = tr("Control start must not exceed its end.");
            return {};
        }
        controls.append(QJsonObject { { "mode", row->mode },
            { "image", QString::fromLatin1(BaronPanel::png(image).toBase64()) },
            { "strength", row->strength->value() }, { "start", row->start->value() },
            { "end", row->end->value() }, { "region", row->region->currentData().toString() } });
    }
    for (auto row : m_regions) {
        QImage mask;
        if (row->layer->currentData() == "selection")
            mask = selection;
        else if (m_host) {
            const auto image
                = m_host->layerImage(row->layer->currentData().toString(), bounds, error);
            if (!image.isNull()) {
                mask = QImage(image.size(), QImage::Format_Grayscale8);
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x)
                        mask.scanLine(y)[x] = qAlpha(image.pixel(x, y));
            }
        }
        if (mask.isNull()) {
            if (error->isEmpty())
                *error = tr("Choose a region layer or make a selection first.");
            return {};
        }
        regions.append(QJsonObject { { "id", row->id },
            { "layer", row->layer->currentData().toString() },
            { "layer_title", row->layer->currentText() },
            { "mask", QString::fromLatin1(BaronPanel::png(mask).toBase64()) },
            { "prompt", row->prompt->toPlainText() } });
    }
    return { { "controls", controls }, { "regions", regions } };
}

void GuidancePanel::setRootEditors(QPlainTextEdit* positive, QPlainTextEdit* negative) {
    m_rootPositive = positive;
    m_rootNegative = negative;
    auto button = new QToolButton(this);
    button->setAutoRaise(true);
    button->setObjectName("rootRegionSummary");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    PluginUi::setIcon(button, "region");
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->hide();
    qobject_cast<QVBoxLayout*>(layout())->insertWidget(2, button);
    m_rootSummary = button;
    connect(button, &QToolButton::clicked, this, [this] { activateRegion({}); });
    connect(positive, &QPlainTextEdit::textChanged, this, &GuidancePanel::updateRootSummary);
    updateRootSummary();
}
void GuidancePanel::updateRootSummary() {
    if (auto button = qobject_cast<QToolButton*>(m_rootSummary))
        button->setText(fontMetrics().elidedText(
            m_rootPositive->toPlainText().simplified(), Qt::ElideRight, qMax(80, width() - 60)));
}
void GuidancePanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateRootSummary();
}
void GuidancePanel::activateRegion(const QString& id) {
    m_activeRegion = id;
    if (m_rootPositive)
        m_rootPositive->setVisible(id.isEmpty());
    if (m_rootNegative)
        m_rootNegative->setVisible(id.isEmpty());
    if (m_rootSummary)
        m_rootSummary->setVisible(!id.isEmpty());
    for (int i = 0; i < m_regions.size(); ++i) {
        auto row = m_regions[i];
        const bool active = row->id == id;
        row->prompt->setVisible(active);
        row->select->setText(active ? tr("Region %1 - Regional Prompt").arg(i)
                                    : tr("Region %1").arg(i) + " - "
                    + row->prompt->toPlainText().simplified().left(40));
    }
    for (auto control : m_controls)
        control->setVisible(control->region->currentData().toString() == id);
    emit activeRegionChanged(id);
    emit changed();
}
