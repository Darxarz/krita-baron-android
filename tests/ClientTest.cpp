// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include "BaronUpdates.h"
#include "GuidancePanel.h"
#include "Localization.h"
#include "ModelThumbnails.h"
#include "OrchestrionClient.h"
#include "PluginUi.h"
#include "PromptActions.h"
#include "PromptLogic.h"
#include "BackgroundWork.h"
#include "OrientationLayouts.h"
#include "InpaintWidget.h"
#include <QProcess>
#include <QFontDatabase>
#include <QCryptographicHash>
#include <QMainWindow>
#include <QToolBar>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include "ModelCatalog.h"
#include "TagModel.h"
#include "InterfaceSettings.h"
#include "AuthorCredits.h"
#include "ConnectionButton.h"
#include "CloudWorkflow.h"
#include "UpscaleWidget.h"
#include <QGroupBox>
#include "BaronDiagnostics.h"
#include <QCompleter>
#include <QInputMethodEvent>
#include <QTextBlock>
#include <QTextLayout>
#include <QListView>
#include <QSet>
#include <QAbstractItemView>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QBuffer>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMenu>
#include <QNetworkReply>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QTouchDevice>
#include <QSettings>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QUuid>
#include <cstring>
#include <QClipboard>
#include <QMimeData>
#include <QContextMenuEvent>
#include <QMenu>

