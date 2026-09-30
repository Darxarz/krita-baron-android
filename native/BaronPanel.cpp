// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include "Localization.h"
#include <QBuffer>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSettings>
#include <QSplitter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>

namespace {
class ModelCardDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(210, 254);
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const bool selected = option.state & QStyle::State_Selected;
        const auto card = option.rect.adjusted(3, 3, -3, -3);
        painter->setPen(
            selected ? option.palette.highlight().color() : option.palette.mid().color());
        painter->setBrush(selected ? option.palette.highlight() : option.palette.base());
        painter->drawRoundedRect(card, 10, 10);
        const auto data = index.data(Qt::UserRole).toJsonObject();
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const QRect artwork(card.left() + 12, card.top() + 12, card.width() - 24, 150);
        if (!icon.isNull())
            icon.paint(painter, artwork, Qt::AlignCenter);
        else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(option.palette.alternateBase());
            painter->drawRoundedRect(artwork, 6, 6);
            auto placeholderFont = option.font;
            placeholderFont.setPointSize(32);
            placeholderFont.setBold(true);
            painter->setFont(placeholderFont);
            painter->setPen(option.palette.mid().color());
            painter->drawText(
                artwork, Qt::AlignCenter, data["family"].toString().left(1).toUpper());
        }
        painter->setPen(
            selected ? option.palette.highlightedText().color() : option.palette.text().color());
        auto font = option.font;
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(QRect(card.left() + 12, card.top() + 170, card.width() - 24, 42),
            Qt::TextWordWrap | Qt::AlignTop, data["title"].toString());
        font.setBold(false);
        painter->setFont(font);
        painter->drawText(QRect(card.left() + 12, card.bottom() - 26, card.width() - 24, 20),
            Qt::AlignLeft | Qt::AlignVCenter,
            painter->fontMetrics().elidedText(
                data["family"].toString() + (data["kind"] == "lora" ? " · LoRA" : ""),
                Qt::ElideRight, card.width() - 24));
        painter->restore();
    }
};
QFormLayout* section(QFormLayout* parent, const QString& title, bool expanded = false) {
    auto header = new QToolButton(parent->parentWidget());
    header->setText(title);
    header->setCheckable(true);
    header->setChecked(expanded);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header->setMinimumHeight(40);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    parent->addRow(header);
    auto box = new QWidget(parent->parentWidget());
    parent->addRow(box);
    auto layout = new QFormLayout(box);
    layout->setContentsMargins(8, 4, 8, 10);
    layout->setVerticalSpacing(10);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    box->setVisible(expanded);
    QObject::connect(header, &QToolButton::toggled, box, [box, header](bool show) {
        box->setVisible(show);
        header->setArrowType(show ? Qt::DownArrow : Qt::RightArrow);
    });
    return layout;
}
}

