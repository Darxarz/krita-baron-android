// SPDX-License-Identifier: GPL-3.0-or-later
// Layout and behavior ported from Acly's ai_diffusion/ui/upscale.py.
#include "UpscaleWidget.h"
#include "InterfaceSettings.h"
#include "PluginUi.h"
#include <QHBoxLayout>
#include <QJsonArray>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>

UpscaleWidget::UpscaleWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("upscaleWorkspace");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 2, 4, 0);
    upscaler = new QComboBox(this);
    upscaler->setObjectName("upscalerSelect");
    upscaler->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    upscaler->setMinimumContentsLength(15);
    upscaler->hide(); // Placed beside the shared workspace selector by BaronPanel.
    m_factorSlider = new QSlider(Qt::Horizontal, this);
    m_factorSlider->setObjectName("upscaleFactorSlider");
    m_factorSlider->setRange(100, 400);
    m_factorSlider->setTickInterval(50);
    m_factorSlider->setTickPosition(QSlider::TicksBelow);
    m_factorSlider->setSingleStep(50);
    m_factorSlider->setPageStep(50);
    factor = new QDoubleSpinBox(this);
    factor->setObjectName("upscaleFactor");
    factor->setRange(1, 4);
    factor->setSingleStep(.5);
    factor->setDecimals(2);
    factor->setPrefix(tr("Scale") + ": ");
    factor->setSuffix("x");
    auto factorRow = new QHBoxLayout;
    factorRow->addWidget(m_factorSlider, 1);
    factorRow->addWidget(factor);
    layout->addLayout(factorRow);
    m_target = new QLabel(this);
    m_target->setObjectName("upscaleTargetSize");
    m_target->setAlignment(Qt::AlignRight);
    m_target->setStyleSheet("color: #888888;");
    layout->addWidget(m_target);
    m_refine = new QGroupBox(tr("Refine upscaled image"), this);
    m_refine->setObjectName("upscaleRefinement");
    m_refine->setCheckable(true);
    m_refine->setChecked(true);
    auto group = new QVBoxLayout(m_refine);
    style = new QComboBox(m_refine);
    style->setObjectName("upscaleStyleSelect");
    style->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    style->setMinimumContentsLength(15);
    auto styleRow = new QHBoxLayout;
    styleRow->addWidget(style, 1);
    auto settings = new QToolButton(m_refine);
    settings->setAutoRaise(true);
    settings->setObjectName("upscaleStyleSettings");
    PluginUi::setIcon(settings, "settings");
    connect(settings, &QToolButton::clicked, this, &UpscaleWidget::configureStyle);
    styleRow->addWidget(settings);
    group->addLayout(styleRow);
    auto strengthRow = [this, group](const QString& label, const QString& name, int low, int high, int initial, QSpinBox*& spin) {
        auto row = new QWidget(m_refine);
        auto layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel(label, row), 1);
        auto slider = new QSlider(Qt::Horizontal, row);
        slider->setObjectName(name + "Slider");
        slider->setRange(low, high);
        slider->setSingleStep(5);
        spin = new QSpinBox(row);
        spin->setObjectName(name);
        spin->setRange(low == 0 ? 0 : 1, 100);
        spin->setSuffix("%");
        layout->addWidget(slider, 2);
        layout->addWidget(spin);
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, slider](int value) {
            const QSignalBlocker block(slider);
            slider->setValue(value);
            emit changed();
        });
        spin->setValue(initial);
        group->addWidget(row);
        return row;
    };
    m_strengthRow = strengthRow(tr("Strength"), "upscaleStrength", 20, 50, 30, m_strength);
    m_guidanceRow = strengthRow(tr("Image guidance"), "upscaleGuidance", 0, 100, 50, m_guidance);
    auto overlapRow = new QHBoxLayout;
    overlapRow->addWidget(new QLabel(tr("Tile Overlap"), m_refine), 2);
    m_overlapMode = new QComboBox(m_refine);
    m_overlapMode->setObjectName("upscaleOverlapMode");
    m_overlapMode->addItem(tr("Automatic"), "auto");
    m_overlapMode->addItem(tr("Custom"), "custom");
    m_overlap = new QSpinBox(m_refine);
    m_overlap->setObjectName("upscaleOverlap");
    m_overlap->setRange(0, 128);
    m_overlap->setSingleStep(8);
    m_overlap->setSuffix(" px");
    m_overlap->setValue(48);
    m_overlap->setEnabled(false);
    overlapRow->addWidget(m_overlapMode);
    overlapRow->addWidget(m_overlap);
    group->addLayout(overlapRow);
    auto promptRow = new QHBoxLayout;
    promptRow->addWidget(new QLabel(tr("Use Prompt"), m_refine));
    m_prompt = new QLabel(m_refine);
    m_prompt->setTextFormat(Qt::PlainText);
    m_prompt->setMinimumWidth(40);
    m_prompt->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    promptRow->addWidget(m_prompt, 1);
    m_warning = new QLabel(m_refine);
    m_warning->setPixmap(PluginUi::icon("warning", this).pixmap(16, 16));
    m_warning->setToolTip(tr("Text prompt regions have not been set up.\nIt is not recommended to use a single text description for tiled upscale,\nunless it can be generally applied to all parts of the image."));
    m_warning->hide();
    promptRow->addWidget(m_warning);
    m_usePrompt = new ToggleSwitch(m_refine, tr("On"), tr("Off"));
    m_usePrompt->setObjectName("upscaleUsePrompt");
    promptRow->addWidget(m_usePrompt);
    group->addLayout(promptRow);
    layout->addWidget(m_refine);
    connect(m_factorSlider, &QSlider::valueChanged, this, [this](int value) {
        const int snapped = int(std::nearbyint(value / 50.0)) * 50;
        if (value != snapped) m_factorSlider->setValue(snapped);
        else factor->setValue(value / 100.0);
    });
    connect(factor, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        const QSignalBlocker block(m_factorSlider);
        m_factorSlider->setValue(int(value * 100));
        updateTarget();
        emit changed();
    });
    connect(m_overlapMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_overlap->setEnabled(index == 1);
        emit changed();
    });
    connect(m_overlap, QOverload<int>::of(&QSpinBox::valueChanged), this, &UpscaleWidget::changed);
    connect(m_refine, &QGroupBox::toggled, this, &UpscaleWidget::changed);
    connect(m_usePrompt, &QAbstractButton::toggled, this, &UpscaleWidget::changed);
    connect(upscaler, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (upscaler->currentIndex() >= 0) m_savedUpscaler = upscaler->currentData().toString();
        emit changed();
    });
    factor->setValue(2);
}
double UpscaleWidget::effectiveStrength() const {
    return QStringList { "flux_k", "qwen_e", "qwen_e_p", "qwen_l" }.contains(m_arch)
        ? 1 : m_strength->value() / 100.0;
}
QJsonObject UpscaleWidget::state() const {
    return { { "model", m_savedUpscaler }, { "factor", factor->value() },
        { "use_diffusion", m_refine->isChecked() }, { "strength", m_strength->value() / 100.0 },
        { "unblur_strength", m_guidanceRow->isEnabled() ? m_guidance->value() / 100.0 : 0 },
        { "guidance", m_guidance->value() / 100.0 },
        { "tile_overlap_mode", m_overlapMode->currentData().toString() }, { "tile_overlap", m_overlap->value() },
        { "use_prompt", m_usePrompt->isChecked() } };
}
void UpscaleWidget::restore(const QJsonObject& data) {
    m_savedUpscaler = data["model"].toString();
    upscaler->setCurrentIndex(upscaler->findData(m_savedUpscaler));
    factor->setValue(data["factor"].toDouble(2));
    m_refine->setChecked(data["use_diffusion"].toBool(true));
    m_strength->setValue(qRound(data["strength"].toDouble(.3) * 100));
    m_guidance->setValue(qRound(data["guidance"].toDouble(data["unblur_strength"].toDouble(.5)) * 100));
    m_overlapMode->setCurrentIndex(data["tile_overlap_mode"] == "custom" ? 1 : 0);
    m_overlap->setValue(data["tile_overlap"].toInt(48));
    m_usePrompt->setChecked(data["use_prompt"].toBool());
}
void UpscaleWidget::setResources(const QJsonObject& resources) {
    m_resources = resources;
    QStringList names;
    for (auto name : resources["upscalers"].toArray()) names.append(name.toString());
    const QStringList preferred { "4x_NMKD-Superscale-SP_178000_G.pth", "OmniSR_X4_DIV2K.safetensors", "HAT_SRx4_ImageNet-pretrain.pth", "Real_HAT_GAN_sharper.pth" };
    std::sort(names.begin(), names.end(), [preferred](const QString& a, const QString& b) {
        const auto rank = [preferred](const QString& name) { const int index = preferred.indexOf(name); return index < 0 ? preferred.size() : index; };
        return rank(a) == rank(b) ? a < b : rank(a) < rank(b);
    });
    const QSignalBlocker block(upscaler);
    upscaler->clear();
    for (const auto& name : names) {
        if (name == "OmniSR_X2_DIV2K.safetensors" || name == "OmniSR_X3_DIV2K.safetensors") continue;
        const int index = preferred.indexOf(name);
        auto title = name;
        title.remove(QRegularExpression("\\.(pth|safetensors)$"));
        if (index >= 0) title = QString("%1 (%2)").arg(QStringList { tr("Default"), tr("Fast"), tr("Quality"), tr("Sharp") }[index], title);
        upscaler->addItem(title, name);
    }
    int index = upscaler->findData(m_savedUpscaler);
    upscaler->setCurrentIndex(index >= 0 ? index : 0);
    m_savedUpscaler = upscaler->currentData().toString();
    setArchitecture(m_arch);
}
void UpscaleWidget::setArchitecture(const QString& family) {
    auto arch = family.toLower();
    if (arch == "illustrious" || arch == "illustrious xl") arch = "illu";
    else if (arch == "pony") arch = "sdxl";
    else if (arch == "sd1.5" || arch == "sd 1.5") arch = "sd15";
    else if (arch == "qwen21" || arch == "qwen-image21" || arch == "qwen image 2.1") arch = "qwen2";
    else if (arch == "krea 2") arch = "krea2";
    else if (arch == "flux 2 klein 9b") arch = "flux2_9b";
    else if (arch == "flux 2 klein 4b") arch = "flux2_4b";
    m_arch = arch;
    const bool edit = QStringList { "flux_k", "qwen_e", "qwen_e_p", "qwen_l" }.contains(arch);
    bool blur = false;
    const auto resources = m_resources["resources"].toObject();
    for (auto it = resources.begin(); it != resources.end(); ++it)
        if ((it.key() == "controlnet-blur-" + arch || (arch == "illu_v" && it.key() == "controlnet-blur-illu")
            || it.key() == "model_patch-blur-" + arch
            || it.key() == "controlnet-universal-" + arch || it.key() == "model_patch-universal-" + arch)
            && !it.value().isNull()) blur = true;
    m_strengthRow->setEnabled(!edit);
    m_strengthRow->setToolTip(edit ? tr("Not supported for edit models") : QString());
    m_guidanceRow->setEnabled(!edit && blur);
    m_guidanceRow->setToolTip(edit ? tr("Not supported for edit models") : blur
        ? tr("When enabled, the low resolution image is used as guidance for refining the upscaled image.\nThis produces results which are closer to the original while enhancing local details.")
        : tr("The tile/unblur control model is not installed."));
}
void UpscaleWidget::setCanvasSize(QSize size) { m_canvas = size; updateTarget(); }
void UpscaleWidget::updateTarget() {
    m_target->setText(m_canvas.isValid() ? tr("Target size") + QString(": %1 x %2")
        .arg(int(std::nearbyint(m_canvas.width() * factor->value())))
        .arg(int(std::nearbyint(m_canvas.height() * factor->value()))) : QString());
}
void UpscaleWidget::setPrompt(const QString& prompt, int regionCount) {
    m_prompt->setText(regionCount > 0 ? QString("%1 %2 | %3").arg(regionCount).arg(tr("Regions"), prompt) : prompt);
    m_prompt->setToolTip(prompt);
    m_warning->setVisible(m_usePrompt->isChecked() && regionCount == 0);
}