struct PreviewResponse {
    int status;
    QUrl redirect;
    QByteArray body;
    int delay = 0;
};
class PreviewReply : public QNetworkReply {
public:
    PreviewReply(const QNetworkRequest& request, const PreviewResponse& response, QObject* parent)
        : QNetworkReply(parent)
        , m_body(response.body) {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
        setAttribute(QNetworkRequest::RedirectionTargetAttribute, response.redirect);
        open(QIODevice::ReadOnly);
        QTimer::singleShot(response.delay, this, [this] {
            if (isFinished())
                return;
            emit readyRead();
            if (!isFinished()) {
                setFinished(true);
                emit finished();
            }
        });
    }
    void abort() override {
        if (!isFinished()) {
            setError(OperationCanceledError, "Aborted");
            setFinished(true);
            emit finished();
        }
    }
    qint64 bytesAvailable() const override {
        return m_body.size() - m_offset + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override {
        const auto count = qMin(maxSize, qint64(m_body.size()) - m_offset);
        if (count <= 0)
            return -1;
        std::memcpy(data, m_body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
};
class PreviewNetwork : public QNetworkAccessManager {
public:
    QList<PreviewResponse> responses;
    QList<QNetworkRequest> requests;
    QList<QByteArray> bodies;
    QList<Operation> operations;
    std::function<PreviewResponse(Operation, const QNetworkRequest&, const QByteArray&)> respond;

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoing) override {
        requests.append(request);
        const QByteArray body = outgoing ? outgoing->readAll() : QByteArray();
        bodies.append(body); operations.append(op);
        return new PreviewReply(request,
            respond ? respond(op, request, body) : responses.isEmpty() ? PreviewResponse { 500, {}, {} } : responses.takeFirst(), this);
    }
};
class ClientTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory;
private slots:
    void upscaleControlsMatchOriginalPlugin() {
        UpscaleWidget widget;
        auto slider = widget.findChild<QSlider*>("upscaleFactorSlider");
        QCOMPARE(slider->minimum(), 100);
        QCOMPARE(slider->maximum(), 400);
        QCOMPARE(slider->singleStep(), 50);
        QCOMPARE(slider->pageStep(), 50);
        QCOMPARE(slider->tickInterval(), 50);
        QCOMPARE(widget.factor->singleStep(), .5);
        widget.setCanvasSize(QSize(1536, 1024));
        slider->setValue(174);
        QCOMPARE(widget.factor->value(), 1.5);
        slider->setValue(175);
        QCOMPARE(widget.factor->value(), 2.0); // Python round uses ties to even.
        widget.factor->setValue(2.25);
        QCOMPARE(slider->value(), 225); // Typed values must not be snapped.
        QVERIFY(widget.findChild<QLabel*>("upscaleTargetSize")->text().contains("3456 x 2304"));
        widget.setResources({ { "upscalers", QJsonArray { "other.pth", "OmniSR_X2_DIV2K.safetensors", "4x_NMKD-Superscale-SP_178000_G.pth" } },
            { "resources", QJsonObject { { "controlnet-blur-sdxl", "tile.pth" } } } });
        QCOMPARE(widget.upscaler->count(), 2);
        QCOMPARE(widget.upscaler->currentData().toString(), QString("4x_NMKD-Superscale-SP_178000_G.pth"));
        widget.setArchitecture("Illustrious");
        QVERIFY(!widget.findChild<QSpinBox*>("upscaleGuidance")->isEnabled());
        widget.setArchitecture("Pony");
        QVERIFY(widget.findChild<QSpinBox*>("upscaleGuidance")->isEnabled());
        widget.findChild<QComboBox*>("upscaleOverlapMode")->setCurrentIndex(1);
        widget.findChild<QSpinBox*>("upscaleOverlap")->setValue(64);
        widget.findChild<ToggleSwitch*>("upscaleUsePrompt")->setChecked(true);
        const auto state = widget.state();
        QVERIFY(state["use_diffusion"].toBool());
        QVERIFY(state["use_prompt"].toBool());
        QCOMPARE(state["tile_overlap"].toInt(), 64);
        UpscaleWidget restored;
        restored.restore(state);
        restored.setResources({ { "upscalers", QJsonArray { "4x_NMKD-Superscale-SP_178000_G.pth" } },
            { "resources", QJsonObject { { "controlnet-blur-sdxl", "tile.pth" } } } });
        restored.setArchitecture("sdxl");
        QCOMPARE(restored.state(), state);
        widget.setArchitecture("qwen_l");
        QCOMPARE(widget.effectiveStrength(), 1.0);
        QVERIFY(!widget.findChild<QSpinBox*>("upscaleStrength")->isEnabled());
        widget.setArchitecture("qwen2");
        QCOMPARE(widget.effectiveStrength(), .3);
        QVERIFY(widget.findChild<QSpinBox*>("upscaleStrength")->isEnabled());
    }
    void cloudUpscaleUsesChosenModelAndOriginalTileExtents() {
        QImage source(512, 512, QImage::Format_ARGB32);
        source.fill(Qt::red);
        QJsonObject input { { "mode", "upscale" }, { "model", "xl.safetensors" },
            { "width", 512 }, { "height", 512 }, { "image", QString::fromLatin1(BaronPanel::png(source).toBase64()) },
            { "scale", 1.5 }, { "upscale_options", QJsonObject { { "model", "chosen.pth" },
                { "use_diffusion", true }, { "strength", .3 }, { "tile_overlap_mode", "custom" }, { "tile_overlap", 64 } } } };
        QJsonObject resources { { "checkpoints", QJsonObject { { "xl.safetensors", QJsonObject { { "arch", "sdxl" } } } } },
            { "upscalers", QJsonArray { "other.pth", "chosen.pth" } } };
        QString issue;
        auto work = CloudWorkflow::prepare(input, resources, &issue);
        QVERIFY2(issue.isEmpty(), qPrintable(issue));
        QCOMPARE(work["kind"].toString(), QString("upscale_tiled"));
        QCOMPARE(work["upscale"].toObject()["model"].toString(), QString("chosen.pth"));
        QCOMPARE(work["upscale"].toObject()["tile_overlap"].toInt(), 64);
        const auto extent = work["images"].toObject()["extent"].toObject();
        QCOMPARE(extent["input"].toArray(), QJsonArray({ 512, 512 }));
        QCOMPARE(extent["initial"].toArray(), QJsonArray({ 768, 768 }));
        QCOMPARE(extent["desired"].toArray(), QJsonArray({ 896, 896 }));
        QCOMPARE(extent["target"].toArray(), QJsonArray({ 768, 768 }));
        QCOMPARE(work["conditioning"].toObject()["positive"].toString(), QString("4k uhd"));
        input["scale"] = 1;
        work = CloudWorkflow::prepare(input, resources, &issue);
        QVERIFY2(issue.isEmpty(), qPrintable(issue));
        QCOMPARE(work["upscale"].toObject()["model"].toString(), QString());
    }
    void crashDiagnosticsKeepOnlyNumericPromptStagesAndAllowClosing() {
        QStandardPaths::setTestModeEnabled(true);
        BaronDiagnostics::record("paste.insert.begin", {{ "characters", 1420 }, { "position", 0 }});
        const auto report = BaronDiagnostics::localReport();
        QVERIFY(report.contains("paste.insert.begin"));
        QVERIFY(report.contains("1420"));
        QVERIFY(!report.contains("<lora:"));
        QWidget host;
        BaronDiagnostics::show(&host);
        auto dialog = host.findChild<QDialog*>("crashDiagnosticsDialog");
        QVERIFY(dialog);
        auto field = dialog->findChild<QPlainTextEdit*>("crashDiagnosticsReport");
        QVERIFY(field && field->isReadOnly());
        QVERIFY(!field->testAttribute(Qt::WA_InputMethodEnabled));
        QCOMPARE(field->contextMenuPolicy(), Qt::NoContextMenu);
        dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!host.findChild<QDialog*>("crashDiagnosticsDialog"));
    }
    void originalAuthorAndConnectionsRemainSeparate() {
        AuthorCredits credits;
        QString text;
        for (auto label : credits.findChildren<QLabel*>()) text += label->text();
        QVERIFY(text.contains("Acly")); QVERIFY(text.contains("Darxarz"));
        QVERIFY(text.contains("https://github.com/Acly/krita-ai-diffusion"));
        QVERIFY(text.contains("https://www.interstice.cloud"));
        QVERIFY(text.contains("https://docs.interstice.cloud"));
        QVERIFY(text.contains("https://discord.gg/pWyzHfHHhU"));
        QVERIFY(text.contains("https://youtu.be/Ly6USRwTHe0"));
        QSettings settings("BaronEdition", "Orchestrion");
        settings.setValue("connectionBackend", int(OrchestrionClient::orchestrion));
        BaronPanel panel(nullptr);
        auto choices = panel.findChild<QComboBox*>("connectionMode");
        auto address = panel.findChild<QLineEdit*>("websiteAddress");
        QVERIFY(choices); QCOMPARE(choices->count(), 3);
        QVERIFY(panel.findChild<ConnectionButton*>("connectInterstice"));
        QVERIFY(panel.findChild<ConnectionButton*>("connectComfyUI"));
        QVERIFY(!panel.findChild<ConnectionButton*>("localManagedServer")->isEnabled());
        choices->setCurrentIndex(2); address->setText("http://127.0.0.1:9876/prefix");
        choices->setCurrentIndex(0);
        QCOMPARE(address->text(), QString("https://api.interstice.cloud"));
        QVERIFY(address->isHidden());
        choices->setCurrentIndex(2);
        QCOMPARE(address->text(), QString("http://127.0.0.1:9876/prefix"));
        choices->setCurrentIndex(1);
    }
    void nativeCloudWorkflowUsesOriginalSerialization() {
        const QJsonObject resources { { "checkpoints", QJsonObject { { "test.safetensors", QJsonObject {
            { "filename", "test.safetensors" }, { "arch", "sdxl" }, { "format", "checkpoint" } } } } },
            { "loras", QJsonArray { "style/ink.safetensors" } } };
        QImage source(512, 512, QImage::Format_ARGB32); source.fill(qRgba(10, 20, 30, 120));
        QImage mask(512, 512, QImage::Format_Grayscale8); mask.fill(255);
        auto encode = [](const QImage& image) { QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG"); return QString::fromLatin1(bytes.toBase64()); };
        QJsonObject input { { "mode", "edit" }, { "width", 512 }, { "height", 512 }, { "model", "test.safetensors" },
            { "image", encode(source) }, { "mask", encode(mask) }, { "prompt", "portrait <lora:style/ink:0.7>" },
            { "strength", .1 }, { "steps", 20 }, { "seed", 4294967295.0 },
            { "controls", QJsonArray { QJsonObject { { "mode", "reference" }, { "image", encode(source) } } } },
            { "regions", QJsonArray { QJsonObject { { "id", "character" }, { "prompt", "face" }, { "mask", encode(mask) } } } } };
        QString error;
        auto work = CloudWorkflow::prepare(input, resources, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(work["kind"].toString(), QString("refine_region"));
        QCOMPARE(work["sampling"].toObject()["total_steps"].toInt(), 40);
        QCOMPARE(work["sampling"].toObject()["start_step"].toInt(), 36);
        QCOMPARE(work["models"].toObject()["loras"].toArray().first().toObject()["strength"].toDouble(), .7);
        QCOMPARE(work["conditioning"].toObject()["regions"].toArray().size(), 2);
        QVERIFY(work["images"].toObject()["extent"].toObject()["target"].isArray());
        const auto data = work["image_data"].toObject();
        const auto offsets = data["offsets"].toArray();
        const auto blob = QByteArray::fromBase64(data["base64"].toString().toLatin1());
        const auto control = work["conditioning"].toObject()["control"].toArray().first().toObject();
        int index = control["image"].toInt(), start = offsets[index].toInt();
        int end = index + 1 < offsets.size() ? offsets[index + 1].toInt() : blob.size();
        QCOMPARE(QImage::fromData(blob.mid(start, end - start)).size(), QSize(224, 224));
        const auto fixture = qEnvironmentVariable("BARON_CLOUD_WORKFLOW");
        if (!fixture.isEmpty()) {
            QFile file(fixture); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(work).toJson());
        }
        input["mode"] = "unknown"; QVERIFY(CloudWorkflow::prepare(input, resources, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void intersticeTransportProgressResultsAndCancellation() {
        PreviewNetwork network;
        QImage first(32, 24, QImage::Format_ARGB32), second(32, 24, QImage::Format_ARGB32);
        first.fill(Qt::red); second.fill(qRgba(0, 0, 255, 70));
        auto png = [](const QImage& image) { QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG"); return bytes; };
        const auto one = png(first), two = png(second);
        int polls = 0, cancels = 0, generations = 0;
        network.respond = [&](auto, const QNetworkRequest& request, const QByteArray& body) {
            const auto path = request.url().path();
            QJsonObject response;
            if (path == "/auth/initiate") response = { { "url", "/auth/client/test" } };
            else if (path == "/auth/confirm") response = { { "status", "authorized" }, { "token", "cloud-secret" } };
            else {
                if (request.rawHeader("Authorization") != "Bearer cloud-secret") return PreviewResponse { 401, {}, "{}" };
                if (path == "/user") response = { { "name", "artist" }, { "credits", 100 } };
                else if (path == "/plugin/resources") response = { { "checkpoints", QJsonObject { { "test", QJsonObject { { "arch", "sdxl" } } } } } };
                else if (path == "/generate") {
                    ++generations;
                    const auto input = QJsonDocument::fromJson(body).object()["input"].toObject();
                    if (input["workflow"].toObject()["kind"] != "generate") return PreviewResponse { 400, {}, "{}" };
                    response = { { "id", "cloud-job" }, { "worker_id", "cloud-worker" } };
                } else if (path == "/status/cloud-job") {
                    if (++polls == 1) response = { { "status", "in_progress" }, { "output", QJsonObject { { "progress", .4 } } } };
                    else response = { { "status", "completed" }, { "output", QJsonObject { { "images", QJsonObject {
                        { "base64", QString::fromLatin1(QByteArray(one + two).toBase64()) }, { "offsets", QJsonArray { 0, one.size() } } } } } } };
                } else if (path == "/cancel/cloud-worker/cloud-job") { ++cancels; response = {}; }
                else return PreviewResponse { 404, {}, "{}" };
            }
            return PreviewResponse { 200, {}, QJsonDocument(response).toJson(QJsonDocument::Compact) };
        };
        OrchestrionClient client(nullptr, &network); client.setBackend(OrchestrionClient::interstice);
        QVERIFY(client.setRoot("https://api.interstice.cloud"));
        QSignalSpy browser(&client, &OrchestrionClient::browserLogin), login(&client, &OrchestrionClient::authenticated),
            catalog(&client, &OrchestrionClient::modelsReady), progress(&client, &OrchestrionClient::progressChanged),
            ready(&client, &OrchestrionClient::jobReady), result(&client, &OrchestrionClient::imageReady),
            errors(&client, &OrchestrionClient::error), stopped(&client, &OrchestrionClient::cancelled);
        client.signIn(); QTRY_COMPARE(browser.size(), 1);
        QCOMPARE(browser.first().first().toUrl().host(), QString("www.interstice.cloud"));
        QTRY_COMPARE_WITH_TIMEOUT(login.size(), 1, 3000); QTRY_COMPARE(catalog.size(), 1);
        client.submit({ { "kind", "generate" } });
        QTRY_COMPARE(progress.size(), 1); QCOMPARE(progress.first().first().toDouble(), .4);
        QTRY_COMPARE(ready.size(), 1);
        const auto descriptors = ready.first().first().toJsonObject()["outputs"].toObject()["cloud"].toObject()["images"].toArray();
        client.fetchImage(descriptors[0].toObject(), 0); client.fetchImage(descriptors[1].toObject(), 1);
        QCOMPARE(result.size(), 2);
        QCOMPARE(QImage::fromData(result[1][0].toByteArray()).pixel(0, 0), second.pixel(0, 0));
        client.submit({ { "kind", "generate" } }); client.cancelJob();
        QTRY_COMPARE(stopped.size(), 1); QCOMPARE(cancels, 1); QCOMPARE(generations, 2); QCOMPARE(errors.size(), 0);
        for (const auto& request : network.requests) QVERIFY(!request.url().path().startsWith("/api/krita"));
    }
    void cloudImageUploadUsesPresignedUrlWithoutAccountToken() {
        PreviewNetwork network;
        const QByteArray bytes(8192, 'x'); int uploads = 0;
        network.respond = [&](auto op, const QNetworkRequest& request, const QByteArray& body) {
            if (request.url().host() == "storage.example.test") {
                if (op == QNetworkAccessManager::PutOperation && request.rawHeader("Authorization").isEmpty() && body == bytes) ++uploads;
                return PreviewResponse { 200, {}, "{}" };
            }
            QJsonObject data;
            if (request.url().path() == "/upload/image") data = { { "url", "https://storage.example.test/input?signed=1" }, { "object", "input-object" } };
            else if (request.url().path() == "/generate") {
                const auto images = QJsonDocument::fromJson(body).object()["input"].toObject()["workflow"].toObject()["image_data"].toObject();
                if (images["s3_object"] != "input-object" || images.contains("base64")) return PreviewResponse { 400, {}, "{}" };
                data = { { "id", "uploaded" }, { "worker_id", "worker" } };
            } else return PreviewResponse { 404, {}, "{}" };
            return PreviewResponse { 200, {}, QJsonDocument(data).toJson() };
        };
        OrchestrionClient client(nullptr, &network); client.setBackend(OrchestrionClient::interstice);
        QVERIFY(client.setRoot("https://api.interstice.cloud")); client.setAccessToken("secret");
        QSignalSpy accepted(&client, &OrchestrionClient::submitted), errors(&client, &OrchestrionClient::error);
        client.submit({ { "kind", "refine" }, { "image_data", QJsonObject { { "base64", QString::fromLatin1(bytes.toBase64()) }, { "offsets", QJsonArray { 0 } } } } });
        QTRY_COMPARE(accepted.size(), 1); QCOMPARE(uploads, 1); QCOMPARE(errors.size(), 0);
    }
    void directComfyConnectionPreservesPrefixQueryAndAnonymousAccess() {
        PreviewNetwork network;
        network.respond = [&](auto, const QNetworkRequest& request, const QByteArray&) {
            if (!request.rawHeader("Authorization").isEmpty()) return PreviewResponse { 401, {}, "{}" };
            const auto path = request.url().path();
            if (!path.startsWith("/prefix/") || !request.url().query().contains("token=external")) return PreviewResponse { 400, {}, "{}" };
            QByteArray result = "{}";
            if (path == "/prefix/baron/native/models") result = R"({"items":[]})";
            else if (path == "/prefix/baron/native/prepare") result = R"({"prompt":{"1":{"class_type":"SaveImage","inputs":{}}}})";
            else if (path == "/prefix/prompt") result = R"({"prompt_id":"direct"})";
            return PreviewResponse { 200, {}, result };
        };
        OrchestrionClient client(nullptr, &network); client.setBackend(OrchestrionClient::comfyui);
        QVERIFY(client.setRoot("127.0.0.1:9876/prefix/?token=external"));
        QSignalSpy login(&client, &OrchestrionClient::authenticated), prepared(&client, &OrchestrionClient::prepared),
            accepted(&client, &OrchestrionClient::submitted), errors(&client, &OrchestrionClient::error);
        client.signIn(); QTRY_COMPARE(login.size(), 1); QVERIFY(client.signedIn());
        client.prepare({ { "mode", "generate" } }); QTRY_COMPARE(prepared.size(), 1);
        client.submit(prepared.first().first().toJsonObject()["prompt"].toObject()); QTRY_COMPARE(accepted.size(), 1);
        QCOMPARE(errors.size(), 0);
        OrchestrionClient clone(nullptr, &network); clone.copyConnection(client); QVERIFY(clone.signedIn());
        QCOMPARE(clone.backend(), OrchestrionClient::comfyui);
        QVERIFY(client.setRoot("127.0.0.1:9876/prefix/?token=different")); QVERIFY(!client.signedIn());
        QVERIFY(clone.signedIn());
        client.setBackend(OrchestrionClient::orchestrion); QVERIFY(!client.signedIn());
    }
    void customInpaintMatchesPythonContextAndRetainsMaskLayer() {
        QFile file(QFINDTESTDATA("fixtures/inpaint-context-golden.json")); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases=QJsonDocument::fromJson(file.readAll()).array();QCOMPARE(cases.size(),32);
        auto rect=[](const QJsonValue& v){auto a=v.toArray();return QRect(a[0].toInt(),a[1].toInt(),a[2].toInt(),a[3].toInt());};
        for(auto value:cases){auto item=value.toObject();QCOMPARE(InpaintWidget::contextBounds(rect(item["canvas"]),rect(item["mask"]),item["options"].toObject(),rect(item["layer"])),rect(item["expected"]));}
        QCOMPARE(InpaintWidget::diffusionMultiple("sd15"),16);
        QCOMPARE(InpaintWidget::diffusionMultiple("sdxl"),16);
        QCOMPARE(InpaintWidget::diffusionMultiple("qwen2"),32);
        InpaintWidget widget;
        widget.restore({{"context","layer_bounds"},{"context_layer_id","mask-1"},{"fill","blur"},{"use_inpaint_model",false},{"use_condition_mask",true}});
        widget.setLayers({QJsonObject{{"id","mask-1"},{"name","Selection mask"},{"mask",true}},QJsonObject{{"id","paint"},{"name","Paint layer"}}});
        QCOMPARE(widget.state()["context"].toString(),QString("layer_bounds"));
        QCOMPARE(widget.state()["context_layer_id"].toString(),QString("mask-1"));
        QCOMPARE(widget.state()["fill"].toString(),QString("blur"));QVERIFY(!widget.state()["use_inpaint_model"].toBool());QVERIFY(widget.state()["use_condition_mask"].toBool());
        QCOMPARE(widget.findChild<QComboBox*>("inpaintContext")->count(),4);
        widget.setCapabilities("sdxl",.35,false);QVERIFY(!widget.findChild<QComboBox*>("inpaintFill")->isEnabled());
        widget.setCapabilities("qwen2",1,true);QVERIFY(!widget.findChild<QComboBox*>("inpaintFill")->isEnabled());QVERIFY(widget.findChild<QCheckBox*>("inpaintEdit")->isEnabled());
        widget.setCapabilities("qwen",1,false);QVERIFY(widget.findChild<QCheckBox*>("inpaintSeamless")->isEnabled());
        widget.setCapabilities("sdxl",1,false);QVERIFY(widget.findChild<QComboBox*>("inpaintFill")->isEnabled());
    }
    void orientationProfilesReachDiskAndLateDockRestores() {
        QSettings settings("BaronEdition","Orchestrion");settings.remove("orientation_layout");settings.sync();
        QMainWindow window;window.resize(700,500);window.setCentralWidget(new QWidget(&window));
        auto dock=new QDockWidget("Layers",&window);dock->setObjectName("persistentLayers");dock->setWidget(new QWidget(dock));window.addDockWidget(Qt::LeftDockWidgetArea,dock);
        window.show();auto controller=new OrientationLayouts(&window,false);controller->setOrientation(Qt::PortraitOrientation);
        QTRY_COMPARE(controller->currentOrientation(),1);QTest::qWait(350);controller->captureNow();
        const auto expected=QCryptographicHash::hash(settings.value("orientation_layout/portrait").toByteArray(),QCryptographicHash::Sha256).toHex();
        QProcess child;child.start(QCoreApplication::applicationFilePath(),{"--verify-orientation-profile",QString::fromLatin1(expected)});
        QVERIFY(child.waitForFinished(15000));QCOMPARE(child.exitCode(),0);
        delete controller;
        window.removeDockWidget(dock);delete dock;
        QMainWindow restored;restored.resize(700,500);restored.setCentralWidget(new QWidget(&restored));restored.show();
        auto reloader=new OrientationLayouts(&restored,false);reloader->setOrientation(Qt::PortraitOrientation);QTRY_COMPARE(reloader->currentOrientation(),1);
        auto late=new QDockWidget("Layers",&restored);late->setObjectName("persistentLayers");late->setWidget(new QWidget(late));restored.addDockWidget(Qt::RightDockWidgetArea,late);
        QTRY_COMPARE(restored.dockWidgetArea(late),Qt::LeftDockWidgetArea);
        delete reloader;settings.remove("orientation_layout");settings.sync();
    }
    void orientationLayoutsRestorePanelsToolbarsAndSurviveRestart() {
        QSettings settings("BaronEdition", "Orchestrion");
        settings.remove("orientation_layout"); settings.setValue("orientation_layouts_enabled", true);
        {
            QMainWindow window; window.resize(700, 500);
            window.setCentralWidget(new QWidget(&window));
            auto dock = new QDockWidget("Layers", &window); dock->setObjectName("testLayers");
            dock->setWidget(new QWidget(dock)); window.addDockWidget(Qt::RightDockWidgetArea, dock);
            auto toolbar = new QToolBar("Tools", &window); toolbar->setObjectName("testTools");
            window.addToolBar(Qt::TopToolBarArea, toolbar); toolbar->addAction("Draw");
            auto history = new QDockWidget("History", &window); history->setObjectName("testHistory");
            history->setWidget(new QWidget(history)); window.addDockWidget(Qt::RightDockWidgetArea, history);
            window.tabifyDockWidget(dock, history);
            window.show();
            auto layouts = new OrientationLayouts(&window, false);
            layouts->setOrientation(Qt::LandscapeOrientation);
            QTRY_COMPARE(layouts->currentOrientation(), 0); QTest::qWait(350);
            layouts->captureNow();
            QVERIFY(!settings.value("orientation_layout/landscape").toByteArray().isEmpty());
            layouts->setOrientation(Qt::PortraitOrientation);
            QTRY_COMPARE(layouts->currentOrientation(), 1); QTest::qWait(350);
            window.addDockWidget(Qt::LeftDockWidgetArea, dock);
            window.addDockWidget(Qt::BottomDockWidgetArea, history);
            window.addToolBar(Qt::BottomToolBarArea, toolbar);
            QCoreApplication::processEvents(); layouts->captureNow();
            const auto portrait = settings.value("orientation_layout/portrait").toByteArray();
            QVERIFY(!portrait.isEmpty());
            window.resize(700, 300); QTest::qWait(180);
            QCOMPARE(layouts->currentOrientation(), 1);
            layouts->setOrientation(Qt::InvertedPortraitOrientation); QTest::qWait(280);
            QCOMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
            layouts->setOrientation(Qt::LandscapeOrientation);
            QTRY_COMPARE(window.dockWidgetArea(dock), Qt::RightDockWidgetArea);
            QCOMPARE(window.toolBarArea(toolbar), Qt::TopToolBarArea); QTest::qWait(350);
            QVERIFY(window.tabifiedDockWidgets(dock).contains(history));
            layouts->setOrientation(Qt::PortraitOrientation);
            QTRY_COMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
            QCOMPARE(window.toolBarArea(toolbar), Qt::BottomToolBarArea); QTest::qWait(350);
            QCOMPARE(window.dockWidgetArea(history), Qt::BottomDockWidgetArea);
        }
        {
            QMainWindow window; window.resize(700, 500); window.setCentralWidget(new QWidget(&window));
            auto dock = new QDockWidget("Layers", &window); dock->setObjectName("testLayers");
            dock->setWidget(new QWidget(dock)); window.addDockWidget(Qt::RightDockWidgetArea, dock); window.show();
            auto layouts = new OrientationLayouts(&window, false);
            layouts->setOrientation(Qt::PortraitOrientation);
            QTRY_COMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
            QTest::qWait(350);
            layouts->setEnabled(false); layouts->setOrientation(Qt::LandscapeOrientation); QTest::qWait(300);
            QCOMPARE(window.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
        }
        settings.remove("orientation_layout"); settings.remove("orientation_layouts_enabled");
    }
    void orientationLayoutsDoNotRememberCanvasOnlyAndIgnoreRapidTurns() {
        QSettings settings("BaronEdition", "Orchestrion"); settings.remove("orientation_layout");
        QMainWindow window; window.resize(700, 500); window.setCentralWidget(new QWidget(&window));
        auto dock = new QDockWidget("Layers", &window); dock->setObjectName("testLayers");
        dock->setWidget(new QWidget(dock)); window.addDockWidget(Qt::RightDockWidgetArea, dock);
        auto canvas = new QAction(&window); canvas->setObjectName("view_show_canvas_only"); canvas->setCheckable(true);
        window.show(); auto layouts = new OrientationLayouts(&window, false);
        layouts->setOrientation(Qt::LandscapeOrientation);
        QTRY_COMPARE(layouts->currentOrientation(), 0); QTest::qWait(350);
        const auto original = settings.value("orientation_layout/landscape").toByteArray();
        canvas->setChecked(true); dock->hide(); layouts->captureNow();
        QCOMPARE(settings.value("orientation_layout/landscape").toByteArray(), original);
        layouts->setOrientation(Qt::PortraitOrientation); QTest::qWait(300);
        QCOMPARE(layouts->currentOrientation(), 0);
        layouts->setOrientation(Qt::LandscapeOrientation); canvas->setChecked(false); dock->show();
        layouts->setOrientation(Qt::PortraitOrientation); layouts->setOrientation(Qt::LandscapeOrientation);
        QTest::qWait(350); QCOMPARE(layouts->currentOrientation(), 0);
        OrientationLayouts::forgetSaved();
        QVERIFY(settings.value("orientation_layout/portrait").toByteArray().isEmpty());
        delete layouts; settings.remove("orientation_layout");
    }
    void historyThumbnailsStayInGridWhenDragged() {
        HistoryList list;
        list.resize(360, 300);
        list.setViewMode(QListView::IconMode);
        list.setResizeMode(QListView::Adjust);
        list.setFlow(QListView::LeftToRight);
        list.setWrapping(true);
        for (int i = 0; i < 9; ++i) {
            auto item = new QListWidgetItem(QString::number(i), &list);
            item->setSizeHint(QSize(90, 90));
            item->setData(Qt::UserRole, i);
        }
        list.show();
        QCoreApplication::processEvents();
        QCOMPARE(list.movement(), QListView::Static);
        QCOMPARE(list.dragDropMode(), QAbstractItemView::NoDragDrop);
        QVERIFY(!list.dragEnabled());
        QVERIFY(!list.acceptDrops());
        QVERIFY(!list.viewport()->acceptDrops());
        QList<QRect> positions;
        for (int i = 0; i < list.count(); ++i) positions.append(list.visualItemRect(list.item(i)));
        const auto start = positions.first().center();
        const auto target = start + QPoint(115, 130);
        QTest::mousePress(list.viewport(), Qt::LeftButton, Qt::NoModifier, start);
        for (int step = 1; step <= 5; ++step) {
            QMouseEvent move(QEvent::MouseMove, start + (target - start) * step / 5,
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(list.viewport(), &move);
        }
        QTest::mouseRelease(list.viewport(), Qt::LeftButton, Qt::NoModifier, target);
        QCoreApplication::processEvents();
        for (int i = 0; i < list.count(); ++i) {
            QCOMPARE(list.visualItemRect(list.item(i)), positions[i]);
            QCOMPARE(list.item(i)->data(Qt::UserRole).toInt(), i);
        }
    }
    void historyTouchSwipeScrollsWithInertiaWithoutClicking_data() {
        QTest::addColumn<QString>("area");
        QTest::newRow("thumbnail") << QString("thumbnail");
        QTest::newRow("empty-space") << QString("empty-space");
        QTest::newRow("apply-overlay") << QString("apply-overlay");
    }
    void historyTouchSwipeScrollsWithInertiaWithoutClicking() {
        QFETCH(QString, area);
        HistoryList list;
        list.resize(320, 260);
        list.setViewMode(QListView::IconMode);
        list.setFlow(QListView::LeftToRight);
        list.setWrapping(true);
        list.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        for (int i = 0; i < 120; ++i) {
            auto item = new QListWidgetItem(QString::number(i), &list);
            item->setSizeHint(QSize(90, 90));
        }
        list.show();
        QCoreApplication::processEvents();
        QSignalSpy clicked(&list, &QListWidget::itemClicked);
        QSignalSpy doubleClicked(&list, &QListWidget::itemDoubleClicked);
        QSignalSpy emptyClicked(&list, &HistoryList::emptyClicked);
        QSignalSpy applied(&list, &HistoryList::applyRequested);
        auto scroller = QScroller::scroller(list.viewport());
        QVERIFY(QScroller::hasScroller(list.viewport()));
        QCOMPARE(list.verticalScrollMode(), QAbstractItemView::ScrollPerPixel);
        QVERIFY(list.verticalScrollBar()->maximum() > 1000);
        auto device = QTest::createTouchDevice(QTouchDevice::TouchScreen);
        QPoint start(130, 210);
        if (area == "empty-space") {
            start.setX(list.viewport()->width() - 4);
            QVERIFY(!list.itemAt(start));
        } else if (area == "apply-overlay") {
            list.setCurrentItem(list.itemAt(QPoint(130, 130)));
            auto apply = list.findChild<QPushButton*>("historyApplyOverlay");
            QVERIFY(apply && apply->isVisible());
            start = apply->mapTo(list.viewport(), apply->rect().center());
            QVERIFY(list.viewport()->rect().contains(start));
        }
        QTest::touchEvent(list.viewport(), device).press(0, start, list.viewport());
        QTest::qWait(25);
        QTest::touchEvent(list.viewport(), device).move(0, start - QPoint(0, 40), list.viewport());
        QTest::qWait(25);
        QTest::touchEvent(list.viewport(), device).move(0, start - QPoint(0, 90), list.viewport());
        QTest::qWait(25);
        QTest::touchEvent(list.viewport(), device).move(0, start - QPoint(0, 140), list.viewport());
        QTest::qWait(25);
        QTest::touchEvent(list.viewport(), device).release(0, start - QPoint(0, 160), list.viewport());
        QTRY_COMPARE(scroller->state(), QScroller::Scrolling);
        const int released = list.verticalScrollBar()->value();
        QTRY_VERIFY(list.verticalScrollBar()->value() > released + 20);
        QCOMPARE(clicked.size(), 0);
        QCOMPARE(doubleClicked.size(), 0);
        QCOMPARE(emptyClicked.size(), 0);
        QCOMPARE(applied.size(), 0);
        scroller->stop();
        auto item = list.itemAt(QPoint(50, 50));
        QVERIFY(item);
        const QPoint center = list.visualItemRect(item).center();
        QTest::touchEvent(list.viewport(), device).press(0, center, list.viewport());
        QTest::qWait(30);
        QTest::touchEvent(list.viewport(), device).release(0, center, list.viewport());
        QTRY_COMPARE(clicked.size(), 1);
        const QPoint empty(list.viewport()->width() - 4, 50);
        QVERIFY(!list.itemAt(empty));
        QTest::touchEvent(list.viewport(), device).press(0, empty, list.viewport());
        QTest::qWait(30);
        QTest::touchEvent(list.viewport(), device).release(0, empty, list.viewport());
        QTRY_COMPARE(emptyClicked.size(), 1);
        QCOMPARE(clicked.size(), 1);
        QCOMPARE(applied.size(), 0);
    }
    void historyEmptyTapHidesPreviewWithoutApplyingOrDeleting() {
        class Host : public CanvasHost {
        public:
            int previews = 0, applications = 0, hides = 0, clears = 0;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { ++applications; return true; }
            bool preview(const QString&, const QImage&, const QImage&, QString*) override { ++previews; return true; }
            void hidePreview() override { ++hides; }
            void clearPreview() override { ++clears; }
        } host;
        BaronPanel panel(&host);
        panel.resize(500, 1000);
        panel.show();
        auto history = panel.findChild<HistoryList*>("resultHistory");
        QVERIFY(history);
        QImage image(32, 32, QImage::Format_ARGB32);
        image.fill(Qt::red);
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(image)), "", history);
        item->setSizeHint(QSize(96, 96));
        item->setData(Qt::UserRole, QVariantMap { { "image", image }, { "target", "original" }, { "key", "first" } });
        QCoreApplication::processEvents();
        history->setCurrentItem(item);
        history->itemClicked(item);
        QCOMPARE(host.previews, 1);
        const QPoint empty(history->viewport()->width() - 8, history->viewport()->height() - 8);
        QVERIFY(!history->itemAt(empty));
        QTest::mouseClick(history->viewport(), Qt::RightButton, Qt::NoModifier, empty);
        QCOMPARE(host.hides, 0);
        QTest::mousePress(history->viewport(), Qt::LeftButton, Qt::NoModifier, empty);
        QTest::mouseRelease(history->viewport(), Qt::LeftButton, Qt::NoModifier, empty - QPoint(50, 0));
        QCOMPARE(host.hides, 0);
        QTest::mouseClick(history->viewport(), Qt::LeftButton, Qt::NoModifier, empty);
        QCOMPARE(host.hides, 1);
        QCOMPARE(host.applications, 0);
        QCOMPARE(host.clears, 0);
        QCOMPARE(history->count(), 1);
        QVERIFY(history->selectedItems().isEmpty());
        history->itemClicked(item);
        QCOMPARE(host.previews, 2);
        QCOMPARE(host.applications, 0);
        history->itemClicked(item);
        QCOMPARE(host.hides, 2);
        QCOMPARE(host.applications, 0);
        history->setCurrentItem(item);
        history->itemClicked(item);
        QCoreApplication::processEvents();
        auto apply = history->findChild<QPushButton*>("historyApplyOverlay");
        QVERIFY(apply && apply->isVisible());
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(host.applications, 1);
        QCOMPARE(host.clears, 0);
        history->setCurrentItem(item);
        history->itemClicked(item);
        QTest::mouseDClick(history->viewport(), Qt::LeftButton, Qt::NoModifier,
            history->visualItemRect(item).center());
        QTest::mouseRelease(history->viewport(), Qt::LeftButton, Qt::NoModifier,
            history->visualItemRect(item).center());
        QCOMPARE(host.applications, 2);
        QCOMPARE(host.previews, 4);
    }
    void controlPresetsCustomRangeAndLegacyValues() {
        GuidancePanel panel(nullptr);
        panel.setArchitecture("sdxl");
        panel.findChild<QToolButton*>("addControlLayer")->menu()->actions()[0]->trigger();
        auto preset = panel.findChild<QSlider*>("controlPreset");
        auto strength = panel.findChild<QDoubleSpinBox*>("controlStrength");
        auto custom = panel.findChild<QCheckBox*>("controlCustom");
        auto slider = panel.findChild<QSlider*>("controlStrengthSlider");
        auto interval = panel.findChild<IntervalSlider*>("controlRange");
        QCOMPARE(preset->maximum(), 4);
        QCOMPARE(strength->value(), 1.0);
        auto state = [&] { return panel.state()["controls"].toArray().first().toObject(); };
        preset->setValue(0);
        preset->setValue(2);
        QCOMPARE(strength->value(), .8);
        QCOMPARE(state()["start"].toDouble(), .2);
        QCOMPARE(state()["end"].toDouble(), .8);
        preset->setValue(0);
        QCOMPARE(strength->value(), .4);
        preset->setValue(1);
        QCOMPARE(strength->value(), .6);
        preset->setValue(4);
        QCOMPARE(strength->value(), 1.0);
        QCOMPARE(interval->low(), 0);
        QCOMPARE(interval->high(), 20);
        custom->setChecked(true);
        QVERIFY(!preset->isEnabled());
        QVERIFY(slider->isEnabled());
        QVERIFY(interval->isEnabled());
        slider->setValue(63);
        interval->setInterval(4, 15);
        QCOMPARE(strength->value(), 1.26);
        QCOMPARE(state()["start"].toDouble(), .2);
        QCOMPARE(state()["end"].toDouble(), .75);
        const auto saved = panel.state();
        GuidancePanel reopened(nullptr);
        reopened.restoreState(saved);
        QCOMPARE(reopened.state()["controls"].toArray().first().toObject(), state());
        auto legacy = state(); legacy.remove("use_custom_strength"); legacy.remove("preset_value");
        legacy["strength"] = .73; legacy["start"] = .15; legacy["end"] = .65;
        reopened.restoreState({ { "controls", QJsonArray { legacy } } });
        const auto migrated = reopened.state()["controls"].toArray().first().toObject();
        QCOMPARE(migrated["strength"].toDouble(), .73);
        QCOMPARE(migrated["start"].toDouble(), .15);
        QCOMPARE(migrated["end"].toDouble(), .65);
        QVERIFY(migrated["use_custom_strength"].toBool());
        custom->setChecked(false);
        QCOMPARE(strength->value(), 1.0);
        panel.setArchitecture("flux"); preset->setValue(2);
        QCOMPARE(strength->value(), .6);
        QCOMPARE(state()["end"].toDouble(), .5);
        panel.setArchitecture("qwen2");
        QVERIFY(preset->isHidden());
        auto mode = panel.findChild<QComboBox*>("controlMode");
        mode->setCurrentIndex(mode->findData("normal"));
        QString error;
        QCOMPARE(panel.input(QRect(0, 0, 8, 8), {}, &error)["controls"].toArray().size(), 0);
    }
    void addControlReusesLastModeAndActiveLayer() {
        class Host : public CanvasHost {
        public:
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return false; }
            QJsonArray layers() const override {
                return { QJsonObject { { "id", "other" }, { "name", "Other" } },
                    QJsonObject { { "id", "active" }, { "name", "Active" }, { "active", true } } };
            }
        } host;
        GuidancePanel panel(&host);
        panel.setArchitecture("sdxl");
        auto add = panel.findChild<QToolButton*>("addControlLayer");
        add->click();
        const auto modes = panel.findChildren<QComboBox*>("controlMode");
        QCOMPARE(modes.size(), 1);
        QCOMPARE(modes[0]->currentData().toString(), QString("scribble"));
        QCOMPARE(panel.findChild<QComboBox*>("controlLayer")->currentData().toString(), QString("active"));
        modes[0]->setCurrentIndex(modes[0]->findData("depth"));
        add->click();
        QCOMPARE(panel.findChildren<QComboBox*>("controlMode")[1]->currentData().toString(), QString("depth"));
        panel.setArchitecture("qwen_e_p");
        add->click();
        QCOMPARE(panel.findChildren<QComboBox*>("controlMode")[2]->currentData().toString(), QString("reference"));
        add->menu()->actions()[7]->trigger();
        QCOMPARE(panel.findChildren<QComboBox*>("controlMode").size(), 4);
        QCOMPARE(panel.findChildren<QComboBox*>("controlMode")[3]->currentData().toString(), QString("canny_edge"));
    }
    void controlPreprocessorVisibility_data() {
        QTest::addColumn<QString>("architecture");
        QTest::addColumn<int>("width");
        for (const auto& arch : QStringList { "sdxl", "qwen2", "flux2_9b", "krea2", "qwen_l" })
            for (const int width : { 320, 560 })
                QTest::newRow(qPrintable(QString("%1-%2").arg(arch).arg(width))) << arch << width;
    }
    void controlPreprocessorVisibility() {
        QFETCH(QString, architecture);
        QFETCH(int, width);
        GuidancePanel panel(nullptr);
        panel.setArchitecture(architecture);
        panel.findChild<QToolButton*>("addControlLayer")->menu()->actions()[0]->trigger();
        panel.setFixedSize(width, 200);
        panel.show();
        QCoreApplication::processEvents();
        auto mode = panel.findChild<QComboBox*>("controlMode");
        auto button = panel.findChild<QToolButton*>("controlFromImage");
        QVERIFY(button);
        for (int i = 0; i < mode->count(); ++i) {
            mode->setCurrentIndex(i);
            QCoreApplication::processEvents();
            const auto value = mode->currentData().toString();
            bool expected = QStringList { "scribble", "line_art", "soft_edge", "canny_edge",
                "depth", "normal", "pose", "segmentation", "hands" }.contains(value);
            if (architecture == "qwen2" || architecture == "flux2_9b")
                expected = expected && QStringList { "scribble", "line_art", "canny_edge", "depth", "pose" }.contains(value);
            else if (architecture == "krea2" || architecture == "qwen_l") expected = false;
            QVERIFY2(button->isVisible() == expected, qPrintable(value));
            const auto regions = panel.findChild<QToolButton*>("controlFromRegions");
            QCOMPARE(regions->isVisible(), architecture == "sdxl" && value == "segmentation");
        }
    }
    void controlPreprocessorUsesCanvasAndTargetsItsOwnRow() {
        class Host : public CanvasHost {
        public:
            int captures = 0, layerReads = 0;
            bool includeMask = true, empty = false;
            CanvasSnapshot capture(bool mask, QString* error) override {
                ++captures; includeMask = mask;
                if (empty) { *error = "No document"; return {}; }
                QImage image(32, 24, QImage::Format_ARGB32); image.fill(Qt::green);
                return { image, {}, "canvas", QRect(0, 0, 32, 24) };
            }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return true; }
            QJsonArray layers() const override {
                return { QJsonObject { { "id", "source" }, { "name", "Source" }, { "active", true } },
                    QJsonObject { { "id", "map" }, { "name", "Control map" } } };
            }
            QImage layerImage(const QString&, const QRect&, QString*) override {
                ++layerReads; QImage image(32, 24, QImage::Format_ARGB32); image.fill(Qt::red); return image;
            }
        } host;
        GuidancePanel panel(&host);
        panel.setArchitecture("qwen2");
        panel.restoreState({ { "controls", QJsonArray {
            QJsonObject { { "id", "first" }, { "mode", "canny_edge" }, { "layer", "source" } },
            QJsonObject { { "id", "second" }, { "mode", "depth" }, { "layer", "source" } } } } });
        QSignalSpy requested(&panel, &GuidancePanel::controlRequested);
        QSignalSpy failed(&panel, &GuidancePanel::error);
        const auto buttons = panel.findChildren<QToolButton*>("controlFromImage");
        const auto sources = panel.findChildren<QComboBox*>("controlLayer");
        buttons[1]->click();
        QCOMPARE(requested.size(), 1);
        const auto input = requested[0][0].toJsonObject();
        QCOMPARE(input["operation"].toString(), QString("control_image"));
        QCOMPARE(input["control_mode"].toString(), QString("depth"));
        QCOMPARE(input["control_id"].toString(), QString("second"));
        QCOMPARE(input["target"].toString(), QString("canvas"));
        QCOMPARE(input["width"].toInt(), 32);
        QCOMPARE(input["height"].toInt(), 24);
        const auto image = QImage::fromData(QByteArray::fromBase64(input["image"].toString().toLatin1()));
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::green));
        QCOMPARE(host.captures, 1);
        QCOMPARE(host.layerReads, 0);
        QVERIFY(!host.includeMask);
        panel.setControlBusy("second", true);
        buttons[1]->click();
        QCOMPARE(requested.size(), 1);
        QVERIFY(!sources[1]->isEnabled());
        QVERIFY(buttons[0]->isEnabled());
        panel.setControlBusy("second", false);
        panel.setControlLayer("map", "second");
        QCOMPARE(sources[0]->currentData().toString(), QString("source"));
        QCOMPARE(sources[1]->currentData().toString(), QString("map"));
        host.empty = true;
        buttons[1]->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed[0][0].toString(), QString("No document"));
    }
    void intervalSliderMaintainsRangeOnTouchAndKeyboard() {
        IntervalSlider slider;
        slider.resize(200, 32);
        slider.show();
        slider.setInterval(4, 16);
        QTest::keyClick(&slider, Qt::Key_Right);
        QCOMPARE(slider.low(), 5);
        QTest::keyClick(&slider, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(slider.high(), 15);
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(145, 16));
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, QPoint(199, 16));
        QCOMPARE(slider.high(), 20);
        slider.setInterval(24, -1);
        QCOMPARE(slider.low(), 20);
        QCOMPARE(slider.high(), 20);
    }
    void segmentationFromLinkedRegionsAndControlJobBusy() {
        class Host : public CanvasHost {
        public:
            CanvasSnapshot capture(bool, QString*) override {
                QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::white);
                return { image, {}, "canvas", QRect(0, 0, 8, 8) };
            }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return true; }
            QJsonArray layers() const override { return { QJsonObject { { "id", "top" }, { "name", "Top" } }, QJsonObject { { "id", "bottom" }, { "name", "Bottom" } } }; }
            QImage layerImage(const QString& id, const QRect&, QString*) override {
                QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::transparent);
                image.setPixel(1, 1, id == "top" ? qRgba(0, 0, 0, 128) : qRgba(0, 0, 0, 255));
                return image;
            }
        } host;
        GuidancePanel panel(&host);
        panel.restoreState({ { "regions", QJsonArray { QJsonObject { { "id", "one" }, { "layer", "bottom" } }, QJsonObject { { "id", "two" }, { "layer", "top" } } } },
            { "controls", QJsonArray { QJsonObject { { "id", "segmentation" }, { "mode", "segmentation" }, { "layer", "bottom" } } } } });
        QSignalSpy generated(&panel, &GuidancePanel::controlMapGenerated);
        auto button = panel.findChild<QToolButton*>("controlFromRegions");
        QVERIFY(button);
        panel.setControlBusy("segmentation", true);
        QVERIFY(!button->isEnabled());
        QVERIFY(!panel.findChild<QComboBox*>("controlLayer")->isEnabled());
        panel.setControlBusy("segmentation", false);
        button->click();
        QCOMPARE(generated.size(), 1);
        QCOMPARE(generated[0][0].toString(), QString("canvas"));
        QCOMPARE(generated[0][1].toString(), QString("segmentation"));
        const auto image = generated[0][2].value<QImage>();
        QCOMPARE(image.pixelColor(0, 0), QColor(Qt::white));
        QCOMPARE(image.pixelColor(1, 1), QColor(255, 127, 127));
    }
    void generateButtonCapturesSubmitsAndReceivesResults() {
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QByteArray> annotations;
            int captures = 0, previews = 0, controlApplications = 0, scales = 0;
            bool lastCaptureIncludedMask = false;
            QSize scaledSize;
            bool masked = false;
            CanvasSnapshot capture(bool includeMask, QString*) override {
                ++captures;
                lastCaptureIncludedMask = includeMask;
                QImage image(32, 24, QImage::Format_ARGB32);
                image.fill(Qt::green);
                QImage mask;
                if (masked && includeMask) {
                    mask = QImage(image.size(), QImage::Format_Grayscale8);
                    mask.fill(127);
                }
                return { image, mask, "target", QRect(10, 20, 32, 24) };
            }
            bool apply(const QString& target, const QImage& image, const QImage& mask, const QString&, QString*) override {
                if (target != "target" || !mask.isNull() || image.pixelColor(0, 0) != QColor(Qt::blue)) return false;
                ++controlApplications; return true;
            }
            QString appliedLayerId() const override { return "control-map"; }
            bool scaleTarget(const QString& target, QSize size, QString*) override {
                if (target != "target") return false;
                scaledSize = size; ++scales; return true;
            }
            QRect imageBounds(bool) const override { return QRect(0, 0, 32, 24); }
            QJsonArray layers() const override {
                QJsonArray layers { QJsonObject { { "id", "source" }, { "name", "Source" }, { "active", true } } };
                if (controlApplications) layers.append(QJsonObject { { "id", "control-map" }, { "name", "Control map" } });
                return layers;
            }
            bool preview(const QString&, const QImage&, const QImage&, QString*) override { ++previews; return true; }
            QString documentId() const override { return id; }
            QByteArray annotation(const QString& key) const override { return annotations.value(key); }
            void setAnnotation(const QString& key, const QByteArray& value) override { annotations[key] = value; }
            QString restoreTarget(const QRect&, const QImage&) override { return "target"; }
        } host;
        QSettings settings("BaronEdition", "Orchestrion");
        QVariantMap saved;
        for (const auto& key : settings.allKeys()) saved[key] = settings.value(key);
        settings.clear();
        settings.setValue("autoUpdates", false);
        settings.setValue("pollInterval", 1);
        settings.setValue("generation_finished_action", "preview");
        settings.setValue("debug_dump_workflow", true);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        settings.setValue("website", QString("http://127.0.0.1:%1").arg(server.serverPort()));
        QList<QJsonObject> requests;
        int submissions = 0;
        QImage result(32, 24, QImage::Format_ARGB32);
        result.fill(Qt::blue);
        const auto image = BaronPanel::png(result);
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const QByteArray bytes = socket->property("requestBytes").toByteArray() + socket->readAll();
                socket->setProperty("requestBytes", bytes);
                const auto boundary = bytes.indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("replied").toBool()) return;
                int length = 0;
                for (const auto& line : bytes.left(boundary).split('\n'))
                    if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                if (bytes.size() < boundary + 4 + length) return;
                socket->setProperty("replied", true);
                const auto path = bytes.split(' ').value(1);
                QByteArray response;
                if (path.contains("/device/start")) response = R"({"device_code":"test","user_code":"AB12345678","verification_uri":"/connect/krita","expires_in":600,"interval":5})";
                else if (path.contains("/device/poll")) response = R"({"access_token":"ork_krita_test"})";
                else if (path.contains("/account")) response = R"({"coins":42})";
                else if (path.contains("/models")) response = R"({"items":[{"kind":"checkpoint","name":"test.safetensors","title":"Test","family":"sdxl"}]})";
                else if (path.contains("/object_info")) {
                    QVERIFY(bytes.contains("Authorization: Bearer ork_krita_test"));
                    response = R"({"UpscaleModelLoader":{"input":{"required":{"model_name":[["chosen.pth"]]}}}})";
                }
                else if (path.contains("/native/prepare")) {
                    QVERIFY(bytes.contains("Authorization: Bearer ork_krita_test"));
                    requests.append(QJsonDocument::fromJson(bytes.mid(boundary + 4, length)).object());
                    response = R"({"prompt":{"1":{"class_type":"SaveImage","inputs":{}}},"coins":1})";
                } else if (path.contains("/prompt")) {
                    ++submissions;
                    response = "{\"prompt_id\":\"job-" + QByteArray::number(submissions) + "\"}";
                } else if (path.contains("/history/")) {
                    const auto job = QJsonDocument::fromJson(R"({"status":{"status_str":"success"},"outputs":{"1":{"images":[{"filename":"result.png","type":"output"}]}}})").object();
                    response = QJsonDocument(QJsonObject { { QString::fromLatin1(path.mid(path.lastIndexOf('/') + 1)), job } }).toJson();
                } else if (path.contains("/view?")) response = image;
                else if (path.contains("/quote")) response = R"({"coins":1})";
                else if (path.contains("/connection/ws?")) {
                    socket->write("HTTP/1.1 503 Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                } else QFAIL("Unexpected generation request");
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(response.size()) + "\r\nConnection: close\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        {
            BaronPanel panel(&host);
            panel.documentChanged();
            QCoreApplication::processEvents();
            auto client = panel.findChild<OrchestrionClient*>();
            QVERIFY(client->setRoot(QString("http://127.0.0.1:%1").arg(server.serverPort())));
            auto model = panel.findChild<QComboBox*>("checkpointSelect");
            client->signIn();
            QTRY_VERIFY_WITH_TIMEOUT(client->signedIn() && model->count() == 1, 7000);
            auto prompt = panel.findChild<PromptEditor*>("positivePrompt");
            auto negative = panel.findChild<PromptEditor*>("negativePrompt");
            const QString text = QString(4096, 'a') + " <lora:folder/Identity:0.75>";
            prompt->setPlainText(text);
            negative->setPlainText(QString(4096, 'b'));
            auto history = panel.findChild<QListWidget*>("resultHistory");
            auto queue = panel.findChild<JobQueue*>();
            QSignalSpy completed(queue, &JobQueue::completed);
            auto run = panel.findChild<QPushButton*>("generateButton");
            run->click();
            QTRY_COMPARE(requests.size(), 1);
            QCOMPARE(host.captures, 1);
            QCOMPARE(requests[0]["prompt"].toString(), text);
            QCOMPARE(requests[0]["width"].toInt(), 32);
            QCOMPARE(requests[0]["height"].toInt(), 24);
            QVERIFY(!requests[0].contains("image"));
            QVERIFY(requests[0]["style_options"].isObject());
            QVERIFY(requests[0]["style_options"].toObject().isEmpty());
            QCOMPARE(requests[0]["resolution_multiplier"].toDouble(), 1.0);
            QCOMPARE(requests[0]["max_pixel_count"].toInt(), 6);
            QTRY_COMPARE(completed.size(), 1);
            QCOMPARE(host.previews, 1);
            QCOMPARE(history->count(), 2);
            QFile workflow(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs/workflow.json");
            QVERIFY(workflow.open(QIODevice::ReadOnly));
            QCOMPARE(QJsonDocument::fromJson(workflow.readAll()).object()["1"].toObject()["class_type"].toString(), QString("SaveImage"));
            workflow.close();
            workflow.remove();
            host.masked = true;
            run->click();
            QTRY_COMPARE(requests.size(), 2);
            QCOMPARE(host.captures, 2);
            QVERIFY(!QImage::fromData(QByteArray::fromBase64(requests[1]["image"].toString().toLatin1())).isNull());
            QCOMPARE(QImage::fromData(QByteArray::fromBase64(requests[1]["mask"].toString().toLatin1())).pixelColor(0, 0).red(), 127);
            QTRY_COMPARE(completed.size(), 2);
            QCOMPARE(host.previews, 1);
            QCOMPARE(history->count(), 4);
            host.masked = false;
            panel.findChild<QSlider*>("denoiseSlider")->setValue(43);
            run->click();
            QTRY_COMPARE(requests.size(), 3);
            QCOMPARE(host.captures, 3);
            QCOMPARE(requests[2]["mode"].toString(), QString("edit"));
            QCOMPARE(requests[2]["strength"].toDouble(), .43);
            QVERIFY(!requests[2].contains("mask"));
            QVERIFY(!QImage::fromData(QByteArray::fromBase64(requests[2]["image"].toString().toLatin1())).isNull());
            QTRY_COMPARE(completed.size(), 3);
            QCOMPARE(host.previews, 1);
            QCOMPARE(history->count(), 6);
            panel.findChild<QToolButton*>("addControlLayer")->menu()->actions()[8]->trigger();
            auto fromImage = panel.findChild<QToolButton*>("controlFromImage");
            QVERIFY(fromImage);
            fromImage->click();
            QVERIFY(!fromImage->isEnabled());
            QTRY_COMPARE(requests.size(), 4);
            QCOMPARE(requests[3]["operation"].toString(), QString("control_image"));
            QCOMPARE(requests[3]["control_mode"].toString(), QString("depth"));
            QTRY_COMPARE(completed.size(), 4);
            QCOMPARE(host.controlApplications, 1);
            QCOMPARE(history->count(), 6);
            QCOMPARE(host.previews, 1);
            QVERIFY(fromImage->isEnabled());
            QCOMPARE(panel.findChild<QComboBox*>("controlLayer")->currentData().toString(), QString("control-map"));
            panel.findChild<GuidancePanel*>()->restoreState({});
            auto resolution = panel.findChild<QSlider*>("resolutionSlider");
            QCOMPARE(resolution->pageStep(), 1);
            resolution->setValue(5);
            run->click();
            QTRY_COMPARE(requests.size(), 5);
            QCOMPARE(requests[4]["resolution_multiplier"].toDouble(), .5);
            QTRY_COMPARE(completed.size(), 5);
            settings.setValue("performanceResolutionMultiplier", .8);
            resolution->setValue(10);
            host.masked = true;
            for (auto action : panel.findChild<QToolButton*>("workspaceSelect")->menu()->actions())
                if (action->data() == "upscale") action->trigger();
            QVERIFY(prompt->isHidden());
            QVERIFY(!panel.findChild<UpscaleWidget*>()->isHidden());
            QVERIFY(!resolution->isHidden());
            QVERIFY(panel.findChild<QSlider*>("batchSlider")->isHidden());
            run->click();
            QTRY_COMPARE(requests.size(), 6);
            QVERIFY(!host.lastCaptureIncludedMask);
            QVERIFY(!requests[5].contains("mask"));
            QCOMPARE(requests[5]["resolution_multiplier"].toDouble(), .8);
            QCOMPARE(requests[5]["batch"].toInt(), 1);
            QVERIFY(requests[5]["upscale_options"].toObject()["use_diffusion"].toBool());
            QCOMPARE(requests[5]["upscale_options"].toObject()["model"].toString(), QString("chosen.pth"));
            QCOMPARE(host.scales, 1);
            QCOMPARE(host.scaledSize, QSize(64, 48));
            QTRY_COMPARE(completed.size(), 6);
            QTRY_COMPARE(host.controlApplications, 2); // Upscale is automatically applied, like the original.
            workflow.remove();
        }
        settings.clear();
        for (auto it = saved.cbegin(); it != saved.cend(); ++it) settings.setValue(it.key(), it.value());
    }
    void interfaceSettingsPersistAndAffectThePanel() {
        QSettings settings("BaronEdition", "Orchestrion");
        const auto previousLanguage = settings.value("language");
        settings.setValue("show_negative_prompt", true);
        settings.setValue("prompt_line_count", 14);
        BaronPanel panel(nullptr);
        auto page = panel.findChild<InterfaceSettings*>();
        QVERIFY(page);
        auto lines = page->findChild<QSpinBox*>("prompt_line_count");
        QCOMPARE(lines->value(), 14);
        lines->setValue(5);
        QCOMPARE(panel.findChild<PromptEditor*>("positivePrompt")->height(), panel.fontMetrics().lineSpacing() * 5 + 10);
        panel.findChild<QToolButton*>("addRegion")->click();
        auto region = panel.findChild<PromptEditor*>("regionPrompt");
        QVERIFY(region);
        QCOMPARE(region->height(), region->fontMetrics().lineSpacing() * 5 + 10);
        page->findChild<ToggleSwitch*>("show_negative_prompt")->click();
        QVERIFY(panel.findChild<PromptEditor*>("negativePrompt")->isHidden());
        page->findChild<ToggleSwitch*>("show_steps")->setChecked(true);
        auto denoise = panel.findChild<QSlider*>("denoiseSlider");
        denoise->setValue(50);
        bool displayed = false;
        for (auto spin : panel.findChildren<QSpinBox*>()) displayed |= spin->suffix().contains("/");
        QVERIFY(displayed);
        auto format = page->findChild<QComboBox*>("save_image_format");
        format->setCurrentIndex(format->findData("webp_lossless"));
        QVERIFY(!page->findChild<ToggleSwitch*>("save_image_metadata")->isEnabled());
        format->setCurrentIndex(format->findData("png"));
        QVERIFY(page->findChild<ToggleSwitch*>("save_image_metadata")->isEnabled());
        QVERIFY(!page->findChild<QComboBox*>("apply_behavior_live")->isEnabled());
        QVERIFY(!page->findChild<QComboBox*>("prompt_translation")->isEnabled());
        auto datasets = page->findChild<QWidget*>("tagDatasetList")->findChildren<QCheckBox*>();
        QCOMPARE(datasets.size(), 4);
        InterfaceSettings reopened;
        QCOMPARE(reopened.findChild<QSpinBox*>("prompt_line_count")->value(), 5);
        QCOMPARE(reopened.findChild<QComboBox*>("save_image_format")->currentData().toString(), QString("png"));
        page->reset();
        lines = page->findChild<QSpinBox*>("prompt_line_count");
        QCOMPARE(lines->value(), 2);
        QCOMPARE(panel.findChild<PromptEditor*>("positivePrompt")->height(), panel.fontMetrics().lineSpacing() * 2 + 10);
        QVERIFY(panel.findChild<PromptEditor*>("negativePrompt")->isHidden());
        settings.setValue("language", previousLanguage);
        settings.remove("show_negative_prompt");
        settings.remove("prompt_line_count");
        settings.remove("promptHeight");
        settings.remove("show_steps");
        settings.remove("save_image_format");
        settings.setValue("tagFiles", QStringList { "Danbooru", "e621" });
    }
    void finishedActionControlsPreviewAndApplication() {
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QByteArray> annotations;
            int previews = 0, applications = 0;
            QJsonObject options;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return false; }
            bool applyConfigured(const QString&, const QImage&, const QImage&, const QString&, const QJsonObject& value, QString*) override {
                options = value;
                ++applications;
                return true;
            }
            bool preview(const QString&, const QImage&, const QImage&, QString*) override { ++previews; return true; }
            QString documentId() const override { return id; }
            QByteArray annotation(const QString& key) const override { return annotations.value(key); }
            void setAnnotation(const QString& key, const QByteArray& value) override { annotations[key] = value; }
            QString restoreTarget(const QRect&, const QImage&) override { return "original"; }
        } host;
        QSettings settings("BaronEdition", "Orchestrion");
        settings.setValue("generation_finished_action", "none");
        BaronPanel panel(&host);
        panel.documentChanged();
        auto client = panel.findChild<OrchestrionClient*>();
        client->setRoot("http://127.0.0.1:9");
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::green);
        auto queue = panel.findChild<JobQueue*>();
        const auto deliver = [&] {
            const auto id = queue->start({}, {
                { "document", host.id }, { "mode", "generate" }, { "target", "original" },
                { "bounds", QJsonArray { 0, 0, 8, 8 } },
                { "settings", QJsonObject { { "prompt", "test" } } } }, {}, false);
            const auto job = queue->job(id);
            job->images[0] = image;
            job->result = HistoryStore::prepare({image});
            job->state = JobQueue::finished;
            queue->completed(id);
        };
        deliver();
        QCOMPARE(host.previews, 0);
        QCOMPARE(host.applications, 0);
        settings.setValue("generation_finished_action", "preview");
        deliver();
        QCOMPARE(host.previews, 1);
        deliver();
        QCOMPARE(host.previews, 1);
        settings.setValue("generation_finished_action", "apply");
        settings.setValue("apply_behavior", "layer_active");
        settings.setValue("apply_region_behavior", "no_hide");
        deliver();
        QCOMPARE(host.applications, 1);
        QCOMPARE(host.options["apply"].toString(), QString("layer_active"));
        QCOMPARE(host.options["region_apply"].toString(), QString("no_hide"));
        settings.remove("generation_finished_action");
        settings.remove("apply_behavior");
        settings.remove("apply_region_behavior");
    }
    void customTagsAcceptUppercaseExtension() {
        const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags");
        QVERIFY(QDir().mkpath(directory.path()));
        QFile file(directory.filePath("Baron-Case-Test.CSV"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("unusual_font,0,99\n");
        file.close();
        auto model = TagModel::shared();
        model->reload({ "Baron-Case-Test" }, true);
        const auto count = model->rowCount();
        const auto text = model->data(model->index(0)).toString();
        file.remove();
        model->reload({ "Danbooru", "e621" }, true);
        QCOMPARE(count, 1);
        QCOMPARE(text, QString("unusual font"));
    }
    void modelCatalogFoldersFiltersAndFavorites() {
        QSettings("BaronEdition", "Orchestrion").remove("modelFavorites");
        ModelCatalog catalog;
        const QJsonArray models{
            QJsonObject{{"name","characters/hero.safetensors"},{"title","Hero"},{"kind","lora"},{"family","Qwen"},{"triggerWords",QJsonArray{"brave warrior"}}},
            QJsonObject{{"name","characters/poses/sitting.safetensors"},{"title","Sitting"},{"kind","lora"},{"family","Qwen"}},
            QJsonObject{{"name","characters-extra/other.safetensors"},{"title","Other"},{"kind","checkpoint"},{"family","SDXL"}},
            QJsonObject{{"name","root.safetensors"},{"title","Root"},{"kind","checkpoint"},{"family","Qwen"}}};
        catalog.setModels(models);
        auto visible=[&]{int count=0;for(int i=0;i<catalog.gallery()->count();++i)count+=!catalog.gallery()->item(i)->isHidden();return count;};
        auto tree=catalog.findChild<QTreeWidget*>("modelFolders");
        QVERIFY(tree);
        for(QTreeWidgetItemIterator it(tree);*it;++it)
            if((*it)->data(0,Qt::UserRole)=="characters")tree->setCurrentItem(*it);
        QCOMPARE(visible(),2);
        catalog.search()->setText("warrior characters");
        QCOMPARE(visible(),1);
        QListWidgetItem* hero=nullptr;
        for(int i=0;i<catalog.gallery()->count();++i)if(!catalog.gallery()->item(i)->isHidden())hero=catalog.gallery()->item(i);
        QVERIFY(hero);
        catalog.gallery()->setCurrentItem(hero);
        catalog.findChild<QToolButton*>("modelFavoriteButton")->click();
        QVERIFY(hero->data(Qt::UserRole+1).toBool());
        catalog.search()->clear();
        catalog.findChild<QCheckBox*>("modelFavoritesOnly")->setChecked(true);
        QCOMPARE(visible(),1);
        catalog.findChild<QComboBox*>("modelFamilyFilter")->setCurrentIndex(2);
        QCOMPARE(visible(),0);
        ModelCatalog restored;
        restored.setModels(models);
        restored.findChild<QCheckBox*>("modelFavoritesOnly")->setChecked(true);
        int selected=0;for(int i=0;i<restored.gallery()->count();++i)selected+=!restored.gallery()->item(i)->isHidden();
        QCOMPARE(selected,1);
        QCOMPARE(ModelCatalog::folder("folder\\sub\\name.safetensors"),QString("folder/sub"));
    }
    void loraInsertionUsesPluginGrammar() {
        PromptEditor editor;
        QJsonObject lora{{"name","styles\\Fantasy.safetensors"},{"triggerWords",QJsonArray{"enchanted"}}};
        editor.setPlainText("portrait");
        editor.moveCursor(QTextCursor::End);
        editor.insertLora(lora,.4);
        QCOMPARE(editor.toPlainText(),QString("portrait <lora:styles/Fantasy:0.4> enchanted"));
        editor.insertLora(lora,.7);
        QCOMPARE(editor.toPlainText(),QString("portrait <lora:styles/Fantasy:0.7> enchanted"));
        PromptEditor another;
        another.setLoraCatalog({lora});
        auto completion=another.findChild<QCompleter*>();
        QVERIFY(completion);
    }
    void tagsFromOriginalDatasetsAndImeCompletion() {
        BaronLocalization::install();
        auto tags=TagModel::shared();
        tags->reload({"Danbooru","e621"});
        QVERIFY(tags->rowCount()>40000);
        QSet<QString> unique;
        for(int i=0;i<tags->rowCount();++i){const auto value=tags->index(i,0).data().toString();QVERIFY(!value.contains('_'));QVERIFY(!unique.contains(value));unique.insert(value);}
        QVERIFY(unique.contains("blue eyes"));
        PromptEditor editor;
        editor.resize(600,180);editor.show();editor.setFocus();
        QCoreApplication::processEvents();
        editor.setPlainText("portrait, blue ey");editor.moveCursor(QTextCursor::End);
        auto completion=editor.findChild<QCompleter*>();
        QTRY_VERIFY(completion->popup()->isVisible());
        QCOMPARE(completion->completionPrefix(),QString("blue ey"));
        QVERIFY(QMetaObject::invokeMethod(completion,"activated",Q_ARG(QString,"blue eyes")));
        QCOMPARE(editor.toPlainText(),QString("portrait, blue eyes"));
        editor.setPlainText("char");editor.moveCursor(QTextCursor::End);
        QTRY_COMPARE(completion->completionPrefix(),QString("char"));
        QVERIFY(QMetaObject::invokeMethod(completion,"activated",Q_ARG(QString,"character (series)")));
        QCOMPARE(editor.toPlainText(),QString("character \\(series\\)"));
        tags->reload({});QCOMPARE(tags->rowCount(),0);
        editor.reloadTags();
    }
    void embeddedCompletionKeepsPromptFocusWhileTypingAndSelecting() {
        QWidget host;
        host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor.hasFocus());
        editor.setPlainText("portrait, blue ey"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        auto list = host.findChild<QListView*>("promptCompletionList");
        QTRY_VERIFY(list->isVisible());
        QVERIFY(!list->isWindow());
        QCOMPARE(list->focusPolicy(), Qt::NoFocus);
        QCOMPARE(QApplication::focusWidget(), &editor);
        QVERIFY(!QApplication::activePopupWidget());
        QTest::keyClicks(&editor, "e");
        QCOMPARE(editor.toPlainText(), QString("portrait, blue eye"));
        QTRY_VERIFY(list->model()->rowCount() > 0);
        QCOMPARE(QApplication::focusWidget(), &editor);
        const auto index = list->model()->index(0, 0);
        const auto completion = index.data(Qt::EditRole).toString();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
            list->visualRect(index).center());
        QCOMPARE(editor.toPlainText(), QString("portrait, ") + completion);
        QVERIFY(!list->isVisible());
        QCOMPARE(QApplication::focusWidget(), &editor);
        QTest::keyClicks(&editor, ", more");
        QVERIFY(editor.toPlainText().endsWith(", more"));
    }
    void embeddedCompletionRespectsImePreeditAndDismissal() {
        QWidget host;
        host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor.hasFocus());
        editor.setPlainText("blue ey"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        auto list = host.findChild<QListView*>("promptCompletionList");
        QTRY_VERIFY(list->isVisible());
        QInputMethodEvent preedit("es", {});
        QApplication::sendEvent(&editor, &preedit);
        QVERIFY(!list->isVisible());
        QCOMPARE(editor.toPlainText(), QString("blue ey"));
        QCOMPARE(QApplication::focusWidget(), &editor);
        QInputMethodEvent commit;
        commit.setCommitString("es");
        QApplication::sendEvent(&editor, &commit);
        QCOMPARE(editor.toPlainText(), QString("blue eyes"));
        QTRY_VERIFY(list->isVisible());
        QTest::keyClick(&editor, Qt::Key_Escape);
        QVERIFY(!list->isVisible());
        QCOMPARE(QApplication::focusWidget(), &editor);
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QTRY_VERIFY(list->isVisible());
        QTest::mouseClick(&host, Qt::LeftButton, Qt::NoModifier, QPoint(20, 330));
        QVERIFY(!list->isVisible());
        QCOMPARE(editor.toPlainText(), QString("blue eye"));
    }
    void embeddedCompletionStaysHiddenWhileSelectingText() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor.hasFocus());
        editor.setPlainText("portrait, blue ey"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        auto list = host.findChild<QListView*>("promptCompletionList");
        QTRY_VERIFY(list->isVisible());
        QInputMethodEvent select({}, {{ QInputMethodEvent::Selection, 10, 6, {} }});
        QApplication::sendEvent(&editor, &select);
        QVERIFY(editor.textCursor().hasSelection());
        QTest::qWait(100);
        QVERIFY(!list->isVisible());
        editor.selectAll();
        QTest::qWait(100);
        QVERIFY(!list->isVisible());
        QTest::keyClicks(&editor, "blue ey");
        QTRY_VERIFY(list->isVisible());
        QCOMPARE(editor.toPlainText(), QString("blue ey"));
    }
    void promptRepeatedImeSelectionIsIdempotent() {
        QWidget host; host.resize(300, 200);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 220, 70);
        host.show(); host.activateWindow(); editor.setFocus();
        editor.replacePromptText(QString("blue eyes, ").repeated(150));
        QSignalSpy selection(&editor, &QPlainTextEdit::selectionChanged);
        const int revision = editor.document()->revision();
        for (int i = 0; i < 100; ++i) {
            QInputMethodEvent select({}, {{ QInputMethodEvent::Selection, 20, -10, {} }});
            QApplication::sendEvent(&editor, &select);
        }
        QCOMPARE(editor.textCursor().anchor(), 20);
        QCOMPARE(editor.textCursor().position(), 10);
        QCOMPARE(editor.document()->revision(), revision);
        QCOMPARE(selection.size(), 1);
    }
    void embeddedLoraCompletionKeepsGrammarAndFocus() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        editor.setLoraCatalog({QJsonObject{{"kind", "lora"}, {"name", "styles/Fantasy.safetensors"},
            {"triggerWords", QJsonArray{"enchanted"}}}});
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        QVERIFY(editor.hasFocus());
        editor.setPlainText("portrait <lora:styles/Fan"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        auto list = host.findChild<QListView*>("promptCompletionList");
        QTRY_VERIFY(list->isVisible());
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.toPlainText(), QString("portrait <lora:styles/Fantasy:1> enchanted"));
        QCOMPARE(QApplication::focusWidget(), &editor);
        QVERIFY(!list->isVisible());
    }
    void promptClipboardPastePreservesTextSelectionAndUndo() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        editor.setPlainText("portrait, blue ey"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        const QString pasted = QString::fromUtf8("Привет 🌙\n<lora:styles/Fantasy:0.7> blue eyes");
        auto mime = new QMimeData;
        mime->setText(pasted);
        mime->setHtml("<b>different HTML</b>");
        QApplication::clipboard()->setMimeData(mime);
        editor.selectAll();
        QTest::keyClick(&editor, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(editor.toPlainText(), QString("portrait, blue ey"));
        QTRY_COMPARE(editor.toPlainText(), pasted);
        QTest::qWait(100);
        QVERIFY(!host.findChild<QListView*>("promptCompletionList")->isVisible());
        QCOMPARE(QApplication::focusWidget(), &editor);
        editor.undo();
        QCOMPARE(editor.toPlainText(), QString("portrait, blue ey"));
        editor.redo();
        QCOMPARE(editor.toPlainText(), pasted);
        QApplication::clipboard()->clear();
    }
    void exactPromptSelfCopyPasteAndSourceDestruction() {
        class InspectablePrompt : public PromptEditor {
        public:
            using PromptEditor::PromptEditor;
            using PromptEditor::createMimeDataFromSelection;
        };
        auto fixturePath = qEnvironmentVariable("BARON_SELF_COPY_FIXTURE");
        if (fixturePath.isEmpty()) fixturePath = QFINDTESTDATA("fixtures/prompt-self-copy.txt");
        QFile fixture(fixturePath);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        QString text = QString::fromUtf8(fixture.readAll());
        if (text.endsWith('\n')) text.chop(1);
        QVERIFY(text.contains(QChar(0x200b)));
        QWidget host; host.resize(420, 360);
        auto editor = new InspectablePrompt(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor->setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor->setFocus();
        QCoreApplication::processEvents();
        editor->setPlainText(text);
        editor->selectAll();
        QScopedPointer<QMimeData> copied(editor->createMimeDataFromSelection());
        qInfo() << "Internal prompt clipboard formats:" << copied->formats();
        QCOMPARE(copied->formats(), QStringList{"text/plain"});
        QCOMPARE(copied->text(), text);
        for (int repeat = 0; repeat < 10; ++repeat) {
            editor->selectAll();
            QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
            QCOMPARE(QApplication::clipboard()->text(), text);
            QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
            QTest::qWait(80);
            QCOMPARE(editor->toPlainText(), text);
            QCOMPARE(QApplication::focusWidget(), editor);
        }
        editor->selectAll();
        editor->copy();
        delete editor;
        PromptEditor destination(&host, true, PromptEditor::CompletionDisplay::Embedded);
        destination.setGeometry(10, 10, 400, 180); destination.show(); destination.setFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(&destination, Qt::Key_V, Qt::ControlModifier);
        QTRY_COMPARE(destination.toPlainText(), text);
        destination.undo(); QCOMPARE(destination.toPlainText(), QString());
        destination.redo(); QCOMPARE(destination.toPlainText(), text);
        QApplication::clipboard()->clear();
    }
    void promptBulkImeCommitPreservesUnicodeAndLoraText() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        editor.setPlainText("portrait, blue ey"); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        QInputMethodEvent preedit("es", {});
        QApplication::sendEvent(&editor, &preedit);
        const QString pasted = QString::fromUtf8("Привет 🌙\n<lora:styles/Fantasy:0.7> blue eyes");
        QInputMethodEvent commit;
        commit.setCommitString(pasted);
        QApplication::sendEvent(&editor, &commit);
        QCOMPARE(editor.toPlainText(), QString("portrait, blue ey") + pasted);
        QTest::qWait(100);
        QVERIFY(!host.findChild<QListView*>("promptCompletionList")->isVisible());
        QCOMPARE(QApplication::focusWidget(), &editor);
        editor.undo();
        QCOMPARE(editor.toPlainText(), QString("portrait, blue ey"));
    }
    void promptImeSelectionAfterLongPreeditAndCommit_data() {
        QTest::addColumn<bool>("separateSelection");
        QTest::addColumn<bool>("negative");
        QTest::newRow("combined-positive") << false << false;
        QTest::newRow("combined-negative") << false << true;
        QTest::newRow("android-separate-positive") << true << false;
        QTest::newRow("android-separate-negative") << true << true;
    }
    void promptImeSelectionAfterLongPreeditAndCommit() {
        QFETCH(bool, separateSelection);
        QFETCH(bool, negative);
        QWidget host; host.resize(260, 180);
        PromptEditor editor(&host, negative, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 220, 70);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        QFile fixture(QFINDTESTDATA("fixtures/prompt-self-copy.txt"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(fixture.readAll()).trimmed();
        editor.replacePromptText(text); editor.selectAll();
        QInputMethodEvent preedit(text, {});
        QApplication::sendEvent(&editor, &preedit);
        const QList<QInputMethodEvent::Attribute> selections = {{ QInputMethodEvent::Selection, text.size(), 0, {} }};
        QInputMethodEvent commit({}, separateSelection ? QList<QInputMethodEvent::Attribute>{} : selections);
        commit.setCommitString(text);
        QApplication::sendEvent(&editor, &commit);
        if (separateSelection) {
            QInputMethodEvent selection({}, selections);
            QApplication::sendEvent(&editor, &selection);
        }
        QCOMPARE(editor.toPlainText(), text);
        QCOMPARE(editor.textCursor().position(), text.size());
        QCOMPARE(editor.textCursor().anchor(), text.size());
        QVERIFY(editor.textCursor().block().layout()->preeditAreaText().isEmpty());
        editor.undo(); QCOMPARE(editor.toPlainText(), QString());
        editor.redo(); QCOMPARE(editor.toPlainText(), text);
    }
    void promptImeSelectionCancelsPreeditAndPreservesDirection() {
        QWidget host; host.resize(260, 180);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 220, 70);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        const QString text = QString("blue eyes, ").repeated(150);
        editor.replacePromptText(text); editor.moveCursor(QTextCursor::End);
        QInputMethodEvent preedit("тест", {{ QInputMethodEvent::Cursor, 4, 1, {} }});
        QApplication::sendEvent(&editor, &preedit);
        QInputMethodEvent select({}, {{ QInputMethodEvent::Selection, 20, -10, {} }});
        QApplication::sendEvent(&editor, &select);
        QCOMPARE(editor.toPlainText(), text);
        QCOMPARE(editor.textCursor().anchor(), 20);
        QCOMPARE(editor.textCursor().position(), 10);
        QVERIFY(editor.textCursor().block().layout()->preeditAreaText().isEmpty());
        QInputMethodEvent stale({}, {{ QInputMethodEvent::Selection, -100, 100000, {} }});
        QApplication::sendEvent(&editor, &stale);
        QCOMPARE(editor.textCursor().anchor(), 0);
        QCOMPARE(editor.textCursor().position(), text.size());
        QCOMPARE(editor.toPlainText(), text);
    }
    void promptCopyCutKeepsPlainTextAndParagraphsWithSuggestions() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        const QString text = QString::fromUtf8("<lora:styles/Fantasy:0.4>\nПривет ​мир\nblue ey");
        editor.setPlainText(text); editor.moveCursor(QTextCursor::End);
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList"));
        QTRY_VERIFY(host.findChild<QListView*>("promptCompletionList")->isVisible());
        editor.selectAll();
        QTest::keyClick(&editor, Qt::Key_X, Qt::ControlModifier);
        QCOMPARE(editor.toPlainText(), QString());
        QCOMPARE(QApplication::clipboard()->text(), text);
        QVERIFY(!QApplication::clipboard()->mimeData()->hasHtml());
        QTest::keyClick(&editor, Qt::Key_V, Qt::ControlModifier);
        QTRY_COMPARE(editor.toPlainText(), text);
        QCOMPARE(QApplication::focusWidget(), &editor);
        editor.undo(); QCOMPARE(editor.toPlainText(), QString());
        editor.undo(); QCOMPARE(editor.toPlainText(), text);
        QApplication::clipboard()->clear();
    }
    void promptContextMenuPasteAndEmptyClipboardAreSafe() {
        QWidget host; host.resize(420, 360);
        PromptEditor editor(&host, true, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); editor.setFocus();
        QCoreApplication::processEvents();
        editor.setPlainText("original "); editor.moveCursor(QTextCursor::End);
        QApplication::clipboard()->setText("pasted");
        QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(20, 20), editor.viewport()->mapToGlobal(QPoint(20, 20)));
        QApplication::sendEvent(editor.viewport(), &event);
        auto menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        QVERIFY(menu);
        auto paste = menu->findChild<QAction*>("edit-paste");
        QVERIFY(paste); QVERIFY(paste->isEnabled());
        QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(paste).center());
        QTRY_COMPARE(editor.toPlainText(), QString("original pasted"));
        QCOMPARE(QApplication::focusWidget(), &editor);
        QApplication::clipboard()->clear();
        QTest::keyClick(&editor, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::processEvents();
        QCOMPARE(editor.toPlainText(), QString("original pasted"));
        editor.setReadOnly(true);
        QApplication::clipboard()->setText("blocked");
        QTest::keyClick(&editor, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::processEvents();
        QCOMPARE(editor.toPlainText(), QString("original pasted"));
        QApplication::clipboard()->clear();
    }
    void promptQueuedPasteDoesNotEditAfterFocusMovesOrDestruction() {
        QWidget host; host.resize(420, 360);
        auto editor = new PromptEditor(&host, false, PromptEditor::CompletionDisplay::Embedded);
        editor->setGeometry(10, 10, 400, 180);
        QLineEdit other(&host); other.setGeometry(10, 220, 400, 40);
        host.show(); host.activateWindow(); editor->setFocus();
        QCoreApplication::processEvents();
        editor->setPlainText("original");
        QApplication::clipboard()->setText("pasted");
        QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
        other.setFocus();
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QString("original"));
        editor->setFocus();
        QTest::keyClick(editor, Qt::Key_V, Qt::ControlModifier);
        delete editor;
        QCoreApplication::processEvents();
        QCOMPARE(other.text(), QString());
        QApplication::clipboard()->clear();
    }
    void embeddedCompletionFollowsReparentedEditor() {
        QWidget first, second;
        first.resize(420, 360); second.resize(420, 360);
        auto editor = new PromptEditor(&first, false, PromptEditor::CompletionDisplay::Embedded);
        editor->setGeometry(10, 10, 400, 180);
        first.show(); first.activateWindow(); editor->setFocus();
        QCoreApplication::processEvents();
        editor->setPlainText("blue ey"); editor->moveCursor(QTextCursor::End);
        QTRY_VERIFY(first.findChild<QListView*>("promptCompletionList"));
        editor->setParent(&second);
        second.show(); editor->show(); second.activateWindow(); editor->setFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(editor, Qt::Key_E);
        QTRY_VERIFY(second.findChild<QListView*>("promptCompletionList"));
        QVERIFY(!first.findChild<QListView*>("promptCompletionList"));
        QCOMPARE(editor->toPlainText(), QString("blue eye"));
    }
    void generationPanelFitsNarrowDock() {
        BaronPanel panel(nullptr);
        panel.resize(320, 900); panel.show();
        QCoreApplication::processEvents();
        QCOMPARE(panel.width(), 320);
        for (const auto name : {"styleSelect", "positivePrompt", "denoiseSlider", "generateButton",
                 "generationSettings", "modelGalleryButton", "resultHistory"}) {
            auto widget = panel.findChild<QWidget*>(name);
            QVERIFY2(widget, name);
            const auto bounds = QRect(widget->mapTo(&panel, QPoint()), widget->size());
            QVERIFY2(bounds.left() >= 0 && bounds.right() < panel.width(), name);
        }
    }
    void pluginIconsRespectTheme() {
        BaronLocalization::install();
        QWidget widget;
        for (const auto& color : { QColor(40, 40, 40), QColor(240, 240, 240) }) {
            auto palette = widget.palette();
            palette.setColor(QPalette::Window, color);
            widget.setPalette(palette);
            for (const auto& name :
                  { "workspace-generation", "settings", "control-add", "region-add", "reload-preset", "document-open" }) {
                const auto icon = PluginUi::icon(name, &widget);
                QVERIFY(!icon.pixmap(24, 24).isNull());
            }
        }
    }
    void initTestCase() {
        QVERIFY(settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
        QStandardPaths::setTestModeEnabled(true);
    }
    void startupWithPreviousSettings_data() {
        QSettings("BaronEdition", "Orchestrion").remove("promptBanks");
        QTest::addColumn<QString>("mode");
        QTest::addColumn<int>("strength");
        for (const auto& mode : { "generate", "edit", "upscale", "background" })
            for (int strength : { 1, 43, 100 })
                QTest::newRow(qPrintable(QString("%1-%2").arg(mode).arg(strength)))
                    << QString(mode) << strength;
    }
    void startupWithPreviousSettings() {
        QFETCH(QString, mode);
        QFETCH(int, strength);
        QSettings settings("BaronEdition", "Orchestrion");
        const auto previous = settings.allKeys();
        QVariantMap saved;
        for (const auto& key : previous)
            saved[key] = settings.value(key);
        settings.clear();
        settings.setValue("mode", mode);
        settings.setValue("strength", strength);
        settings.setValue("prompt", "Existing prompt <lora:test:1>");
        settings.setValue("negative", "Existing negative prompt");
        settings.setValue("encryptedLogin", "invalid-test-data");
        settings.setValue("autoUpdates", false);
        {
            QDockWidget dock;
            auto panel = new BaronPanel(nullptr, &dock);
            dock.setWidget(panel);
            dock.resize(456, 900);
            dock.show();
            QTest::qWait(5);
            QCOMPARE(panel->findChild<QSlider*>("denoiseSlider")->value(), strength);
            QCOMPARE(panel->findChild<QPlainTextEdit*>("positivePrompt")->toPlainText(),
                QString("Existing prompt <lora:test:1>"));
        }
        settings.clear();
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            settings.setValue(it.key(), it.value());
    }
    void stoppedJobsDoNotConsumeQueueCapacityAndLateRepliesDoNotReviveThem() {
        class Client : public OrchestrionClient {
        public:
            using OrchestrionClient::OrchestrionClient;
            int preparations = 0, cancellations = 0;
            void prepare(const QJsonObject&) override { ++preparations; }
            void submit(const QJsonObject&, bool) override { }
            void cancelJob() override { ++cancellations; }
        };
        OrchestrionClient connection;
        QList<Client*> clients;
        JobQueue queue(&connection, nullptr, [&](QObject* parent) {
            auto client = new Client(parent); clients.append(client); return client;
        });
        for (int i = 0; i < 8; ++i) {
            const auto id = queue.start({{"prompt", "old"}}, {}, {}, false);
            QVERIFY(!id.isEmpty());
            clients.last()->error("network interrupted");
            QCOMPARE(queue.job(id)->state, JobQueue::failed);
        }
        QCOMPARE(queue.activeCount(), 0);
        const auto cancelledId = queue.start({{"prompt", "cancel me"}}, {}, {}, false);
        auto cancelledClient = clients.last();
        queue.cancel(cancelledId);
        QCOMPARE(queue.activeCount(), 0);
        QCOMPARE(queue.job(cancelledId)->state, JobQueue::cancelled);
        QCOMPARE(cancelledClient->cancellations, 1);
        cancelledClient->submitted("late-ack");
        cancelledClient->progressChanged(.9);
        QCOMPARE(queue.job(cancelledId)->state, JobQueue::cancelled);
        for (int i = 0; i < JobQueue::max_jobs; ++i)
            QVERIFY(!queue.start({{"prompt", "new"}}, {}, {}, false).isEmpty());
        QCOMPARE(queue.activeCount(), JobQueue::max_jobs);
        QVERIFY(queue.start({{"prompt", "overflow"}}, {}, {}, false).isEmpty());
        int preparations = 0;
        for (int i = 9; i < clients.size(); ++i) preparations += clients[i]->preparations;
        QCOMPARE(preparations, 4);
        auto waiting = clients.last();
        QString id;
        for (const auto& job : queue.jobs()) if (job->client == waiting) id = job->id;
        queue.cancel(id);
        QCOMPARE(waiting->cancellations, 0);
        QCOMPARE(queue.activeCount(), JobQueue::max_jobs - 1);
        QVERIFY(!queue.start({{"prompt", "replacement"}}, {}, {}, false).isEmpty());
        clients[9]->prepared({{"prompt", QJsonObject{{"node", 1}}}, {"coins", 1}});
        clients[9]->submitted("first-new");
        QCOMPARE(clients[13]->preparations, 1);
        cancelledClient->error("cancel endpoint unavailable");
        QVERIFY(!queue.job(cancelledId)->cancelPending);
        QCOMPARE(queue.activeCount(), JobQueue::max_jobs);
        queue.cancel(cancelledId);
        QCOMPARE(cancelledClient->cancellations, 2);
        cancelledClient->cancelled();
        QVERIFY(!queue.job(cancelledId)->cancelPending);
    }
    void cancelledSubmissionStillCancelsItsLateAcceptedPrompt() {
        PreviewNetwork network;
        int posts = 0, deletes = 0;
        bool targetedDelete = false;
        network.respond = [&](auto op, const QNetworkRequest& request, const QByteArray& body) {
            const auto path = request.url().path();
            if (path.endsWith("/prepare")) return PreviewResponse{200, {}, R"({"prompt":{"1":{}},"coins":1})"};
            if (path.endsWith("/prompt")) { ++posts; return PreviewResponse{200, {}, R"({"prompt_id":"late-own-job"})", 100}; }
            if (path.endsWith("/queue") && op == QNetworkAccessManager::PostOperation) {
                const auto data = QJsonDocument::fromJson(body).object();
                targetedDelete = data["delete"].toArray() == QJsonArray{"late-own-job"};
                ++deletes;
            }
            return PreviewResponse{200, {}, R"({"queue_running":[],"queue_pending":[]})"};
        };
        OrchestrionClient connection;
        QVERIFY(connection.setRoot("http://127.0.0.1:9")); connection.setAccessToken("test");
        JobQueue queue(&connection, nullptr, [&](QObject* parent) { return new OrchestrionClient(parent, &network); });
        const auto id = queue.start({{"prompt", "test"}}, {}, {}, false);
        QTRY_COMPARE(queue.job(id)->state, JobQueue::submitting);
        queue.cancel(id);
        QCOMPARE(queue.activeCount(), 0);
        QTRY_VERIFY(!queue.job(id)->cancelPending);
        QCOMPARE(queue.job(id)->state, JobQueue::cancelled);
        QCOMPARE(posts, 1); QCOMPARE(deletes, 1);
        QVERIFY(targetedDelete);
    }
    void queuedCancellationHandlesTaskStartingDuringDelete() {
        PreviewNetwork network;
        int reads = 0, interrupts = 0, deletes = 0;
        bool targetedInterrupt = false;
        network.respond = [&](auto op, const QNetworkRequest& request, const QByteArray& body) {
            if (request.url().path().endsWith("/interrupt")) {
                ++interrupts;
                targetedInterrupt = QJsonDocument::fromJson(body).object()["prompt_id"] == "own-job";
            } else if (op == QNetworkAccessManager::PostOperation) ++deletes;
            else if (++reads == 2)
                return PreviewResponse{200, {}, R"({"queue_running":[[0,"own-job"]],"queue_pending":[]})"};
            return PreviewResponse{200, {}, R"({"queue_running":[],"queue_pending":[]})"};
        };
        OrchestrionClient client(nullptr, &network);
        QVERIFY(client.setRoot("http://127.0.0.1:9")); client.setAccessToken("test");
        client.resume("own-job");
        QSignalSpy cancelled(&client, &OrchestrionClient::cancelled);
        client.cancelJob();
        QTRY_COMPARE(cancelled.size(), 1);
        QCOMPARE(deletes, 1); QCOMPARE(interrupts, 1); QVERIFY(targetedInterrupt);
    }
    void missingServerJobsStopPollingWithoutSubmittingAgain() {
        PreviewNetwork network;
        int posts = 0;
        network.respond = [&](auto op, const QNetworkRequest&, const QByteArray&) {
            posts += op == QNetworkAccessManager::PostOperation;
            return PreviewResponse{200, {}, "{}"};
        };
        QSettings settings("BaronEdition", "Orchestrion");
        const auto previous = settings.value("pollInterval"); settings.setValue("pollInterval", 1);
        OrchestrionClient client(nullptr, &network);
        if (previous.isValid()) settings.setValue("pollInterval", previous); else settings.remove("pollInterval");
        QVERIFY(client.setRoot("http://127.0.0.1:9")); client.setAccessToken("test");
        QSignalSpy errors(&client, &OrchestrionClient::error);
        client.resume("lost-job");
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 5000);
        QCOMPARE(errors.first().first().toString(), QCoreApplication::translate("OrchestrionClient",
            "The job is no longer in the server queue or history. No new generation was started."));
        const auto requests = network.requests.size(); QTest::qWait(1100);
        QCOMPARE(network.requests.size(), requests); QCOMPARE(posts, 0);
    }
    void parallelJobsKeepTargetsAndRetryWithoutResubmit() {
        class Client : public OrchestrionClient {
        public:
            using OrchestrionClient::OrchestrionClient;
            QJsonObject input;
            int submissions = 0, resumed = 0, cancellations = 0;
            bool front = false;
            void prepare(const QJsonObject& value) override { input = value; }
            void submit(const QJsonObject&, bool value) override {
                ++submissions;
                front = value;
            }
            void resume(const QString&) override { ++resumed; }
            void cancelJob() override {
                ++cancellations;
                emit cancelled();
            }
            void fetchImage(const QJsonObject&, int) override { }
        };
        OrchestrionClient connection;
        QVERIFY(connection.setRoot("https://orchestrion.su"));
        QList<Client*> clients;
        JobQueue queue(&connection, nullptr, [&](QObject* parent) {
            auto client = new Client(parent);
            clients.append(client);
            return client;
        });
        const auto first = queue.start(
            { { "prompt", "first" }, { "seed", 12 } }, { { "target", "canvas-one" } }, {}, false);
        const auto second = queue.start(
            { { "prompt", "second" }, { "seed", 42 } }, { { "target", "canvas-two" } }, {}, true);
        QCOMPARE(clients.size(), 2);
        QCOMPARE(clients[0]->input["seed"].toInt(), 12);
        QCOMPARE(clients[1]->input["seed"].toInt(), 42);
        for (auto client : clients)
            client->prepared({ { "coins", 1 }, { "prompt", QJsonObject { { "node", 1 } } } });
        QCOMPARE(clients[0]->submissions, 1);
        QVERIFY(clients[1]->front);
        clients[0]->submitted("remote-one");
        clients[1]->submitted("remote-two");
        clients[0]->jobRunning(true);
        QCOMPARE(queue.job(first)->state, JobQueue::running);
        clients[1]->error("lost connection");
        QCOMPARE(queue.job(second)->state, JobQueue::failed);
        queue.retry(second);
        QCOMPARE(clients[1]->resumed, 1);
        QCOMPARE(clients[1]->submissions, 1);
        queue.cancel(first);
        QCOMPARE(clients[0]->cancellations, 1);
        QCOMPARE(clients[1]->cancellations, 0);
        QCOMPARE(queue.job(first)->state, JobQueue::cancelled);
        QJsonObject result { { "outputs",
            QJsonObject { { "node",
                QJsonObject { { "images",
                    QJsonArray { QJsonObject { { "filename", "test.png" } } } } } } } } };
        clients[1]->jobReady(result);
        QCOMPARE(queue.job(second)->state, JobQueue::downloading);
        QString target;
        connect(&queue, &JobQueue::completed, &queue, [&](const QString& id) {
            target = queue.job(id)->context["target"].toString();
            QCOMPARE(queue.job(id)->images.first().pixelColor(0, 0), QColor(Qt::red));
        });
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(Qt::red);
        clients[1]->imageReady(BaronPanel::png(image), 0);
        QTRY_COMPARE(target, QString("canvas-two"));
        QVERIFY(!queue.job(second));
    }
    void documentControlsRoundTripAndSwitch() {
        class Host : public CanvasHost {
        public:
            QJsonObject state;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(
                const QString&, const QImage&, const QImage&, const QString&, QString*) override {
                return false;
            }
            QString documentId() const override { return "doc"; }
            QJsonObject documentState() const override { return state; }
            void saveDocumentState(const QJsonObject& value) override { state = value; }
            QJsonArray layers() const override {
                return { QJsonObject { { "id", "layer" }, { "name", "Layer" } } };
            }
        } host;
        BaronPanel panel(&host);
        auto guidance = panel.findChild<GuidancePanel*>();
        QVERIFY(guidance);
        QImage reference(16, 16, QImage::Format_ARGB32);
        reference.fill(Qt::blue);
        const QJsonObject data { { "regions",
                                     QJsonArray { QJsonObject { { "id", "region" },
                                         { "layer", "layer" }, { "prompt", "blue sky" } } } },
            { "controls",
                QJsonArray { QJsonObject { { "id", "control" }, { "mode", "reference" },
                    { "title", "Reference" }, { "layer", "file" },
                    { "external", QString::fromLatin1(BaronPanel::png(reference).toBase64()) },
                    { "region", "region" }, { "strength", .7 } } } } };
        guidance->restoreState(data);
        panel.flushDocumentState();
        QCOMPARE(host.state["guidance"].toObject()["regions"].toArray().size(), 1);
        guidance->restoreState({});
        panel.documentChanged();
        const auto restored = guidance->state();
        QCOMPARE(restored["controls"].toArray().first().toObject()["region"].toString(),
            QString("region"));
        QCOMPARE(restored["controls"].toArray().first().toObject()["external"].toString(),
            data["controls"].toArray().first().toObject()["external"].toString());
        host.state = {};
        panel.documentChanged();
        QVERIFY(guidance->state()["controls"].toArray().isEmpty());
    }
    void fixedSeedReplacesRandomSentinel() {
        BaronPanel panel(nullptr);
        auto seed = panel.findChild<QLineEdit*>("generationSeed");
        auto fixed = panel.findChild<QCheckBox*>("fixedSeed");
        QVERIFY(seed);
        QVERIFY(fixed);
        fixed->setChecked(false);
        seed->setText("-1");
        fixed->setChecked(true);
        bool valid = false;
        const auto value = seed->text().toLongLong(&valid);
        QVERIFY(valid && value >= 0 && value <= 4294967295LL);
        fixed->setChecked(false);
        fixed->setChecked(true);
        QCOMPARE(seed->text().toLongLong(), value);
    }
    void historyPreviewSwitchAndSecondClickApply() {
        QSettings("BaronEdition", "Orchestrion").setValue("history_click_behavior", "apply_on_second_click");
        class Host : public CanvasHost {
        public:
            QString previewId;
            int previews = 0, applied = 0;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool preview(const QString& id, const QImage&, const QImage&, QString*) override {
                previewId = id;
                ++previews;
                return true;
            }
            bool apply(const QString& id, const QImage&, const QImage&, const QString&,
                QString*) override {
                previewId = id;
                ++applied;
                return true;
            }
        } host;
        BaronPanel panel(&host);
        auto history = panel.findChild<QListWidget*>("resultHistory");
        QVERIFY(history);
        auto pages = panel.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QCOMPARE(pages->currentIndex(), 0);
        auto item = [&](const QString& id) {
            auto result = new QListWidgetItem(id, history);
            QImage image(32, 32, QImage::Format_ARGB32);
            image.fill(Qt::red);
            result->setData(
                Qt::UserRole, QVariantMap { { "image", image }, { "target", id }, { "key", id } });
            return result;
        };
        auto first = item("first"), second = item("second");
        history->itemClicked(first);
        QCOMPARE(host.previews, 1);
        QCOMPARE(host.applied, 0);
        history->itemClicked(second);
        QCOMPARE(host.previewId, QString("second"));
        QCOMPARE(host.previews, 2);
        history->itemClicked(second);
        QCOMPARE(host.applied, 1);
        history->itemClicked(first);
        QCOMPARE(host.previews, 3);
        QCOMPARE(host.applied, 1);
        history->setCurrentItem(first);
        auto favorite = panel.findChild<QAction*>("history_favorite");
        QVERIFY(favorite);
        favorite->trigger();
        QVERIFY(first->data(Qt::UserRole).toMap()["favorite"].toBool());
        QCOMPARE(host.applied, 1);
        auto data = first->data(Qt::UserRole).toMap();
        data["settings"] = QJsonObject { { "prompt", "stored prompt" }, { "seed", "12345" },
            { "fixed_seed", true }, { "strength", .43 } };
        first->setData(Qt::UserRole, data);
        auto reuse = panel.findChild<QAction*>("history_reuse");
        QVERIFY(reuse);
        reuse->trigger();
        QCOMPARE(panel.findChild<QLineEdit*>("generationSeed")->text(), QString("12345"));
        QCOMPARE(panel.findChild<QSlider*>("denoiseSlider")->value(), 43);
        auto clear = panel.findChild<QAction*>("history_clear");
        QVERIFY(clear);
        clear->trigger();
        QCOMPARE(history->count(), 0);
        QSettings("BaronEdition", "Orchestrion").remove("history_click_behavior");
        auto settingsScroll = panel.findChild<QScrollArea*>("generationSettingsScroll");
        QVERIFY(settingsScroll);
        QCOMPARE(settingsScroll->horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
        auto control = panel.findChild<QToolButton*>("addControlLayer");
        QVERIFY(control);
        QCOMPARE(control->menu()->actions().size(), 15);
        QVERIFY(panel.findChild<QToolButton*>("generationSettings"));
    }
    void pluginLayoutAndRegionalEditorStates() {
        BaronPanel panel(nullptr);
        panel.resize(456, 900);
        panel.show();
        QCoreApplication::processEvents();
        for (auto tabs : panel.findChildren<QTabWidget*>())
            QVERIFY(!tabs->isVisible());
        auto positive = panel.findChild<PromptEditor*>("positivePrompt");
        auto negative = panel.findChild<PromptEditor*>("negativePrompt");
        auto styles = panel.findChild<QComboBox*>("styleSelect");
        auto run = panel.findChild<QPushButton*>("generateButton");
        auto history = panel.findChild<QListWidget*>("resultHistory");
        auto control = panel.findChild<QToolButton*>("addControlLayer");
        QVERIFY(positive && negative && styles && run && history && control);
        QVERIFY(styles->mapTo(&panel, QPoint()).y() < positive->mapTo(&panel, QPoint()).y());
        QVERIFY(positive->mapTo(&panel, QPoint()).y() < control->mapTo(&panel, QPoint()).y());
        QVERIFY(run->mapTo(&panel, QPoint()).y() < history->mapTo(&panel, QPoint()).y());
        QCOMPARE(positive->frameShape(), QFrame::NoFrame);
        QSignalSpy generate(positive, &PromptEditor::activated);
        QTest::keyClick(positive, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(generate.count(), 1);
        control->menu()->actions().first()->trigger();
        QCoreApplication::processEvents();
        auto row = panel.findChild<QComboBox*>("controlMode");
        QVERIFY(row && row->isVisible());
        auto options = panel.findChild<QWidget*>("controlOptions");
        QVERIFY(options && !options->isVisible());
        panel.findChild<QToolButton*>("addRegion")->click();
        QCoreApplication::processEvents();
        QVERIFY(!positive->isVisible());
        QVERIFY(!negative->isVisible());
        auto regional = panel.findChild<QPlainTextEdit*>("regionPrompt");
        QVERIFY(regional && regional->isVisible());
        auto guidance = panel.findChild<GuidancePanel*>();
        QSignalSpy regionalGenerate(guidance, &GuidancePanel::activated);
        QTest::keyClick(regional, Qt::Key_Return, Qt::ControlModifier);
        QCOMPARE(regionalGenerate.count(), 1);
        const auto state = guidance->state();
        guidance->restoreState(state);
        QCoreApplication::processEvents();
        QCOMPARE(guidance->state()["active_region"], state["active_region"]);
        regional = panel.findChild<QPlainTextEdit*>("regionPrompt");
        QVERIFY(regional && regional->isVisible());
        auto summary = panel.findChild<QToolButton*>("rootRegionSummary");
        QVERIFY(summary && summary->isVisible());
        summary->click();
        QVERIFY(positive->isVisible());
        QVERIFY(!regional->isVisible());
        QCOMPARE(history->iconSize(), QSize(96, 96));
    }
    void updateManifestRejectsWrongOriginAndPackage() {
        QJsonObject package { { "version", "0.1.6" }, { "version_code", 5050406 },
            { "package", "org.krita.baron.debug" }, { "bytes", 3 },
            { "url", "https://orchestrion.su/baron-updates/releases/update.apk" },
            { "sha256", QString(64, 'a') } };
        QJsonObject manifest { { "schema", 1 }, { "edition", "baron" }, { "channel", "stable" },
            { "android", package } };
        QVERIFY(!BaronUpdates::validatePackage(manifest, BaronUpdates::feedUrl()).isEmpty());
        for (const auto& url : { "https://evil.example/baron-updates/releases/update.apk",
                 "http://orchestrion.su/baron-updates/releases/update.apk",
                 "https://user@orchestrion.su/baron-updates/releases/update.apk" }) {
            package["url"] = url;
            manifest["android"] = package;
            QVERIFY(BaronUpdates::validatePackage(manifest, BaronUpdates::feedUrl()).isEmpty());
        }
    }
    void updaterDownloadsOnlyMatchingBytes() {
        QStandardPaths::setTestModeEnabled(true);
        for (bool corrupt : { false, true }) {
            const QByteArray bytes("test apk payload");
            QJsonObject package { { "version", "0.1.8" }, { "version_code", BaronUpdates::installedVersionCode() + 1 },
                { "package", "org.krita.baron.debug" }, { "bytes", bytes.size() },
                { "url", "https://orchestrion.su/baron-updates/releases/update.apk" },
                { "sha256",
                    QString::fromLatin1(
                        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) } };
            const QJsonObject manifest { { "schema", 1 }, { "edition", "baron" },
                { "channel", "stable" }, { "android", package } };
            PreviewNetwork network;
            network.responses = { { 200, {}, QJsonDocument(manifest).toJson() },
                { 200, {}, corrupt ? QByteArray("wrong") : bytes } };
            BaronUpdates updater(nullptr, &network);
            updater.check();
            QTRY_COMPARE(updater.state(), BaronUpdates::available);
            updater.download();
            QTRY_COMPARE(updater.state(), corrupt ? BaronUpdates::failed : BaronUpdates::ready);
            if (!corrupt) {
                QFile file(updater.downloadedFile());
                QVERIFY(file.open(QIODevice::ReadOnly));
                QCOMPARE(file.readAll(), bytes);
                file.close();
                file.remove();
                network.responses.append({ 200, {}, QJsonDocument(manifest).toJson() });
                updater.check();
                QTRY_COMPARE(updater.state(), BaronUpdates::available);
            }
            for (const auto& request : network.requests)
                QVERIFY(!request.hasRawHeader("Authorization"));
        }
    }
    void thumbnailAddressesAndRedirects() {
        const QUrl original("https://image.civitai.com/account/id/original=true/photo.png");
        QCOMPARE(ModelThumbnails::previewUrl(original),
            QUrl(
                "https://image.civitai.com/account/id/width=320,quality=85,format=jpeg/photo.png"));
        const QUrl blob("https://blobs-b2.civitai.com/file/blobs-managed-public/image.jpg");
        QCOMPARE(ModelThumbnails::redirectUrl(original, blob), blob);
        QCOMPARE(ModelThumbnails::previewUrl(blob), blob);
        for (const auto& url : { "http://image.civitai.com/image", "https://evil.example/image",
                 "https://image.civitai.com.evil.example/image",
                 "https://user@image.civitai.com/image", "https://image.civitai.com:8188/image",
                 "file:///private", "https://127.0.0.1/image" }) {
            QVERIFY(ModelThumbnails::previewUrl(QUrl(url)).isEmpty());
            QVERIFY(ModelThumbnails::redirectUrl(original, QUrl(url)).isEmpty());
        }
    }
    void thumbnailRedirectLoadsAndCachesImage() {
        QImage source(320, 640, QImage::Format_RGB32);
        source.fill(Qt::cyan);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        QVERIFY(source.save(&buffer, "PNG"));
        PreviewNetwork network;
        network.responses = { { 301, QUrl("https://blobs-b2.civitai.com/file/preview.jpg"), {} },
            { 200, {}, png } };
        ModelThumbnails thumbnails(nullptr, &network);
        int calls = 0;
        QImage result;
        const QUrl original("https://image.civitai.com/account/id/original=true/photo.png");
        thumbnails.load(original, [&](const QImage& image) {
            result = image;
            ++calls;
        });
        QTRY_COMPARE(calls, 1);
        QCOMPARE(result.size(), QSize(80, 160));
        QCOMPARE(network.requests.size(), 2);
        for (const auto& request : network.requests) {
            QVERIFY(!request.hasRawHeader("Authorization"));
            QVERIFY(!request.hasRawHeader("Cookie"));
            QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                int(QNetworkRequest::ManualRedirectPolicy));
        }
        thumbnails.load(original, [&](const QImage& image) {
            result = image;
            ++calls;
        });
        QCOMPARE(calls, 2);
        QCOMPARE(network.requests.size(), 2);
    }
    void thumbnailDiskCacheAndConcurrentRequests() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QImage image(320,640,QImage::Format_RGB32); image.fill(Qt::green);
        QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer,"PNG"));
        PreviewNetwork network; network.responses={{200,{},png,40}};
        const QUrl url("https://image.civitai.com/account/persistent/original=true/image.png");
        int calls=0;
        {
            ModelThumbnails loader(nullptr,&network,directory.path());
            for(int i=0;i<3;++i)loader.load(url,[&](const QImage& result) { QCOMPARE(result.size(),QSize(80,160)); ++calls; });
            QTRY_COMPARE(calls,3); QCOMPARE(network.requests.size(),1);
            QCOMPARE(QDir(directory.path()).entryList({"*.png"},QDir::Files).size(),1);
        }
        {
            ModelThumbnails loader(nullptr,&network,directory.path());
            loader.load(url,[&](const QImage& result) { QCOMPARE(result.size(),QSize(80,160)); ++calls; });
            QTRY_COMPARE(calls,4); QCOMPARE(network.requests.size(),1);
        }
        const auto files=QDir(directory.path()).entryList({"*.png"},QDir::Files);
        QFile damaged(directory.filePath(files.first())); QVERIFY(damaged.open(QIODevice::WriteOnly)); damaged.write("damaged image"); damaged.close();
        network.responses={{200,{},png}};
        ModelThumbnails recovery(nullptr,&network,directory.path());
        recovery.load(url,[&](const QImage& result) { QVERIFY(!result.isNull()); ++calls; });
        QTRY_COMPARE(calls,5); QCOMPARE(network.requests.size(),2);
    }
    void modelInformationUsesEncodedPathAndDoesNotStopGeneration() {
        PreviewNetwork network;
        network.responses={{200,{},R"({"success":true,"data":{"trainedWords":["hero"],"description":"Author notes"}})"},{404,{},"{}"}};
        OrchestrionClient client(nullptr,&network); QVERIFY(client.setRoot("https://orchestrion.su")); client.setAccessToken("test-token");
        QSignalSpy errors(&client,&OrchestrionClient::error); int calls=0;
        client.modelMetadata("characters/Hero #2.safetensors","lora",[&](const QJsonObject& data,const QString& error) {
            QVERIFY(error.isEmpty()); QCOMPARE(data.value("trainedWords").toArray().first().toString(),QString("hero")); ++calls;
        });
        QTRY_COMPARE(calls,1);
        QCOMPARE(network.requests.first().url().toEncoded(),QByteArray("https://orchestrion.su/api/krita/model-metadata?name=characters%2FHero%20%232.safetensors&kind=lora"));
        QCOMPARE(network.requests.first().rawHeader("Authorization"),QByteArray("Bearer test-token"));
        client.modelMetadata("private.safetensors","checkpoint",[&](const QJsonObject& data,const QString& error) {
            QVERIFY(data.isEmpty()); QVERIFY(!error.isEmpty()); ++calls;
        });
        QTRY_COMPARE(calls,2); QCOMPARE(errors.count(),0);
    }
    void catalogMetadataTagsAndInformation() {
        ModelCatalog catalog; catalog.resize(1000,720); catalog.show();
        catalog.setModels({QJsonObject{{"name","characters/Hero.safetensors"},{"kind","lora"},{"triggers",QJsonArray{"brave hero"}},{"tags",QJsonArray{"character"}}},
            QJsonObject{{"name","style/Ink.safetensors"},{"kind","lora"},{"tags",QJsonArray{"style"}}}});
        catalog.mergeMetadata({QJsonObject{{"name","characters\\Hero.safetensors"},{"trainedWords",QJsonArray{"hero"}},{"tags",QJsonArray{"character","fantasy"}}},
            QJsonObject{{"name","unavailable.safetensors"},{"tags",QJsonArray{"unknown"}}}});
        QCOMPARE(catalog.gallery()->count(),2);
        auto tags=catalog.findChild<QListWidget*>("modelTags"); QCOMPARE(tags->count(),3);
        for(int i=0;i<tags->count();++i)if(tags->item(i)->data(Qt::UserRole)=="fantasy")tags->item(i)->setCheckState(Qt::Checked);
        int visible=0; QJsonObject hero;
        for(int i=0;i<catalog.gallery()->count();++i)if(!catalog.gallery()->item(i)->isHidden()) { ++visible; hero=catalog.gallery()->item(i)->data(Qt::UserRole).toJsonObject(); }
        QCOMPARE(visible,1); QCOMPARE(hero.value("triggerWords").toArray().size(),2);
        catalog.search()->setText("brave fantasy"); QVERIFY(!catalog.gallery()->item(0)->isHidden());
        QCOMPARE(catalog.gallery()->movement(),QListView::Static); QCOMPARE(catalog.gallery()->dragDropMode(),QAbstractItemView::NoDragDrop);
        catalog.setMetadataProvider([](const QString&,const QString&,ModelCatalog::MetadataCallback done,bool) {
            done({{"modelDescription","<p>Use 0.7 strength</p>"},{"trainedWords",QJsonArray{"hero"}},
                {"images",QJsonArray{QJsonObject{{"meta",QJsonObject{{"steps",28},{"cfgScale",5.5}}}}}}},{});
        });
        QSignalSpy chosen(&catalog,&ModelCatalog::modelChosen),inserted(&catalog,&ModelCatalog::triggerWordsRequested);
        QTimer::singleShot(30,&catalog,[&] {
            auto dialog=catalog.findChild<QDialog*>("modelInformationDialog"); QVERIFY(dialog);
            QTimer::singleShot(1000,dialog,&QDialog::reject);
            const QString notes = QCoreApplication::translate("ModelCatalog","Author notes") + "\nUse 0.7 strength";
            QCOMPARE(dialog->findChild<QLabel*>("modelAuthorNotes")->text(),notes);
            QVERIFY(dialog->findChild<QLabel*>("modelExampleSettings")->text().contains(
                QCoreApplication::translate("ModelCatalog","Example settings (not author recommendations)")));
            dialog->findChild<QPushButton*>("insertModelTriggers")->click();
        });
        catalog.showInformation(hero); QCOMPARE(chosen.count(),0); QCOMPARE(inserted.count(),1);
    }
    void catalogInformationButtonDoesNotChooseModel() {
        ModelCatalog catalog; catalog.resize(1000,720); catalog.show();
        catalog.setModels({QJsonObject{{"name","Hero"},{"kind","checkpoint"}}});
        QApplication::processEvents();
        QSignalSpy clicks(catalog.gallery(),&QListWidget::itemClicked),chosen(&catalog,&ModelCatalog::modelChosen);
        QTimer::singleShot(30,&catalog,[&] {
            auto dialog=catalog.findChild<QDialog*>("modelInformationDialog"); QVERIFY(dialog); dialog->reject();
        });
        const auto rect=catalog.gallery()->visualItemRect(catalog.gallery()->item(0));
        QTest::mouseClick(catalog.gallery()->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(rect.right()-25,rect.bottom()-25));
        QCOMPARE(clicks.count(),0); QCOMPARE(chosen.count(),0);
    }
    void thumbnailFailures_data() {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("external-redirect") << "external";
        QTest::newRow("redirect-loop") << "loop";
        QTest::newRow("oversize") << "oversize";
        QTest::newRow("not-an-image") << "invalid";
    }
    void thumbnailFailures() {
        QFETCH(QString, scenario);
        PreviewNetwork network;
        const QUrl original("https://image.civitai.com/account/id/original=true/photo.png");
        if (scenario == "external")
            network.responses = { { 302, QUrl("https://evil.example/private"), {} } };
        else if (scenario == "loop")
            for (int i = 0; i < 4; ++i)
                network.responses.append({ 301, original, {} });
        else
            network.responses = { { 200, {},
                scenario == "oversize" ? QByteArray(5 * 1024 * 1024 + 1, 'x')
                                       : QByteArray("not an image") } };
        ModelThumbnails thumbnails(nullptr, &network);
        int calls = 0;
        thumbnails.load(original, [&](const QImage& image) {
            QVERIFY(image.isNull());
            ++calls;
        });
        QTRY_COMPARE(calls, 1);
        QCOMPARE(network.requests.size(), scenario == "loop" ? 4 : 1);
    }
    void liveCivitaiThumbnail() {
        const auto url = qEnvironmentVariable("BARON_LIVE_PREVIEW_URL");
        if (url.isEmpty())
            QSKIP("Live preview URL not requested");
        ModelThumbnails thumbnails;
        int calls = 0;
        QImage result;
        thumbnails.load(QUrl(url), [&](const QImage& image) {
            result = image;
            ++calls;
        });
        QTRY_COMPARE_WITH_TIMEOUT(calls, 1, 60000);
        QVERIFY(!result.isNull());
        QVERIFY(result.width() <= 160 && result.height() <= 160);
    }
    void fullScreenGallerySearchAndSelection() {
        BaronPanel panel(nullptr);
        panel.resize(520, 840);
        panel.show();
        auto client = panel.findChild<OrchestrionClient*>();
        QVERIFY(client);
        client->modelsReady({ { "items",
            QJsonArray { QJsonObject { { "name", "qwen.safetensors" }, { "title", "Qwen Edit" },
                             { "family", "Qwen" }, { "kind", "checkpoint" } },
                QJsonObject { { "name", "krea.safetensors" }, { "title", "Krea Turbo" },
                    { "family", "Krea" }, { "kind", "diffusion_model" } },
                QJsonObject { { "name", "identity.safetensors" }, { "title", "Identity LoRA" },
                    { "family", "Krea" }, { "kind", "lora" } } } } });
        int sections = 0;
        for (auto button : panel.findChildren<QToolButton*>())
            if (button->isCheckable()) {
                ++sections;
            }
        QVERIFY(sections >= 4);
        auto choose = panel.findChild<QPushButton*>("chooseModelButton");
        QVERIFY(choose);
        bool inspected = false;
        QTimer::singleShot(100, &panel, [&] {
            auto dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto gallery = dialog->findChild<QListWidget*>("modelGallery");
            auto filter = dialog->findChild<QLineEdit*>("modelSearchField");
            inspected = dialog->isFullScreen() && gallery && filter;
            if (inspected) {
                filter->setText("krea");
                inspected = gallery->item(0)->isHidden() && !gallery->item(1)->isHidden()
                    && gallery->item(2)->isHidden();
                gallery->itemClicked(gallery->item(1));
            } else
                dialog->reject();
        });
        choose->click();
        QVERIFY(inspected);
        bool selected = false;
        for (auto combo : panel.findChildren<QComboBox*>())
            selected = selected || combo->currentData().toString() == "krea.safetensors";
        QVERIFY(selected);
        QVERIFY(panel.findChild<QListWidget*>("modelGallery"));
        QCOMPARE(panel.palette().color(QPalette::Window),
            QApplication::palette().color(QPalette::Window));
    }
    void importedPresetsSelectInputsAndFields() {
        QJsonObject preset { { "id", "imported-test" }, { "name", "Character style" },
            { "checkpoints", QJsonArray { "test-model" } }, { "sampler_steps", 17 },
            { "cfg_scale", 3.5 }, { "style_prompt", "watercolor" },
            { "loras",
                QJsonArray { QJsonObject { { "name", "identity-test" }, { "strength", .75 } } } } };
        QSettings settings("BaronEdition", "Orchestrion");
        settings.setValue("stylePresets", QJsonDocument(QJsonArray { preset }).toJson());
        settings.setValue("styleId", "imported-test");
        BaronPanel panel(nullptr);
        panel.findChild<OrchestrionClient*>()->modelsReady(QJsonObject { { "items",
            QJsonArray { QJsonObject { { "name", "test-model" }, { "title", "Test checkpoint" },
                { "family", "SDXL" }, { "kind", "checkpoint" } } } } });
        QCOMPARE(panel.findChild<QComboBox*>("styleSelect")->currentData().toString(),
            QString("imported-test"));
        QCOMPARE(panel.findChild<QComboBox*>("checkpointSelect")->currentData().toString(),
            QString("test-model"));
        QCOMPARE(panel.findChild<QSpinBox*>("generationSteps")->value(), 17);
        QCOMPARE(panel.findChild<QDoubleSpinBox*>("generationCfg")->value(), 3.5);
        QCOMPARE(
            panel.findChild<QPlainTextEdit*>("stylePrompt")->toPlainText(), QString("watercolor"));
        QCOMPARE(panel.findChild<QListWidget*>("styleLoras")->count(), 1);
        auto styles = panel.findChild<QComboBox*>("styleSelect");
        styles->setCurrentIndex(styles->findData("model:test-model"));
        QCOMPARE(panel.findChild<QListWidget*>("styleLoras")->count(), 0);
        QCOMPARE(panel.findChild<QPlainTextEdit*>("stylePrompt")->toPlainText(), QString());
    }
    void generationAndEditingKeepSeparateStyles() {
        class Host : public CanvasHost {
        public:
            QString id = "style-document";
            QMap<QString, QJsonObject> states;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return false; }
            QString documentId() const override { return id; }
            QJsonObject documentState() const override { return states.value(id); }
            void saveDocumentState(const QJsonObject& value) override { states[id] = value; }
        } host;
        QSettings settings("BaronEdition", "Orchestrion");
        settings.remove("styleBanks");
        settings.remove("promptBanks");
        settings.setValue("mode", "generate");
        const QJsonObject generation { { "id", "gen-style" }, { "name", "Generation" },
            { "checkpoints", QJsonArray { "test-model" } }, { "sampler_steps", 28 },
            { "cfg_scale", 7 }, { "linked_edit_style", "edit-style" }, { "style_prompt", "painted" } };
        const QJsonObject editing { { "id", "edit-style" }, { "name", "Editing" },
            { "checkpoints", QJsonArray { "edit-model" } }, { "sampler_steps", 9 },
            { "cfg_scale", 1 }, { "style_prompt", "keep the composition" },
            { "live_steps", 3 }, { "extension_option", "preserved" } };
        settings.setValue("stylePresets", QJsonDocument(QJsonArray { generation, editing }).toJson());
        settings.setValue("styleId", "gen-style");
        const QJsonObject catalog { { "items", QJsonArray {
            QJsonObject { { "name", "test-model" }, { "title", "Test" }, { "family", "SDXL" } },
            QJsonObject { { "name", "edit-model" }, { "title", "Edit" }, { "family", "Qwen" } } } },
            { "resources", QJsonObject { { "vae", QJsonArray { "qwen_vae.safetensors" } } } } };
        {
            BaronPanel panel(&host);
            panel.findChild<OrchestrionClient*>()->modelsReady(catalog);
            auto mode = panel.findChild<QComboBox*>("workspaceMode");
            auto styles = panel.findChild<QComboBox*>("styleSelect");
            auto steps = panel.findChild<QSpinBox*>("generationSteps");
            QCOMPARE(styles->currentData().toString(), QString("gen-style"));
            steps->setValue(26);
            mode->setCurrentIndex(mode->findData("edit"));
            QCOMPARE(styles->currentData().toString(), QString("edit-style"));
            QCOMPARE(steps->value(), 9);
            QCOMPARE(panel.findChild<QComboBox*>("checkpointSelect")->currentData().toString(), QString("edit-model"));
            steps->setValue(11);
            bool saved = false;
            QTimer::singleShot(0, &panel, [&] {
                auto editor = panel.findChild<PromptEditor*>("stylePrompt");
                editor->replacePromptText("change the lighting");
                panel.findChild<QComboBox*>("styleVae")->setCurrentText("qwen_vae.safetensors");
                const auto presets = QJsonDocument::fromJson(settings.value("stylePresets").toByteArray()).array();
                for (auto value : presets) {
                    const auto preset = value.toObject();
                    if (preset["id"] == "edit-style")
                        saved = preset["style_prompt"] == "change the lighting"
                            && preset["extension_option"] == "preserved" && preset["live_steps"] == 3;
                }
                editor->window()->close();
            });
            panel.findChild<QToolButton*>("styleSettings")->click();
            QVERIFY(saved);
            mode->setCurrentIndex(mode->findData("generate"));
            QCOMPARE(styles->currentData().toString(), QString("gen-style"));
            QCOMPARE(steps->value(), 26);
            mode->setCurrentIndex(mode->findData("edit"));
            QCOMPARE(styles->currentData().toString(), QString("edit-style"));
            QCOMPARE(steps->value(), 11);
            panel.flushDocumentState();
            QCOMPARE(host.states[host.id]["style_banks"].toObject()["generate"].toObject()["style_id"].toString(), QString("gen-style"));
            const auto id = host.id;
            host.id = "different-document";
            panel.documentChanged();
            host.id = id;
            panel.documentChanged();
            QCOMPARE(styles->currentData().toString(), QString("edit-style"));
            mode->setCurrentIndex(mode->findData("generate"));
            QCOMPARE(styles->currentData().toString(), QString("gen-style"));
            QCOMPARE(steps->value(), 26);
            panel.flushDocumentState();
        }
        settings.remove("stylePresets");
        BaronPanel reopened(&host);
        reopened.documentChanged();
        reopened.findChild<OrchestrionClient*>()->modelsReady(catalog);
        auto mode = reopened.findChild<QComboBox*>("workspaceMode");
        mode->setCurrentIndex(mode->findData("generate"));
        QCOMPARE(reopened.findChild<QComboBox*>("styleSelect")->currentData().toString(), QString("gen-style"));
        QCOMPARE(reopened.findChild<QSpinBox*>("generationSteps")->value(), 26);
    }
    void websiteAddresses_data() {
        QTest::addColumn<QString>("address");
        QTest::addColumn<bool>("valid");
        QTest::newRow("https") << "https://orchestrion.su" << true;
        QTest::newRow("lan") << "http://192.168.50.191:3000" << true;
        QTest::newRow("lan-v6") << "http://[fd00::1]:3000" << true;
        QTest::newRow("localhost") << "http://127.0.0.1:3000" << true;
        QTest::newRow("public-http") << "http://orchestrion.su" << false;
        QTest::newRow("credentials") << "https://user:secret@orchestrion.su" << false;
        QTest::newRow("query-token") << "https://orchestrion.su?token=secret" << false;
        QTest::newRow("path-token") << "https://orchestrion.su/krita/secret" << false;
        QTest::newRow("fragment") << "https://orchestrion.su#token" << false;
        QTest::newRow("file") << "file:///tmp/private" << false;
    }
    void websiteAddresses() {
        QFETCH(QString, address);
        QFETCH(bool, valid);
        QCOMPARE(!OrchestrionClient::validateRoot(address).isEmpty(), valid);
    }
    void pngPreservesTransparency() {
        QImage source(3, 2, QImage::Format_ARGB32);
        source.fill(qRgba(30, 80, 120, 17));
        auto restored = QImage::fromData(BaronPanel::png(source));
        QCOMPARE(restored.size(), source.size());
        QCOMPARE(qAlpha(restored.pixel(0, 0)), 17);
    }
    void refusesExternalLoginAddress() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll();
                const QByteArray body
                    = R"({"device_code":"abc","user_code":"123","verification_uri":"https://foreign.example/login","expires_in":600})";
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        OrchestrionClient client;
        QVERIFY(client.setRoot(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        QSignalSpy error(&client, &OrchestrionClient::error),
            browser(&client, &OrchestrionClient::browserLogin);
        client.signIn();
        QTRY_COMPARE(error.size(), 1);
        QCOMPARE(browser.size(), 0);
        QVERIFY(!client.signedIn());
    }

    void promptServicesUseExistingWebsiteEndpointsAndIsolateErrors() {
        PreviewNetwork network;
        network.responses = {
            {200, {}, R"({"success":true,"translatedText":"soft light"})"},
            {200, {}, R"({"supported":true,"ai":true})"},
            {200, {}, R"({"success":true,"labels":["lighting"]})"},
            {503, {}, R"({"error":"Translation unavailable"})"}
        };
        OrchestrionClient client(nullptr, &network);
        QVERIFY(client.setRoot("https://orchestrion.su"));
        client.setAccessToken("test-device-token");
        int replies = 0;
        client.translatePrompt("мягкий свет", "translate", [&](const QJsonObject& data) { QCOMPARE(data["translatedText"].toString(), QString("soft light")); ++replies; }, "qwen-image-edit-2509.safetensors");
        QTRY_COMPARE(replies,1);
        QCOMPARE(network.requests.last().url().path(),QString("/api/translate"));
        QCOMPARE(network.requests.last().rawHeader("Authorization"),QByteArray("Bearer test-device-token"));
        QCOMPARE(QJsonDocument::fromJson(network.bodies.last()).object()["mode"].toString(),QString("translate"));
        QCOMPARE(QJsonDocument::fromJson(network.bodies.last()).object()["model"].toString(),QString("qwen-image-edit-2509.safetensors"));
        client.promptOrganizerCapabilities([&](const QJsonObject& data) { QVERIFY(data["ai"].toBool()); ++replies; });
        QTRY_COMPARE(replies,2);
        QCOMPARE(network.requests.last().url().path(),QString("/api/prompt/organize/capabilities"));
        client.promptOrganizerLabels({"soft light"},"illustrious",[&](const QJsonObject& data) { QCOMPARE(data["labels"].toArray().first().toString(),QString("lighting")); ++replies; });
        QTRY_COMPARE(replies,3);
        QCOMPARE(network.requests.last().url().path(),QString("/api/prompt/organize/labels"));
        QSignalSpy errors(&client,&OrchestrionClient::error);
        client.translatePrompt("новый текст","tags",[&](const QJsonObject&) { ++replies; });
        QTRY_COMPARE(errors.count(),1);
        QCOMPARE(replies,3); QVERIFY(client.signedIn());
    }
    void editingCreatesIndependentPresetWithoutALink() {
        QSettings settings("BaronEdition", "Orchestrion");
        settings.remove("styleBanks"); settings.remove("promptBanks");
        settings.setValue("mode", "generate");
        settings.setValue("styleId", "unlinked-style");
        const QJsonObject original { { "id", "unlinked-style" }, { "name", "My model" },
            { "checkpoints", QJsonArray { "test-model" } }, { "sampler_steps", 18 },
            { "style_prompt", "watercolor" } };
        settings.setValue("stylePresets", QJsonDocument(QJsonArray { original }).toJson());
        BaronPanel panel(nullptr);
        panel.findChild<OrchestrionClient*>()->modelsReady({ { "items", QJsonArray {
            QJsonObject { { "name", "test-model" }, { "title", "Test model" }, { "family", "SDXL" } } } } });
        auto mode = panel.findChild<QComboBox*>("workspaceMode");
        auto styles = panel.findChild<QComboBox*>("styleSelect");
        mode->setCurrentIndex(mode->findData("edit"));
        const auto editId = styles->currentData().toString();
        QVERIFY(editId != "unlinked-style" && !editId.isEmpty());
        QTimer::singleShot(0, &panel, [&] {
            auto editor = panel.findChild<PromptEditor*>("stylePrompt");
            editor->replacePromptText("change the background");
            editor->window()->close();
        });
        panel.findChild<QToolButton*>("styleSettings")->click();
        mode->setCurrentIndex(mode->findData("generate"));
        QCOMPARE(styles->currentData().toString(), QString("unlinked-style"));
        QCOMPARE(panel.findChild<PromptEditor*>("stylePrompt")->toPlainText(), QString("watercolor"));
        mode->setCurrentIndex(mode->findData("edit"));
        QCOMPARE(styles->currentData().toString(), editId);
        QCOMPARE(panel.findChild<PromptEditor*>("stylePrompt")->toPlainText(), QString("change the background"));
        mode->setCurrentIndex(mode->findData("generate"));
        QTimer::singleShot(0, &panel, [&] {
            auto linked = panel.findChild<QComboBox*>("linkedEditStyle");
            linked->setCurrentIndex(0);
            linked->window()->close();
        });
        panel.findChild<QToolButton*>("styleSettings")->click();
        mode->setCurrentIndex(mode->findData("edit"));
        QVERIFY(styles->currentData().toString() != editId);
        QCOMPARE(panel.findChild<PromptEditor*>("stylePrompt")->toPlainText(), QString("watercolor"));
    }
    void generationAndEditPromptsSurviveSwitchesDocumentsAndRestart() {
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QJsonObject> states;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return false; }
            QString documentId() const override { return id; }
            QJsonObject documentState() const override { return states.value(id); }
            void saveDocumentState(const QJsonObject& value) override { states[id] = value; }
        } host;
        QSettings settings("BaronEdition", "Orchestrion");
        settings.remove("promptBanks"); settings.setValue("mode", "generate");
        settings.setValue("prompt", "legacy generation");
        const auto originalId = host.id;
        {
            BaronPanel panel(&host, nullptr, PromptEditor::CompletionDisplay::Embedded);
            auto mode = panel.findChild<QComboBox*>("workspaceMode");
            auto prompt = panel.findChild<PromptEditor*>("positivePrompt");
            auto negative = panel.findChild<PromptEditor*>("negativePrompt");
            QVERIFY(mode && prompt && negative);
            QCOMPARE(prompt->toPlainText(), QString("legacy generation"));
            prompt->replacePromptText("generation <lora:test:0.7>");
            negative->replacePromptText("grain");
            prompt->restoreDisabledFragments(QJsonArray { QJsonObject { { "text", "watercolor" } } });
            mode->setCurrentIndex(mode->findData("edit"));
            QCOMPARE(prompt->toPlainText(), QString());
            QCOMPARE(negative->toPlainText(), QString());
            QVERIFY(prompt->disabledFragments().isEmpty());
            prompt->replacePromptText("change only the sky"); negative->replacePromptText("clouds");
            panel.findChild<QComboBox*>("promptSyntax")->setCurrentIndex(1);
            panel.findChild<QCheckBox*>("a1111GpuNoise")->setChecked(true);
            panel.findChild<QLineEdit*>("a1111Ensd")->setText("31337");
            QTimer::singleShot(0, &panel, [&panel] {
                panel.findChild<QComboBox*>("promptSyntax")->window()->close();
            });
            panel.findChild<QToolButton*>("styleSettings")->click();
            QCOMPARE(panel.findChild<QComboBox*>("promptSyntax")->currentData().toString(), QString("a1111"));
            mode->setCurrentIndex(mode->findData("generate"));
            QCOMPARE(prompt->toPlainText(), QString("generation <lora:test:0.7>"));
            QCOMPARE(negative->toPlainText(), QString("grain"));
            QCOMPARE(prompt->disabledFragments().size(), 1);
            mode->setCurrentIndex(mode->findData("upscale"));
            mode->setCurrentIndex(mode->findData("generate"));
            QCOMPARE(prompt->toPlainText(), QString("generation <lora:test:0.7>"));
            panel.flushDocumentState();
            host.id = QUuid::createUuid().toString();
            host.states[host.id] = { { "schema", 1 }, { "mode", "edit" }, { "prompt", "legacy edit document" } };
            panel.documentChanged();
            QCOMPARE(prompt->toPlainText(), QString("legacy edit document"));
            mode->setCurrentIndex(mode->findData("generate"));
            QCOMPARE(prompt->toPlainText(), QString());
            prompt->replacePromptText("unsaved new document");
            host.id = QUuid::createUuid().toString(); panel.documentChanged();
            QCOMPARE(prompt->toPlainText(), QString());
            host.id = originalId; panel.documentChanged();
            QCOMPARE(panel.findChild<QComboBox*>("promptSyntax")->currentData().toString(), QString("comfy"));
            QCOMPARE(prompt->toPlainText(), QString("generation <lora:test:0.7>"));
            mode->setCurrentIndex(mode->findData("edit"));
            QCOMPARE(panel.findChild<QComboBox*>("promptSyntax")->currentData().toString(), QString("a1111"));
            QVERIFY(panel.findChild<QCheckBox*>("a1111GpuNoise")->isChecked());
            QCOMPARE(prompt->toPlainText(), QString("change only the sky"));
            panel.flushDocumentState();
        }
        BaronPanel reopened(&host);
        reopened.documentChanged();
        auto mode = reopened.findChild<QComboBox*>("workspaceMode");
        QCOMPARE(reopened.findChild<QLineEdit*>("a1111Ensd")->text(), QString("31337"));
        QCOMPARE(reopened.findChild<PromptEditor*>("positivePrompt")->toPlainText(), QString("change only the sky"));
        mode->setCurrentIndex(mode->findData("generate"));
        QCOMPARE(reopened.findChild<PromptEditor*>("positivePrompt")->toPlainText(), QString("generation <lora:test:0.7>"));
    }
    void websitePromptLogicGolden() {
        PromptLogic logic;
        QVERIFY2(logic.error().isEmpty(), qPrintable(logic.error()));
        QFile file(QFINDTESTDATA("fixtures/prompt-site-golden.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).array();
        QVERIFY(cases.size() >= 280);
        int i = 0;
        for (const auto& value : cases) {
            const auto test = value.toObject();
            const auto result = logic.call(test["name"].toString(), test["args"].toArray());
            QVERIFY2(logic.error().isEmpty(), qPrintable(logic.error()));
            QVERIFY2(result == test["expected"], qPrintable(QString("Case %1: %2 expected %3 got %4")
                .arg(i++).arg(test["name"].toString())
                .arg(QString::fromUtf8(QJsonDocument(QJsonArray{test["expected"]}).toJson(QJsonDocument::Compact)))
                .arg(QString::fromUtf8(QJsonDocument(QJsonArray{result}).toJson(QJsonDocument::Compact)))));
        }
    }
    void promptContextActionsKeepFocusUndoAndDisabledState() {
        QWidget window;
        window.resize(800, 700);
        PromptEditor positive(&window, false, PromptEditor::CompletionDisplay::Embedded);
        positive.setGeometry(20, 160, 500, 150);
        PromptEditor negative(&window, true, PromptEditor::CompletionDisplay::Embedded);
        negative.setGeometry(20, 320, 500, 80);
        positive.setPromptActionTarget(&negative);
        negative.setPromptActionTarget(&positive);
        positive.setPlainText("masterpiece, (long_hair:1.2), <lora:demo:0.8>, forest");
        window.show(); positive.setFocus();
        auto actions = positive.findChild<PromptActions*>(); QVERIFY(actions);
        actions->open(20);
        QTRY_VERIFY(window.findChild<QFrame*>("promptActionBar")->isVisible());
        actions->perform("toggleMore");
        QTRY_VERIFY(window.findChild<QFrame*>("promptActionBar")->height() > 140);
        actions->refresh();
        QVERIFY(window.findChild<QFrame*>("promptActionBar")->height() > 140);
        auto up = window.findChild<QPushButton*>("promptAction_weight_0.1"); QVERIFY(up);
        QCOMPARE(up->focusPolicy(), Qt::NoFocus);
        QTest::mouseClick(up, Qt::LeftButton);
        QTRY_VERIFY(positive.toPlainText().contains("(long_hair:1.3)"));
        QVERIFY(positive.hasFocus());
        positive.undo(); QCOMPARE(positive.toPlainText(), QString("masterpiece, (long_hair:1.2), <lora:demo:0.8>, forest"));
        actions->open(20); actions->perform("moveOther");
        QCOMPARE(negative.toPlainText(), QString("(long_hair:1.2)"));
        QVERIFY(!positive.toPlainText().contains("long_hair"));
        actions->open(positive.toPlainText().indexOf("<lora:")); actions->perform("park");
        QCOMPARE(positive.disabledFragments().size(), 1);
        QVERIFY(!positive.toPlainText().contains("<lora:"));
        PromptEditor restored;
        restored.restoreDisabledFragments(positive.disabledFragments());
        QCOMPARE(restored.disabledFragments(), positive.disabledFragments());
        const auto buttons = positive.findChild<QScrollArea*>("promptDisabledFragments")->findChildren<QPushButton*>();
        QVERIFY(!buttons.isEmpty()); QTest::mouseClick(buttons.first(), Qt::LeftButton);
        QTRY_VERIFY(positive.toPlainText().contains("<lora:demo:0.8>"));
        QVERIFY(positive.disabledFragments().isEmpty());
        actions->open(0); actions->perform("selectAll"); actions->perform("weight", .1);
        QVERIFY(positive.toPlainText().contains("(masterpiece:1.1)"));
        QVERIFY(positive.toPlainText().contains("<lora:demo:0.9>"));
    }
    void promptActionPasteUsesSafeClipboardPathAndStaleGuard() {
        QWidget window; window.resize(700,500);
        PromptEditor editor(&window, false, PromptEditor::CompletionDisplay::Embedded);
        editor.setGeometry(20,160,450,150); window.show(); editor.setFocus();
        editor.setPlainText("cat, dog");
        QTRY_VERIFY(editor.hasFocus());
        auto actions = editor.findChild<PromptActions*>(); QVERIFY(actions);
        QApplication::clipboard()->setText("blue eyes");
        actions->open(1); actions->perform("paste");
        QTRY_COMPARE(editor.toPlainText(), QString("cat, blue eyes, dog"));
        editor.undo(); QCOMPARE(editor.toPlainText(), QString("cat, dog"));
        actions->open(1); actions->perform("paste");
        editor.replacePromptText("new document");
        QTest::qWait(50); QCOMPARE(editor.toPlainText(), QString("new document"));
    }
    void largeResultsKeepEventLoopAliveAndCancelLateDecode() {
        class Client : public OrchestrionClient {
        public:
            using OrchestrionClient::OrchestrionClient;
            void prepare(const QJsonObject&) override { }
            void fetchImage(const QJsonObject&, int) override { }
            void cancelJob() override { emit cancelled(); }
        };
        OrchestrionClient connection;
        Client* client = nullptr;
        JobQueue queue(&connection, nullptr, [&](QObject* parent) { client = new Client(parent); return client; });
        const auto id = queue.start({}, {{"target", "original"}}, {}, false);
        const auto job = queue.job(id);
        QImage source(2048,2048,QImage::Format_ARGB32);
        QRandomGenerator random(123);
        for (int y=0; y<source.height(); ++y) {
            auto line = reinterpret_cast<QRgb*>(source.scanLine(y));
            for (int x=0; x<source.width(); ++x) line[x] = random.generate() | 0xff000000;
        }
        const auto bytes = BaronPanel::png(source);
        const QJsonObject result{{"outputs", QJsonObject{{"node", QJsonObject{{"images", QJsonArray{QJsonObject{{"filename","test.png"}}}}}}}}};
        QSignalSpy complete(&queue, &JobQueue::completed);
        QTimer heartbeat; heartbeat.setInterval(2); int ticks = 0;
        qint64 gap = 0; QElapsedTimer elapsed; elapsed.start();
        connect(&heartbeat, &QTimer::timeout, &queue, [&] { gap = qMax(gap, elapsed.restart()); ++ticks; });
        heartbeat.start();
        client->jobReady(result);
        client->imageReady(bytes,0);
        QCOMPARE(complete.count(), 0);
        client->progressChanged(1); QCOMPARE(job->state, JobQueue::downloading);
        QTRY_COMPARE_WITH_TIMEOUT(complete.count(), 1, 15000);
        heartbeat.stop();
        QVERIFY2(ticks > 3, "UI heartbeat did not run during result processing");
        QVERIFY2(gap < 400, qPrintable(QString("Main-thread stall: %1ms").arg(gap)));
        QVERIFY(!job->result.bytes.isEmpty()); QCOMPARE(job->result.images.first(), source);
        const auto cancelledId = queue.start({}, {}, {}, false);
        client->jobReady(result); client->imageReady(bytes,0); queue.cancel(cancelledId);
        QTRY_VERIFY_WITH_TIMEOUT(queue.job(cancelledId)->decodingBytes == 0, 15000);
        QCOMPARE(complete.count(), 1); QCOMPARE(queue.job(cancelledId)->state, JobQueue::cancelled);
        auto disposable = new QObject;
        bool delivered = false;
        BackgroundWork::run(disposable, [] { QThread::msleep(30); return 1; }, [&](int) { delivered = true; });
        delete disposable; QTest::qWait(80); QVERIFY(!delivered);
    }
    void preparedHistoryPreservesAlphaAndAsyncRecovery() {
        QTemporaryDir dir; HistoryStore store("async-document", dir.path());
        QImage image(256,128,QImage::Format_ARGB32); image.fill(QColor(100,120,140,80));
        QImage mask(image.size(),QImage::Format_Grayscale8); mask.fill(128);
        const auto prepared = HistoryStore::prepare({image},BaronPanel::png(mask));
        QVERIFY(prepared.error.isEmpty());
        const auto id = store.appendPrepared(prepared, QRect(0,0,256,128), {{"prompt","alpha"}});
        QCOMPARE(store.image(id,0),image);
        QVERIFY(!store.cachedThumbnail(id,0,96).isNull());
        QMap<QString,QByteArray> annotations; bool saved = false;
        store.saveAsync(this,[&](const QString& key,const QByteArray& bytes){ annotations[key]=bytes; },[&](QString issue){ QVERIFY(issue.isEmpty()); saved=true; });
        QVERIFY(annotations.contains("ai_diffusion/ui.json"));
        QTRY_VERIFY(saved);
        HistoryStore reopened("async-document",dir.path()); QVERIFY(reopened.load([&](const QString& key){return annotations[key];}));
        QCOMPARE(reopened.image(id,0),image);
        const auto transparent = HistoryStore::prepare({image,mask},{},"background",image);
        QVERIFY(transparent.error.isEmpty()); QCOMPARE(transparent.images.size(),1);
        QCOMPARE(transparent.images[0].pixelColor(0,0).alpha(),40);
    }
    void historyAnnotationsAndRecovery() {
        QTemporaryDir directory;
        QMap<QString, QByteArray> annotations;
        annotations["ai_diffusion/ui.json"] = R"({"version":1,"root":{"prompt":"keep"},"custom":{"graph":"keep"},"history":[]})";
        auto read = [&](const QString& key) { return annotations.value(key); };
        auto write = [&](const QString& key, const QByteArray& bytes) { annotations[key] = bytes; };
        HistoryStore store("document-one", directory.path());
        QVERIFY(store.load(read));
        QImage red(24, 16, QImage::Format_ARGB32), blue(24, 16, QImage::Format_ARGB32);
        red.fill(qRgba(255, 0, 0, 70));
        blue.fill(Qt::blue);
        const QJsonObject settings { { "prompt", "character" }, { "negative", "blurry" },
            { "seed", "4294967295" }, { "strength", .43 }, { "model", "qwen.safetensors" } };
        const auto id = store.append({ red, blue }, QRect(12, 35, 24, 16), settings);
        QVERIFY(!id.isEmpty());
        store.mark(id, 1, true, true);
        QVERIFY(store.save(write));
        const auto state = QJsonDocument::fromJson(annotations["ai_diffusion/ui.json"]).object();
        QCOMPARE(state["root"].toObject()["prompt"].toString(), QString("keep"));
        QCOMPARE(state["custom"].toObject()["graph"].toString(), QString("keep"));
        QVERIFY(annotations.contains("ai_diffusion/result0.webp"));
        HistoryStore reopened("document-one", directory.path());
        QVERIFY(reopened.load(read));
        QCOMPARE(reopened.image(id, 0).pixelColor(0, 0), red.pixelColor(0, 0));
        QCOMPARE(reopened.image(id, 1).pixelColor(0, 0), blue.pixelColor(0, 0));
        QCOMPARE(HistoryStore::bounds(reopened.entries()[0].toObject()), QRect(12, 35, 24, 16));
        QCOMPARE(HistoryStore::settings(reopened.entries()[0].toObject())["seed"].toString(), QString("4294967295"));
        const auto next = reopened.append({ blue }, QRect(0, 0, 24, 16), settings);
        QVERIFY(reopened.save()); // Simulate a result after the last .kra save, then process termination.
        HistoryStore recovered("document-one", directory.path());
        QVERIFY(recovered.load(read));
        QCOMPARE(recovered.entries().size(), 2);
        QVERIFY(!recovered.image(next, 0).isNull());
        // A separately edited Windows document wins over an older local recovery cache.
        auto changed = state;
        changed["root"] = QJsonObject { { "prompt", "changed on Windows" } };
        annotations["ai_diffusion/ui.json"] = QJsonDocument(changed).toJson();
        HistoryStore windows("document-one", directory.path());
        QVERIFY(windows.load(read));
        QCOMPARE(windows.entries().size(), 1);
        QVERIFY(windows.remove(id, 0));
        QCOMPARE(windows.image(id, 0).pixelColor(0, 0), blue.pixelColor(0, 0));
        QVERIFY(windows.entries()[0].toObject()["in_use"].toObject()["0"].toBool());
        QVERIFY(windows.save(write));
        windows.clear();
        QVERIFY(windows.save(write));
        QVERIFY(annotations["ai_diffusion/result0.webp"].isEmpty());
        HistoryStore cleared("document-one", directory.path());
        QVERIFY(cleared.load(read));
        QCOMPARE(cleared.entries().size(), 0);
        HistoryStore other("document-two", directory.path());
        QVERIFY(other.load([](const QString&) { return QByteArray(); }));
        QCOMPARE(other.entries().size(), 0);
    }
    void progressFiltersAndTracksSamplerSteps() {
        OrchestrionClient client;
        const QJsonObject graph {
            { "1", QJsonObject { { "class_type", "LoadImage" } } },
            { "2", QJsonObject { { "class_type", "KSampler" }, { "inputs", QJsonObject { { "steps", 20 } } } } },
            { "3", QJsonObject { { "class_type", "SaveImage" } } } };
        client.submit(graph); // No connection: initializes graph without sending a paid job.
        client.resume("own-job");
        QSignalSpy progress(&client, &OrchestrionClient::progressChanged);
        auto event = [&](const QString& prompt, int step, int max = 20) {
            client.receiveProgress({ { "type", "progress" }, { "data", QJsonObject {
                { "prompt_id", prompt }, { "node", "2" }, { "value", step }, { "max", max } } } });
        };
        event("someone-else", 20);
        QCOMPARE(progress.size(), 0);
        event("own-job", 1, 0);
        QCOMPARE(progress.size(), 0);
        event("own-job", 5);
        QVERIFY(progress.last()[0].toDouble() > 0 && progress.last()[0].toDouble() < .3);
        event("own-job", 10);
        const auto halfway = progress.last()[0].toDouble();
        QVERIFY(halfway > .4 && halfway < .6);
        event("own-job", 2); // A second sampler pass must not make the bar go backwards.
        QCOMPARE(progress.last()[0].toDouble(), halfway);
        event("own-job", 20);
        QVERIFY(progress.last()[0].toDouble() < 1);
    }
    void pythonHistoryInteroperability() {
        const auto input = qEnvironmentVariable("BARON_PYTHON_HISTORY");
        const auto output = qEnvironmentVariable("BARON_NATIVE_HISTORY");
        if (input.isEmpty() || output.isEmpty())
            QSKIP("Set BARON_PYTHON_HISTORY and BARON_NATIVE_HISTORY for cross-runtime test");
        QFile file(input);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        QMap<QString, QByteArray> annotations;
        for (auto it = fixture.begin(); it != fixture.end(); ++it)
            annotations[it.key()] = QByteArray::fromBase64(it.value().toString().toLatin1());
        QTemporaryDir directory;
        HistoryStore store("python-document", directory.path());
        QVERIFY(store.load([&](const QString& key) { return annotations.value(key); }));
        QCOMPARE(store.entries().size(), 1);
        const auto python = store.entries()[0].toObject();
        QCOMPARE(HistoryStore::bounds(python), QRect(11, 29, 32, 24));
        QCOMPARE(HistoryStore::settings(python)["seed"].toString(), QString("4294967295"));
        QCOMPARE(store.image("python-job", 0).size(), QSize(32, 24));
        QCOMPARE(store.image("python-job", 1).pixelColor(0, 0).alpha(), 70);
        QImage rgba(32, 24, QImage::Format_ARGB32);
        rgba.fill(qRgba(10, 120, 240, 70));
        const auto native = store.append({ rgba, rgba }, QRect(42, 80, 32, 24),
            { { "prompt", "native character" }, { "negative", "blurry" }, { "seed", "123" },
              { "model", "qwen.safetensors" }, { "strength", .43 } });
        store.mark(native, 1, true, true);
        QVERIFY(store.save([&](const QString& key, const QByteArray& bytes) { annotations[key] = bytes; }));
        QJsonObject encoded;
        for (auto it = annotations.begin(); it != annotations.end(); ++it)
            encoded[it.key()] = QString::fromLatin1(it.value().toBase64());
        QFile result(output);
        QVERIFY(result.open(QIODevice::WriteOnly));
        QVERIFY(result.write(QJsonDocument(encoded).toJson()) > 0);
    }
    void pythonHistoryCopyPromptThenPasteInNewDocument() {
        auto fixturePath = qEnvironmentVariable("BARON_COPY_HISTORY_FIXTURE");
        if (fixturePath.isEmpty()) fixturePath = QFINDTESTDATA("fixtures/history-copy-from-python.json");
        QFile fixture(fixturePath);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto exported = QJsonDocument::fromJson(fixture.readAll()).object();
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QMap<QString, QByteArray>> data;
            QMap<QString, QJsonObject> states;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return true; }
            QString documentId() const override { return id; }
            QByteArray annotation(const QString& key) const override { return data.value(id).value(key); }
            void setAnnotation(const QString& key, const QByteArray& bytes) override { data[id][key] = bytes; }
            QJsonObject documentState() const override { return states.value(id); }
            void saveDocumentState(const QJsonObject& state) override { states[id] = state; }
        } host;
        for (auto it = exported.begin(); it != exported.end(); ++it)
            host.data[host.id][it.key()] = QByteArray::fromBase64(it.value().toString().toLatin1());
        const auto oldId = host.id;
        const auto pythonState = QJsonDocument::fromJson(host.annotation("ai_diffusion/ui.json")).object();
        const auto text = pythonState["history"].toArray().first().toObject()["params"].toObject()
            ["metadata"].toObject()["prompt"].toString();
        QVERIFY(text.contains(QChar(0x200b)));
        BaronPanel panel(&host, nullptr, PromptEditor::CompletionDisplay::Embedded);
        panel.resize(420, 900); panel.show(); panel.activateWindow();
        panel.documentChanged();
        auto history = panel.findChild<HistoryList*>("resultHistory");
        auto prompt = panel.findChild<PromptEditor*>("positivePrompt");
        auto negative = panel.findChild<PromptEditor*>("negativePrompt");
        auto copy = panel.findChild<QAction*>("history_copy");
        QVERIFY(history && prompt && negative && copy);
        QCOMPARE(history->count(), 3);
        for (int repeat = 0; repeat < 5; ++repeat) {
            host.id = oldId; panel.documentChanged();
            history->setCurrentItem(history->item(1));
            copy->trigger();
            QCOMPARE(prompt->toPlainText(), text);
            QCOMPARE(negative->toPlainText(), QString("blurry"));
            QCOMPARE(QApplication::clipboard()->text(), text);
            panel.flushDocumentState();
            host.id = QUuid::createUuid().toString();
            host.states[host.id] = { { "schema", 1 }, { "prompt", "" }, { "negative", "" } };
            panel.documentChanged();
            QCOMPARE(history->count(), 0);
            QCOMPARE(prompt->toPlainText(), QString());
            prompt->setFocus(); QCoreApplication::processEvents();
            QTest::keyClick(prompt, Qt::Key_V, Qt::ControlModifier);
            QTRY_COMPARE(prompt->toPlainText(), text);
            prompt->undo(); QCOMPARE(prompt->toPlainText(), QString());
            prompt->redo(); QCOMPARE(prompt->toPlainText(), text);
            panel.flushDocumentState();
            QCOMPARE(host.states[host.id]["prompt"].toString(), text);
        }
        QApplication::clipboard()->clear();
    }
    void documentSwitchCancelsQueuedPasteAndPreedit() {
        QWidget host; host.resize(420, 360);
        PromptEditor prompt(&host, false, PromptEditor::CompletionDisplay::Embedded);
        prompt.setGeometry(10, 10, 400, 180);
        host.show(); host.activateWindow(); prompt.setFocus();
        QCoreApplication::processEvents();
        prompt.replacePromptText("old project");
        PluginUi::copyText("obsolete clipboard request");
        QTest::keyClick(&prompt, Qt::Key_V, Qt::ControlModifier);
        prompt.resetInputState();
        QCoreApplication::processEvents();
        QCOMPARE(prompt.toPlainText(), QString("old project"));
        QInputMethodEvent composing("composition", {});
        QCoreApplication::sendEvent(&prompt, &composing);
        PluginUi::copyText("pending paste from old project");
        QTest::keyClick(&prompt, Qt::Key_V, Qt::ControlModifier);
        prompt.replacePromptText("new project");
        QCoreApplication::processEvents();
        QCOMPARE(prompt.toPlainText(), QString("new project"));
        prompt.selectAll();
        PluginUi::copyText("fresh paste");
        QTest::keyClick(&prompt, Qt::Key_V, Qt::ControlModifier);
        QTRY_COMPARE(prompt.toPlainText(), QString("fresh paste"));
        QApplication::clipboard()->clear();
    }
    void updateVersionUsesReleaseMetadataAndCurrentFeed() {
        QFile file(QFINDTESTDATA("../android/version.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto metadata = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(BaronUpdates::version(), metadata["version"].toString());
        QCOMPARE(BaronUpdates::installedVersionCode(), metadata["version_code"].toInt());
        QJsonObject package { { "version", metadata["version"] }, { "version_code", metadata["version_code"] },
            { "package", "org.krita.baron.debug" }, { "bytes", 42 },
            { "url", "https://orchestrion.su/baron-updates/releases/update.apk" },
            { "sha256", QString(64, 'a') } };
        QJsonObject manifest { { "schema", 1 }, { "edition", "baron" }, { "channel", "stable" }, { "android", package } };
        PreviewNetwork network;
        network.responses = { { 200, {}, QJsonDocument(manifest).toJson() } };
        BaronUpdates updater(nullptr, &network);
        QSignalSpy status(&updater, &BaronUpdates::message);
        updater.check();
        QTRY_COMPARE(updater.state(), BaronUpdates::latest);
        QVERIFY(status.last().first().toString().contains(BaronUpdates::version()));
        package["version_code"] = BaronUpdates::installedVersionCode() + 1;
        manifest["android"] = package;
        network.responses.append({ 200, {}, QJsonDocument(manifest).toJson() });
        updater.check();
        QTRY_COMPARE(updater.state(), BaronUpdates::available);
        QCOMPARE(network.requests.size(), 2);
        QVERIFY(network.requests[0].url().hasQuery());
        QCOMPARE(network.requests[0].attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(), int(QNetworkRequest::AlwaysNetwork));
    }
    void historyGroupsSameParametersAcrossSeedsAndWraps() {
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QByteArray> annotations;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage&, const QImage&, const QString&, QString*) override { return true; }
            QString documentId() const override { return id; }
            QByteArray annotation(const QString& key) const override { return annotations.value(key); }
            void setAnnotation(const QString& key, const QByteArray& bytes) override { annotations[key] = bytes; }
        } host;
        QTemporaryDir cache;
        HistoryStore store(host.id, cache.path());
        QImage image(32, 32, QImage::Format_ARGB32); image.fill(Qt::red);
        QJsonObject settings { { "prompt", "same prompt" }, { "model", "test.safetensors" }, { "strength", .43 } };
        QString first;
        for (int i = 0; i < 6; ++i) {
            settings["seed"] = QString::number(i);
            settings["fixed_seed"] = i % 2 == 0;
            settings["batch"] = i % 2 + 1;
            const auto id = store.append({ image }, QRect(0, 0, 32, 32), settings);
            if (!i) first = id;
        }
        store.mark(first, 0, true, true);
        settings["prompt"] = "different prompt";
        store.append({ image }, QRect(0, 0, 32, 32), settings);
        QVERIFY(store.save([&](const QString& key, const QByteArray& bytes) { host.annotations[key] = bytes; }));
        QWidget holder;
        BaronPanel panel(&host);
        panel.documentChanged();
        auto list = panel.findChild<HistoryList*>("resultHistory");
        QCOMPARE(list->count(), 9);
        QVERIFY(list->item(0)->data(Qt::UserRole).toMap()["header"].toBool());
        QVERIFY(list->item(7)->data(Qt::UserRole).toMap()["header"].toBool());
        QVERIFY(list->item(0)->text().contains("43% - same prompt"));
        QCOMPARE(list->item(0)->textAlignment(), int(Qt::AlignLeft));
        list->setParent(&holder);
        holder.resize(320, 500);
        list->setGeometry(holder.rect());
        holder.show();
        QCoreApplication::processEvents();
        const auto firstTile = list->visualItemRect(list->item(1));
        const auto secondTile = list->visualItemRect(list->item(2));
        QCOMPARE(firstTile.top(), secondTile.top());
        QVERIFY(secondTile.left() > firstTile.left());
        QVERIFY(list->visualItemRect(list->item(4)).top() > firstTile.top());
        QVERIFY(list->visualItemRect(list->item(7)).top() > list->visualItemRect(list->item(6)).bottom());
        QVERIFY(list->item(1)->data(Qt::UserRole).toMap()["favorite"].toBool());
        panel.documentChanged();
        QCOMPARE(list->count(), 9);
        QCOMPARE(list->item(0)->textAlignment(), int(Qt::AlignLeft));
    }
    void historyPanelReopensAndKeepsDocumentsSeparate() {
        class Host : public CanvasHost {
        public:
            QString id = QUuid::createUuid().toString();
            QMap<QString, QMap<QString, QByteArray>> data;
            QRect restored;
            int applied = 0;
            CanvasSnapshot capture(bool, QString*) override { return {}; }
            bool apply(const QString&, const QImage& image, const QImage&, const QString&, QString*) override {
                ++applied;
                return image.size() == QSize(32, 24);
            }
            bool preview(const QString&, const QImage&, const QImage&, QString*) override { return true; }
            QString documentId() const override { return id; }
            QByteArray annotation(const QString& key) const override { return data.value(id).value(key); }
            void setAnnotation(const QString& key, const QByteArray& bytes) override { data[id][key] = bytes; }
            QString restoreTarget(const QRect& area, const QImage&) override { restored = area; return "restored"; }
        } host;
        QTemporaryDir directory;
        HistoryStore store(host.id, directory.path());
        QVERIFY(store.load([&](const QString& key) { return host.annotation(key); }));
        QImage image(32, 24, QImage::Format_ARGB32);
        image.fill(Qt::red);
        store.append({ image }, QRect(42, 80, 32, 24), { { "prompt", "from desktop" }, { "seed", "123" } });
        QVERIFY(store.save([&](const QString& key, const QByteArray& bytes) { host.setAnnotation(key, bytes); }));
        {
            BaronPanel panel(&host);
            panel.documentChanged();
            auto history = panel.findChild<QListWidget*>("resultHistory");
            QCOMPARE(history->count(), 2);
            history->itemClicked(history->item(1));
            QCOMPARE(host.restored, QRect(42, 80, 32, 24));
            history->itemDoubleClicked(history->item(1));
            QCOMPARE(host.applied, 1);
            const auto first = host.id;
            host.id = QUuid::createUuid().toString();
            panel.documentChanged();
            QCOMPARE(history->count(), 0);
            host.id = first;
            panel.documentChanged();
            QCOMPARE(history->count(), 2);
        }
        BaronPanel reopened(&host);
        reopened.documentChanged();
        auto history = reopened.findChild<QListWidget*>("resultHistory");
        QCOMPARE(history->count(), 2);
        QVERIFY(history->item(1)->data(Qt::UserRole).toMap()["applied"].toBool());
        auto categories = reopened.findChild<QListWidget*>("settingsCategories");
        QVERIFY(categories);
        QCOMPARE(categories->count(), 6);
        categories->setCurrentRow(3);
        QCOMPARE(reopened.findChild<QStackedWidget*>("settingsPages")->currentIndex(), 3);
    }
    void deviceLoginPrepareAndDownload() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int authorized = 0;
        bool cancelTest = false;
        int targetedInterrupts = 0;
        QImage source(8, 8, QImage::Format_ARGB32);
        source.fill(qRgba(80, 120, 160, 70));
        const auto image = BaronPanel::png(source);
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                QByteArray bytes
                    = socket->property("requestBytes").toByteArray() + socket->readAll();
                socket->setProperty("requestBytes", bytes);
                int boundary = bytes.indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("replied").toBool())
                    return;
                int length = 0;
                for (auto line : bytes.left(boundary).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                if (bytes.size() < boundary + 4 + length)
                    return;
                socket->setProperty("replied", true);
                const auto path = bytes.split(' ').value(1);
                if (!path.contains("/device/")) {
                    QVERIFY(bytes.contains("Authorization: Bearer ork_krita_test"));
                    ++authorized;
                }
                QByteArray response;
                if (path.contains("/device/start"))
                    response
                        = R"({"device_code":"abc","user_code":"AB12345678","verification_uri":"/connect/krita","expires_in":600,"interval":5})";
                else if (path.contains("/device/poll"))
                    response = R"({"access_token":"ork_krita_test"})";
                else if (path.contains("/account"))
                    response = R"({"name":"Test artist","coins":42})";
                else if (path.contains("/models"))
                    response = R"({"items":[]})";
                else if (path.contains("/object_info"))
                    response = "{}";
                else if (path.contains("/native/prepare"))
                    response
                        = R"({"prompt":{"1":{"class_type":"SaveImage","inputs":{}}},"coins":7})";
                else if (path.contains("/prompt"))
                    response = R"({"prompt_id":"test-job"})";
                else if (path.contains("/queue")) {
                    QVERIFY(cancelTest);
                    response
                        = R"({"queue_running":[[0,"test-job"],[1,"someone-else"]],"queue_pending":[]})";
                } else if (path.contains("/interrupt")) {
                    QVERIFY(cancelTest);
                    const auto body = QJsonDocument::fromJson(bytes.mid(boundary + 4)).object();
                    QCOMPARE(body["prompt_id"].toString(), QString("test-job"));
                    QVERIFY(!body.contains("clear"));
                    ++targetedInterrupts;
                    response = "{}";
                } else if (path.contains("/history/"))
                    response
                        = R"({"test-job":{"status":{"status_str":"success"},"outputs":{"1":{"images":[{"filename":"result.png","type":"output","subfolder":""}]}}}})";
                else if (path.contains("/view?"))
                    response = image;
                else if (path.contains("/connection/ws?")) {
                    socket->write("HTTP/1.1 503 Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                else
                    QFAIL("Unexpected native client request");
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(response.size())
                    + "\r\nConnection: close\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        OrchestrionClient client;
        QVERIFY(client.setRoot(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        QSignalSpy login(&client, &OrchestrionClient::authenticated),
            prepared(&client, &OrchestrionClient::prepared),
            submitted(&client, &OrchestrionClient::submitted),
            ready(&client, &OrchestrionClient::jobReady),
            downloaded(&client, &OrchestrionClient::imageReady),
            errors(&client, &OrchestrionClient::error);
        client.signIn();
        QTRY_COMPARE_WITH_TIMEOUT(login.size(), 1, 7000);
        client.prepare({ { "mode", "generate" } });
        QTRY_COMPARE(prepared.size(), 1);
        client.submit(prepared.first().first().toJsonObject()["prompt"].toObject());
        QTRY_COMPARE(submitted.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 4000);
        const auto descriptor = ready.first()
                                    .first()
                                    .toJsonObject()["outputs"]
                                    .toObject()["1"]
                                    .toObject()["images"]
                                    .toArray()
                                    .first()
                                    .toObject();
        client.fetchImage(descriptor, 0);
        QTRY_COMPARE(downloaded.size(), 1);
        QCOMPARE(QImage::fromData(downloaded.first().first().toByteArray()).pixel(0, 0),
            source.pixel(0, 0));
        QVERIFY(authorized >= 6);
        QCOMPARE(errors.size(), 0);
        cancelTest = true;
        QSignalSpy cancelled(&client, &OrchestrionClient::cancelled);
        client.submit(prepared.first().first().toJsonObject()["prompt"].toObject());
        client.cancelJob();
        QTRY_COMPARE(cancelled.size(), 1);
        QCOMPARE(targetedInterrupts, 1);
        QCOMPARE(errors.size(), 0);
    }
};
int main(int argc,char** argv) {
    QApplication app(argc,argv);
#ifdef Q_OS_WIN
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    app.setFont(QFont("Segoe UI",10));
#endif
    if(app.arguments().contains("--verify-orientation-profile")) {
        QSettings settings("BaronEdition","Orchestrion");settings.sync();
        return QCryptographicHash::hash(settings.value("orientation_layout/portrait").toByteArray(),QCryptographicHash::Sha256).toHex()==app.arguments().last().toLatin1()?0:2;
    }
    ClientTest test;return QTest::qExec(&test,argc,argv);
}
#include "ClientTest.moc"