BaronPanel::BaronPanel(CanvasHost* host, QWidget* parent)
    : QWidget(parent)
    , m_host(host) {
    BaronLocalization::install();
    setObjectName("BaronOrchestrionPanel");
    m_client = new OrchestrionClient(this);
    auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    auto heading = new QLabel("<b>Orchestrion · Baron Edition</b>", this);
    layout->addWidget(heading);
    auto tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);
    auto connection = new QWidget(tabs);
    auto connectLayout = new QFormLayout(connection);
    m_url = new QLineEdit("https://orchestrion.su", connection);
    m_url->setObjectName("websiteAddress");
    connectLayout->addRow(tr("Website"), m_url);
    auto signIn = new QPushButton(tr("Sign in through your browser"), connection);
    signIn->setMinimumHeight(42);
    connectLayout->addRow(signIn);
    m_account = new QLabel(tr("Not connected"), connection);
    m_account->setTextFormat(Qt::PlainText);
    m_account->setWordWrap(true);
    connectLayout->addRow(m_account);
    auto logout = new QPushButton(tr("Sign out"), connection);
    connectLayout->addRow(logout);
    connect(signIn, &QPushButton::clicked, this, [this] {
        if (m_client->setRoot(m_url->text()))
            m_client->signIn();
    });
    connect(logout, &QPushButton::clicked, m_client, &OrchestrionClient::signOut);
    connect(m_client, &OrchestrionClient::browserLogin, this,
        [this](const QUrl& url, const QString& code) {
            m_account->setText(tr("Confirm connection in your browser. Code: %1").arg(code));
            QDesktopServices::openUrl(url);
        });
    connect(m_client, &OrchestrionClient::authenticated, this, [this, tabs] {
        saveSettings();
        tabs->setCurrentIndex(0);
    });
    connect(m_client, &OrchestrionClient::accountReady, this, [this](const auto& data) {
        m_account->setText(data.isEmpty() ? tr("Not connected")
                                          : tr("%1 · %2 bleatbucks")
                                                .arg(data["name"].toString())
                                                .arg(data["coins"].toDouble(), 0, 'g', 8));
    });

    auto scroll = new QScrollArea(tabs);
    scroll->setWidgetResizable(true);
    auto generation = new QWidget(scroll);
    scroll->setWidget(generation);
    auto generationLayout = new QVBoxLayout(generation);
    generationLayout->setContentsMargins(0, 0, 0, 0);
    auto formBody = new QWidget(generation);
    formBody->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    generationLayout->addWidget(formBody);
    generationLayout->addStretch(1);
    auto form = new QFormLayout(formBody);
    form->setFormAlignment(Qt::AlignTop);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_mode = new QComboBox(generation);
    for (const auto& entry :
        QList<QPair<QString, QString>> { { tr("Generation"), "generate" }, { tr("Edit"), "edit" },
            { tr("Upscale"), "upscale" }, { tr("Separate background"), "background" } })
        m_mode->addItem(entry.first, entry.second);
    form->addRow(tr("Workspace"), m_mode);
    m_model = new QComboBox(generation);
    m_model->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_model->setMinimumContentsLength(15);
    auto modelRow = new QWidget(generation);
    auto modelLayout = new QHBoxLayout(modelRow);
    modelLayout->setContentsMargins(0, 0, 0, 0);
    auto chooseModel = new QPushButton(tr("Browse…"), modelRow);
    chooseModel->setMinimumHeight(40);
    chooseModel->setObjectName("chooseModelButton");
    modelLayout->addWidget(m_model, 1);
    modelLayout->addWidget(chooseModel);
    form->addRow(tr("Model"), modelRow);
    connect(chooseModel, &QPushButton::clicked, this, [this] { openCatalog("checkpoint"); });
    auto chooseLoras = new QPushButton(tr("Choose LoRAs…"), generation);
    chooseLoras->setMinimumHeight(40);
    chooseLoras->setObjectName("chooseLorasButton");
    form->addRow(chooseLoras);
    connect(chooseLoras, &QPushButton::clicked, this, [this] { openCatalog("lora"); });
    m_prompt = new QPlainTextEdit(generation);
    m_prompt->setPlaceholderText(tr("Describe the image or the change you want"));
    m_prompt->setFixedHeight(100);
    form->addRow(tr("Prompt"), m_prompt);
    m_negative = new QPlainTextEdit(generation);
    m_negative->setFixedHeight(54);
    auto promptOptions = section(form, tr("Prompt options"));
    promptOptions->addRow(tr("Negative prompt"), m_negative);
    auto imageSettings = section(form, tr("Image settings"));
    auto sizeRow = new QWidget(generation);
    sizeRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    m_width = new QSpinBox(sizeRow);
    m_height = new QSpinBox(sizeRow);
    for (auto spin : { m_width, m_height }) {
        spin->setRange(64, 8192);
        spin->setSingleStep(64);
        spin->setValue(1024);
        sizeLayout->addWidget(spin);
    }
    imageSettings->addRow(tr("Size"), sizeRow);
    m_strength = new QDoubleSpinBox(generation);
    m_strength->setRange(0.05, 1);
    m_strength->setSingleStep(.05);
    m_strength->setValue(1);
    imageSettings->addRow(tr("Strength"), m_strength);
    m_scale = new QDoubleSpinBox(generation);
    m_scale->setRange(1, 4);
    m_scale->setSingleStep(.25);
    m_scale->setValue(2);
    imageSettings->addRow(tr("Scale"), m_scale);
    auto advancedForm = section(form, tr("Sampling settings"));
    m_steps = new QSpinBox(generation);
    m_steps->setRange(1, 100);
    m_steps->setValue(20);
    advancedForm->addRow(tr("Steps"), m_steps);
    m_cfg = new QDoubleSpinBox(generation);
    m_cfg->setRange(0, 30);
    m_cfg->setValue(4);
    advancedForm->addRow(tr("CFG"), m_cfg);
    m_seed = new QLineEdit("-1", generation);
    advancedForm->addRow(tr("Seed"), m_seed);
    m_batch = new QSpinBox(generation);
    m_batch->setRange(1, 4);
    m_batch->setValue(1);
    advancedForm->addRow(tr("Images"), m_batch);
    auto referenceForm = section(form, tr("References"));
    auto kreaForm = section(referenceForm, tr("Krea 2 reference settings"));
    m_referencePurpose = new QComboBox(generation);
    m_referencePurpose->addItem(tr("Identity"), "identity");
    m_referencePurpose->addItem(tr("Style"), "style");
    kreaForm->addRow(tr("Reference purpose"), m_referencePurpose);
    m_fidelity = new QDoubleSpinBox(generation);
    m_fidelity->setRange(0, 12);
    m_fidelity->setValue(4);
    kreaForm->addRow(tr("Reference fidelity"), m_fidelity);
    m_referenceDetail = new QComboBox(generation);
    for (int detail : { 512, 768, 1024 })
        m_referenceDetail->addItem(QString::number(detail), detail);
    m_referenceDetail->setCurrentIndex(1);
    kreaForm->addRow(tr("Reference detail"), m_referenceDetail);
    connect(m_model, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        const auto name = m_model->currentData().toString().toLower();
        if (name.contains("krea")) {
            m_cfg->setValue(1);
            m_steps->setValue(10);
        } else if (name.contains("qwen")) {
            m_cfg->setValue(1);
            m_steps->setValue(25);
        } else if (name.contains("klein")) {
            m_cfg->setValue(1);
            m_steps->setValue(4);
        } else {
            m_cfg->setValue(4);
            m_steps->setValue(20);
        }
    });
    m_refs = new QListWidget(generation);
    m_refs->setFixedHeight(88);
    m_refs->setIconSize(QSize(64, 64));
    m_refs->hide();
    auto addRefs = new QPushButton(tr("Add reference images…"), generation);
    referenceForm->addRow(addRefs);
    referenceForm->addRow(m_refs);
    auto clearRefs = new QPushButton(tr("Clear references"), generation);
    referenceForm->addRow(clearRefs);
    clearRefs->hide();
    connect(addRefs, &QPushButton::clicked, this, &BaronPanel::addReferences);
    connect(clearRefs, &QPushButton::clicked, this, [this, clearRefs] {
        m_referenceImages.clear();
        m_refs->clear();
        m_refs->hide();
        clearRefs->hide();
    });
    connect(addRefs, &QPushButton::clicked, this, [this, clearRefs] {
        m_refs->setVisible(!m_referenceImages.isEmpty());
        clearRefs->setVisible(!m_referenceImages.isEmpty());
    });
    auto hint = new QLabel(
        tr("References are numbered in their listed order. Describe what to copy from each image. "
           "In Edit, the current canvas is the source; an active selection limits the change."),
        generation);
    hint->setWordWrap(true);
    m_prompt->setToolTip(hint->text());
    referenceForm->addRow(hint);
    m_price = new QLabel(tr("Price estimate appears here"), generation);
    form->addRow(m_price);
    auto actions = new QWidget(generation);
    auto actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    m_quote = new QPushButton(tr("Estimate price"), actions);
    m_run = new QPushButton(tr("Generate"), actions);
    m_run->setMinimumHeight(44);
    m_run->setObjectName("generateButton");
    actionLayout->addWidget(m_quote);
    actionLayout->addWidget(m_run);
    form->addRow(actions);
    connect(m_run, &QPushButton::clicked, this, [this] { prepare(true); });
    connect(m_quote, &QPushButton::clicked, this, [this] { prepare(false); });
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this] { updateMode(); });

    auto catalogContainer = new QWidget(tabs);
    m_catalogContainer = new QVBoxLayout(catalogContainer);
    auto fullscreen = new QPushButton(tr("Open full-screen model gallery"), catalogContainer);
    fullscreen->setObjectName("openGalleryButton");
    fullscreen->setMinimumHeight(44);
    m_catalogContainer->addWidget(fullscreen);
    connect(fullscreen, &QPushButton::clicked, this, [this] { openCatalog("all"); });
    auto catalog = new QWidget(catalogContainer);
    m_catalogView = catalog;
    auto catalogLayout = new QVBoxLayout(catalog);
    m_catalogContainer->addWidget(catalog, 1);
    m_catalogKind = new QComboBox(catalog);
    m_catalogKind->addItem(tr("All models"), "all");
    m_catalogKind->addItem(tr("Checkpoints"), "checkpoint");
    m_catalogKind->addItem(tr("LoRAs"), "lora");
    catalogLayout->addWidget(m_catalogKind);
    m_filter = new QLineEdit(catalog);
    m_filter->setPlaceholderText(tr("Search models and LoRAs"));
    m_filter->setObjectName("modelSearchField");
    m_filter->setMinimumHeight(40);
    catalogLayout->addWidget(m_filter);
    m_gallery = new QListWidget(catalog);
    m_gallery->setViewMode(QListView::IconMode);
    m_gallery->setResizeMode(QListView::Adjust);
    m_gallery->setIconSize(QSize(160, 160));
    m_gallery->setGridSize(QSize(210, 254));
    m_gallery->setWordWrap(true);
    m_gallery->setSpacing(8);
    m_gallery->setItemDelegate(new ModelCardDelegate(m_gallery));
    m_gallery->setObjectName("modelGallery");
    catalogLayout->addWidget(m_gallery, 1);
    QScroller::grabGesture(m_gallery->viewport(), QScroller::TouchGesture);
    m_thumbnails = new QNetworkAccessManager(this);
    connect(m_gallery->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this] { loadThumbnails(); });
    connect(m_catalogKind, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this] { filterCatalog(); });
    m_loras = new QListWidget(catalog);
    m_loras->setMaximumHeight(96);
    m_loras->hide();
    catalogLayout->addWidget(m_loras);
    auto loraSettings = new QWidget(catalog);
    auto loraSettingsLayout = new QHBoxLayout(loraSettings);
    loraSettingsLayout->setContentsMargins(0, 0, 0, 0);
    auto loraStrength = new QDoubleSpinBox(loraSettings);
    loraStrength->setRange(-2, 2);
    loraStrength->setSingleStep(.1);
    loraStrength->setValue(1);
    loraSettingsLayout->addWidget(new QLabel(tr("LoRA strength"), loraSettings));
    loraSettingsLayout->addWidget(loraStrength);
    auto clearLoras = new QPushButton(tr("Clear LoRAs"), catalog);
    loraSettingsLayout->addWidget(clearLoras);
    catalogLayout->addWidget(loraSettings);
    loraSettings->hide();
    auto selectionChanged = [this, loraSettings, chooseLoras] {
        const auto count = m_loras->count();
        m_loras->setVisible(count > 0);
        loraSettings->setVisible(count > 0);
        chooseLoras->setText(count ? tr("LoRAs: %1 · Change…").arg(count) : tr("Choose LoRAs…"));
    };
    connect(m_loras->model(), &QAbstractItemModel::rowsInserted, this, selectionChanged);
    connect(m_loras->model(), &QAbstractItemModel::rowsRemoved, this, selectionChanged);
    connect(m_loras->model(), &QAbstractItemModel::modelReset, this, selectionChanged);
    connect(m_loras, &QListWidget::currentItemChanged, this, [loraStrength](QListWidgetItem* item) {
        if (item)
            loraStrength->setValue(item->data(Qt::UserRole).toJsonObject()["strength"].toDouble(1));
    });
    connect(loraStrength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this](double value) {
            if (auto item = m_loras->currentItem()) {
                auto data = item->data(Qt::UserRole).toJsonObject();
                data["strength"] = value;
                item->setData(Qt::UserRole, data);
                item->setText(data["title"].toString() + " · " + QString::number(value));
            }
        });
    connect(clearLoras, &QPushButton::clicked, m_loras, &QListWidget::clear);
    connect(m_gallery, &QListWidget::itemClicked, this, [this, tabs](QListWidgetItem* item) {
        const auto data = item->data(Qt::UserRole).toJsonObject();
        if (data["kind"] == "lora") {
            for (int i = 0; i < m_loras->count(); ++i)
                if (m_loras->item(i)->data(Qt::UserRole).toJsonObject()["name"] == data["name"])
                    return;
            if (m_loras->count() >= 5) {
                m_status->setText(tr("Up to five LoRAs can be selected"));
                return;
            }
            auto lora = new QListWidgetItem(data["title"].toString(), m_loras);
            lora->setData(Qt::UserRole, data);
            m_loras->setCurrentItem(lora);
        } else {
            m_model->setCurrentIndex(m_model->findData(data["name"].toString()));
            tabs->setCurrentIndex(0);
            if (auto dialog = qobject_cast<QDialog*>(m_catalogView->parentWidget()))
                dialog->accept();
        }
    });
    connect(m_filter, &QLineEdit::textChanged, this, [this] { filterCatalog(); });
    connect(m_client, &OrchestrionClient::modelsReady, this, &BaronPanel::showModels);

    auto results = new QWidget(tabs);
    auto resultsLayout = new QVBoxLayout(results);
    m_preview = new QLabel(results);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(200, 200);
    resultsLayout->addWidget(m_preview, 1);
    m_results = new QListWidget(results);
    m_results->setIconSize(QSize(88, 88));
    m_results->setMaximumHeight(120);
    resultsLayout->addWidget(m_results);
    connect(m_results, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_result = item->data(Qt::UserRole).value<QImage>();
        m_preview->setPixmap(QPixmap::fromImage(
            m_result.scaled(640, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    });
    m_apply = new QPushButton(tr("Apply as new layer"), results);
    m_apply->setMinimumHeight(44);
    m_apply->setEnabled(false);
    resultsLayout->addWidget(m_apply);
    m_resume = new QPushButton(tr("Retrieve the existing result"), results);
    m_resume->hide();
    resultsLayout->addWidget(m_resume);
    connect(m_resume, &QPushButton::clicked, this, [this] {
        if (m_pendingJob.isEmpty() || !m_client->signedIn())
            return;
        m_busy = true;
        m_resume->hide();
        m_run->setEnabled(false);
        m_quote->setEnabled(false);
        m_jobImages.clear();
        m_results->clear();
        m_progress->show();
        m_client->resume(m_pendingJob);
        m_status->setText(tr("Retrieving the existing job. No new generation is started."));
    });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        QString issue;
        if (!m_host
            || !m_host->apply(m_targetId, m_resultMask.isNull() ? m_result : m_jobImages.value(0),
                m_resultMask, tr("Orchestrion result"), &issue))
            m_status->setText(issue);
        else
            m_status->setText(tr("Result added as a new layer. You can undo this in Krita."));
    });
    tabs->addTab(scroll, tr("Generation"));
    tabs->addTab(catalogContainer, tr("Models"));
    tabs->addTab(results, tr("Results"));
    tabs->addTab(connection, tr("Connection"));
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    connect(m_client, &OrchestrionClient::error, this, [this](const QString& issue) {
        m_busy = false;
        m_run->setEnabled(true);
        m_quote->setEnabled(true);
        m_progress->hide();
        m_status->setText(issue);
        m_resume->setVisible(!m_pendingJob.isEmpty());
    });
    connect(m_client, &OrchestrionClient::prepared, this, [this](const QJsonObject& data) {
        m_price->setText(tr("≈ %1 bleatbucks").arg(data["coins"].toDouble(), 0, 'g', 8));
        if (m_submitAfterPrepare)
            m_client->submit(data["prompt"].toObject());
        else {
            m_busy = false;
            m_run->setEnabled(true);
            m_quote->setEnabled(true);
            m_progress->hide();
            m_status->setText(tr("Estimate ready. No generation was started."));
        }
    });
    connect(m_client, &OrchestrionClient::submitted, this, [this](const QString& id) {
        m_pendingJob = id;
        m_status->setText(tr("Queued on Orchestrion. Waiting for the result…"));
    });
    connect(m_client, &OrchestrionClient::jobReady, this, [this](const QJsonObject& job) {
        if (job["status"].toObject()["status_str"] == "error") {
            m_pendingJob.clear();
            m_resume->hide();
            m_busy = false;
            m_run->setEnabled(true);
            m_quote->setEnabled(true);
            m_progress->hide();
            m_status->setText(tr("Generation failed on the server. No result was applied."));
            return;
        }
        int index = 0;
        const auto outputs = job["outputs"].toObject();
        for (auto it = outputs.begin(); it != outputs.end(); ++it)
            for (auto image : it.value().toObject()["images"].toArray())
                m_client->fetchImage(image.toObject(), index++);
        m_expectedImages = index;
        if (!index) {
            m_busy = false;
            m_run->setEnabled(true);
            m_quote->setEnabled(true);
            m_progress->hide();
            m_status->setText(tr("No images in the server result"));
        }
    });
    connect(m_client, &OrchestrionClient::imageReady, this, &BaronPanel::showImage);
    QSettings settings("BaronEdition", "Orchestrion");
    m_url->setText(settings.value("website", "https://orchestrion.su").toString());
    m_prompt->setPlainText(settings.value("prompt").toString());
    if (m_client->setRoot(m_url->text()))
        m_client->restoreLogin();
    updateMode();
}

