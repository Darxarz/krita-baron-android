// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include "BackgroundWork.h"
#include "AuthorCredits.h"
#include "ConnectionButton.h"
#include <QButtonGroup>
#include <QGridLayout>
#include "BaronUpdates.h"
#include "BaronDiagnostics.h"
#include "GuidancePanel.h"
#include "InterfaceSettings.h"
#include "UpscaleWidget.h"
#include "InpaintWidget.h"
#include "TagModel.h"
#include "Localization.h"
#include "ModelCatalog.h"
#include "PluginUi.h"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QCompleter>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScopedValueRollback>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QWidgetAction>
#include <QtMath>
#include <cmath>

namespace {
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
    box->setProperty("sectionHeader", QVariant::fromValue<QObject*>(header));
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

BaronPanel::BaronPanel(CanvasHost* host, QWidget* parent, PromptEditor::CompletionDisplay promptDisplay)
    : QWidget(parent)
    , m_host(host) {
    BaronDiagnostics::startSession();
    BaronDiagnostics::record("panel.start", {{ "version", BaronUpdates::version() }});
    BaronLocalization::install();
    setObjectName("BaronOrchestrionPanel");
    m_client = new OrchestrionClient(this);
    m_jobs = new JobQueue(m_client, this);
    auto layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(0, 2, 2, 0);
    auto tabs = new QStackedWidget(this);
    tabs->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_pages = tabs;
    layout->addWidget(tabs, 1);
    auto connection = new QWidget(tabs);
    auto connectLayout = new QFormLayout(connection);
    m_connectionMode = new QComboBox(connection);
    m_connectionMode->setObjectName("connectionMode");
    m_connectionMode->addItem(tr("Online Service"), OrchestrionClient::interstice);
    m_connectionMode->addItem("Orchestrion", OrchestrionClient::orchestrion);
    m_connectionMode->addItem(tr("Custom Server"), OrchestrionClient::comfyui);
    m_connectionMode->hide();
    auto choices = new QWidget(connection);
    auto choicesLayout = new QGridLayout(choices); choicesLayout->setContentsMargins(0, 0, 0, 12);
    auto choiceGroup = new QButtonGroup(choices); choiceGroup->setExclusive(true);
    QList<ConnectionButton*> connectionButtons;
    for (int i = 0; i < 3; ++i) {
        auto button = new ConnectionButton(m_connectionMode->itemText(i), choices);
        button->setObjectName(i == 0 ? "connectInterstice" : i == 1 ? "connectOrchestrion" : "connectComfyUI");
        choiceGroup->addButton(button, i); connectionButtons.append(button);
        choicesLayout->addWidget(button, i == 2 ? 1 : 0, i == 2 ? 1 : i);
        connect(button, &QPushButton::clicked, this, [this, i] { m_connectionMode->setCurrentIndex(i); });
    }
    auto managed = new ConnectionButton(tr("Local Managed Server"), choices);
    managed->setObjectName("localManagedServer"); managed->setEnabled(false);
    managed->setStatus(tr("Not available"), QColor("#888888"));
    managed->setToolTip(tr("The Android port connects to a remote server; running ComfyUI on the tablet is not supported."));
    choicesLayout->addWidget(managed, 1, 0);
    connectLayout->addRow(choices);
    m_url = new QLineEdit("https://orchestrion.su", connection);
    m_url->setObjectName("websiteAddress");
    connectLayout->addRow(tr("Website"), m_url);
    auto signIn = new QPushButton(tr("Sign in through your browser"), connection);
    signIn->setMinimumHeight(42);
    connectLayout->addRow(signIn);
    auto token = new QLineEdit(connection);
    token->setObjectName("externalAccessToken"); token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText(tr("Optional access token")); token->hide(); connectLayout->addRow(token);
    auto originalSite = new QLabel("<a href='https://www.interstice.cloud'>" + tr("Visit Website") + "</a>", connection);
    originalSite->setObjectName("intersticeWebsite"); originalSite->setOpenExternalLinks(true); originalSite->hide();
    connectLayout->addRow(originalSite);
    auto serverGuide = new QLabel("<a href='https://docs.interstice.cloud/comfyui-setup'>" + tr("Custom ComfyUI Setup") + "</a><br>"
        "<a href='https://github.com/Darxarz/krita-baron-android/tree/codex/native-android/server/comfyui-baron-native'>" + tr("Native workflow compiler setup") + "</a>", connection);
    serverGuide->setOpenExternalLinks(true); serverGuide->hide(); connectLayout->addRow(serverGuide);
    auto accountLink = new QLabel(
        "<a href='https://www.interstice.cloud/user'>" + tr("View Account") + "</a> · "
        "<a href='https://www.interstice.cloud/checkout/tokens5000'>" + tr("Buy Tokens") + " (5000)</a> · "
        "<a href='https://www.interstice.cloud/checkout/tokens15000'>" + tr("Buy Tokens") + " (15000)</a>", connection);
    accountLink->setOpenExternalLinks(true); accountLink->setWordWrap(true); accountLink->hide(); connectLayout->addRow(accountLink);
    auto changeConnection = [this, signIn, token, originalSite, serverGuide, accountLink, choiceGroup, connectionButtons, connectLayout] {
        QSettings settings("BaronEdition", "Orchestrion");
        const auto backend = OrchestrionClient::Backend(m_connectionMode->currentData().toInt());
        if (!m_client->root().isEmpty()) {
            if (m_client->backend() == OrchestrionClient::orchestrion) settings.setValue("website", m_url->text());
            else if (m_client->backend() == OrchestrionClient::comfyui) {
                QUrl previous(m_url->text().contains("://") ? m_url->text() : QString("http://" + m_url->text()));
                previous.setQuery(QString()); settings.setValue("comfyServer", previous.toString());
            }
        }
        m_client->setBackend(backend);
        m_account->setText(tr("Not connected"));
        for (auto button : connectionButtons) button->setStatus(tr("Not connected"), QColor("#888888"));
        choiceGroup->button(m_connectionMode->currentIndex())->setChecked(true);
        m_url->setText(backend == OrchestrionClient::interstice ? "https://api.interstice.cloud"
            : backend == OrchestrionClient::comfyui ? settings.value("comfyServer", "http://192.168.50.191:8188").toString()
            : settings.value("website", "https://orchestrion.su").toString());
        m_url->setVisible(backend != OrchestrionClient::interstice);
        connectLayout->labelForField(m_url)->setVisible(backend != OrchestrionClient::interstice);
        token->setVisible(backend == OrchestrionClient::comfyui); token->clear();
        originalSite->setVisible(backend == OrchestrionClient::interstice);
        accountLink->setVisible(backend == OrchestrionClient::interstice);
        serverGuide->setVisible(backend == OrchestrionClient::comfyui);
        signIn->setText(backend == OrchestrionClient::comfyui ? tr("Connect") : tr("Sign in through your browser"));
        settings.setValue("connectionBackend", int(backend));
        if (m_client->setRoot(m_url->text())) m_client->restoreLogin();
        m_catalog = {}; m_model->clear(); m_loras->clear(); m_gallery->clear(); m_catalogBrowser->setModels({});
        rebuildStyles();
        m_price->setVisible(backend == OrchestrionClient::orchestrion);
    };
    m_account = new QLabel(tr("Not connected"), connection);
    m_account->setTextFormat(Qt::PlainText);
    m_account->setWordWrap(true);
    connectLayout->addRow(m_account);
    auto logout = new QPushButton(tr("Sign out"), connection);
    connectLayout->addRow(logout);
    auto updater = new BaronUpdates(this);
    auto pluginPage = new QWidget;
    auto pluginForm = new QFormLayout(pluginPage);
    pluginForm->addRow(new AuthorCredits(pluginPage));
    auto diagnostics = new QPushButton(tr("Crash diagnostics"), pluginPage);
    diagnostics->setObjectName("showCrashDiagnostics");
    pluginForm->addRow(diagnostics);
    connect(diagnostics, &QPushButton::clicked, this, [this] { BaronDiagnostics::show(m_settingsDialog); });
    auto updateForm = section(pluginForm, tr("Plugin Information and Updates"), true);
    updateForm->addRow(new QLabel("Baron " + BaronUpdates::version(), connection));
    auto automaticUpdates = new QCheckBox(tr("Check for updates on startup"), connection);
    QSettings updateSettings("BaronEdition", "Orchestrion");
    automaticUpdates->setChecked(updateSettings.value("autoUpdates", true).toBool());
    updateForm->addRow(automaticUpdates);
    auto updateStatus = new QLabel(connection);
    updateStatus->setWordWrap(true);
    updateForm->addRow(updateStatus);
    auto checkUpdate = new QPushButton(tr("Check for Updates"), connection);
    auto installUpdate = new QPushButton(tr("Download and Install"), connection);
    installUpdate->setEnabled(false);
    updateForm->addRow(checkUpdate, installUpdate);
    auto updateProgress = new QProgressBar(connection);
    updateProgress->hide();
    updateForm->addRow(updateProgress);
    auto cancelUpdate = new QPushButton(tr("Cancel download"), connection);
    cancelUpdate->hide();
    updateForm->addRow(cancelUpdate);
    connect(automaticUpdates, &QCheckBox::toggled, this, [](bool enabled) {
        QSettings("BaronEdition", "Orchestrion").setValue("autoUpdates", enabled);
    });
    connect(checkUpdate, &QPushButton::clicked, updater, &BaronUpdates::check);
    connect(cancelUpdate, &QPushButton::clicked, updater, &BaronUpdates::cancel);
    connect(installUpdate, &QPushButton::clicked, updater, [updater] {
        if (updater->state() == BaronUpdates::ready)
            updater->install();
        else
            updater->download();
    });
    connect(updater, &BaronUpdates::message, updateStatus, &QLabel::setText);
    connect(updater, &BaronUpdates::progress, updateProgress,
        [updateProgress](qint64 received, qint64 total) {
            updateProgress->setValue(total ? int(received * 100 / total) : 0);
        });
    connect(updater, &BaronUpdates::changed, this,
        [updater, checkUpdate, installUpdate, updateProgress, cancelUpdate] {
            const auto state = updater->state();
            checkUpdate->setEnabled(
                state != BaronUpdates::checking && state != BaronUpdates::downloading);
            installUpdate->setEnabled(state == BaronUpdates::available
                || state == BaronUpdates::ready
                || (state == BaronUpdates::failed && !updater->latestVersion().isEmpty()));
            installUpdate->setText(
                state == BaronUpdates::ready ? tr("Install update") : tr("Download and Install"));
            updateProgress->setVisible(state == BaronUpdates::downloading);
            cancelUpdate->setVisible(state == BaronUpdates::downloading);
        });
    if (automaticUpdates->isChecked())
        QTimer::singleShot(3000, updater, &BaronUpdates::check);
    connect(signIn, &QPushButton::clicked, this, [this, token] {
        if (m_client->setRoot(m_url->text())) {
            if (m_client->backend() == OrchestrionClient::comfyui) {
                m_client->restoreLogin(false);
                if (!token->text().isEmpty()) m_client->setAccessToken(token->text().toUtf8());
            }
            m_client->signIn();
        }
    });
    connect(logout, &QPushButton::clicked, m_client, &OrchestrionClient::signOut);
    connect(m_client, &OrchestrionClient::browserLogin, this,
        [this](const QUrl& url, const QString& code) {
            m_account->setText(code.isEmpty() ? tr("Confirm connection in your browser.") : tr("Confirm connection in your browser. Code: %1").arg(code));
            QDesktopServices::openUrl(url);
        });
    connect(m_client, &OrchestrionClient::authenticated, this, [this, tabs] {
        saveSettings();
        tabs->setCurrentIndex(0);
    });
    connect(m_client, &OrchestrionClient::accountReady, this, [this, connectionButtons](const auto& data) {
        for (auto button : connectionButtons) button->setStatus(tr("Not connected"), QColor("#888888"));
        if (!data.isEmpty()) connectionButtons[m_connectionMode->currentIndex()]->setStatus(tr("Connected"), QColor("#55aa55"));
        if (m_client->backend() == OrchestrionClient::interstice) {
            m_account->setText(data.isEmpty() ? tr("Not connected") : tr("Account: %1\nTotal generated: %2\nImage tokens remaining: %3")
                .arg(data["name"].toString()).arg(data["images_generated"].toInt()).arg(data["credits"].toDouble())); return;
        }
        if (m_client->backend() == OrchestrionClient::comfyui) { m_account->setText(data.isEmpty() ? tr("Not connected") : tr("Connected to ComfyUI")); return; }
        m_account->setText(data.isEmpty() ? tr("Not connected")
                                          : tr("%1 · %2 bleatbucks")
                                                .arg(data["name"].toString())
                                                .arg(data["coins"].toDouble(), 0, 'g', 8));
    });

    auto scroll = new QWidget(tabs);
    auto generation = scroll;
    auto generationLayout = new QVBoxLayout(generation);
    generationLayout->setContentsMargins(0, 0, 0, 0);
    auto formBody = new QWidget(generation);
    formBody->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    generationLayout->addWidget(formBody);

    auto form = new QFormLayout(formBody);
    form->setContentsMargins(0, 0, 0, 0);
    form->setFormAlignment(Qt::AlignTop);
    form->setVerticalSpacing(6);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_mode = new QComboBox(generation);
    m_mode->setObjectName("workspaceMode");
    for (const auto& entry :
        QList<QPair<QString, QString>> { { tr("Generation"), "generate" }, { tr("Edit"), "edit" },
            { tr("Upscale"), "upscale" }, { tr("Separate background"), "background" } })
        m_mode->addItem(entry.first, entry.second);
    m_mode->hide();
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
    m_model->setObjectName("checkpointSelect");
    m_styles = new QComboBox(generation);
    m_styles->setObjectName("styleSelect");
    m_styles->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_styles->setMinimumContentsLength(15);
    auto styleRow = new QHBoxLayout;
    m_workspace = new QToolButton(generation);
    m_workspace->setObjectName("workspaceSelect");
    m_workspace->setPopupMode(QToolButton::InstantPopup);
    PluginUi::setIcon(m_workspace, "workspace-generation");
    auto workspaceMenu = new QMenu(m_workspace);
    const QList<QPair<QString, QString>> workspaces { { "generate", tr("Generation") },
        { "upscale", tr("Upscaling") }, { "background", tr("Separate background") } };
    for (const auto& workspace : workspaces) {
        const auto name = workspace.first == "generate" ? "workspace-generation"
            : workspace.first == "upscale"              ? "workspace-upscaling"
                                                        : "workspace-background";
        auto action = workspaceMenu->addAction(PluginUi::icon(name, this), workspace.second);
        action->setData(workspace.first);
        connect(action, &QAction::triggered, this,
            [this, workspace] { m_mode->setCurrentIndex(m_mode->findData(workspace.first)); });
    }
    m_workspace->setMenu(workspaceMenu);
    styleRow->addWidget(m_workspace);
    styleRow->addWidget(m_styles, 1);
    m_upscale = new UpscaleWidget(generation);
    styleRow->addWidget(m_upscale->upscaler, 1);
    m_upscale->style->setModel(m_styles->model());
    connect(m_upscale->style, QOverload<int>::of(&QComboBox::currentIndexChanged),
        m_styles, &QComboBox::setCurrentIndex);
    connect(m_styles, QOverload<int>::of(&QComboBox::currentIndexChanged),
        m_upscale->style, &QComboBox::setCurrentIndex);
    auto styleSettings = new QToolButton(generation);
    styleSettings->setObjectName("styleSettings");
    styleSettings->setAutoRaise(true);
    PluginUi::setIcon(styleSettings, "settings");
    styleSettings->setToolTip(tr("Configure"));
    styleRow->addWidget(styleSettings);
    form->addRow(styleRow);
    form->addRow(m_upscale);
    m_upscale->hide();
    connect(m_upscale, &UpscaleWidget::configureStyle, this, [this] { openSettings(1); });
    connect(m_upscale, &UpscaleWidget::changed, this, &BaronPanel::updateMode);
    connect(styleSettings, &QToolButton::clicked, this, [this] { openSettings(1); });
    connect(m_styles, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &BaronPanel::selectStyle);
    modelRow->hide();
    connect(chooseModel, &QPushButton::clicked, this, [this] { openCatalog("checkpoint"); });
    auto chooseLoras = new QPushButton(tr("Choose LoRAs…"), generation);
    chooseLoras->setMinimumHeight(40);
    chooseLoras->setObjectName("chooseLorasButton");
    chooseLoras->hide();
    connect(chooseLoras, &QPushButton::clicked, this, [this] { openCatalog("lora"); });
    m_prompt = new PromptEditor(generation, false, promptDisplay);
    connect(static_cast<PromptEditor*>(m_prompt), &PromptEditor::activated, this,
        [this] { prepare(true); });
    connect(
        static_cast<PromptEditor*>(m_prompt), &PromptEditor::heightChanged, this, [](int height) {
            QSettings settings("BaronEdition", "Orchestrion");
            settings.setValue("promptHeight", height);
            settings.setValue("prompt_line_count", qMax(1, (height - 10) / qMax(1, QFontMetrics(QApplication::font()).lineSpacing())));
        });
    m_prompt->setPlaceholderText(tr("Describe the content you want to see, or leave empty."));
    m_prompt->setObjectName("positivePrompt");
    m_prompt->setFixedHeight(QSettings("BaronEdition", "Orchestrion")
            .value("promptHeight", fontMetrics().lineSpacing() * 10 + 10)
            .toInt());
    form->addRow(m_prompt);
    m_negative = new PromptEditor(generation, true, promptDisplay);
    m_prompt->setPromptActionTarget(m_negative);
    m_negative->setPromptActionTarget(m_prompt);
    m_prompt->setPromptActionClient(m_client);
    m_negative->setPromptActionClient(m_client);
    connect(m_model, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        const auto name = m_model->currentData().toString();
        m_prompt->setPromptActionModel(name, ""); m_negative->setPromptActionModel(name, "");
    });
    connect(static_cast<PromptEditor*>(m_negative), &PromptEditor::activated, this,
        [this] { prepare(true); });
    m_negative->setObjectName("negativePrompt");
    m_negative->setFixedHeight(fontMetrics().lineSpacing() + 10);
    m_negative->setPlaceholderText(tr("Describe the content you want to avoid..."));
    form->addRow(m_negative);
    m_guidance = new GuidancePanel(m_host, generation);
    connect(m_guidance, &GuidancePanel::activated, this, [this] { prepare(true); });
    connect(m_guidance, &GuidancePanel::controlRequested, this, &BaronPanel::prepareControl);
    connect(m_guidance, &GuidancePanel::controlMapGenerated, this,
        [this](const QString& target, const QString& id, const QImage& image) {
            QString issue;
            if (m_host && m_host->apply(target, image, {}, tr("Control map"), &issue)) {
                m_previewResult.clear();
                m_guidance->setControlLayer(m_host->appliedLayerId(), id);
                m_status->setText(tr("Control map added as a linked Krita layer."));
            } else m_status->setText(issue);
        });
    connect(m_guidance, &GuidancePanel::error, this,
        [this](const QString& error) { m_status->setText(error); });
    form->addRow(m_guidance);
    m_guidance->setRootEditors(m_prompt, m_negative);
    auto queueOptions = new QWidget(generation);
    auto queueForm = new QFormLayout(queueOptions);
    queueForm->setContentsMargins(6, 6, 6, 6);
    auto options = new QWidget(generation);
    options->setMinimumWidth(320);
    auto optionsForm = new QFormLayout(options);
    auto imageSettings = section(optionsForm, tr("Image settings"));
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
    sizeRow->hide();
    imageSettings->addRow(
        new QLabel(tr("Size follows the canvas or selected area automatically."), generation));
    m_strength = new QDoubleSpinBox(generation);
    m_strength->setRange(0.05, 1);
    m_strength->setSingleStep(.05);
    m_strength->setValue(1);
    m_strength->hide();
    auto strengthRow = new QWidget(generation);
    auto strengthLayout = new QHBoxLayout(strengthRow);
    strengthLayout->setContentsMargins(0, 0, 0, 0);
    auto strengthSlider = new QSlider(Qt::Horizontal, strengthRow);
    strengthSlider->setObjectName("denoiseSlider");
    strengthRow->setObjectName("generationStrengthRow");
    strengthSlider->setRange(5, 100);
    strengthSlider->setValue(100);
    strengthSlider->setSingleStep(5);
    auto strengthPercent = new QSpinBox(strengthRow);
    strengthPercent->setRange(5, 100);
    strengthPercent->setValue(100);
    strengthPercent->setSuffix(" %");
    strengthPercent->setPrefix(tr("Strength") + ": ");
    strengthLayout->addWidget(strengthSlider, 1);
    strengthLayout->addWidget(strengthPercent);
    strengthLayout->setSpacing(6);
    strengthPercent->setSuffix("%");
    strengthSlider->setRange(1, 100);
    strengthPercent->setRange(1, 100);
    m_strength->setRange(.01, 1);
    auto controlButton = m_guidance->findChild<QToolButton*>("addControlLayer");
    auto regionButton = m_guidance->findChild<QToolButton*>("addRegion");
    controlButton->setParent(strengthRow);
    regionButton->setParent(strengthRow);
    strengthLayout->addWidget(controlButton);
    strengthLayout->addWidget(regionButton);
    form->addRow(strengthRow);
    connect(strengthSlider, &QSlider::valueChanged, strengthPercent, &QSpinBox::setValue);
    connect(m_strength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), strengthPercent,
        [strengthPercent](double value) { strengthPercent->setValue(qRound(value * 100)); });
    connect(strengthPercent, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [this, strengthSlider](int value) {
            strengthSlider->setValue(value);
            m_strength->setValue(value / 100.0);
            updateMode();
        });
    m_scale = new QDoubleSpinBox(generation);
    m_scale->setRange(.3, 1.5);
    m_scale->setSingleStep(.1);
    m_scale->setValue(1);
    m_scale->hide();
    auto resolutionRow = new QHBoxLayout;
    auto resolutionSlider = new QSlider(Qt::Horizontal, queueOptions);
    resolutionSlider->setObjectName("resolutionSlider");
    resolutionSlider->setRange(3, 15);
    resolutionSlider->setSingleStep(1);
    resolutionSlider->setPageStep(1);
    resolutionSlider->setToolTip(tr("Scaling factor for generation. Values below 1.0 improve performance for high resolution canvas."));
    resolutionSlider->setValue(10);
    auto resolutionLabel = new QLabel("1.0 x", queueOptions);
    resolutionRow->addWidget(resolutionSlider, 1);
    resolutionRow->addWidget(resolutionLabel);
    queueForm->addRow(tr("Resolution"), resolutionRow);
    connect(resolutionSlider, &QSlider::valueChanged, this,
        [this](int value) { m_scale->setValue(value / 10.0); });
    connect(m_scale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [resolutionSlider, resolutionLabel](double value) {
            resolutionSlider->setValue(qRound(value * 10));
            resolutionLabel->setText(QString::number(value, 'f', 1) + " x");
        });
    m_upscaleFactor = m_upscale->factor;
    auto advancedForm = section(optionsForm, tr("Sampling settings"), true);
    m_steps = new QSpinBox(generation);
    m_steps->setObjectName("generationSteps");
    m_steps->setRange(1, 100);
    m_steps->setValue(20);
    advancedForm->addRow(tr("Steps"), m_steps);
    m_cfg = new QDoubleSpinBox(generation);
    m_cfg->setObjectName("generationCfg");
    m_cfg->setRange(0, 30);
    m_cfg->setValue(4);
    advancedForm->addRow(tr("CFG"), m_cfg);
    m_sampler = new QComboBox(options);
    m_sampler->addItem(tr("Model default"), "");
    for (const auto& name : { "euler", "euler_ancestral", "dpmpp_2m", "dpmpp_2m_sde", "dpmpp_sde",
             "heun", "lcm", "res_multistep" })
        m_sampler->addItem(name, name);
    advancedForm->addRow(tr("Sampler"), m_sampler);
    m_scheduler = new QComboBox(options);
    m_scheduler->addItem(tr("Model default"), "");
    for (const auto& name :
        { "normal", "karras", "exponential", "simple", "sgm_uniform", "beta", "ddim_uniform" })
        m_scheduler->addItem(name, name);
    advancedForm->addRow(tr("Scheduler"), m_scheduler);
    m_seed = new QLineEdit("-1", generation);
    m_seed->setObjectName("generationSeed");
    auto seedRow = new QHBoxLayout;
    m_fixedSeed = new QCheckBox(tr("Fixed"), options);
    m_fixedSeed->setObjectName("fixedSeed");
    seedRow->addWidget(m_fixedSeed);
    seedRow->addWidget(m_seed, 1);
    auto randomSeed = new QToolButton(options);
    randomSeed->setText("⚄");
    randomSeed->setMinimumSize(36, 36);
    randomSeed->setToolTip(tr("Random seed"));
    seedRow->addWidget(randomSeed);
    connect(randomSeed, &QToolButton::clicked, this, [this] {
        m_seed->setText(QString::number(QRandomGenerator::global()->generate()));
        m_fixedSeed->setChecked(true);
    });
    connect(m_fixedSeed, &QCheckBox::toggled, this, [this](bool fixed) {
        bool valid = false;
        const auto seed = m_seed->text().toLongLong(&valid);
        if (fixed && (!valid || seed < 0 || seed > 4294967295LL))
            m_seed->setText(QString::number(QRandomGenerator::global()->generate()));
    });
    queueForm->insertRow(0, tr("Seed"), seedRow);
    m_seed->setEnabled(false);
    randomSeed->setEnabled(false);
    PluginUi::setIcon(randomSeed, "random");
    randomSeed->setText({});
    randomSeed->setMinimumSize(0, 0);
    connect(m_fixedSeed, &QCheckBox::toggled, m_seed, &QLineEdit::setEnabled);
    connect(m_fixedSeed, &QCheckBox::toggled, randomSeed, &QToolButton::setEnabled);
    m_batch = new QSpinBox(generation);
    m_batch->setRange(1, JobQueue::max_batch);
    m_batch->setValue(1);
    m_batch->hide();
    auto batchRow = new QHBoxLayout;
    auto batchSlider = new QSlider(Qt::Horizontal, queueOptions);
    batchSlider->setObjectName("batchSlider");
    batchSlider->setRange(1, JobQueue::max_batch);
    batchSlider->setValue(1);
    auto batchLabel = new QLabel("1", queueOptions);
    batchRow->addWidget(batchSlider, 1);
    batchRow->addWidget(batchLabel);
    queueForm->insertRow(0, tr("Batches"), batchRow);
    connect(batchSlider, &QSlider::valueChanged, m_batch, &QSpinBox::setValue);
    connect(m_batch, QOverload<int>::of(&QSpinBox::valueChanged), this,
        [batchSlider, batchLabel](int value) {
            batchSlider->setValue(value);
            batchLabel->setText(QString::number(value));
        });
    m_queuePosition = new QComboBox(options);
    m_queuePosition->addItem(tr("Back of queue"), false);
    m_queuePosition->addItem(tr("Front of queue"), true);
    m_queuePosition->addItem(tr("Replace Queue"), "replace");
    queueForm->addRow(tr("Enqueue"), m_queuePosition);
    auto jobsForm = section(queueForm, tr("Jobs"));
    m_jobList = new QListWidget(options);
    m_jobList->setObjectName("jobQueue");
    m_jobList->setMinimumHeight(120);
    m_jobList->setMaximumHeight(200);
    jobsForm->addRow(m_jobList);
    auto jobActions = new QVBoxLayout;
    auto retryJob = new QPushButton(tr("Resume result download"), options);
    auto removeJob = new QPushButton(tr("Remove stopped job"), options);
    retryJob->setMinimumHeight(40);
    removeJob->setMinimumHeight(40);
    jobActions->addWidget(retryJob);
    jobActions->addWidget(removeJob);
    jobsForm->addRow(jobActions);
    connect(retryJob, &QPushButton::clicked, this, [this] {
        if (auto item = m_jobList->currentItem())
            m_jobs->retry(item->data(Qt::UserRole).toString());
    });
    connect(removeJob, &QPushButton::clicked, this, [this] {
        if (auto item = m_jobList->currentItem())
            m_jobs->dismiss(item->data(Qt::UserRole).toString());
        updateJobs();
    });
    auto cancelActions = new QHBoxLayout;
    for (const auto& label : { tr("Active"), tr("Queued"), tr("All") }) {
        auto button = new QPushButton(label, options);
        PluginUi::setIcon(button, "cancel");
        cancelActions->addWidget(button);
    }
    for (int i = 0; i < 3; ++i)
        connect(qobject_cast<QPushButton*>(cancelActions->itemAt(i)->widget()),
            &QPushButton::clicked, this, [this, i] { cancelJobs(i); });
    queueForm->insertRow(4, tr("Cancel"), cancelActions);
    auto referenceForm = section(optionsForm, tr("Reference settings"));
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
    addRefs->hide();
    auto clearRefs = new QPushButton(tr("Clear references"), generation);
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
    m_price = new QLabel(tr("≈ %1 bleatbucks").arg(QString::fromUtf8("—")), generation);
    m_price->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_price->setMinimumWidth(0);
    m_price->setObjectName("priceEstimate");
    m_price->setMinimumHeight(0);
    auto actions = new QWidget(generation);
    auto actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    m_quote = new QPushButton("≈", actions);
    m_quote->setToolTip(tr("Estimate price"));
    m_quote->setMaximumWidth(44);
    m_quote->hide();
    auto generateActions = new QWidget(actions);
    auto generateLayout = new QHBoxLayout(generateActions);
    generateLayout->setContentsMargins(0, 0, 0, 0);
    generateLayout->setSpacing(0);
    m_run = new QPushButton(tr("Generate"), generateActions);
    m_run->setMinimumHeight(12 + int(1.3 * fontMetrics().height()));
    PluginUi::setIcon(m_run, "workspace-generation");
    m_run->setIconSize(QSize(24, 24));
    m_run->setObjectName("generateButton");

    generateLayout->addWidget(m_run);
    actionLayout->addWidget(generateActions, 1);
    auto settingsButton = new QToolButton(actions);
    settingsButton->setObjectName("generationSettings");
    settingsButton->setText("0");
    settingsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    PluginUi::setIcon(settingsButton, "queue-inactive");
    m_queueButton = settingsButton;
    settingsButton->setToolTip(tr("Generation settings"));
    auto settingsMenu = new QMenu(settingsButton);
    auto settingsAction = new QWidgetAction(settingsMenu);
    auto optionsScroll = new QScrollArea(settingsMenu);
    optionsScroll->setObjectName("generationSettingsScroll");
    optionsScroll->setWidgetResizable(true);
    optionsScroll->setFrameShape(QFrame::NoFrame);
    optionsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    optionsScroll->setWidget(queueOptions);
    optionsScroll->setMinimumWidth(360);
    optionsScroll->setMaximumHeight(600);
    QScroller::grabGesture(optionsScroll->viewport(), QScroller::TouchGesture);
    connect(settingsMenu, &QMenu::aboutToShow, this, [this, optionsScroll] {
        const auto screen = this->screen();
        optionsScroll->setFixedHeight(qMin(optionsScroll->widget()->sizeHint().height() + 4,
            qMin(600, screen ? qMax(220, screen->availableGeometry().height() - 100) : 600)));
    });
    settingsAction->setDefaultWidget(optionsScroll);
    settingsMenu->addAction(settingsAction);
    settingsButton->setMenu(settingsMenu);
    settingsButton->setPopupMode(QToolButton::InstantPopup);
    actionLayout->addWidget(settingsButton);
    form->addRow(actions);
    auto priceRow = new QHBoxLayout;
    priceRow->setContentsMargins(0, 3, 0, 3);
    priceRow->addWidget(m_price, 1);
    auto modelGallery = new QPushButton(tr("Models"), generation);
    modelGallery->setObjectName("modelGalleryButton");
    priceRow->addWidget(modelGallery);
    form->addRow(priceRow);
    connect(modelGallery, &QPushButton::clicked, this, [this] { openCatalog("all"); });
    connect(m_run, &QPushButton::clicked, this, [this] { prepare(true); });
    connect(m_quote, &QPushButton::clicked, this, [this] { prepare(false); });
    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this] { updateMode(); });
    connect(m_model, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        QString architecture;
        for (auto value : m_catalog) {
            const auto model = value.toObject();
            if (model["name"].toString() == m_model->currentData().toString()) {
                architecture = model["architecture"].toString(model["family"].toString());
                break;
            }
        }
        m_guidance->setArchitecture(architecture);
        m_upscale->setArchitecture(architecture);
        const auto family = architecture.startsWith("flux", Qt::CaseInsensitive) ? QString("flux")
            : architecture.startsWith("krea", Qt::CaseInsensitive) ? QString("krea") : architecture.toLower();
        m_prompt->setPromptActionModel(m_model->currentData().toString(), family);
        m_negative->setPromptActionModel(m_model->currentData().toString(), family);
        if (m_uiReady) updatePromptSyntax();
    });

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
    m_catalogBrowser = new ModelCatalog(catalog);
    catalogLayout->addWidget(m_catalogBrowser, 1);
    m_catalogKind = m_catalogBrowser->kind();
    m_filter = m_catalogBrowser->search();
    m_gallery = m_catalogBrowser->gallery();
    m_thumbnails = new ModelThumbnails(this);
    m_catalogBrowser->setMetadataProvider([this](const QString& name, const QString& kind,
        ModelCatalog::MetadataCallback done, bool refresh) { m_client->modelMetadata(name, kind, std::move(done), refresh); });
    connect(m_catalogBrowser, &ModelCatalog::triggerWordsRequested, this, [this, tabs](const QStringList& words) {
        auto editor = static_cast<PromptEditor*>(m_prompt);
        if (!m_prompt->isVisible())
            if (auto regional = m_guidance->findChild<PromptEditor*>("regionPrompt")) editor = regional;
        auto text = editor->toPlainText().trimmed();
        if (!text.isEmpty()) text += ", ";
        editor->replacePromptText(text + words.join(", "));
        if (auto dialog = qobject_cast<QDialog*>(m_catalogView->parentWidget())) dialog->accept();
        tabs->setCurrentIndex(0); editor->setFocus();
    });
    connect(m_gallery->verticalScrollBar(), &QScrollBar::valueChanged, this,
        [this] { loadThumbnails(); });
    connect(m_catalogBrowser, &ModelCatalog::filtersChanged, this,
        [this] { QTimer::singleShot(0, this, [this] { loadThumbnails(); }); });
    m_loras = new QListWidget(catalog);
    m_loras->setObjectName("styleLoras");
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
    auto chooseCatalogModel = [this, tabs](const QJsonObject& data) {
        if (data["kind"] == "lora") {
            if (!m_settingsDialog->isVisible()) {
                auto editor = m_prompt;
                if (!m_prompt->isVisible())
                    if (auto regional = m_guidance->findChild<PromptEditor*>("regionPrompt"))
                        editor = regional;
                static_cast<PromptEditor*>(editor)->insertLora(data);
                tabs->setCurrentIndex(0);
                if (auto dialog = qobject_cast<QDialog*>(m_catalogView->parentWidget()))
                    dialog->accept();
                editor->setFocus();
                return;
            }
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
            const auto index = m_styles->findData(QString("model:" + data["name"].toString()));
            if (index >= 0 && !m_settingsDialog->isVisible())
                m_styles->setCurrentIndex(index);
            tabs->setCurrentIndex(0);
            if (auto dialog = qobject_cast<QDialog*>(m_catalogView->parentWidget()))
                dialog->accept();
        }
    };
    connect(m_gallery, &QListWidget::itemClicked, this, [chooseCatalogModel](QListWidgetItem* item) {
        chooseCatalogModel(ModelCatalog::normalize(item->data(Qt::UserRole).toJsonObject()));
    });
    connect(m_catalogBrowser, &ModelCatalog::modelChosen, this, chooseCatalogModel);
    connect(m_client, &OrchestrionClient::modelsReady, this, &BaronPanel::showModels);

    auto results = new QWidget(tabs);
    auto resultsLayout = new QVBoxLayout(results);
    m_preview = new QLabel(results);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(200, 200);
    resultsLayout->addWidget(m_preview, 1);
    auto historyList = new HistoryList(results);
    m_results = historyList;
    m_results->setObjectName("resultHistory");
    m_results->setViewMode(QListView::IconMode);
    m_results->setResizeMode(QListView::Adjust);
    const int historySize = QSettings("BaronEdition", "Orchestrion").value("thumbnailSize", 96).toInt();
    m_results->setIconSize(QSize(historySize, historySize));
    m_results->setSpacing(0);
    m_results->setFrameShape(QFrame::NoFrame);
    m_results->setFlow(QListView::LeftToRight);
    m_results->setWrapping(true);
    m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_results->setMinimumHeight(100);
    resultsLayout->addWidget(m_results);
    connect(m_results, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { selectResult(item); });
    connect(m_results, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { selectResult(item, true); });
    connect(historyList, &HistoryList::applyRequested, this, [this](QListWidgetItem* item) { selectResult(item, true); });
    connect(historyList, &HistoryList::contextRequested, this, [this](QListWidgetItem* item, const QPoint& point) {
        m_results->setCurrentItem(item);
        if (auto action = findChild<QAction*>("history_copy"))
            if (auto menu = qobject_cast<QMenu*>(action->parent())) menu->popup(point);
    });
    connect(historyList, &HistoryList::emptyClicked, this, [this] {
        if (m_host) m_host->hidePreview();
        m_previewResult.clear();
        m_results->clearSelection();
        m_results->setCurrentItem(nullptr);
        m_apply->setEnabled(false);
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
        m_progress->show();
        m_client->resume(m_pendingJob);
        m_status->setText(tr("Retrieving the existing job. No new generation is started."));
    });
    connect(m_apply, &QPushButton::clicked, this, [this] {
        if (m_results->currentItem())
            selectResult(m_results->currentItem(), true);
    });
    tabs->addWidget(scroll);
    tabs->addWidget(catalogContainer);
    generationLayout->addWidget(m_results, 1);
    generationLayout->addWidget(m_resume);
    auto previewActions = new QHBoxLayout;
    auto hidePreview = new QToolButton(generation);
    hidePreview->setText(tr("Hide preview"));
    connect(hidePreview, &QToolButton::clicked, historyList, &HistoryList::emptyClicked);
    previewActions->addWidget(hidePreview);
    auto cancelJob = new QToolButton(generation);
    cancelJob->setText(tr("Cancel active job"));
    connect(cancelJob, &QToolButton::clicked, this, [this] {
        for (const auto& job : m_jobs->jobs())
            if (job->state == JobQueue::running) {
                m_jobs->cancel(job->id);
                return;
            }
        if (auto item = m_jobList->currentItem())
            m_jobs->cancel(item->data(Qt::UserRole).toString());
    });
    previewActions->addWidget(cancelJob);
    auto historyButton = new QToolButton(generation);
    historyButton->setText(tr("History"));
    historyButton->setMinimumHeight(36);
    auto historyMenu = new QMenu(historyButton);
    const QList<QPair<QString, QString>> historyActions { { "copy", tr("Copy Prompt") },
        { "copy_evaluated", tr("Copy Prompt (Evaluated)") }, { "strength", tr("Copy Strength") },
        { "style", tr("Copy Style") }, { "seed", tr("Copy Seed") },
        { "info", tr("Info to Clipboard") }, { "export", tr("Save Image") },
        { "delete", tr("Discard Image") }, { "clear", tr("Clear History") },
        { "preview", tr("Preview on canvas") }, { "apply", tr("Apply as new layer") },
        { "reuse", tr("Reuse generation settings") }, { "favorite", tr("Toggle favorite") } };
    for (const auto& entry : historyActions) {
        auto action = historyMenu->addAction(entry.second);
        action->setObjectName("history_" + entry.first);
        if (QStringList { "preview", "apply", "reuse", "favorite" }.contains(entry.first))
            action->setVisible(false);
        connect(action, &QAction::triggered, this, [this, entry] { historyAction(entry.first); });
    }
    connect(historyMenu, &QMenu::aboutToShow, this, [this, historyMenu] {
        for (auto action : historyMenu->actions())
            action->setEnabled(action->objectName() == "history_clear" || m_results->currentItem());
    });
    historyButton->setMenu(historyMenu);
    historyButton->setPopupMode(QToolButton::InstantPopup);
    previewActions->addWidget(historyButton);
    m_results->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_results, &QListWidget::customContextMenuRequested, this,
        [this, historyMenu](const QPoint& point) {
            if (auto item = m_results->itemAt(point)) {
                m_results->setCurrentItem(item);
                historyMenu->popup(m_results->viewport()->mapToGlobal(point));
            }
        });
    for (int i = 0; i < previewActions->count(); ++i)
        previewActions->itemAt(i)->widget()->hide();
    delete previewActions;
    m_jobCount = new QLabel(queueOptions);
    queueForm->insertRow(0, tr("Jobs"), m_jobCount);
    results->hide();
    m_inpaint = new InpaintWidget(generation);
    m_inpaint->hide();
    int actionRow; QFormLayout::ItemRole actionRole; form->getWidgetPosition(actions, &actionRow, &actionRole);
    form->insertRow(actionRow, m_inpaint);
    connect(m_inpaint, &InpaintWidget::editChanged, this, [this](bool edit) { m_mode->setCurrentIndex(edit ? 1 : 0); });
    connect(m_inpaint, &InpaintWidget::changed, this, [this] { updateMode(); if (m_saveTimer) m_saveTimer->start(); });
    auto modeMenu = new QMenu(m_run);
    for (int i = 0; i < 2; ++i) {
        auto action = modeMenu->addAction(PluginUi::icon(i ? "edit" : "workspace-generation", this),
            i ? tr("Edit") : tr("Generate"));
        connect(action, &QAction::triggered, this, [this, i] { m_inpaintMode = "automatic"; m_mode->setCurrentIndex(i); updateMode(); });
    }
    for (const auto& entry : QList<QPair<QString,QString>> {{"automatic",tr("Automatic")},{"fill",tr("Fill")},{"expand",tr("Expand")},{"add_object",tr("Add Content")},{"remove_object",tr("Remove Content")},{"replace_background",tr("Replace Background")},{"custom",tr("Generate (Custom)")}}) {
        auto action = modeMenu->addAction(PluginUi::icon("inpaint-"+entry.first,this),entry.second);
        action->setObjectName("inpaintMode_"+entry.first);
        connect(action,&QAction::triggered,this,[this,entry] { m_inpaintMode=entry.first; if(entry.first!="custom")m_mode->setCurrentIndex(0);updateMode();if(m_saveTimer)m_saveTimer->start(); });
    }
    connect(modeMenu,&QMenu::aboutToShow,this,[this,modeMenu] {
        const bool selection=m_host&&m_host->hasSelection();
        m_inpaint->setLayers(m_host ? m_host->layers() : QJsonArray());
        for(auto action:modeMenu->actions()) {
            if(!action->objectName().startsWith("inpaintMode_"))continue;
            const bool custom=action->objectName()=="inpaintMode_custom";
            action->setVisible(selection&&(custom||m_strength->value()==1));
            if(custom)action->setText(m_mode->currentData()=="edit"?tr("Edit (Custom)"):m_strength->value()<1?tr("Refine (Custom)"):tr("Generate (Custom)"));
        }
    });
    auto modeButton = new QToolButton(generation);
    m_modeButton = modeButton;
    modeButton->setObjectName("generationMode");
    modeButton->setArrowType(Qt::DownArrow);
    modeButton->setMenu(modeMenu);
    modeButton->setPopupMode(QToolButton::InstantPopup);
    modeButton->setFixedHeight(m_run->minimumHeight() - 3);
    settingsButton->setFixedHeight(m_run->minimumHeight() - 3);
    auto runRow = qobject_cast<QHBoxLayout*>(m_run->parentWidget()->layout());
    if (runRow) {
        runRow->addWidget(modeButton);
    }
    m_settingsDialog = new QDialog(this);
    m_settingsDialog->setObjectName("pluginSettingsDialog");
    m_settingsDialog->setWindowTitle(tr("Krita AI Diffusion Baron Edition"));
    auto dialogLayout = new QVBoxLayout(m_settingsDialog);
    auto settingsBody = new QHBoxLayout;
    auto categories = new QListWidget(m_settingsDialog);
    categories->setObjectName("settingsCategories");
    categories->setFrameShape(QFrame::NoFrame);
    categories->setFixedWidth(120);
    categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto settingPages = new QStackedWidget(m_settingsDialog);
    settingPages->setObjectName("settingsPages");
    settingsBody->addWidget(categories);
    settingsBody->addWidget(settingPages, 1);
    dialogLayout->addLayout(settingsBody, 1);
    auto addPage = [categories, settingPages](QWidget* page, const QString& title) {
        auto scroll = new QScrollArea(settingPages);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setWidget(page);
        categories->addItem(title);
        settingPages->addWidget(scroll);
    };
    connect(categories, &QListWidget::currentRowChanged, settingPages, &QStackedWidget::setCurrentIndex);
    addPage(connection, tr("Connection"));
    auto stylePage = new QWidget(settingPages);
    auto styleForm = new QFormLayout(stylePage);
    styleForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    styleForm->setVerticalSpacing(14);
    auto heading = new QLabel(tr("Style Presets"), stylePage);
    QFont headingFont = heading->font();
    headingFont.setPointSizeF(headingFont.pointSizeF() * 1.2);
    heading->setFont(headingFont);
    styleForm->addRow(heading);
    auto presetBox = new QGroupBox(stylePage);
    auto presetLayout = new QVBoxLayout(presetBox);
    auto toolbar = new QHBoxLayout;
    auto stylePicker = new QComboBox(presetBox);
    stylePicker->setObjectName("settingsStyleSelect");
    stylePicker->setEditable(true);
    stylePicker->setInsertPolicy(QComboBox::NoInsert);
    stylePicker->completer()->setFilterMode(Qt::MatchContains);
    stylePicker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    stylePicker->setMinimumContentsLength(12);
    toolbar->addWidget(stylePicker, 1);
    auto addStyle = new QToolButton(presetBox);
    addStyle->setText("+");
    addStyle->setToolTip(tr("Create new style"));
    toolbar->addWidget(addStyle);
    auto duplicateStyle = new QToolButton(presetBox);
    PluginUi::setIcon(duplicateStyle, "copy-image");
    duplicateStyle->setToolTip(tr("Duplicate style"));
    toolbar->addWidget(duplicateStyle);
    auto deleteStyle = new QToolButton(presetBox);
    PluginUi::setIcon(deleteStyle, "discard");
    deleteStyle->setToolTip(tr("Delete style"));
    toolbar->addWidget(deleteStyle);
    auto refreshStyle = new QToolButton(presetBox);
    PluginUi::setIcon(refreshStyle, "reset");
    refreshStyle->setToolTip(tr("Refresh models"));
    toolbar->addWidget(refreshStyle);
    connect(refreshStyle, &QToolButton::clicked, m_client, &OrchestrionClient::models);
    presetLayout->addLayout(toolbar);
    auto styleMode = new QComboBox(presetBox);
    styleMode->setObjectName("styleModeLabel");
    styleMode->addItem(tr("Generation style"), "generate");
    styleMode->addItem(tr("Editing style"), "edit");
    presetLayout->addWidget(styleMode);
    connect(styleMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, styleMode] {
        if (!m_styleLoading) m_mode->setCurrentIndex(m_mode->findData(styleMode->currentData()));
    });
    auto styleHelp = new QLabel(tr("Changes are saved automatically. Generation and editing remember their own style."), presetBox);
    styleHelp->setWordWrap(true);
    presetLayout->addWidget(styleHelp);
    styleForm->addRow(presetBox);
    connect(stylePicker, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this, stylePicker](int index) {
            if (index >= 0 && !m_styleLoading)
                m_styles->setCurrentIndex(m_styles->findData(stylePicker->currentData()));
        });
    connect(addStyle, &QToolButton::clicked, this, [this] {
        QScopedValueRollback<bool> loading(m_styleLoading, true);
        m_styleId.clear();
        m_settingsDialog->findChild<QLineEdit*>("styleName")->setText(tr("New style"));
        m_settingsDialog->findChild<QPlainTextEdit*>("stylePrompt")->clear();
        m_settingsDialog->findChild<QPlainTextEdit*>("styleNegative")->clear();
        saveStyle();
    });
    connect(duplicateStyle, &QToolButton::clicked, this, [this] {
        QScopedValueRollback<bool> loading(m_styleLoading, true);
        const auto previous = m_styleId;
        m_styleId = QUuid::createUuid().toString();
        for (auto style : m_stylePresets)
            if (style["id"] == previous) {
                style["id"] = m_styleId;
                m_stylePresets.append(style);
                break;
            }
        auto name = m_settingsDialog->findChild<QLineEdit*>("styleName");
        name->setText(name->text() + tr(" (copy)"));
        saveStyle();
    });
    connect(deleteStyle, &QToolButton::clicked, this, [this] {
        if (m_styleId.startsWith("model:") || QMessageBox::question(m_settingsDialog,
            tr("Delete style"), tr("Delete the selected style?")) != QMessageBox::Yes)
            return;
        for (int i = m_stylePresets.size() - 1; i >= 0; --i)
            if (m_stylePresets[i]["id"] == m_styleId)
                m_stylePresets.removeAt(i);
        QJsonArray saved;
        for (const auto& style : m_stylePresets)
            saved.append(style);
        QSettings("BaronEdition", "Orchestrion").setValue("stylePresets", QJsonDocument(saved).toJson());
        m_styleId.clear();
        rebuildStyles();
        selectStyle();
    });
    modelRow->setParent(stylePage);
    modelRow->show();

    chooseLoras->setParent(stylePage);
    chooseLoras->show();

    auto styleName = new QLineEdit(stylePage);
    styleName->setObjectName("styleName");
    styleForm->addRow(tr("Name"), styleName);
    styleForm->addRow(tr("Model"), modelRow);
    auto styleWarning = new QLabel(stylePage);
    styleWarning->setObjectName("styleWarning");
    styleWarning->setWordWrap(true);
    styleForm->addRow(styleWarning);
    auto modelOptions = section(styleForm, tr("Additional model settings"));
    auto architecture = new QLabel(stylePage);
    architecture->setObjectName("styleArchitecture");
    modelOptions->addRow(tr("Model architecture"), architecture);
    auto vae = new QComboBox(stylePage);
    vae->setEditable(true);
    vae->setInsertPolicy(QComboBox::NoInsert);
    vae->addItem("Checkpoint Default");
    vae->setObjectName("styleVae");
    modelOptions->addRow(tr("VAE"), vae);
    auto clipSkip = new QSpinBox(stylePage);
    clipSkip->setObjectName("styleClipSkip");
    clipSkip->setRange(0, 12);
    clipSkip->setSpecialValueText(tr("Model default"));
    modelOptions->addRow(tr("Clip Skip"), clipSkip);
    auto preferred = new QSpinBox(stylePage);
    preferred->setObjectName("styleResolution");
    preferred->setRange(0, 4096);
    preferred->setSingleStep(64);
    preferred->setSpecialValueText(tr("Model default"));
    modelOptions->addRow(tr("Preferred Resolution"), preferred);
    for (const auto& name : { "v_prediction_zsnr", "self_attention_guidance" }) {
        auto check = new QCheckBox(QString(name) == "v_prediction_zsnr"
                ? tr("V-Prediction / Zero Terminal SNR")
                : tr("Self-Attention Guidance"),
            stylePage);
        check->setObjectName(name);
        modelOptions->addRow(check);
    }
    auto loraHeading = new QLabel(tr("LoRA"), stylePage);
    QFont bold = loraHeading->font();
    bold.setBold(true);
    loraHeading->setFont(bold);
    styleForm->addRow(loraHeading);
    styleForm->addRow(chooseLoras);
    auto styleLoras = new QListWidget(stylePage);
    styleLoras->setObjectName("settingsLoras");
    styleLoras->setMaximumHeight(96);
    styleForm->addRow(styleLoras);
    auto loraActions = new QHBoxLayout;
    auto editStrength = new QDoubleSpinBox(stylePage);
    editStrength->setRange(-2, 2);
    editStrength->setSingleStep(.05);
    editStrength->setPrefix(tr("Strength") + ": ");
    auto removeLora = new QToolButton(stylePage);
    auto enableLora = new QCheckBox(tr("Enabled"), stylePage);
    enableLora->setObjectName("styleLoraEnabled");
    PluginUi::setIcon(removeLora, "discard");
    removeLora->setToolTip(tr("Remove LoRA"));
    loraActions->addWidget(editStrength);
    loraActions->addWidget(enableLora);
    loraActions->addWidget(removeLora);
    loraActions->addStretch();
    auto loraControls = new QWidget(stylePage);
    loraControls->setLayout(loraActions);
    styleForm->addRow(loraControls);
    auto refreshLoras = [this, styleLoras, editStrength, enableLora, loraControls] {
        const int row = styleLoras->currentRow();
        QSignalBlocker blocker(styleLoras);
        styleLoras->clear();
        for (int i = 0; i < m_loras->count(); ++i) {
            const auto data = m_loras->item(i)->data(Qt::UserRole).toJsonObject();
            styleLoras->addItem(QString(data["title"].toString(data["name"].toString())
                + " · " + QString::number(data["strength"].toDouble(1) * 100) + "%"));
        }
        styleLoras->setCurrentRow(qBound(0, row, styleLoras->count() - 1));
        styleLoras->setVisible(styleLoras->count() > 0);
        loraControls->setVisible(styleLoras->count() > 0);
        if (auto item = m_loras->item(styleLoras->currentRow())) {
            QSignalBlocker strengthBlocker(editStrength);
            QSignalBlocker enabledBlocker(enableLora);
            editStrength->setValue(item->data(Qt::UserRole).toJsonObject()["strength"].toDouble(1));
            enableLora->setChecked(item->data(Qt::UserRole).toJsonObject()["enabled"].toBool(true));
        }
    };
    connect(m_loras->model(), &QAbstractItemModel::rowsInserted, this, refreshLoras);
    connect(m_loras->model(), &QAbstractItemModel::rowsRemoved, this, refreshLoras);
    connect(m_loras->model(), &QAbstractItemModel::modelReset, this, refreshLoras);
    connect(m_loras->model(), &QAbstractItemModel::dataChanged, this, refreshLoras);
    refreshLoras();
    connect(styleLoras, &QListWidget::currentRowChanged, this, [this, editStrength, enableLora](int row) {
        if (auto item = m_loras->item(row)) {
            QSignalBlocker blocker(editStrength);
            QSignalBlocker enabledBlocker(enableLora);
            editStrength->setValue(item->data(Qt::UserRole).toJsonObject()["strength"].toDouble(1));
            enableLora->setChecked(item->data(Qt::UserRole).toJsonObject()["enabled"].toBool(true));
        }
    });
    connect(enableLora, &QCheckBox::toggled, this, [this, styleLoras](bool value) {
        if (m_styleLoading) return;
        if (auto item = m_loras->item(styleLoras->currentRow())) {
            auto data = item->data(Qt::UserRole).toJsonObject();
            data["enabled"] = value;
            item->setData(Qt::UserRole, data);
        }
    });
    connect(editStrength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
        [this, styleLoras](double value) {
            if (auto item = m_loras->item(styleLoras->currentRow())) {
                auto data = item->data(Qt::UserRole).toJsonObject();
                data["strength"] = value;
                item->setData(Qt::UserRole, data);
            }
        });
    connect(removeLora, &QToolButton::clicked, this, [this, styleLoras] {
        delete m_loras->takeItem(styleLoras->currentRow());
    });
    auto stylePrompt = new PromptEditor(stylePage);
    stylePrompt->setObjectName("stylePrompt");
    stylePrompt->setFixedHeight(60);
    stylePrompt->setPlaceholderText(tr("Text added to every prompt. Use {prompt} to place the user's prompt."));
    styleForm->addRow(tr("Style Prompt"), stylePrompt);
    auto styleNegative = new PromptEditor(stylePage, true);
    styleNegative->setObjectName("styleNegative");
    styleNegative->setFixedHeight(42);
    styleForm->addRow(tr("Negative Prompt"), styleNegative);
    auto linkedEdit = new QComboBox(stylePage);
    linkedEdit->setObjectName("linkedEditStyle");
    linkedEdit->setToolTip(tr("Choose the style used on the first switch to editing. Later selections are remembered separately."));
    styleForm->addRow(tr("Linked Edit Style"), linkedEdit);
    auto kreaStyle = section(styleForm, tr("Krea 2 reference settings"));
    kreaStyle->parentWidget()->setObjectName("kreaStyleOptions");
    kreaForm->removeWidget(m_referencePurpose);
    kreaForm->removeWidget(m_fidelity);
    kreaForm->removeWidget(m_referenceDetail);
    kreaStyle->addRow(tr("Reference purpose"), m_referencePurpose);
    kreaStyle->addRow(tr("Reference fidelity"), m_fidelity);
    kreaStyle->addRow(tr("Reference detail"), m_referenceDetail);
    kreaForm->parentWidget()->hide();
    if (auto header = qobject_cast<QWidget*>(kreaForm->parentWidget()->property("sectionHeader").value<QObject*>()))
        header->hide();
    styleForm->addRow(new QLabel(tr("Sampling settings"), stylePage));
    m_promptSyntax = new QComboBox(stylePage);
    m_promptSyntax->setObjectName("promptSyntax");
    m_promptSyntax->addItem("ComfyUI", "comfy");
    m_promptSyntax->addItem("AUTOMATIC1111 / Forge", "a1111");
    m_promptSyntax->setToolTip(tr("A1111 weights, BREAK, AND and prompt schedules. Requires smZ nodes on the server; SD 1.5 and SDXL only."));
    styleForm->addRow(tr("Prompt syntax"), m_promptSyntax);
    m_gpuNoise = new QCheckBox(tr("A1111 seeds (GPU noise)"), stylePage);
    m_gpuNoise->setObjectName("a1111GpuNoise");
    styleForm->addRow(m_gpuNoise);
    m_ensd = new QLineEdit("0", stylePage);
    m_ensd->setObjectName("a1111Ensd");
    m_ensd->setToolTip(tr("Extra noise seed delta. Keep 0 unless matching an A1111 preset."));
    styleForm->addRow("ENSD", m_ensd);
    auto updateSyntax = [this] { updatePromptSyntax(); };
    connect(m_promptSyntax, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateSyntax);
    connect(m_gpuNoise, &QCheckBox::toggled, this, updateSyntax);
    updateSyntax();
    auto quality = section(styleForm, tr("Quality (generate and upscale)"), true);
    auto samplerPreset = new QComboBox(stylePage);
    samplerPreset->setObjectName("styleSamplerPreset");
    samplerPreset->addItem(tr("Custom"), "");
    QFile samplerFile(":/baron/presets/samplers.json");
    QJsonObject samplerPresets;
    if (samplerFile.open(QIODevice::ReadOnly))
        samplerPresets = QJsonDocument::fromJson(samplerFile.readAll()).object();
    for (auto it = samplerPresets.begin(); it != samplerPresets.end(); ++it)
        samplerPreset->addItem(it.key(), it.key());
    quality->addRow(tr("Sampler preset"), samplerPreset);
    connect(samplerPreset, QOverload<int>::of(&QComboBox::activated), this,
        [this, samplerPreset, samplerPresets] {
            const auto preset = samplerPresets[samplerPreset->currentData().toString()].toObject();
            if (preset.isEmpty()) return;
            QScopedValueRollback<bool> loading(m_styleLoading, true);
            for (auto pair : QList<QPair<QComboBox*, QString>> { { m_sampler, "sampler" }, { m_scheduler, "scheduler" } }) {
                const auto value = preset[pair.second].toString();
                if (pair.first->findData(value) < 0) pair.first->addItem(value, value);
                pair.first->setCurrentIndex(pair.first->findData(value));
            }
            m_steps->setValue(preset["steps"].toInt(20));
            m_cfg->setValue(preset["cfg"].toDouble(4));
            saveStyle();
        });
    advancedForm->removeWidget(m_steps);
    advancedForm->removeWidget(m_cfg);
    advancedForm->removeWidget(m_sampler);
    advancedForm->removeWidget(m_scheduler);
    quality->addRow(tr("Sampler"), m_sampler);
    quality->addRow(tr("Scheduler"), m_scheduler);
    quality->addRow(tr("Steps"), m_steps);
    quality->addRow(tr("CFG"), m_cfg);
    advancedForm->parentWidget()->hide();
    if (auto header = qobject_cast<QWidget*>(advancedForm->parentWidget()->property("sectionHeader").value<QObject*>()))
        header->hide();
    auto savePreset = new QPushButton(tr("Save Style"), stylePage);
    styleForm->addRow(savePreset);
    connect(savePreset, &QPushButton::clicked, this, &BaronPanel::saveStyle);
    auto importPreset = new QPushButton(tr("Import Style"), stylePage);
    styleForm->addRow(importPreset);
    connect(importPreset, &QPushButton::clicked, this, [this] {
        const auto path
            = QFileDialog::getOpenFileName(this, tr("Import Style"), {}, tr("Style (*.json)"));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
            return;
        auto data = QJsonDocument::fromJson(file.readAll()).object();
        if (data["name"].toString().isEmpty() || data["checkpoints"].toArray().isEmpty())
            return;
        data["id"] = QUuid::createUuid().toString();
        m_stylePresets.append(data);
        m_styleId = data["id"].toString();
        QJsonArray saved;
        for (const auto& style : m_stylePresets)
            saved.append(style);
        QSettings("BaronEdition", "Orchestrion")
            .setValue("stylePresets", QJsonDocument(saved).toJson());
        rebuildStyles();
        selectStyle();
    });
    auto exportPreset = new QPushButton(tr("Export Style"), stylePage);
    styleForm->addRow(exportPreset);
    connect(exportPreset, &QPushButton::clicked, this, [this] {
        saveStyle();
        const auto path = QFileDialog::getSaveFileName(m_settingsDialog, tr("Export Style"), "style.json", tr("Style (*.json)"));
        if (path.isEmpty()) return;
        for (auto preset : m_stylePresets) {
            if (preset["id"] != m_styleId) continue;
            preset.remove("id");
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(preset).toJson());
                file.commit();
            }
        }
    });
    auto autoSaveStyle = [this] {
        if (m_uiReady && !m_restoring && !m_styleLoading && m_settingsDialog->isVisible())
            saveStyle();
    };
    for (auto field : stylePage->findChildren<QLineEdit*>())
        connect(field, &QLineEdit::editingFinished, this, autoSaveStyle);
    for (auto field : stylePage->findChildren<QPlainTextEdit*>())
        connect(field, &QPlainTextEdit::textChanged, this, autoSaveStyle);
    for (auto field : stylePage->findChildren<QComboBox*>())
        if (field != stylePicker && field != samplerPreset)
            connect(field, QOverload<int>::of(&QComboBox::currentIndexChanged), this, autoSaveStyle);
    for (auto field : stylePage->findChildren<QSpinBox*>())
        connect(field, QOverload<int>::of(&QSpinBox::valueChanged), this, autoSaveStyle);
    for (auto field : stylePage->findChildren<QDoubleSpinBox*>())
        connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, autoSaveStyle);
    for (auto field : stylePage->findChildren<QCheckBox*>())
        if (field != enableLora) connect(field, &QCheckBox::toggled, this, autoSaveStyle);
    connect(m_loras->model(), &QAbstractItemModel::dataChanged, this, autoSaveStyle);
    connect(m_loras->model(), &QAbstractItemModel::rowsRemoved, this, autoSaveStyle);
    addPage(stylePage, tr("Styles"));
    for (auto button : stylePage->findChildren<QToolButton*>()) button->setMinimumSize(40, 40);
    for (auto combo : stylePage->findChildren<QComboBox*>()) combo->setMinimumHeight(36);
    for (auto scroll : m_settingsDialog->findChildren<QScrollArea*>())
        QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    addPage(options, tr("Diffusion"));
    auto interfacePage = new InterfaceSettings(settingPages);
    auto updateStrengthSuffix = [this, strengthPercent] {
        const bool steps = QSettings("BaronEdition", "Orchestrion").value("show_steps", false).toBool();
        strengthPercent->setSuffix(steps ? QString("% (%1/%2)")
            .arg(qCeil(m_steps->value() * m_strength->value())).arg(m_steps->value()) : "%");
    };
    connect(m_steps, QOverload<int>::of(&QSpinBox::valueChanged), this, updateStrengthSuffix);
    connect(m_strength, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateStrengthSuffix);
    connect(interfacePage, &InterfaceSettings::changed, this,
        [this, updateStrengthSuffix](const QString& key, const QVariant& value) {
            if (key == "show_negative_prompt") m_negative->setVisible(value.toBool());
            else if (key == "prompt_line_count") {
                const auto height = fontMetrics().lineSpacing() * value.toInt() + 10;
                m_prompt->setFixedHeight(height);
                QSettings("BaronEdition", "Orchestrion").setValue("promptHeight", height);
                for (auto editor : m_guidance->findChildren<PromptEditor*>())
                    editor->setFixedHeight(height);
            } else if (key == "show_steps") updateStrengthSuffix();
            else if (key == "recent_styles_count") rebuildStyles();
            else if (key == "tagFiles") {
                TagModel::shared()->reload(value.toStringList(), true);
                for (auto editor : findChildren<PromptEditor*>()) editor->reloadTags();
            } else if (key == "thumbnailSize") {
                const int size = value.toInt();
                m_results->setIconSize(QSize(size, size));
                for (int i = 0; i < m_results->count(); ++i)
                    if (!m_results->item(i)->data(Qt::UserRole).toMap()["header"].toBool())
                        updateHistoryItem(m_results->item(i));
            }
        });
    m_negative->setVisible(QSettings("BaronEdition", "Orchestrion").value("show_negative_prompt", true).toBool());
    m_prompt->setFixedHeight(fontMetrics().lineSpacing() * interfacePage->findChild<QSpinBox*>("prompt_line_count")->value() + 10);
    updateStrengthSuffix();
    categories->addItem(tr("Interface"));
    settingPages->addWidget(interfacePage);
    connect(m_styles, QOverload<int>::of(&QComboBox::activated), this, [this] {
        QSettings settings("BaronEdition", "Orchestrion");
        auto recent = settings.value("recentStyles").toStringList();
        const auto id = m_styles->currentData().toString();
        if (id.isEmpty()) return;
        recent.removeAll(id);
        recent.prepend(id);
        while (recent.size() > 10) recent.removeLast();
        settings.setValue("recentStyles", recent);
        rebuildStyles();
    });
    auto performancePage = new QWidget;
    auto performanceForm = new QFormLayout(performancePage);
    auto performanceResolution = new QDoubleSpinBox(performancePage);
    performanceResolution->setObjectName("performanceResolutionMultiplier");
    performanceResolution->setRange(.1, 2);
    performanceResolution->setSingleStep(.1);
    performanceResolution->setValue(QSettings("BaronEdition", "Orchestrion").value("performanceResolutionMultiplier", 1).toDouble());
    performanceForm->addRow(tr("Resolution Multiplier"), performanceResolution);
    connect(performanceResolution, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value) {
        QSettings("BaronEdition", "Orchestrion").setValue("performanceResolutionMultiplier", value);
    });
    auto maximumPixels = new QSpinBox(performancePage);
    maximumPixels->setObjectName("maximumPixelCount");
    maximumPixels->setRange(0, 16);
    maximumPixels->setSuffix(" MP");
    maximumPixels->setValue(QSettings("BaronEdition", "Orchestrion").value("maximumPixelCount", 6).toInt());
    performanceForm->addRow(tr("Maximum Pixel Count"), maximumPixels);
    connect(maximumPixels, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int value) {
        QSettings("BaronEdition", "Orchestrion").setValue("maximumPixelCount", value);
    });
    auto pollInterval = new QSpinBox(performancePage);
    pollInterval->setRange(1, 10);
    pollInterval->setSuffix(tr(" s"));
    pollInterval->setValue(QSettings("BaronEdition", "Orchestrion").value("pollInterval", 2).toInt());
    performanceForm->addRow(tr("Result check interval"), pollInterval);
    connect(pollInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int value) {
        QSettings("BaronEdition", "Orchestrion").setValue("pollInterval", value);
    });
    auto historyHint = new QLabel(tr("History is saved inside the document and in a local recovery cache. "
        "Full images are loaded only when selected. Clear unwanted results from the history menu."), performancePage);
    historyHint->setWordWrap(true);
    performanceForm->addRow(historyHint);
    addPage(performancePage, tr("Performance"));
    addPage(pluginPage, tr("Plugin"));
    categories->setCurrentRow(0);
    auto footer = new QHBoxLayout;
    auto reset = new QPushButton(tr("Restore Defaults"), m_settingsDialog);
    reset->setObjectName("resetInterfaceSettings");
    reset->setEnabled(false);
    connect(categories, &QListWidget::currentRowChanged, reset, [reset](int page) { reset->setEnabled(page == 3); });
    connect(reset, &QPushButton::clicked, interfacePage, &InterfaceSettings::reset);
    footer->addWidget(reset);
    auto folder = new QPushButton(tr("Open Settings Folder"), m_settingsDialog);
    connect(folder, &QPushButton::clicked, this, [] {
        const auto path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    footer->addStretch();
    auto version = new QLabel(tr("Plugin version: %1").arg(BaronUpdates::version()), m_settingsDialog);
    auto versionFont = version->font();
    versionFont.setItalic(true);
    version->setFont(versionFont);
    version->setEnabled(false);
    footer->addWidget(version);
    footer->addStretch();
    footer->addWidget(folder);
    auto doneSettings = new QPushButton(tr("OK"), m_settingsDialog);
    footer->addWidget(doneSettings);
    dialogLayout->addLayout(footer);
    connect(doneSettings, &QPushButton::clicked, this, [this] {
        saveSettings();
        flushDocumentState();
        m_settingsDialog->accept();
    });
    m_progress = new QProgressBar(generation);
    m_progress->setObjectName("generationProgress");
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->setVisible(false);
    form->addRow(m_progress);
    m_status = new QLabel(this);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    form->addRow(m_status);
    connect(m_client, &OrchestrionClient::error, this, [this](const QString& issue) {
        m_busy = false;
        m_run->setEnabled(true);
        m_quote->setEnabled(true);
        m_progress->hide();
        m_status->setText(issue);
        m_resume->setVisible(!m_pendingJob.isEmpty());
    });
    connect(m_client, &OrchestrionClient::prepared, this, [this](const QJsonObject& data) {
        if (m_client->backend() == OrchestrionClient::orchestrion)
            m_price->setText(tr("≈ %1 bleatbucks").arg(data["coins"].toDouble(), 0, 'g', 8));
        {
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
    connect(m_jobs, &JobQueue::changed, this, [this](const QString& id) {
        updateJobs();
        const auto job = m_jobs->job(id);
        if (job && job->context["mode"].toString() == "control")
            m_guidance->setControlBusy(job->context["control_id"].toString(), JobQueue::active(job->state));
        if (job && job->state == JobQueue::failed)
            m_status->setText(job->issue);
        else if (job && job->state == JobQueue::cancelled)
            m_status->setText(tr("Job cancelled"));
    });
    connect(m_jobs, &JobQueue::error, m_status, &QLabel::setText);
    connect(m_jobs, &JobQueue::estimate, this, [this](const QString&, double coins) {
        m_price->setText(tr("≈ %1 bleatbucks").arg(coins, 0, 'g', 8));
    });
    connect(m_jobs, &JobQueue::completed, this, [this](const QString& id) {
        const auto job = m_jobs->job(id);
        if (!job) return;
        if (!job->result.bytes.isEmpty()) receiveResult(job->context, job->result);
        else {
            const auto context = job->context;
            const auto images = job->images.values();
            const auto source = job->source;
            BackgroundWork::run(this, [images, context, source] {
                return HistoryStore::prepare(images, QByteArray::fromBase64(
                    context["selection_mask"].toString().toLatin1()), context["mode"].toString(), source);
            }, [this, context](const HistoryStore::Prepared& result) { receiveResult(context, result); });
        }
        QTimer::singleShot(0, this, [this] { updateJobs(); });
        m_client->account();
    });
    m_uiReady = true;
    QSettings settings("BaronEdition", "Orchestrion");
    m_restoring = true;
    m_url->setText(settings.value("website", "https://orchestrion.su").toString());
    m_prompt->setPlainText(settings.value("prompt").toString());
    m_negative->setPlainText(settings.value("negative").toString());
    m_mode->setCurrentIndex(qMax(0, m_mode->findData(settings.value("mode", "generate"))));
    m_promptBankMode = m_mode->currentData() == "edit" ? "edit" : "generate";
    m_promptBanks = QJsonDocument::fromJson(settings.value("promptBanks").toByteArray()).object();
    if (m_promptBanks.isEmpty()) capturePromptBank();
    restorePromptBank();
    m_restoring = false;
    m_scale->setValue(settings.value("resolutionMultiplier", 1).toDouble());
    m_upscaleFactor->setValue(settings.value("upscaleFactor", 2).toDouble());
    const auto upscaleSettings = QJsonDocument::fromJson(settings.value("upscaleSettings").toByteArray()).object();
    if (!upscaleSettings.isEmpty()) m_upscale->restore(upscaleSettings);
    strengthPercent->setValue(settings.value("strength", 100).toInt());
    m_batch->setValue(settings.value("batch", 1).toInt());
    m_seed->setText(settings.value("seed", "-1").toString());
    m_fixedSeed->setChecked(settings.value("fixedSeed", false).toBool());
    m_sampler->setCurrentIndex(qMax(0, m_sampler->findData(settings.value("sampler", ""))));
    m_scheduler->setCurrentIndex(qMax(0, m_scheduler->findData(settings.value("scheduler", ""))));
    m_promptSyntax->setCurrentIndex(qMax(0, m_promptSyntax->findData(settings.value("promptMode", "comfy"))));
    m_gpuNoise->setChecked(settings.value("a1111GpuNoise", false).toBool());
    m_ensd->setText(settings.value("a1111Ensd", "0").toString());
    updateMode();
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(700);
    connect(m_prompt, &PromptEditor::contextActionsChanged, m_saveTimer, QOverload<>::of(&QTimer::start));
    connect(m_negative, &PromptEditor::contextActionsChanged, m_saveTimer, QOverload<>::of(&QTimer::start));
    connect(m_saveTimer, &QTimer::timeout, this, [this] {
        saveSettings();
        flushDocumentState();
    });
    auto changed = [this] {
        if (!m_restoring)
            m_saveTimer->start();
    };
    connect(m_prompt, &QPlainTextEdit::textChanged, this, changed);
    connect(m_negative, &QPlainTextEdit::textChanged, this, changed);
    for (auto combo : { m_mode, m_model, m_sampler, m_scheduler, m_queuePosition,
             m_referencePurpose, m_referenceDetail })
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    for (auto spin : { m_steps, m_batch })
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    for (auto spin : { m_strength, m_scale, m_upscaleFactor, m_cfg, m_fidelity })
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, changed);
    connect(m_seed, &QLineEdit::textChanged, this, changed);
    connect(m_fixedSeed, &QCheckBox::toggled, this, changed);
    connect(m_promptSyntax, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    connect(m_gpuNoise, &QCheckBox::toggled, this, changed);
    connect(m_ensd, &QLineEdit::textChanged, this, changed);
    connect(m_guidance, &GuidancePanel::changed, this, changed);
    connect(m_upscale, &UpscaleWidget::changed, this, changed);
    connect(m_loras->model(), &QAbstractItemModel::dataChanged, this, changed);
    connect(m_loras->model(), &QAbstractItemModel::rowsInserted, this, changed);
    connect(m_loras->model(), &QAbstractItemModel::rowsRemoved, this, changed);
    m_priceClient = new OrchestrionClient(this);
    connect(m_priceClient, &OrchestrionClient::quoted, this, [this](const QJsonObject& data) {
        m_priceBusy = false;
        if (!data["coins"].isDouble())
            return;
        m_price->setText(
            tr("≈ %1 bleatbucks").arg(data["coins"].toDouble() * m_batch->value(), 0, 'g', 8));
    });
    connect(m_priceClient, &OrchestrionClient::error, this, [this](const QString&) {
        m_priceBusy = false;
        m_priceKey.clear();
        m_price->setText(tr("Price temporarily unavailable · website rates apply"));
    });
    m_priceTimer = new QTimer(this);
    m_priceTimer->setInterval(1500);
    connect(m_priceTimer, &QTimer::timeout, this, &BaronPanel::refreshPrice);
    m_priceTimer->start();
    m_inpaint->restore(QJsonDocument::fromJson(settings.value("inpaintOptions").toByteArray()).object());
    m_inpaintMode=QJsonDocument::fromJson(settings.value("inpaintOptions").toByteArray()).object()["mode"].toString("automatic");
    m_stylePresets.clear();
    const auto saved
        = QJsonDocument::fromJson(settings.value("stylePresets").toByteArray()).array();
    for (auto item : saved)
        m_stylePresets.append(item.toObject());
    m_styleId = settings.value("styleId").toString();
    m_styleBanks = QJsonDocument::fromJson(settings.value("styleBanks").toByteArray()).object();
    m_editSourceStyle = settings.value("editSourceStyle").toString();
    rebuildStyles();
    updateJobs();
    connect(m_connectionMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changeConnection);
    const int connectionIndex = m_connectionMode->findData(settings.value("connectionBackend", int(OrchestrionClient::orchestrion)));
    if (connectionIndex != m_connectionMode->currentIndex()) m_connectionMode->setCurrentIndex(qMax(0, connectionIndex));
    else changeConnection();
}
BaronPanel::~BaronPanel() {
    saveSettings();
    for (auto object : findChildren<QObject*>())
        QObject::disconnect(object, nullptr, this, nullptr);
}

QByteArray BaronPanel::png(const QImage& image) {
    if (image.isNull())
        return {};
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
void BaronPanel::saveSettings() {
    BaronDiagnostics::record("settings.begin");
    QSettings settings("BaronEdition", "Orchestrion");
    auto inpaint=m_inpaint->state();inpaint["mode"]=m_inpaintMode;
    settings.setValue("inpaintOptions",QJsonDocument(inpaint).toJson(QJsonDocument::Compact));
    settings.setValue("styleId", m_styleId);
    captureStyleBank();
    settings.setValue("styleBanks", QJsonDocument(m_styleBanks).toJson(QJsonDocument::Compact));
    settings.setValue("editSourceStyle", m_editSourceStyle);
    if (m_client->backend() == OrchestrionClient::orchestrion) settings.setValue("website", m_url->text());
    else if (m_client->backend() == OrchestrionClient::comfyui) {
        auto url = QUrl(m_url->text().contains("://") ? m_url->text() : QString("http://" + m_url->text())); url.setQuery(QString());
        settings.setValue("comfyServer", url.toString());
    }
    settings.setValue("prompt", m_prompt->toPlainText());
    settings.setValue("negative", m_negative->toPlainText());
    capturePromptBank();
    settings.setValue("promptBanks", QJsonDocument(m_promptBanks).toJson(QJsonDocument::Compact));
    settings.setValue("mode", m_mode->currentData());
    if (!m_model->currentData().toString().isEmpty())
        settings.setValue("model", m_model->currentData());
    settings.setValue("steps", m_steps->value());
    settings.setValue("cfg", m_cfg->value());
    settings.setValue("strength", int(m_strength->value() * 100));
    settings.setValue("batch", m_batch->value());
    settings.setValue("seed", m_seed->text());
    settings.setValue("fixedSeed", m_fixedSeed->isChecked());
    settings.setValue("resolutionMultiplier", m_scale->value());
    settings.setValue("upscaleFactor", m_upscaleFactor->value());
    settings.setValue("upscaleSettings", QJsonDocument(m_upscale->state()).toJson(QJsonDocument::Compact));
    settings.setValue("sampler", m_sampler->currentData());
    settings.setValue("scheduler", m_scheduler->currentData());
    settings.setValue("promptMode", m_promptSyntax->currentData());
    settings.setValue("a1111GpuNoise", m_gpuNoise->isChecked());
    settings.setValue("a1111Ensd", m_ensd->text());
    BaronDiagnostics::record("settings.end");
}
void BaronPanel::capturePromptBank() {
    m_promptBanks[m_promptBankMode] = QJsonObject {
        { "prompt", m_prompt->toPlainText() }, { "negative", m_negative->toPlainText() },
        { "prompt_disabled", m_prompt->disabledFragments() },
        { "negative_disabled", m_negative->disabledFragments() } };
}
void BaronPanel::updatePromptSyntax() {
    QString architecture;
    for (auto value : m_catalog) {
        const auto model = value.toObject();
        if (model["name"] == m_model->currentData())
            architecture = model["architecture"].toString(model["family"].toString()).toLower();
    }
    const bool family = architecture.isEmpty() || QStringList {
        "sd15", "sd1.5", "sdxl", "illu", "illu_v", "illustrious", "pony", "noobai" }.contains(architecture);
    const bool supported = family && m_client->backend() != OrchestrionClient::interstice;
    m_promptSyntax->setProperty("familySupported", family);
    const bool encoder = m_promptSyntax->property("available").isNull() || m_promptSyntax->property("available").toBool();
    const bool noise = m_gpuNoise->property("available").isNull() || m_gpuNoise->property("available").toBool();
    m_promptSyntax->setEnabled(supported && encoder);
    m_gpuNoise->setEnabled(supported && encoder && noise && m_promptSyntax->currentData() == "a1111");
    m_ensd->setEnabled(m_gpuNoise->isEnabled() && m_gpuNoise->isChecked());
}
void BaronPanel::restorePromptBank() {
    const auto bank = m_promptBanks[m_promptBankMode].toObject();
    m_prompt->replacePromptText(bank["prompt"].toString().left(65536));
    m_negative->replacePromptText(bank["negative"].toString().left(65536));
    m_prompt->restoreDisabledFragments(bank["prompt_disabled"].toArray());
    m_negative->restoreDisabledFragments(bank["negative_disabled"].toArray());
}
void BaronPanel::captureStyleBank() {
    if (m_styleId.isEmpty()) return;
    QJsonArray loras;
    for (int i = 0; i < m_loras->count(); ++i)
        loras.append(m_loras->item(i)->data(Qt::UserRole).toJsonObject());
    m_styleBanks[m_promptBankMode] = QJsonObject {
        { "style_id", m_styleId }, { "steps", m_steps->value() }, { "cfg", m_cfg->value() },
        { "sampler", m_sampler->currentData().toString() }, { "scheduler", m_scheduler->currentData().toString() },
        { "prompt_mode", m_promptSyntax->currentData().toString() }, { "a1111_gpu_noise", m_gpuNoise->isChecked() },
        { "a1111_ensd", m_ensd->text() }, { "loras", loras },
        { "reference_purpose", m_referencePurpose->currentData().toString() },
        { "reference_fidelity", m_fidelity->value() }, { "reference_detail", m_referenceDetail->currentData().toInt() } };
    for (const auto& preset : m_stylePresets)
        if (preset["id"] == m_styleId) {
            auto bank = m_styleBanks[m_promptBankMode].toObject();
            bank["preset"] = preset;
            m_styleBanks[m_promptBankMode] = bank;
        }
    if (m_promptBankMode == "edit" && !m_editSourceStyle.isEmpty()) {
        auto edits = m_styleBanks["edits"].toObject();
        edits[m_editSourceStyle] = m_styleBanks["edit"];
        m_styleBanks["edits"] = edits;
    }
}
void BaronPanel::restoreStyleBank(const QString& previous) {
    auto bank = m_styleBanks[m_promptBankMode].toObject();
    if (m_promptBankMode == "edit") {
        m_editSourceStyle = previous;
        bank = m_styleBanks["edits"].toObject()[previous].toObject();
    }
    QString id = bank.value("style_id").toString();
    if (id.isEmpty() && m_promptBankMode == "edit")
        for (const auto& preset : m_stylePresets)
            if (preset["id"] == previous) id = preset["linked_edit_style"].toString();
    if (id.isEmpty() && m_promptBankMode == "edit" && !previous.startsWith("model:")) {
        QJsonObject copy;
        for (auto& preset : m_stylePresets)
            if (preset["id"] == previous) {
                copy = preset;
                id = QUuid::createUuid().toString();
                copy["id"] = id;
                copy["name"] = QString(copy["name"].toString() + tr(" (editing)"));
                copy.remove("linked_edit_style");
                preset["linked_edit_style"] = id;
                break;
            }
        if (!copy.isEmpty()) {
            m_stylePresets.append(copy);
            QJsonArray saved;
            for (const auto& preset : m_stylePresets) saved.append(preset);
            QSettings("BaronEdition", "Orchestrion").setValue("stylePresets", QJsonDocument(saved).toJson());
            rebuildStyles();
        }
    }
    if (id.isEmpty()) id = previous;
    const int index = m_styles->findData(id);
    if (index < 0) return;
    QScopedValueRollback<bool> loading(m_styleLoading, true);
    m_styles->setCurrentIndex(index);
    selectStyle();
    if (!bank.isEmpty()) {
        m_steps->setValue(bank["steps"].toInt(20));
        m_cfg->setValue(bank["cfg"].toDouble(4));
        for (auto pair : QList<QPair<QComboBox*, QString>> { { m_sampler, "sampler" }, { m_scheduler, "scheduler" }, { m_promptSyntax, "prompt_mode" } }) {
            const int selected = pair.first->findData(bank[pair.second].toString());
            if (selected >= 0) pair.first->setCurrentIndex(selected);
        }
        m_gpuNoise->setChecked(bank["a1111_gpu_noise"].toBool());
        m_ensd->setText(bank["a1111_ensd"].toString("0"));
        m_referencePurpose->setCurrentIndex(qMax(0, m_referencePurpose->findData(bank["reference_purpose"].toString("identity"))));
        m_fidelity->setValue(bank["reference_fidelity"].toDouble(4));
        m_referenceDetail->setCurrentIndex(qMax(0, m_referenceDetail->findData(bank["reference_detail"].toInt(768))));
        m_loras->clear();
        for (auto value : bank["loras"].toArray()) {
            const auto data = value.toObject();
            auto item = new QListWidgetItem(data["title"].toString(data["name"].toString()), m_loras);
            item->setData(Qt::UserRole, data);
        }
    }
    captureStyleBank();
}
void BaronPanel::updateMode() {
    if (!m_uiReady)
        return;
    const auto mode = m_mode->currentData().toString();
    if (!m_restoring && (mode == "generate" || mode == "edit") && mode != m_promptBankMode) {
        capturePromptBank();
        captureStyleBank();
        const auto previous = m_styleId;
        m_promptBankMode = mode;
        restorePromptBank();
        restoreStyleBank(previous);
    }
    const bool upscale = mode == "upscale";
    if(m_inpaint) {
        const bool selection=m_host&&m_host->hasSelection();
        m_inpaint->setVisible(!upscale&&selection&&m_inpaintMode=="custom");
        const auto info=m_model->currentData(Qt::UserRole+1).toJsonObject();
        m_inpaint->setCapabilities(info["architecture"].toString(info["family"].toString()),m_strength->value(),mode=="edit");
    }
    m_upscale->setVisible(upscale);
    m_upscale->upscaler->setVisible(upscale);
    m_styles->setVisible(!upscale);
    findChild<QToolButton*>("styleSettings")->setVisible(!upscale);
    m_prompt->setVisible(!upscale);
    m_negative->setVisible(!upscale && QSettings("BaronEdition", "Orchestrion").value("show_negative_prompt", true).toBool());
    m_guidance->setVisible(!upscale);
    findChild<QWidget*>("generationStrengthRow")->setVisible(!upscale);
    if (m_host) m_upscale->setCanvasSize(m_host->imageBounds(false).size());
    m_upscale->setPrompt(m_prompt->toPlainText(), m_guidance->state()["regions"].toArray().size());
    m_run->setText(mode == "generate" ? (m_strength->value() < 1 ? tr("Refine") : tr("Generate"))
                                      : m_mode->currentText());
    if (upscale && m_upscaleFactor->value() == 1 && m_upscale->state()["use_diffusion"].toBool())
        m_run->setText(tr("Refine"));
    PluginUi::setIcon(m_workspace,
        mode == "upscale"          ? "workspace-upscaling"
            : mode == "background" ? "workspace-background"
                                   : "workspace-generation");
    PluginUi::setIcon(m_run,
        mode == "edit"                                      ? "edit"
            : mode == "generate" && m_strength->value() < 1 ? "refine"
            : mode == "background"                          ? "workspace-background"
            : mode == "upscale"                             ? "workspace-upscaling"
                                                            : "workspace-generation");
    if(m_inpaintMode=="custom"&&m_host&&m_host->hasSelection()&&!upscale)
        m_run->setText(mode=="edit"?tr("Edit (Custom)"):m_strength->value()<1?tr("Refine (Custom)"):tr("Generate (Custom)"));
    if(mode=="generate"&&m_strength->value()==1&&m_host&&m_host->hasSelection()&&m_inpaintMode!="custom") {
        auto inpaintMode=m_inpaintMode;
        if(inpaintMode=="automatic") {
            const auto area=m_host->imageBounds(true), full=m_host->imageBounds(false);
            inpaintMode=area.width()>=full.width()||area.height()>=full.height()?"expand":"fill";
        }
        const QMap<QString,QString> labels{{"fill",tr("Fill")},{"expand",tr("Expand")},{"add_object",tr("Add Content")},{"remove_object",tr("Remove Content")},{"replace_background",tr("Replace Background")}};
        m_run->setText(labels.value(inpaintMode,tr("Generate")));PluginUi::setIcon(m_run,"inpaint-"+inpaintMode);
    }
    m_modeButton->setVisible(mode == "generate" || mode == "edit");
    m_strength->setEnabled(mode == "edit" || mode == "generate");
    if (auto slider = findChild<QSlider*>("denoiseSlider"))
        slider->parentWidget()->setEnabled(mode == "edit" || mode == "generate");
    m_scale->setEnabled(mode == "generate" || mode == "edit");
    m_upscaleFactor->setEnabled(mode == "upscale");
    m_batch->setEnabled(mode == "generate" || mode == "edit");
    m_model->setEnabled(mode == "generate" || mode == "edit" || upscale);
    for (int i = 0; i < m_refs->count(); ++i)
        m_refs->item(i)->setText(tr("Image %1").arg(i + (mode == "edit" ? 2 : 1)));
    for (const auto& name : { "batchSlider" }) {
        auto slider = findChild<QSlider*>(name);
        auto row = qobject_cast<QFormLayout*>(slider->parentWidget()->layout());
        if (!row) continue;
        for (int i = 0; i < row->rowCount(); ++i) {
            auto field = row->itemAt(i, QFormLayout::FieldRole);
            if (!field || !field->layout() || field->layout()->indexOf(slider) < 0) continue;
            for (int j = 0; j < field->layout()->count(); ++j)
                if (auto widget = field->layout()->itemAt(j)->widget()) widget->setVisible(!upscale);
            if (auto label = row->itemAt(i, QFormLayout::LabelRole)) label->widget()->setVisible(!upscale);
        }
    }
}
QJsonObject BaronPanel::input(bool pixels) {
    const auto mode = m_mode->currentData().toString();
    bool validSeed;
    const auto seed = m_fixedSeed->isChecked() ? m_seed->text().toLongLong(&validSeed) : -1;
    if (!m_fixedSeed->isChecked())
        validSeed = true;
    if (!validSeed || seed < -1 || seed > 4294967295LL) {
        m_status->setText(tr("Seed must be -1 or a number from 0 to 4294967295"));
        return {};
    }
    const auto model = m_model->currentData().toString();
    if ((mode == "generate" || mode == "edit" || (mode == "upscale" && m_upscale->state()["use_diffusion"].toBool())) && model.isEmpty()) {
        m_status->setText(tr("Choose a model first"));
        return {};
    }
    QJsonObject data { { "mode", mode }, { "model", model }, { "style_options", QJsonObject() }, { "prompt", m_prompt->toPlainText() },
        { "negative", m_negative->toPlainText() }, { "width", m_width->value() },
        { "height", m_height->value() }, { "steps", m_steps->value() }, { "cfg", m_cfg->value() },
        { "strength", m_strength->value() }, { "scale", m_upscaleFactor->value() },
        { "resolution_multiplier", m_scale->value() },
        { "sampler", m_sampler->currentData().toString() },
        { "scheduler", m_scheduler->currentData().toString() }, { "batch", m_batch->value() },
        { "seed", double(seed == -1 ? QRandomGenerator::global()->generate() : seed) } };
    for (const auto& style : m_stylePresets)
        if (style["id"] == m_styleId)
            data["style_options"] = style;
    if (m_promptSyntax->currentData() == "a1111" && m_promptSyntax->property("familySupported").toBool()
        && (mode == "generate" || mode == "edit" || mode == "upscale")) {
        bool valid;
        const auto ensd = m_ensd->text().toULongLong(&valid);
        if (m_gpuNoise->isChecked() && (!valid || ensd > 4294967295ULL)) {
            m_status->setText(tr("ENSD must be between 0 and 4294967295"));
            return {};
        }
        data["prompt_mode"] = "a1111";
        data["a1111_gpu_noise"] = m_gpuNoise->isChecked();
        data["a1111_ensd"] = double(m_gpuNoise->isChecked() ? ensd : 0);
    }
    data["reference_purpose"] = m_referencePurpose->currentData().toString();
    const QSettings performance("BaronEdition", "Orchestrion");
    data["resolution_multiplier"] = m_scale->value() == 1
        ? performance.value("performanceResolutionMultiplier", 1).toDouble() : m_scale->value();
    data["max_pixel_count"] = performance.value("maximumPixelCount", 6).toInt();
    if (m_client->backend() == OrchestrionClient::interstice)
        data["max_pixel_count"] = qBound(1, data["max_pixel_count"].toInt(), 8);
    if (mode == "upscale") {
        data["upscale_options"] = m_upscale->state();
        data["strength"] = m_upscale->effectiveStrength();
        data["batch"] = 1;
        const auto bounds = m_host ? m_host->imageBounds(false) : QRect();
        if (double(bounds.width()) * bounds.height() * m_upscaleFactor->value() * m_upscaleFactor->value() > 67108864) {
            m_status->setText(tr("Upscale exceeds 64 megapixels"));
            return {};
        }
    }
    data["reference_fidelity"] = m_fidelity->value();
    data["reference_detail"] = m_referenceDetail->currentData().toInt();
    QJsonArray loras;
    QSet<QString> promptLoras;
    const QString promptText = m_prompt->toPlainText() + " " + m_negative->toPlainText()
        + " " + data.value("style_options").toObject().value("style_prompt").toString();
    auto promptTags = QRegularExpression("<lora:([^:<>]+)(?::[^:<>]*)?>",
        QRegularExpression::CaseInsensitiveOption).globalMatch(promptText);
    while (promptTags.hasNext()) {
        auto name = promptTags.next().captured(1).replace('\\', '/').toLower();
        if (name.endsWith(".safetensors")) name.chop(12);
        promptLoras.insert(name);
    }
    for (int i = 0; i < m_loras->count(); ++i) {
        const auto lora = m_loras->item(i)->data(Qt::UserRole).toJsonObject();
        if (!lora["enabled"].toBool(true)) continue;
        auto name = lora["name"].toString().replace('\\', '/').toLower();
        if (name.endsWith(".safetensors")) name.chop(12);
        if (promptLoras.contains(name)) continue;
        loras.append(
            QJsonObject { { "name", lora["name"] }, { "strength", lora["strength"].toDouble(1) } });
    }
    data["loras"] = loras;
    QJsonArray references;
    for (const auto& image : m_referenceImages)
        references.append(QString::fromLatin1(png(image).toBase64()));
    data["references"] = references;
    data["instruction_edit"] = mode == "edit";
    if (mode == "generate" && m_strength->value() < 1)
        data["mode"] = "edit";
    QString issue;
    auto inpaint=m_inpaint->state();inpaint["mode"]=m_inpaintMode;inpaint["strength"]=m_strength->value();inpaint["edit"]=mode=="edit";
    const auto info=m_model->currentData(Qt::UserRole+1).toJsonObject();inpaint["arch"]=info["architecture"].toString(info["family"].toString());
    if(m_host&&m_inpaintMode=="automatic") {
        auto area=m_host->imageBounds(true);auto full=m_host->imageBounds(false);
        inpaint["resolved_mode"]=area.width()>=full.width()||area.height()>=full.height()?"expand":"fill";
    }
    const auto canvas = !m_host ? CanvasSnapshot() : mode=="edit"||mode=="generate"
        ? m_host->captureWithContext(inpaint,&issue) : m_host->capture(false,&issue);
    data["inpaint_options"]=inpaint;
    if (canvas.image.isNull()) {
        m_status->setText(issue.isEmpty() ? tr("Open a Krita document first") : issue);
        return {};
    }
    if (pixels && (mode != "generate" || m_strength->value() < 1 || !canvas.mask.isNull())) {
        data["image"] = QString::fromLatin1(png(canvas.image).toBase64());
        if (!canvas.mask.isNull())
            data["mask"] = QString::fromLatin1(png(canvas.mask).toBase64());
    }
    data["width"] = canvas.image.width();
    data["height"] = canvas.image.height();
    m_targetId = canvas.id;
    const auto outputBounds=canvas.resultBounds.isEmpty()?canvas.bounds:canvas.resultBounds;
    const auto relative=outputBounds.translated(-canvas.bounds.topLeft());
    if(!canvas.mask.isNull()) data["selection_bounds"]=QJsonArray{relative.x(),relative.y(),relative.width(),relative.height()};
    m_captureBounds = outputBounds.isEmpty() ? QRect(QPoint(),canvas.image.size()) : outputBounds;
    m_captureMask = canvas.resultMask.isNull()?canvas.mask:canvas.resultMask;
    m_sourceImage = canvas.resultBounds.isEmpty()?canvas.image:canvas.image.copy(relative);
    if (mode == "generate" || mode == "edit" || (mode == "upscale" && m_upscale->state()["use_prompt"].toBool())) {
        const auto guidance = m_guidance->input(
            canvas.bounds.isEmpty() ? QRect(QPoint(), canvas.image.size()) : canvas.bounds,
            canvas.mask, &issue);
        if (guidance.isEmpty()) {
            m_status->setText(issue);
            return {};
        }
        data["controls"] = guidance["controls"];
        data["regions"] = guidance["regions"];
    }
    return data;
}
void BaronPanel::prepare(bool run) {
    if (m_busy || m_replacingQueue)
        return;
    if (!m_client->signedIn()) {
        m_status->setText(tr("Not signed in. Click Configure to connect."));
        return;
    }
    const auto data = input(true);
    if (data.isEmpty())
        return;
    saveSettings();
    flushDocumentState();
    if (run) {
        if (data["mode"] == "upscale") {
            for (const auto& job : m_jobs->jobs())
                if (JobQueue::active(job->state) && job->context["mode"] == "upscale"
                    && job->context["document"].toString() == m_host->documentId()) {
                    m_status->setText(tr("Wait for the current upscale job to finish."));
                    return;
                }
            m_captureBounds = QRect(0, 0, int(std::nearbyint(data["width"].toInt() * m_upscaleFactor->value())),
                int(std::nearbyint(data["height"].toInt() * m_upscaleFactor->value())));
        }
        const bool replace = m_queuePosition->currentData().toString() == "replace";
        QStringList cancelledJobs;
        int count = 0;
        for (const auto& job : m_jobs->jobs()) {
            if (replace && JobQueue::active(job->state) && job->state != JobQueue::running
                && job->state != JobQueue::downloading)
                cancelledJobs.append(job->id);
            else if (JobQueue::active(job->state))
                ++count;
        }
        if (count + data["batch"].toInt(1) > JobQueue::max_jobs) {
            m_status->setText(tr("The queue is full. Wait for a result or remove a stopped job."));
            return;
        }
        auto context = QJsonObject { { "target", m_targetId }, { "mode", data["mode"] },
            { "document", m_host->documentId() }, { "prompt", data["prompt"] },
            { "bounds", QJsonArray { m_captureBounds.x(), m_captureBounds.y(),
                m_captureBounds.width(), m_captureBounds.height() } },
            { "selection_mask", QString::fromLatin1(png(m_captureMask).toBase64()) },
            { "settings", generationSettings() } };
        auto settings = context["settings"].toObject();
        settings["seed"] = QString::number(quint32(data["seed"].toDouble()));
        settings["fixed_seed"] = true;
        settings["regions"] = data["regions"];
        if (data["mode"] == "upscale") {
            QJsonArray regions;
            for (auto value : data["regions"].toArray()) {
                auto region = value.toObject();
                const auto mask = QImage::fromData(QByteArray::fromBase64(region["mask"].toString().toLatin1()));
                region["mask"] = QString::fromLatin1(png(mask.scaled(m_captureBounds.size(),
                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation)).toBase64());
                regions.append(region);
            }
            settings["regions"] = regions;
        }
        context["settings"] = settings;
        const auto source = data["mode"] == "background" ? m_sourceImage : QImage();
        if (!cancelledJobs.isEmpty()) {
            m_replacingQueue = true;
            m_run->setEnabled(false);
            for (const auto& id : cancelledJobs)
                m_jobs->cancel(id);
            const auto deadline = QDateTime::currentMSecsSinceEpoch() + 30000;
            auto wait = new QTimer(this);
            wait->setInterval(100);
            connect(wait, &QTimer::timeout, this,
                [this, wait, cancelledJobs, deadline, data, context, source] {
                    bool pending = false, failed = false;
                    for (const auto& id : cancelledJobs) {
                        const auto job = m_jobs->job(id);
                        pending |= job && (JobQueue::active(job->state) || job->cancelPending);
                        failed |= job && (job->state == JobQueue::failed
                            || (job->state == JobQueue::cancelled && !job->issue.isEmpty()));
                    }
                    if (pending && QDateTime::currentMSecsSinceEpoch() < deadline && !failed)
                        return;
                    wait->stop();
                    wait->deleteLater();
                    m_replacingQueue = false;
                    m_run->setEnabled(true);
                    if (pending || failed)
                        m_status->setText(
                            tr("Could not replace the queue. No new generation was started."));
                    else
                        submitBatch(data, context, source, true);
                });
            wait->start();
        } else {
            submitBatch(data, context, source, replace || m_queuePosition->currentData().toBool());
        }
        return;
    }
    m_submitAfterPrepare = run;
    m_busy = true;
    m_jobMode = data["mode"].toString();
    m_foreground = QImage();
    m_pendingJob.clear();
    m_resume->hide();
    m_jobImages.clear();
    if (m_host)
        m_host->clearPreview();
    m_previewResult.clear();
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
void BaronPanel::submitBatch(
    const QJsonObject& data, QJsonObject context, const QImage& source, bool front) {
    const auto original = context["settings"].toObject();
    for (int i = 0; i < data["batch"].toInt(1); ++i) {
        auto request = data;
        request["batch"] = 1;
        request["seed"] = double(quint32(data["seed"].toDouble()) + quint32(i));
        auto settings = original;
        settings["seed"] = QString::number(quint32(request["seed"].toDouble()));
        settings["fixed_seed"] = true;
        context["settings"] = settings;
        const auto id = m_jobs->start(request, context, source, front);
        if (id.isEmpty())
            break;
        if (request["mode"] == "upscale") {
            const auto area = context["bounds"].toArray();
            QString issue;
            if (!m_host->scaleTarget(context["target"].toString(), QSize(area[2].toInt(), area[3].toInt()), &issue)) {
                m_jobs->cancel(id);
                m_status->setText(issue);
                break;
            }
            m_previewResult.clear();
            updateMode();
        }
    }
}
void BaronPanel::canvasSelectionChanged() {
    if(!m_uiReady)return;
    m_inpaint->setLayers(m_host ? m_host->layers() : QJsonArray());
    updateMode();
    if(m_priceTimer)m_priceTimer->start();
}
void BaronPanel::documentChanged() {
    m_prompt->resetInputState();
    m_negative->resetInputState();
    m_previewResult.clear();
    restoreDocumentState();
    m_historyDocument = m_host ? m_host->documentId() : QString();
    restoreHistory();
    m_inpaint->setLayers(m_host ? m_host->layers() : QJsonArray());
    updateMode();
    updateJobs();
}
QSharedPointer<HistoryStore> BaronPanel::history(const QString& document) {
    if (document.isEmpty())
        return {};
    if (!m_histories.contains(document)) {
        auto store = QSharedPointer<HistoryStore>::create(document);
        if (!store->load([this, document](const QString& key) {
                return m_host && m_host->documentId() == document ? m_host->annotation(key) : QByteArray();
            })) {
            m_status->setText(tr("Could not load generation history: %1").arg(store->error()));
            return {};
        }
        m_histories[document] = store;
    }
    return m_histories.value(document);
}
void BaronPanel::persistHistory() {
    auto store = m_histories.value(m_historyDocument);
    if (store) store->saveAsync(this, [this](const QString& key, const QByteArray& bytes) {
        if (m_host && m_host->documentId() == m_historyDocument) m_host->setAnnotation(key, bytes);
    }, [this](const QString& issue) {
        if (!issue.isEmpty()) m_status->setText(tr("Could not save generation history: %1").arg(issue));
    });
}
void BaronPanel::restoreHistory(bool appendOnly) {
    QStringList selected;
    for (auto item : m_results->selectedItems()) selected.append(item->data(Qt::UserRole).toMap()["key"].toString());
    QSet<QString> present;
    if (appendOnly) for (int i = 0; i < m_results->count(); ++i) {
        const auto id = m_results->item(i)->data(Qt::UserRole).toMap()["history_id"].toString();
        if (!id.isEmpty()) present.insert(id);
    }
    else {
        m_results->clear();
        m_historyKey.clear();
        m_apply->setEnabled(false);
    }
    const auto store = history(m_historyDocument);
    if (!store)
        return;
    QJsonObject previous;
    for (auto value : store->entries()) {
        const auto entry = value.toObject();
        const auto settings = HistoryStore::settings(entry);
        const auto extra = entry["params"].toObject()["metadata"].toObject()["baron"].toObject();
        const auto time = QDateTime::fromString(extra["time"].toString(), Qt::ISODate)
                              .toLocalTime().toString("HH:mm");
        auto group = entry["params"].toObject();
        group.remove("seed");
        auto metadata = group["metadata"].toObject();
        auto nativeSettings = metadata["baron"].toObject()["settings"].toObject();
        for (const auto& key : { "seed", "fixed_seed", "batch" }) nativeSettings.remove(key);
        metadata.remove("baron");
        if (!nativeSettings.isEmpty()) metadata["native_settings"] = nativeSettings;
        group["metadata"] = metadata;
        if (present.contains(entry["id"].toString())) { previous = group; continue; }
        if (group != previous) {
            const auto prompt = settings["prompt"].toString().simplified();
            const auto strength = settings["strength"].toDouble(1);
            const QString detail = strength == 1 ? QString() : QString::number(strength * 100, 'f', 0) + "% - ";
            auto header = new QListWidgetItem(QString(time + " - " + detail + (prompt.isEmpty() ? QString("<no prompt>") : prompt)), m_results);
            header->setFlags(Qt::NoItemFlags);
            header->setData(Qt::UserRole, QVariantMap { { "header", true } });
            header->setSizeHint(QSize(9999, fontMetrics().lineSpacing() + 4));
            header->setTextAlignment(Qt::AlignLeft);
            header->setToolTip(settings["prompt"].toString());
            previous = group;
        }
        for (int i = 0; i < entry["offsets"].toArray().size(); ++i) {
            const int size = QSettings("BaronEdition", "Orchestrion").value("thumbnailSize", 96).toInt();
            const auto thumbnail = store->cachedThumbnail(entry["id"].toString(), i, size);
            auto item = new QListWidgetItem(tr("Image %1").arg(i + 1), m_results);
            item->setData(Qt::UserRole, QVariantMap {
                { "history_id", entry["id"].toString() }, { "history_index", i },
                { "image", thumbnail }, { "bounds", HistoryStore::bounds(entry) },
                { "selection_mask", extra["selection_mask"].toString() },
                { "key", QString(entry["id"].toString() + ":" + QString::number(i)) },
                { "settings", settings }, { "caption", item->text() }, { "time", time },
                { "favorite", extra["favorites"].toObject()[QString::number(i)].toBool() },
                { "applied", entry["in_use"].toObject()[QString::number(i)].toBool() } });
            updateHistoryItem(item);
            if (thumbnail.isNull()) {
                const auto id = entry["id"].toString();
                const auto document = m_historyDocument;
                const auto snapshot = *store;
                BackgroundWork::run(this, [snapshot, id, i, size] { return snapshot.image(id, i, QSize(size, size)); },
                    [this, store, id, i, size, document](const QImage& thumbnail) {
                        store->cacheThumbnail(id, i, size, thumbnail);
                        if (m_historyDocument != document) return;
                        const QString key = id + ":" + QString::number(i);
                        for (int row = 0; row < m_results->count(); ++row) {
                            auto item = m_results->item(row);
                            auto data = item->data(Qt::UserRole).toMap();
                            if (data["key"].toString() != key) continue;
                            data["image"] = thumbnail;
                            item->setData(Qt::UserRole, data); updateHistoryItem(item); break;
                        }
                    });
            }
            if (selected.contains(item->data(Qt::UserRole).toMap()["key"].toString())) {
                m_results->setCurrentItem(item);
                item->setSelected(true);
                m_apply->setEnabled(true);
            }
        }
    }
    persistHistory();
}
void BaronPanel::updateJobs() {
    const auto selected = m_jobList->currentItem()
        ? m_jobList->currentItem()->data(Qt::UserRole).toString()
        : QString();
    m_jobList->clear();
    int total = 0, document = 0;
    const QStringList labels { tr("Preparing workflow…"), tr("Submitting…"), tr("Queued"),
        tr("Running"), tr("Downloading result…"), tr("Failed"), tr("Cancelled"), tr("Finished") };
    for (const auto& job : m_jobs->jobs()) {
        const auto title = job->context["prompt"].toString().simplified().left(48);
        auto item = new QListWidgetItem(
            labels.at(job->state) + (title.isEmpty() ? QString() : " · " + title), m_jobList);
        item->setData(Qt::UserRole, job->id);
        item->setToolTip(job->issue);
        if (selected == job->id)
            m_jobList->setCurrentItem(item);
        if (JobQueue::active(job->state)) {
            ++total;
            document += m_host && job->context["document"].toString() == m_host->documentId();
        }
    }
    m_jobCount->setText(tr("Document: %1 · Total: %2").arg(document).arg(total));
    PluginUi::setIcon(m_queueButton, total ? "queue-active" : "queue-inactive");
    m_queueButton->setText(QString::number(total));
    if (!m_jobList->currentItem() && m_jobList->count())
        m_jobList->setCurrentRow(0);
    if (!m_busy) {
        bool upscaleRunning = false;
        for (const auto& job : m_jobs->jobs())
            upscaleRunning |= JobQueue::active(job->state) && job->context["mode"] == "upscale"
                && m_host && job->context["document"].toString() == m_host->documentId();
        m_run->setEnabled(m_mode->currentData() != "upscale" || !upscaleRunning);
        m_progress->setVisible(total > 0);
        double progress = -1;
        bool downloading = false;
        for (const auto& job : m_jobs->jobs()) {
            if (job->state == JobQueue::downloading)
                downloading = true;
            if (job->state == JobQueue::running)
                progress = qMax(progress, job->progress);
        }
        if (progress >= 0 && !downloading) {
            m_progress->setRange(0, 100);
            m_progress->setValue(qMin(99, qRound(progress * 100)));
        } else
            m_progress->setRange(0, 0);
    }
}
void BaronPanel::flushDocumentState() {
    if (!m_host || m_restoring || m_host->documentId().isEmpty())
        return;
    if (m_saveTimer)
        m_saveTimer->stop();
    QJsonArray loras;
    for (int i = 0; i < m_loras->count(); ++i)
        loras.append(m_loras->item(i)->data(Qt::UserRole).toJsonObject());
    QJsonObject data { { "schema", 1 }, { "guidance", m_guidance->state() }, { "loras", loras },
        { "style_id", m_styleId }, { "prompt", m_prompt->toPlainText() },
        { "negative", m_negative->toPlainText() },
        { "prompt_disabled", m_prompt->disabledFragments() },
        { "negative_disabled", m_negative->disabledFragments() },
        { "model",
            m_model->currentData().toString().isEmpty() ? m_documentModel
                                                        : m_model->currentData().toString() },
        { "mode", m_mode->currentData().toString() }, { "steps", m_steps->value() },
        { "cfg", m_cfg->value() }, { "strength", m_strength->value() },
        { "batch", m_batch->value() }, { "seed", m_seed->text() },
        { "fixed_seed", m_fixedSeed->isChecked() },
        { "sampler", m_sampler->currentData().toString() },
        { "scheduler", m_scheduler->currentData().toString() }, { "resolution", m_scale->value() },
        { "upscale", m_upscaleFactor->value() },
        { "upscale_options", m_upscale->state() },
        { "inpaint_options", m_inpaint->state() }, {"inpaint_mode",m_inpaintMode},
        { "reference_purpose", m_referencePurpose->currentData().toString() },
        { "reference_fidelity", m_fidelity->value() },
        { "reference_detail", m_referenceDetail->currentData().toInt() } };
    capturePromptBank();
    data["prompt_banks"] = m_promptBanks;
    captureStyleBank();
    data["style_banks"] = m_styleBanks;
    data["edit_source_style"] = m_editSourceStyle;
    data["prompt_bank_mode"] = m_promptBankMode;
    data["prompt_mode"] = m_promptSyntax->currentData().toString();
    data["a1111_gpu_noise"] = m_gpuNoise->isChecked();
    data["a1111_ensd"] = m_ensd->text();
    if (QJsonDocument(data).toJson(QJsonDocument::Compact).size() > 16 * 1024 * 1024) {
        m_status->setText(
            tr("Reference images are too large to save with this document. Use linked layers."));
        return;
    }
    BaronDiagnostics::record("document-save.begin");
    m_host->saveDocumentState(data);
    BaronDiagnostics::record("document-save.end");
    BaronDiagnostics::record("history-save.begin");
    persistHistory();
    BaronDiagnostics::record("history-save.end");
}
void BaronPanel::restoreDocumentState() {
    if (!m_guidance)
        return;
    m_restoring = true;
    const auto data = m_host ? m_host->documentState() : QJsonObject();
    m_promptBanks = data["prompt_banks"].toObject();
    m_styleBanks = data["style_banks"].toObject();
    m_editSourceStyle = data["edit_source_style"].toString();
    const auto presetCount = m_stylePresets.size();
    auto importBank = [this](const QJsonObject& bank) {
        const auto preset = bank["preset"].toObject();
        if (preset["id"].toString().isEmpty() || preset["checkpoints"].toArray().isEmpty()) return;
        for (const auto& existing : m_stylePresets)
            if (existing["id"] == preset["id"]) return;
        m_stylePresets.append(preset);
    };
    importBank(m_styleBanks["generate"].toObject());
    importBank(m_styleBanks["edit"].toObject());
    const auto edits = m_styleBanks["edits"].toObject();
    for (auto it = edits.begin(); it != edits.end(); ++it) importBank(it.value().toObject());
    if (presetCount != m_stylePresets.size()) {
        QJsonArray saved;
        for (const auto& preset : m_stylePresets) saved.append(preset);
        QSettings("BaronEdition", "Orchestrion").setValue("stylePresets", QJsonDocument(saved).toJson());
    }
    rebuildStyles();
    m_promptBankMode = data["prompt_bank_mode"].toString(
        data["mode"] == "edit" ? "edit" : "generate");
    m_guidance->restoreState(data["guidance"].toObject());
    m_prompt->restoreDisabledFragments(data["prompt_disabled"].toArray());
    m_negative->restoreDisabledFragments(data["negative_disabled"].toArray());
    m_loras->clear();
    m_documentModel = data["model"].toString();
    if (data["schema"].toInt() == 1) {
        m_styleId = data["style_id"].toString();
        const auto styleIndex = m_styles->findData(m_styleId);
        if (styleIndex >= 0)
            m_styles->setCurrentIndex(styleIndex);
        m_prompt->replacePromptText(data["prompt"].toString().left(65536));
        m_negative->replacePromptText(data["negative"].toString().left(65536));
        if (m_promptBanks.isEmpty()) capturePromptBank();
        restorePromptBank();
        auto select = [](QComboBox* combo, const QJsonValue& value) {
            const auto index = combo->findData(value.toVariant());
            if (index >= 0)
                combo->setCurrentIndex(index);
        };
        select(m_mode, data["mode"]);
        select(m_model, data["model"]);
        select(m_promptSyntax, data["prompt_mode"].toString("comfy"));
        m_gpuNoise->setChecked(data["a1111_gpu_noise"].toBool());
        m_ensd->setText(data["a1111_ensd"].toString("0"));
        select(m_sampler, data["sampler"]);
        select(m_scheduler, data["scheduler"]);
        select(m_referencePurpose, data["reference_purpose"]);
        select(m_referenceDetail, data["reference_detail"]);
        m_steps->setValue(data["steps"].toInt(20));
        m_cfg->setValue(data["cfg"].toDouble(4));
        m_strength->setValue(data["strength"].toDouble(1));
    m_inpaint->restore(data["inpaint_options"].toObject());m_inpaintMode=data["inpaint_mode"].toString("automatic");
        auto slider = findChild<QSlider*>("denoiseSlider");
        if (slider)
            slider->setValue(qRound(m_strength->value() * 100));
        m_batch->setValue(data["batch"].toInt(1));
        m_seed->setText(data["seed"].toString("-1"));
        m_fixedSeed->setChecked(data["fixed_seed"].toBool());
        m_scale->setValue(data["resolution"].toDouble(1));
        m_upscaleFactor->setValue(data["upscale"].toDouble(2));
        if (data["upscale_options"].isObject()) m_upscale->restore(data["upscale_options"].toObject());
        m_fidelity->setValue(data["reference_fidelity"].toDouble(4));
        for (auto value : data["loras"].toArray()) {
            if (m_loras->count() >= 5)
                break;
            const auto lora = value.toObject();
            if (lora["name"].toString().isEmpty())
                continue;
            auto item = new QListWidgetItem(lora["title"].toString(), m_loras);
            item->setData(Qt::UserRole, lora);
        }
    } else {
        m_promptBankMode = m_mode->currentData() == "edit" ? "edit" : "generate";
        restorePromptBank();
    }
    m_restoring = false;
    updateMode();
}
void BaronPanel::prepareControl(const QJsonObject& input) {
    if (m_busy)
        return;
    if (!m_client->signedIn()) {
        m_status->setText(tr("Not signed in. Click Configure to connect."));
        return;
    }
    m_jobs->start(input,
        QJsonObject { { "target", input["target"] }, { "mode", "control" },
            { "document", m_host->documentId() }, { "control_id", input["control_id"] } },
        {}, m_queuePosition->currentData().toBool());
}
void BaronPanel::showModels(const QJsonObject& data) {
    QScopedValueRollback<bool> loading(m_styleLoading, true);
    const auto resources = data["resources"].toObject();
    {
        QScopedValueRollback<bool> loading(m_styleLoading, true);
        auto vae = m_settingsDialog->findChild<QComboBox*>("styleVae");
        const auto current = vae->currentText();
        vae->clear();
        vae->addItem("Checkpoint Default");
        const auto values = resources["vae"];
        if (values.isArray()) for (auto value : values.toArray()) vae->addItem(value.toString());
        if (values.isObject()) for (const auto& name : values.toObject().keys()) vae->addItem(name);
        vae->setCurrentText(current);
    }
    if (resources.contains("a1111_prompt"))
        m_promptSyntax->setProperty("available", resources["a1111_prompt"].toBool());
    if (resources.contains("a1111_gpu_noise"))
        m_gpuNoise->setProperty("available", resources["a1111_gpu_noise"].toBool());
    m_upscale->setResources(resources);
    ++m_catalogGeneration;
    m_catalog = {};
    for(const auto& value : data["items"].toArray())m_catalog.append(ModelCatalog::normalize(value.toObject()));
    m_gallery->clear();
    m_model->clear();
    m_loadedThumbnails.clear();
    for (const auto& value : m_catalog) {
        const auto model = value.toObject();
        if (model["kind"] != "lora")
            m_model->addItem(
                PluginUi::icon(PluginUi::architectureIcon(model["family"].toString()), this),
                model["title"].toString(), model["name"].toString());
        if(model["kind"] != "lora")m_model->setItemData(m_model->count()-1,model,Qt::UserRole+1);
    }
    m_catalogBrowser->setModels(m_catalog);
    m_status->setText(tr("Connected. Models are ready."));
    QSettings settings("BaronEdition", "Orchestrion");
    const auto index = m_model->findData(
        m_documentModel.isEmpty() ? settings.value("model") : QVariant(m_documentModel));
    if (index >= 0)
        m_model->setCurrentIndex(index);
    const auto document = m_host ? m_host->documentState() : QJsonObject();
    m_steps->setValue(document["schema"].toInt() == 1
            ? document["steps"].toInt(20)
            : settings.value("steps", m_steps->value()).toInt());
    m_cfg->setValue(document["schema"].toInt() == 1
            ? document["cfg"].toDouble(4)
            : settings.value("cfg", m_cfg->value()).toDouble());
    for (auto editor : findChildren<PromptEditor*>())
        editor->setLoraCatalog(m_catalog);
    rebuildStyles();
    if (!m_styleId.startsWith("model:")) {
        selectStyle();
        if (document["schema"].toInt() == 1)
            restoreDocumentState();
    }
    m_status->clear();
    filterCatalog();
}
void BaronPanel::filterCatalog() {
    m_catalogBrowser->applyFilters();
}
void BaronPanel::openCatalog(const QString& kind) {
    m_loadedThumbnails.clear();
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
        m_filter->clearFocus();
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
        const auto kind = model["kind"].toString();
        const QString key = kind + "|" + name;
        const QUrl preview(model["preview"].toString());
        if (m_loadedThumbnails.contains(key) || !item->icon().isNull()
            || ModelThumbnails::previewUrl(preview).isEmpty())
            continue;
        m_loadedThumbnails.insert(key);
        ++m_thumbnailRequests;
        const auto generation = m_catalogGeneration;
        m_thumbnails->load(preview, [this, name, kind, preview, generation](const QImage& image) {
            --m_thumbnailRequests;
            for (int i = 0;
                generation == m_catalogGeneration && !image.isNull() && i < m_gallery->count();
                ++i) {
                const auto model = m_gallery->item(i)->data(Qt::UserRole).toJsonObject();
                if (model["name"].toString() == name
                    && model["kind"].toString() == kind
                    && QUrl(model["preview"].toString()) == preview)
                    m_gallery->item(i)->setIcon(QIcon(QPixmap::fromImage(image)));
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
void BaronPanel::receiveResult(const QJsonObject& context, const HistoryStore::Prepared& result) {
    m_targetId = context["target"].toString();
    m_jobMode = context["mode"].toString();
    m_resultControlId = context["control_id"].toString();
    m_resultMetadata = context["settings"].toObject();
    m_resultDocument = context["document"].toString();
    const auto area = context["bounds"].toArray();
    m_resultBounds = area.size() == 4
        ? QRect(area[0].toInt(), area[1].toInt(), area[2].toInt(), area[3].toInt()) : QRect();
    m_resultSelection = result.selection;
    finishImages(result);
}
void BaronPanel::showImage(const QByteArray& bytes, int index) {
    const auto document = m_resultDocument;
    const auto target = m_targetId;
    BackgroundWork::run(this, [bytes] { return QImage::fromData(bytes); },
        [this, index, document, target](const QImage& image) {
            if (m_resultDocument != document || m_targetId != target) return;
            if (image.isNull()) { m_status->setText(tr("Could not decode the result image")); return; }
            m_jobImages[index] = image;
            if (m_jobImages.size() != m_expectedImages) return;
            QJsonObject context { {"target", target}, {"document", document}, {"mode", m_jobMode},
                {"settings", m_resultMetadata}, {"bounds", QJsonArray{m_resultBounds.x(), m_resultBounds.y(), m_resultBounds.width(), m_resultBounds.height()}},
                {"control_id", m_resultControlId} };
            const auto images = m_jobImages.values();
            const auto source = m_sourceImage;
            const auto selection = m_resultSelection;
            BackgroundWork::run(this, [images, source, selection, context] {
                return HistoryStore::prepare(images, png(selection), context["mode"].toString(), source);
            }, [this, context](const HistoryStore::Prepared& result) { receiveResult(context, result); });
        });
}
void BaronPanel::finishImages(const HistoryStore::Prepared& prepared) {
    if (!prepared.error.isEmpty() || prepared.images.isEmpty()) {
        m_status->setText(tr("Could not decode the result image")); return;
    }
    auto image = prepared.images.first();
    m_jobImages.clear();
    for (int i = 0; i < prepared.images.size(); ++i) m_jobImages[i] = prepared.images[i];
    m_resultMask = prepared.mask;
    if (m_jobMode == "control") {
        QString issue;
        if (m_host
            && m_host->apply(m_targetId, m_jobImages.first(), {}, tr("Control map"), &issue)) {
            m_previewResult.clear();
            m_guidance->setControlLayer(m_host->appliedLayerId(), m_resultControlId);
            m_status->setText(tr("Control map added as a linked Krita layer."));
        } else
            m_status->setText(issue);
        m_pendingJob.clear();
        m_resume->hide();
        m_busy = false;
        m_run->setEnabled(true);
        m_quote->setEnabled(true);
        m_progress->hide();
        return;
    }
    if (m_jobMode == "background") m_foreground = image;
    QListWidgetItem* first = nullptr;
    const QString document = m_resultDocument.isEmpty()
        ? (m_host ? m_host->documentId() : QString()) : m_resultDocument;
    const auto store = history(document);
    if (store) {
        const auto area = m_resultBounds.isEmpty() ? m_captureBounds : m_resultBounds;
        const auto id = store->appendPrepared(prepared, area, m_resultMetadata);
        if (id.isEmpty())
            m_status->setText(tr("Could not save generation history: %1").arg(store->error()));
        if (document == m_historyDocument) {
            restoreHistory(true);
            for (int i = 0; i < m_results->count(); ++i)
                if (m_results->item(i)->data(Qt::UserRole).toMap()["history_id"] == id) {
                    first = m_results->item(i);
                    break;
                }
        } else store->saveAsync(this, [this, document](const QString& key, const QByteArray& bytes) {
            if (m_host) m_host->setDocumentAnnotation(document, key, bytes);
        }, [this](const QString& issue) {
            if (!issue.isEmpty()) m_status->setText(tr("Could not save generation history: %1").arg(issue));
        });
    }
    if (m_jobMode != "background")
        image = m_jobImages.first();
    m_result = image;
    m_preview->setPixmap(QPixmap::fromImage(prepared.preview));
    m_pendingJob.clear();
    m_resume->hide();
    m_apply->setEnabled(true);
    m_busy = false;
    m_run->setEnabled(true);
    m_quote->setEnabled(true);
    m_progress->hide();
    const auto finishedAction = m_jobMode == "upscale" ? QString("apply") : QSettings("BaronEdition", "Orchestrion")
        .value("generation_finished_action", "preview").toString();
    if (first && (finishedAction == "apply" || (finishedAction == "preview" && m_previewResult.isEmpty()))) {
        m_results->setCurrentItem(first);
        if (finishedAction == "apply") m_previewResult = first->data(Qt::UserRole).toMap()["key"].toString();
        selectResult(first, finishedAction == "apply");
    }
}
void BaronPanel::selectResult(QListWidgetItem* item, bool apply) {
    if (!item || item->data(Qt::UserRole).toMap()["header"].toBool())
        return;
    const auto data = item->data(Qt::UserRole).toMap();
    const auto store = m_histories.value(m_historyDocument);
    const auto image = data.contains("history_id") && store
        ? store->image(data["history_id"].toString(), data["history_index"].toInt())
        : data["image"].value<QImage>();
    const auto mask = data["mask"].value<QImage>();
    QString target = data["target"].toString();
    if (target.isEmpty() && m_host) {
        target = m_host->restoreTarget(data["bounds"].toRect(), QImage::fromData(
            QByteArray::fromBase64(data["selection_mask"].toString().toLatin1())));
        auto restored = data;
        restored["target"] = target;
        item->setData(Qt::UserRole, restored);
    }
    const auto key = data["key"].toString();
    QString issue;
    if (!m_host || image.isNull())
        return;
    if (m_previewResult == key && !apply && QSettings("BaronEdition", "Orchestrion")
            .value("history_click_behavior", "toggle_preview").toString() == "toggle_preview") {
        m_host->hidePreview();
        m_previewResult.clear();
        m_results->clearSelection();
        m_results->setCurrentItem(nullptr);
        m_apply->setEnabled(false);
    } else if (m_previewResult == key || apply) {
        QSettings preferences("BaronEdition", "Orchestrion");
        const QJsonObject behavior {
            { "apply", preferences.value("apply_behavior", "layer").toString() },
            { "region_apply", preferences.value("apply_region_behavior", "layer_group").toString() },
            { "regions", data["settings"].toJsonObject()["regions"] } };
        if (m_host->applyConfigured(
                target, image, mask, tr("Orchestrion result"), behavior, &issue)) {
            m_previewResult.clear();
            auto applied = item->data(Qt::UserRole).toMap();
            applied["applied"] = true;
            item->setData(Qt::UserRole, applied);
            updateHistoryItem(item);
            if (store && data.contains("history_id")) {
                store->mark(data["history_id"].toString(), data["history_index"].toInt(),
                    data["favorite"].toBool(), true);
                persistHistory();
            }
            m_status->setText(tr("Result applied to the canvas. You can undo this in Krita."));
        } else
            m_status->setText(issue);
    } else if (m_host->preview(target, image, mask, &issue)) {
        m_previewResult = key;
        m_status->setText(tr("Click to toggle preview, double-click to apply."));
    } else
        m_status->setText(issue);
}
QJsonObject BaronPanel::generationSettings() const {
    QJsonArray loras;
    for (int i = 0; i < m_loras->count(); ++i)
        loras.append(m_loras->item(i)->data(Qt::UserRole).toJsonObject());
    return { { "style_id", m_styleId }, { "prompt", m_prompt->toPlainText() },
        { "prompt_mode", m_promptSyntax->currentData().toString() },
        { "a1111_gpu_noise", m_gpuNoise->isChecked() }, { "a1111_ensd", m_ensd->text() },
        { "negative", m_negative->toPlainText() }, { "model", m_model->currentData().toString() },
        { "loras", loras }, { "mode", m_mode->currentData().toString() },
        { "steps", m_steps->value() }, { "cfg", m_cfg->value() },
        { "strength", m_strength->value() }, { "batch", m_batch->value() },
        { "seed", m_seed->text() }, { "fixed_seed", m_fixedSeed->isChecked() },
        { "sampler", m_sampler->currentData().toString() },
        { "scheduler", m_scheduler->currentData().toString() }, { "resolution", m_scale->value() },
        { "upscale", m_upscaleFactor->value() },
        { "upscale_options", m_upscale->state() },
        { "inpaint_options", m_inpaint->state() }, {"inpaint_mode",m_inpaintMode},
        { "reference_purpose", m_referencePurpose->currentData().toString() },
        { "reference_fidelity", m_fidelity->value() },
        { "reference_detail", m_referenceDetail->currentData().toInt() } };
}
void BaronPanel::restoreGenerationSettings(const QJsonObject& data) {
    auto select = [](QComboBox* combo, const QJsonValue& value) {
        const auto index = combo->findData(value.toVariant());
        if (index >= 0)
            combo->setCurrentIndex(index);
    };
    select(m_mode, data["mode"]);
    select(m_styles, data["style_id"]);
    m_documentModel = data["model"].toString();
    m_model->setCurrentIndex(m_model->findData(m_documentModel));
    select(m_promptSyntax, data["prompt_mode"].toString("comfy"));
    m_gpuNoise->setChecked(data["a1111_gpu_noise"].toBool());
    m_ensd->setText(data["a1111_ensd"].toString("0"));
    m_prompt->replacePromptText(data["prompt"].toString().left(65536));
    m_negative->replacePromptText(data["negative"].toString().left(65536));
    m_steps->setValue(data["steps"].toInt(20));
    m_cfg->setValue(data["cfg"].toDouble(4));
    m_strength->setValue(data["strength"].toDouble(1));
    m_inpaint->restore(data["inpaint_options"].toObject());m_inpaintMode=data["inpaint_mode"].toString("automatic");
    m_batch->setValue(data["batch"].toInt(1));
    m_seed->setText(data["seed"].toString("-1"));
    m_fixedSeed->setChecked(data["fixed_seed"].toBool());
    select(m_sampler, data["sampler"]);
    select(m_scheduler, data["scheduler"]);
    m_scale->setValue(data["resolution"].toDouble(1));
    m_upscaleFactor->setValue(data["upscale"].toDouble(2));
    if (data["upscale_options"].isObject()) m_upscale->restore(data["upscale_options"].toObject());
    select(m_referencePurpose, data["reference_purpose"]);
    select(m_referenceDetail, data["reference_detail"]);
    m_fidelity->setValue(data["reference_fidelity"].toDouble(4));
    m_loras->clear();
    for (auto value : data["loras"].toArray()) {
        if (m_loras->count() >= 5)
            break;
        const auto data = value.toObject();
        if (data["name"].toString().isEmpty())
            continue;
        auto item = new QListWidgetItem(data["title"].toString(), m_loras);
        item->setData(Qt::UserRole, data);
    }
    updateMode();
}
void BaronPanel::updateHistoryItem(QListWidgetItem* item) {
    auto data = item->data(Qt::UserRole).toMap();
    if (!data.contains("caption"))
        data["caption"] = item->text();
    item->setData(Qt::UserRole, data);
    item->setText({});
    auto image = data["image"].value<QImage>();
    auto mask = data["mask"].value<QImage>();
    if (!mask.isNull() && mask.size() == image.size()) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto color = image.pixelColor(x, y);
                image.setPixel(
                    x, y, qRgba(color.red(), color.green(), color.blue(), qGray(mask.pixel(x, y))));
            }
    }
    const int size = m_results->iconSize().width();
    QPixmap thumbnail;
    if (image.isNull()) {
        thumbnail = QPixmap(size, size);
        thumbnail.fill(palette().color(QPalette::AlternateBase));
    } else thumbnail = QPixmap::fromImage(
        image.scaled(size * 2, size * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const int minimumHeight = qMin(4 * (m_results->fontMetrics().height() + 8), size * 2);
    if (!thumbnail.isNull() && thumbnail.height() < minimumHeight) {
        QPixmap padded(thumbnail.width(), minimumHeight);
        padded.fill(Qt::transparent);
        QPainter painter(&padded);
        painter.drawPixmap(0, 0, thumbnail);
        painter.end();
        thumbnail = padded;
    }
    if (data["applied"].toBool() || data["favorite"].toBool()) {
        QPainter painter(&thumbnail);
        QPixmap star(":/baron/icons/star.png");
        painter.drawPixmap(thumbnail.width() - 28, 4, star.scaled(24, 24));
    }
    item->setIcon(QIcon(thumbnail));
    item->setSizeHint(QSize(size + 4, size + 4));
    const auto settings = data["settings"].toJsonObject();
    item->setToolTip(
        settings["prompt"].toString() + "\n" + tr("Seed") + ": " + settings["seed"].toString());
}
void BaronPanel::historyAction(const QString& action) {
    if (action == "clear") {
        m_historyKey.clear();
        for (int i = m_results->count() - 1; i >= 0; --i) {
            const auto item = m_results->item(i);
            const auto data = item->data(Qt::UserRole).toMap();
            if (data["key"].toString() == m_previewResult) {
                if (m_host)
                    m_host->clearPreview();
                m_previewResult.clear();
            }
            delete m_results->takeItem(i);
        }
        if (auto store = m_histories.value(m_historyDocument)) {
            store->clear();
            persistHistory();
        }
        return;
    }
    auto item = m_results->currentItem();
    if (!item)
        return;
    auto data = item->data(Qt::UserRole).toMap();
    if (action == "favorite") {
        data["favorite"] = !data["favorite"].toBool();
        item->setData(Qt::UserRole, data);
        updateHistoryItem(item);
        if (auto store = m_histories.value(m_historyDocument)) {
            store->mark(data["history_id"].toString(), data["history_index"].toInt(),
                data["favorite"].toBool(), data["applied"].toBool());
            persistHistory();
        }
    } else if (action == "delete") {
        if (m_previewResult == data["key"].toString()) {
            if (m_host)
                m_host->clearPreview();
            m_previewResult.clear();
        }
        if (auto store = m_histories.value(m_historyDocument)) {
            if (store->remove(data["history_id"].toString(), data["history_index"].toInt())) {
                persistHistory();
                restoreHistory();
            }
        } else
            delete m_results->takeItem(m_results->row(item));
    } else if (action == "preview") {
        m_previewResult.clear();
        selectResult(item);
    } else if (action == "apply") {
        selectResult(item, true);
    } else if (action == "copy" || action == "copy_evaluated") {
        const auto settings = data["settings"].toJsonObject();
        const auto evaluated = action == "copy_evaluated";
        const auto positive = evaluated
            ? settings["prompt_final"].toString(settings["prompt"].toString())
            : settings["prompt"].toString();
        const auto negative = evaluated
            ? settings["negative_prompt_final"].toString(settings["negative"].toString())
            : settings["negative"].toString();
        m_prompt->replacePromptText(positive);
        m_negative->replacePromptText(negative);
        PluginUi::copyText(positive);
    } else if (action == "strength") {
        m_strength->setValue(data["settings"].toJsonObject()["strength"].toDouble(1));
    } else if (action == "seed") {
        m_seed->setText(data["settings"].toJsonObject()["seed"].toString());
        m_fixedSeed->setChecked(true);
    } else if (action == "style") {
        const auto saved = data["settings"].toJsonObject();
        const auto index = m_styles->findData(saved["style_id"].toString());
        if (index >= 0)
            m_styles->setCurrentIndex(index);
    } else if (action == "info") {
        PluginUi::copyText(
            QString::fromUtf8(QJsonDocument(data["settings"].toJsonObject()).toJson()));
    } else if (action == "reuse") {
        restoreGenerationSettings(data["settings"].toJsonObject());
        m_status->setText(
            tr("Generation settings restored. Controls stay linked to the current document."));
    } else if (action == "export") {
        const auto format = QSettings("BaronEdition", "Orchestrion").value("save_image_format", "png_small").toString();
        const auto extension = format.startsWith("png") ? "png" : format.startsWith("webp") ? "webp" : "jpg";
        const auto filter = format.startsWith("png") ? tr("PNG image (*.png)")
            : format.startsWith("webp") ? "WebP (*.webp)" : "JPEG (*.jpg *.jpeg)";
        auto path = QFileDialog::getSaveFileName(this, tr("Save image…"),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/baron-"
                + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "." + extension, filter);
        if (!path.isEmpty() && QFileInfo(path).suffix().isEmpty()) path += QString("." + QString(extension));
        if (path.isEmpty())
            return;
        auto image = data["image"].value<QImage>();
        if (data.contains("history_id"))
            if (auto store = m_histories.value(m_historyDocument))
                image = store->image(data["history_id"].toString(), data["history_index"].toInt());
        const auto mask = data["mask"].value<QImage>();
        if (!mask.isNull() && mask.size() == image.size()) {
            image = image.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto color = image.pixelColor(x, y);
                    image.setPixel(x, y,
                        qRgba(color.red(), color.green(), color.blue(), qGray(mask.pixel(x, y))));
                }
        }
        QSaveFile file(path);
        file.setDirectWriteFallback(true);
        const bool opened = file.open(QIODevice::WriteOnly);
        QImageWriter writer(&file, format.startsWith("png") ? "PNG" : format.startsWith("webp") ? "WEBP" : "JPEG");
        if (format.startsWith("png")) writer.setCompression(format == "png" ? 0 : 9);
        else writer.setQuality(format == "webp_lossless" ? 100 : format == "jpeg" ? 85 : 80);
        if (format.startsWith("png") && QSettings("BaronEdition", "Orchestrion").value("save_image_metadata", false).toBool()) {
            auto metadata = data["settings"].toJsonObject();
            auto regions = metadata["regions"].toArray();
            for (int i = 0; i < regions.size(); ++i) {
                auto region = regions[i].toObject();
                region.remove("mask");
                regions[i] = region;
            }
            if (!regions.isEmpty()) metadata["regions"] = regions;
            writer.setText("parameters", QString::fromUtf8(QJsonDocument(metadata).toJson(QJsonDocument::Compact)));
        }
        const bool saved = opened && writer.write(image) && file.commit();
        m_status->setText(saved ? tr("Image saved.") : tr("Could not save the image."));
    }
}

void BaronPanel::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_uiReady)
        PluginUi::refresh(this);
}
void BaronPanel::openSettings(int page) {
    m_settingsDialog->findChild<QListWidget*>("settingsCategories")->setCurrentRow(page);
    const auto screen = this->screen();
    const auto available = screen ? screen->availableGeometry().size() : QSize(720, 700);
    m_settingsDialog->resize(qMin(1280, available.width()), qMin(900, available.height()));
    updateStyleFields();
    if (available.width() < 1000)
        m_settingsDialog->showMaximized();
    m_settingsDialog->exec();
}
void BaronPanel::updateStyleFields() {
    QScopedValueRollback<bool> loading(m_styleLoading, true);
    if (auto picker = m_settingsDialog->findChild<QComboBox*>("settingsStyleSelect")) {
        QSignalBlocker blocker(picker);
        picker->clear();
        for (int i = 0; i < m_styles->count(); ++i)
            picker->addItem(m_styles->itemIcon(i), m_styles->itemText(i), m_styles->itemData(i));
        picker->setCurrentIndex(picker->findData(m_styleId));
    }
    m_settingsDialog->findChild<QLineEdit*>("styleName")
        ->setText(m_styles->currentText().remove(" ★"));
    auto current = QJsonObject();
    for (const auto& style : m_stylePresets)
        if (style["id"] == m_styleId)
            current = style;
    m_settingsDialog->findChild<PromptEditor*>("stylePrompt")
        ->replacePromptText(current["style_prompt"].toString());
    m_settingsDialog->findChild<PromptEditor*>("styleNegative")
        ->replacePromptText(current["negative_prompt"].toString());
    m_settingsDialog->findChild<QComboBox*>("styleVae")
        ->setCurrentText(current["vae"].toString("Checkpoint Default"));
    auto styleMode = m_settingsDialog->findChild<QComboBox*>("styleModeLabel");
    styleMode->setCurrentIndex(styleMode->findData(m_promptBankMode));
    QString architecture = current["architecture"].toString();
    if (architecture.isEmpty() || architecture == "auto")
        for (auto value : m_catalog) {
            const auto item = value.toObject();
            if (item["name"] == m_model->currentData())
                architecture = item["architecture"].toString(item["family"].toString());
        }
    m_settingsDialog->findChild<QLabel*>("styleArchitecture")->setText(architecture);
    const bool krea = architecture.contains("krea", Qt::CaseInsensitive);
    auto kreaOptions = m_settingsDialog->findChild<QWidget*>("kreaStyleOptions");
    auto kreaHeader = qobject_cast<QToolButton*>(kreaOptions->property("sectionHeader").value<QObject*>());
    kreaHeader->setVisible(krea);
    kreaOptions->setVisible(krea && kreaHeader->isChecked());
    auto warning = m_settingsDialog->findChild<QLabel*>("styleWarning");
    warning->setText(m_model->currentIndex() < 0 ? tr("The style's model is not installed on this server. Choose an available model.") : QString());
    warning->setVisible(!warning->text().isEmpty());
    auto linked = m_settingsDialog->findChild<QComboBox*>("linkedEditStyle");
    linked->clear();
    linked->addItem(tr("None"), "");
    for (const auto& style : m_stylePresets)
        if (style["id"] != m_styleId) linked->addItem(style["name"].toString(), style["id"].toString());
    linked->setCurrentIndex(qMax(0, linked->findData(current["linked_edit_style"].toString())));
    auto samplerPreset = m_settingsDialog->findChild<QComboBox*>("styleSamplerPreset");
    samplerPreset->setCurrentIndex(qMax(0, samplerPreset->findData(current["sampler"].toString())));
    m_settingsDialog->findChild<QSpinBox*>("styleClipSkip")->setValue(current["clip_skip"].toInt());
    m_settingsDialog->findChild<QSpinBox*>("styleResolution")
        ->setValue(current["preferred_resolution"].toInt());
    for (const auto& name : { "v_prediction_zsnr", "self_attention_guidance" })
        m_settingsDialog->findChild<QCheckBox*>(name)->setChecked(current[name].toBool());
}
void BaronPanel::rebuildStyles() {
    const QSignalBlocker blocker(m_styles);
    const QSignalBlocker upscaleBlocker(m_upscale->style);
    m_styles->clear();
    for (int i = 0; i < m_model->count(); ++i)
        m_styles->addItem(m_model->itemIcon(i), m_model->itemText(i),
            QString("model:" + m_model->itemData(i).toString()));
    for (const auto& style : m_stylePresets) {
        m_styles->insertItem(0,
                PluginUi::icon(PluginUi::architectureIcon(style["architecture"].toString()), this),
                style["name"].toString(), style["id"].toString());
    }
    const auto settings = QSettings("BaronEdition", "Orchestrion");
    const auto recent = settings.value("recentStyles").toStringList().mid(0, settings.value("recent_styles_count", 4).toInt());
    int position = 0;
    for (const auto& id : recent) {
        const auto index = m_styles->findData(id);
        if (index < 0) continue;
        const auto text = m_styles->itemText(index);
        const auto icon = m_styles->itemIcon(index);
        m_styles->removeItem(index);
        m_styles->insertItem(position++, icon, text, id);
    }
    if (position > 0 && position < m_styles->count()) m_styles->insertSeparator(position);
    auto index = m_styles->findData(m_styleId);
    if (index < 0)
        index = m_styles->findData(QString("model:" + m_model->currentData().toString()));
    m_styles->setCurrentIndex(index);
    m_upscale->style->setCurrentIndex(index);
    if (index >= 0) {
        m_styleId = m_styles->currentData().toString();
        m_styles->setItemText(index, m_styles->itemText(index) + " ★");
    }
    if (m_saveTimer && !m_restoring)
        m_saveTimer->start();
}
void BaronPanel::selectStyle() {
    QScopedValueRollback<bool> loading(m_styleLoading, true);
    m_styleId = m_styles->currentData().toString();
    for (int i = 0; i < m_styles->count(); ++i)
        m_styles->setItemText(
            i, m_styles->itemText(i).remove(" ★") + (i == m_styles->currentIndex() ? " ★" : ""));
    if (m_styleId.startsWith("model:")) {
        m_promptSyntax->setCurrentIndex(0);
        m_gpuNoise->setChecked(false);
        m_ensd->setText("0");
        m_model->setCurrentIndex(m_model->findData(m_styleId.mid(6)));
        m_loras->clear();
        updateStyleFields();
        return;
    }
    for (const auto& style : m_stylePresets) {
        if (style["id"] != m_styleId)
            continue;
        m_model->setCurrentIndex(-1);
        for (auto checkpoint : style["checkpoints"].toArray()) {
            const auto index = m_model->findData(checkpoint.toString());
            if (index >= 0) {
                m_model->setCurrentIndex(index);
                break;
            }
        }
        m_steps->setValue(style["sampler_steps"].toInt(20));
        m_cfg->setValue(style["cfg_scale"].toDouble(4));
        m_promptSyntax->setCurrentIndex(qMax(0, m_promptSyntax->findData(style["prompt_mode"].toString("comfy"))));
        m_gpuNoise->setChecked(style["a1111_gpu_noise"].toBool());
        m_ensd->setText(QString::number(quint32(style["a1111_ensd"].toDouble())));
        m_referencePurpose->setCurrentIndex(qMax(0, m_referencePurpose->findData(style["krea2_reference_mode"].toString("identity"))));
        m_fidelity->setValue(style["krea2_ref_boost"].toDouble(4));
        m_referenceDetail->setCurrentIndex(qMax(0, m_referenceDetail->findData(style["krea2_grounding_px"].toInt(768))));
        auto sampler = style["sampler_settings"].toObject();
        if (sampler.isEmpty()) {
            QFile presets(":/baron/presets/samplers.json");
            if (presets.open(QIODevice::ReadOnly))
                sampler = QJsonDocument::fromJson(presets.readAll())
                              .object()[style["sampler"].toString()]
                              .toObject();
        }
        for (auto pair : QList<QPair<QComboBox*, QString>> {
                 { m_sampler, "sampler" }, { m_scheduler, "scheduler" } }) {
            const auto value = sampler[pair.second].toString();
            if (pair.first->findData(value) < 0)
                pair.first->addItem(value, value);
            pair.first->setCurrentIndex(pair.first->findData(value));
        }
        m_loras->clear();
        for (auto lora : style["loras"].toArray()) {
            if (m_loras->count() >= 5)
                break;
            auto data = lora.toObject();
            data["title"] = data["name"].toString();
            auto item = new QListWidgetItem(data["title"].toString(), m_loras);
            item->setData(Qt::UserRole, data);
        }
        break;
    }
    updateStyleFields();
}
void BaronPanel::saveStyle() {
    QScopedValueRollback<bool> loading(m_styleLoading, true);
    const auto name = m_settingsDialog->findChild<QLineEdit*>("styleName")->text().trimmed();
    if (name.isEmpty() || m_model->currentData().toString().isEmpty())
        return;
    QJsonArray loras;
    for (int i = 0; i < m_loras->count(); ++i)
        loras.append(m_loras->item(i)->data(Qt::UserRole).toJsonObject());
    QJsonObject preset;
    for (const auto& style : m_stylePresets)
        if (style["id"] == m_styleId) preset = style;
    const auto oldEditStyle = preset.value("linked_edit_style").toString();
    const auto oldCheckpoints = preset["checkpoints"].toArray();
    auto samplerSettings = preset["sampler_settings"].toObject();
    samplerSettings["sampler"] = m_sampler->currentData().toString();
    samplerSettings["scheduler"] = m_scheduler->currentData().toString();
    const QJsonObject fields { { "id",
                             m_styleId.isEmpty() || m_styleId.startsWith("model:")
                                 ? QUuid::createUuid().toString()
                                 : m_styleId },
        { "name", name }, { "version", preset["version"].toInt(2) },
        { "checkpoints", oldCheckpoints.contains(m_model->currentData().toString())
                ? oldCheckpoints : QJsonArray { m_model->currentData().toString() } },
        { "sampler_steps", m_steps->value() }, { "cfg_scale", m_cfg->value() }, { "loras", loras },
        { "sampler_settings", samplerSettings } };
    for (auto it = fields.begin(); it != fields.end(); ++it) preset[it.key()] = it.value();
    const auto samplerPreset = m_settingsDialog->findChild<QComboBox*>("styleSamplerPreset")->currentData().toString();
    if (!samplerPreset.isEmpty()) {
        QFile file(":/baron/presets/samplers.json");
        if (file.open(QIODevice::ReadOnly)) {
            const auto settings = QJsonDocument::fromJson(file.readAll()).object()[samplerPreset].toObject();
            if (settings["sampler"] == m_sampler->currentData().toString()
                && settings["scheduler"] == m_scheduler->currentData().toString()) preset["sampler"] = samplerPreset;
            else preset.remove("sampler");
        }
    } else preset.remove("sampler");
    preset["linked_edit_style"] = m_settingsDialog->findChild<QComboBox*>("linkedEditStyle")->currentData().toString();
    if (m_promptBankMode == "generate" && oldEditStyle != preset["linked_edit_style"].toString()) {
        auto edits = m_styleBanks["edits"].toObject();
        edits.remove(m_styleId);
        m_styleBanks["edits"] = edits;
    }
    preset["krea2_reference_mode"] = m_referencePurpose->currentData().toString();
    preset["krea2_ref_boost"] = m_fidelity->value();
    preset["krea2_grounding_px"] = m_referenceDetail->currentData().toInt();
    preset["style_prompt"]
        = m_settingsDialog->findChild<QPlainTextEdit*>("stylePrompt")->toPlainText();
    preset["prompt_mode"] = m_promptSyntax->currentData().toString();
    preset["a1111_gpu_noise"] = m_gpuNoise->isChecked();
    preset["a1111_ensd"] = double(m_ensd->text().toULongLong());
    preset["negative_prompt"]
        = m_settingsDialog->findChild<QPlainTextEdit*>("styleNegative")->toPlainText();
    preset["vae"] = m_settingsDialog->findChild<QComboBox*>("styleVae")->currentText();
    preset["clip_skip"] = m_settingsDialog->findChild<QSpinBox*>("styleClipSkip")->value();
    preset["preferred_resolution"]
        = m_settingsDialog->findChild<QSpinBox*>("styleResolution")->value();
    for (const auto& name : { "v_prediction_zsnr", "self_attention_guidance" })
        preset[name] = m_settingsDialog->findChild<QCheckBox*>(name)->isChecked();
    m_styleId = preset["id"].toString();
    bool replaced = false;
    for (auto& style : m_stylePresets)
        if (style["id"] == m_styleId) {
            style = preset;
            replaced = true;
            break;
        }
    if (!replaced)
        m_stylePresets.append(preset);
    QJsonArray saved;
    for (const auto& style : m_stylePresets)
        saved.append(style);
    QSettings("BaronEdition", "Orchestrion")
        .setValue("stylePresets", QJsonDocument(saved).toJson());
    rebuildStyles();
    captureStyleBank();
    auto picker = m_settingsDialog->findChild<QComboBox*>("settingsStyleSelect");
    QSignalBlocker blocker(picker);
    picker->clear();
    for (int i = 0; i < m_styles->count(); ++i)
        picker->addItem(m_styles->itemIcon(i), m_styles->itemText(i), m_styles->itemData(i));
    picker->setCurrentIndex(picker->findData(m_styleId));
}
void BaronPanel::refreshPrice() {
    if (m_client->backend() != OrchestrionClient::orchestrion || !isVisible() || m_priceBusy || !m_client->signedIn() || !m_host)
        return;
    const auto bounds = m_host->imageBounds(m_mode->currentData() != "upscale");
    if (bounds.isEmpty())
        return;
    const auto mode = m_mode->currentData().toString();
    const auto scale = mode == "upscale" ? m_upscaleFactor->value() : m_scale->value() == 1
        ? QSettings("BaronEdition", "Orchestrion").value("performanceResolutionMultiplier", 1).toDouble() : m_scale->value();
    const auto width = int(bounds.width() * scale), height = int(bounds.height() * scale);
    const QByteArray key = QByteArray::number(width) + "x" + QByteArray::number(height) + ":"
        + QByteArray::number(m_batch->value()) + ":" + m_client->root().toEncoded() + ":"
        + QByteArray::number(QDateTime::currentSecsSinceEpoch() / 30);
    if (key == m_priceKey)
        return;
    m_priceKey = key;
    m_priceBusy = true;
    m_priceClient->copyConnection(*m_client);
    m_priceClient->quote(QJsonObject { { "1",
        QJsonObject { { "class_type", "EmptyLatentImage" },
            { "inputs",
                QJsonObject {
                    { "width", width }, { "height", height }, { "batch_size", 1 } } } } } });
}
void BaronPanel::cancelJobs(int kind) {
    for (const auto& job : m_jobs->jobs()) {
        const bool running = job->state == JobQueue::running || job->state == JobQueue::downloading;
        if (kind == 2 || (kind == 0 && running) || (kind == 1 && !running))
            m_jobs->cancel(job->id);
    }
}