QByteArray BaronPanel::png(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
void BaronPanel::saveSettings() {
    QSettings settings("BaronEdition", "Orchestrion");
    settings.setValue("website", m_url->text());
    settings.setValue("prompt", m_prompt->toPlainText());
}
void BaronPanel::updateMode() {
    const auto mode = m_mode->currentData().toString();
    m_run->setText(mode == "generate" ? tr("Generate") : m_mode->currentText());
    m_strength->setEnabled(mode == "edit");
    m_scale->setEnabled(mode == "upscale");
    m_width->setEnabled(mode == "generate");
    m_height->setEnabled(mode == "generate");
    m_batch->setEnabled(mode == "generate");
    m_model->setEnabled(mode == "generate" || mode == "edit");
    for (int i = 0; i < m_refs->count(); ++i)
        m_refs->item(i)->setText(tr("Image %1").arg(i + (mode == "edit" ? 2 : 1)));
}
QJsonObject BaronPanel::input(bool pixels) {
    const auto mode = m_mode->currentData().toString();
    bool validSeed;
    const auto seed = m_seed->text().toLongLong(&validSeed);
    if (!validSeed || seed < -1 || seed > 4294967295LL) {
        m_status->setText(tr("Seed must be -1 or a number from 0 to 4294967295"));
        return {};
    }
    const auto model = m_model->currentData().toString();
    if ((mode == "generate" || mode == "edit") && model.isEmpty()) {
        m_status->setText(tr("Choose a model first"));
        return {};
    }
    QJsonObject data { { "mode", mode }, { "model", model }, { "prompt", m_prompt->toPlainText() },
        { "negative", m_negative->toPlainText() }, { "width", m_width->value() },
        { "height", m_height->value() }, { "steps", m_steps->value() }, { "cfg", m_cfg->value() },
        { "strength", m_strength->value() }, { "scale", m_scale->value() },
        { "batch", mode == "generate" ? m_batch->value() : 1 },
        { "seed", double(seed == -1 ? QRandomGenerator::global()->generate() : seed) } };
    data["reference_purpose"] = m_referencePurpose->currentData().toString();
    data["reference_fidelity"] = m_fidelity->value();
    data["reference_detail"] = m_referenceDetail->currentData().toInt();
    QJsonArray loras;
    for (int i = 0; i < m_loras->count(); ++i) {
        const auto lora = m_loras->item(i)->data(Qt::UserRole).toJsonObject();
        loras.append(
            QJsonObject { { "name", lora["name"] }, { "strength", lora["strength"].toDouble(1) } });
    }
    data["loras"] = loras;
    QJsonArray references;
    for (const auto& image : m_referenceImages)
        references.append(QString::fromLatin1(png(image).toBase64()));
    data["references"] = references;
    if (mode != "generate") {
        QString issue;
        const auto canvas = m_host ? m_host->capture(mode == "edit", &issue) : CanvasSnapshot();
        if (canvas.image.isNull()) {
            m_status->setText(issue.isEmpty() ? tr("Open a Krita document first") : issue);
            return {};
        }
        if (pixels) {
            data["image"] = QString::fromLatin1(png(canvas.image).toBase64());
            if (!canvas.mask.isNull())
                data["mask"] = QString::fromLatin1(png(canvas.mask).toBase64());
        }
        data["width"] = canvas.image.width();
        data["height"] = canvas.image.height();
        m_targetId = canvas.id;
        m_sourceImage = canvas.image;
    } else {
        QString issue;
        const auto canvas = m_host ? m_host->capture(false, &issue) : CanvasSnapshot();
        if (canvas.id.isEmpty()) {
            m_status->setText(tr("Open a Krita document first"));
            return {};
        }
        m_targetId = canvas.id;
    }
    return data;
}
void BaronPanel::prepare(bool run) {
    if (m_busy)
        return;
    if (!m_client->signedIn()) {
        m_status->setText(tr("Sign in through the Connection tab first"));
        return;
    }
    const auto data = input(true);
    if (data.isEmpty())
        return;
    m_submitAfterPrepare = run;
    m_busy = true;
    m_jobMode = data["mode"].toString();
    m_foreground = QImage();
    m_pendingJob.clear();
    m_resume->hide();
    m_jobImages.clear();
    m_results->clear();
    m_expectedImages = 0;
    m_resultMask = QImage();
    m_run->setEnabled(false);
    m_quote->setEnabled(false);
    m_apply->setEnabled(false);
    m_progress->setRange(0, 0);
    m_progress->show();
    m_status->setText(tr("Preparing workflow…"));
    saveSettings();
    m_client->prepare(data);
}
void BaronPanel::showModels(const QJsonObject& data) {
    m_catalog = data["items"].toArray();
    m_gallery->clear();
    m_model->clear();
    m_loadedThumbnails.clear();
    for (const auto& value : m_catalog) {
        const auto model = value.toObject();
        auto item = new QListWidgetItem(
            model["title"].toString() + "\n" + model["family"].toString(), m_gallery);
        item->setToolTip(model["name"].toString() + " " + item->text());
        item->setData(Qt::UserRole, model);
        if (model["kind"] != "lora")
            m_model->addItem(model["title"].toString(), model["name"].toString());
    }
    m_status->setText(tr("Connected. Models are ready."));
    filterCatalog();
}
void BaronPanel::filterCatalog() {
    const auto kind = m_catalogKind->currentData().toString();
    for (int i = 0; i < m_gallery->count(); ++i) {
        auto item = m_gallery->item(i);
        const auto model = item->data(Qt::UserRole).toJsonObject();
        const bool matches
            = kind == "all" || (kind == "lora" ? model["kind"] == "lora" : model["kind"] != "lora");
        item->setHidden(
            !matches || !item->toolTip().contains(m_filter->text(), Qt::CaseInsensitive));
    }
    QTimer::singleShot(0, this, [this] { loadThumbnails(); });
}
void BaronPanel::openCatalog(const QString& kind) {
    if (m_gallery->count() == 0 && m_client->signedIn())
        m_client->models();
    m_catalogKind->setCurrentIndex(m_catalogKind->findData(kind));
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Models and LoRAs"));
    auto layout = new QVBoxLayout(&dialog);
    auto toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(tr("Models and LoRAs"), &dialog), 1);
    auto done = new QPushButton(tr("Done"), &dialog);
    done->setMinimumHeight(44);
    toolbar->addWidget(done);
    layout->addLayout(toolbar);
    connect(done, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(m_catalogView, 1);
    m_catalogView->show();
    dialog.setWindowState(Qt::WindowFullScreen);
    QTimer::singleShot(0, this, [this] {
        m_filter->setFocus();
        loadThumbnails();
    });
    dialog.exec();
    m_catalogContainer->addWidget(m_catalogView, 1);
    m_catalogView->show();
}
void BaronPanel::loadThumbnails() {
    if (!m_gallery->isVisible())
        return;
    for (int i = 0; i < m_gallery->count() && m_thumbnailRequests < 4; ++i) {
        auto item = m_gallery->item(i);
        if (item->isHidden()
            || !m_gallery->visualItemRect(item).intersects(m_gallery->viewport()->rect()))
            continue;
        const auto model = item->data(Qt::UserRole).toJsonObject();
        const auto name = model["name"].toString();
        const QUrl preview(model["preview"].toString());
        if (m_loadedThumbnails.contains(name) || preview.scheme() != "https"
            || preview.host() != "image.civitai.com")
            continue;
        m_loadedThumbnails.insert(name);
        ++m_thumbnailRequests;
        QNetworkRequest request(preview);
        request.setTransferTimeout(15000);
        request.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        auto reply = m_thumbnails->get(request);
        connect(reply, &QNetworkReply::readyRead, reply, [reply] {
            if (reply->bytesAvailable() > 5 * 1024 * 1024)
                reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
            const auto bytes = reply->readAll();
            const auto ok = reply->error() == QNetworkReply::NoError;
            reply->deleteLater();
            --m_thumbnailRequests;
            if (ok && bytes.size() <= 5 * 1024 * 1024) {
                QBuffer buffer;
                buffer.setData(bytes);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer);
                const auto size = reader.size();
                if (size.isValid() && qint64(size.width()) * size.height() <= 16777216) {
                    reader.setScaledSize(size.scaled(160, 160, Qt::KeepAspectRatio));
                    const auto image = reader.read();
                    for (int i = 0; !image.isNull() && i < m_gallery->count(); ++i)
                        if (m_gallery->item(i)->data(Qt::UserRole).toJsonObject()["name"].toString()
                            == name)
                            m_gallery->item(i)->setIcon(QIcon(QPixmap::fromImage(image)));
                }
            }
            QTimer::singleShot(0, this, [this] { loadThumbnails(); });
        });
    }
}
void BaronPanel::addReferences() {
    const auto paths = QFileDialog::getOpenFileNames(
        this, tr("Reference images"), {}, tr("Images (*.png *.jpg *.jpeg *.webp)"));
    for (const auto& path : paths) {
        if (m_referenceImages.size() >= 8)
            break;
        QImage image(path);
        if (image.isNull())
            continue;
        if (image.width() * qint64(image.height()) > 16000000)
            image = image.scaled(4096, 4096, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_referenceImages.append(image);
        new QListWidgetItem(QIcon(QPixmap::fromImage(image.scaled(72, 72, Qt::KeepAspectRatio))),
            tr("Image %1")
                .arg(m_referenceImages.size() + (m_mode->currentData() == "edit" ? 1 : 0)),
            m_refs);
    }
}
void BaronPanel::showImage(const QByteArray& bytes, int index) {
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto size = reader.size();
    auto fail = [this](const QString& issue) {
        m_busy = false;
        m_run->setEnabled(true);
        m_quote->setEnabled(true);
        m_progress->hide();
        m_status->setText(issue);
        m_resume->setVisible(!m_pendingJob.isEmpty());
    };
    if (!size.isValid() || qint64(size.width()) * size.height() > 67108864) {
        fail(tr("Could not decode the result image"));
        return;
    }
    auto image = reader.read();
    if (image.isNull()) {
        fail(tr("Could not decode the result image"));
        return;
    }
    m_jobImages[index] = image;
    if (m_jobImages.size() < m_expectedImages)
        return;
    if (m_jobMode == "background") {
        m_foreground = m_jobImages.value(0).convertToFormat(QImage::Format_ARGB32);
        image = m_jobImages.value(1);
        if (m_foreground.isNull() || image.isNull() || image.size() != m_foreground.size()
            || image.size() != m_sourceImage.size()) {
            fail(tr("The server returned an invalid background mask"));
            return;
        }
        m_resultMask = image.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < image.height(); ++y) {
            auto line = reinterpret_cast<QRgb*>(m_foreground.scanLine(y));
            auto alphaLine = m_resultMask.scanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                const auto alpha
                    = qGray(image.pixel(x, y)) * qAlpha(m_sourceImage.pixel(x, y)) / 255;
                alphaLine[x] = alpha;
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), alpha);
            }
        }
        image = m_foreground;
    } else {
        for (auto it = m_jobImages.begin(); it != m_jobImages.end(); ++it) {
            auto item = new QListWidgetItem(
                QIcon(QPixmap::fromImage(it.value().scaled(88, 88, Qt::KeepAspectRatio))),
                tr("Image %1").arg(it.key() + 1), m_results);
            item->setData(Qt::UserRole, it.value());
        }
        image = m_jobImages.first();
    }
    m_result = image;
    m_preview->setPixmap(
        QPixmap::fromImage(image.scaled(640, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    m_pendingJob.clear();
    m_resume->hide();
    m_apply->setEnabled(true);
    m_busy = false;
    m_run->setEnabled(true);
    m_quote->setEnabled(true);
    m_progress->hide();
    m_status->setText(tr("Result ready. Apply it as a new layer when you are happy with it."));
}
