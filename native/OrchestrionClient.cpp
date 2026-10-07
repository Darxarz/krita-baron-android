// SPDX-License-Identifier: GPL-3.0-or-later
#include "OrchestrionClient.h"
#include "Credentials.h"
#include "CloudWorkflow.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QImageReader>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QSettings>
#include <QSharedPointer>
#include <QUrlQuery>
#include <QUuid>

OrchestrionClient::OrchestrionClient(QObject* parent, QNetworkAccessManager* transport)
    : QObject(parent), m_transport(transport ? transport : &m_network) {
    m_loginTimer.setInterval(5000);
    m_jobTimer.setInterval(qBound(1, QSettings("BaronEdition", "Orchestrion")
        .value("pollInterval", 2).toInt(), 10) * 1000);
    m_clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_reconnect.setSingleShot(true);
    connect(&m_reconnect, &QTimer::timeout, this, &OrchestrionClient::openProgress);
    connect(&m_socket, &QWebSocket::connected, this, [this] { m_reconnectDelay = 2000; });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, [this](const QString& message) {
        receiveProgress(QJsonDocument::fromJson(message.toUtf8()).object());
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        if (!m_promptId.isEmpty() && m_jobTimer.isActive()) {
            m_reconnect.start(m_reconnectDelay);
            m_reconnectDelay = qMin(30000, m_reconnectDelay * 2);
        }
    });
    connect(&m_loginTimer, &QTimer::timeout, this, &OrchestrionClient::pollLogin);
    connect(&m_jobTimer, &QTimer::timeout, this, &OrchestrionClient::pollJob);
    connect(this, &OrchestrionClient::error, this, [this] {
        m_cancelling = false;
        m_submitting = false;
        m_pollingJob = false;
        m_jobTimer.stop();
        m_loginTimer.stop();
        m_reconnect.stop();
        m_socket.close();
    });
}

OrchestrionClient::~OrchestrionClient() {
    disconnect(&m_socket, nullptr, this, nullptr);
    for (auto reply : m_requests) {
        if (!reply) continue;
        disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished()) reply->abort();
    }
}
QUrl OrchestrionClient::validateRoot(const QString& text, QString* error) {
    const QUrl url(text.trimmed());
    auto reject = [&](const QString& message) {
        if (error)
            *error = message;
        return QUrl();
    };
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != "https" && url.scheme() != "http"))
        return reject(tr("Enter the website address: https://orchestrion.su"));
    if (!url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (!url.path().isEmpty() && url.path() != "/"))
        return reject(tr("Enter the website root address without a token or password"));
    if (url.scheme() == "http") {
        QHostAddress ip(url.host());
        bool local = url.host() == "localhost" || ip.isLoopback()
            || ip.isInSubnet(QHostAddress("10.0.0.0"), 8)
            || ip.isInSubnet(QHostAddress("172.16.0.0"), 12)
            || ip.isInSubnet(QHostAddress("192.168.0.0"), 16)
            || ip.isInSubnet(QHostAddress("fc00::"), 7);
        if (!local)
            return reject(tr("Internet connections require HTTPS"));
    }
    QUrl result = url;
    result.setPath("");
    return result;
}

bool OrchestrionClient::setRoot(const QString& text) {
    QString message;
    QUrl url;
    QByteArray query;
    if (m_backend == comfyui) {
        const auto value = text.contains("://") ? text : QString("http://" + text.trimmed());
        QUrl candidate(value);
        if (candidate.isValid() && !candidate.host().isEmpty() && candidate.userInfo().isEmpty()
            && !candidate.hasFragment() && (candidate.scheme() == "https" || candidate.scheme() == "http")) {
            query = candidate.query(QUrl::FullyEncoded).toUtf8();
            candidate.setQuery(QString());
            auto path = candidate.path();
            while (path.endsWith('/')) path.chop(1);
            candidate.setPath(path);
            url = candidate;
        } else message = tr("Enter the ComfyUI server address");
    } else url = validateRoot(text, &message);
    if (url.isEmpty()) {
        emit error(message);
        return false;
    }
    if (url != m_root || query != m_externalQuery) {
        m_reconnect.stop();
        m_socket.abort();
        cancelSignIn();
        m_token.clear();
        m_connected = false;
        m_submitting = false;
        m_cloudImages.clear();
        m_cloudOffsets = {};
        m_jobTimer.stop();
        m_pollingJob = false;
        m_promptId.clear();
        ++m_generation;
    }
    m_root = url;
    m_externalQuery = query;
    return true;
}
void OrchestrionClient::setBackend(Backend backend) {
    if (backend == m_backend) return;
    ++m_generation;
    m_socket.abort(); m_reconnect.stop(); m_jobTimer.stop(); m_loginTimer.stop();
    m_promptId.clear(); m_token.clear(); m_externalQuery.clear();
    m_cloudResources = {}; m_cloudWorkflow = {}; m_cloudImages.clear(); m_cloudOffsets = {};
    m_pollingJob = false; m_submitting = false; m_connected = false;
    m_backend = backend;
    m_jobTimer.setInterval(backend == interstice ? 500 : qBound(1, QSettings("BaronEdition", "Orchestrion")
        .value("pollInterval", 2).toInt(), 10) * 1000);
    m_root = QUrl();
}
void OrchestrionClient::setAccessToken(const QByteArray& token) {
    if (token != m_token) {
        ++m_generation;
        m_socket.abort(); m_reconnect.stop(); m_jobTimer.stop();
        m_connected = false;
        m_token = token;
    }
}
QString OrchestrionClient::transportPath(const QString& path) const {
    return m_backend == comfyui && path.startsWith("/krita/connection/") ? path.mid(17) : path;
}
QUrl OrchestrionClient::serviceUrl(const QString& path) const {
    QUrl relative(transportPath(path));
    auto url = m_root;
    url.setPath(m_root.path() + relative.path());
    QUrlQuery query(QString::fromUtf8(m_externalQuery));
    for (const auto& item : QUrlQuery(relative).queryItems()) query.addQueryItem(item.first, item.second);
    url.setQuery(query);
    return url;
}
void OrchestrionClient::copyConnection(const OrchestrionClient& other) {
    setBackend(other.m_backend);
    setRoot(other.m_root.toString());
    m_token = other.m_token;
    m_externalQuery = other.m_externalQuery;
    m_connected = other.m_connected;
    m_cloudResources = other.m_cloudResources;
}

void OrchestrionClient::request(
    const QString& path, const QJsonObject& body, bool post, Callback done, bool auth) {
    if (m_root.isEmpty()) {
        emit error(tr("Choose an Orchestrion website first"));
        return;
    }
    if (auth && !signedIn()) {
        emit error(tr("Sign in required"));
        return;
    }
    const QUrl url = serviceUrl(path);
    if (url.host() != m_root.host() || url.scheme() != m_root.scheme()
        || url.port() != m_root.port()) {
        emit error(tr("The server returned an invalid address"));
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(path == "/api/translate" ? 100000 : path == "/api/prompt/organize/labels" ? 22000 : 60000);
    if (!m_token.isEmpty() && (auth || m_backend == comfyui))
        req.setRawHeader("Authorization", "Bearer " + m_token);
    auto reply = post ? m_transport->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact))
                      : m_transport->get(req);
    m_requests.append(reply);
    const int timeout = path == "/api/translate" ? 100000 : path == "/api/prompt/organize/labels" ? 22000 : m_cancelRequested && !m_submitting ? 15000
        : post && (path.endsWith("/prepare") || path.endsWith("/prompt") || path == "/generate") ? 120000 : 60000;
    QTimer::singleShot(timeout, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
        m_requests.removeAll(reply);
        const auto bytes = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto failure = reply->error();
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (status == 401) {
            m_token.clear();
            m_connected = false;
            BaronCredentials::clear(m_root);
            m_jobTimer.stop();
            emit error(tr("Your login expired. Sign in again."));
            return;
        }
        if (status >= 300 && status < 400) {
            emit error(tr("The server redirected this request. Check the website address."));
            return;
        }
        if (status < 200 || status >= 300 || failure != QNetworkReply::NoError) {
            const auto issue = QJsonDocument::fromJson(bytes).object()["error"].toString();
            emit error(issue.isEmpty() ? tr("Server request failed (%1): %2").arg(status).arg(reply->errorString()) : issue);
            return;
        }
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
            emit error(tr("The server returned an invalid response"));
            return;
        }
        done(document.object());
    });
}
void OrchestrionClient::translatePrompt(const QString& text, const QString& mode,
    std::function<void(const QJsonObject&)> done, const QString& model) {
    if (m_backend != orchestrion || !signedIn()) { emit error(tr("Your login expired. Sign in again.")); return; }
    request("/api/translate", {{"text", text}, {"mode", mode}, {"model", model}}, true, std::move(done));
}
void OrchestrionClient::promptOrganizerCapabilities(std::function<void(const QJsonObject&)> done) {
    request("/api/prompt/organize/capabilities", {}, false, std::move(done), false);
}
void OrchestrionClient::promptOrganizerLabels(const QJsonArray& fragments, const QString& family,
    std::function<void(const QJsonObject&)> done) {
    request("/api/prompt/organize/labels", {{"segments", fragments}, {"family", family}}, true, std::move(done));
}

void OrchestrionClient::signIn() {
    cancelSignIn();
    if (m_backend == comfyui) {
        request("/system_stats", {}, false, [this](const QJsonObject& data) {
            m_connected = true;
            BaronCredentials::save(m_root, QJsonDocument(QJsonObject {
                { "token", QString::fromUtf8(m_token) }, { "query", QString::fromUtf8(m_externalQuery) }
            }).toJson(QJsonDocument::Compact));
            emit authenticated(); emit accountReady({ { "name", "ComfyUI" }, { "system", data } });
            models();
        }, false);
        return;
    }
    if (m_backend == interstice) {
        m_deviceCode = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request("/auth/initiate", { { "client_id", m_deviceCode },
            { "client_info", "Generative AI for Krita — Baron Android port" } }, true,
            [this](const QJsonObject& data) {
                const QUrl url = QUrl("https://www.interstice.cloud").resolved(QUrl(data["url"].toString()));
                if (data["url"].toString().isEmpty() || url.host() != "www.interstice.cloud"
                    || url.scheme() != "https" || !url.userInfo().isEmpty()) {
                    emit error(tr("The server returned an invalid login address")); return;
                }
                m_loginDeadline = QDateTime::currentMSecsSinceEpoch() + 300000;
                m_loginTimer.start(2000);
                emit browserLogin(url, {});
            }, false);
        return;
    }
    request(
        "/api/krita/device/start", {}, true,
        [this](const QJsonObject& data) {
            m_deviceCode = data["device_code"].toString();
            const auto code = data["user_code"].toString();
            const auto address = m_root.resolved(QUrl(data["verification_uri"].toString()));
            if (m_deviceCode.isEmpty() || code.isEmpty() || address.host() != m_root.host()
                || address.scheme() != m_root.scheme() || address.port() != m_root.port()) {
                emit error(tr("The server returned an invalid login address"));
                return;
            }
            QUrl browser = address;
            QUrlQuery query(browser);
            query.addQueryItem("code", code);
            query.addQueryItem("lang", QLocale().name().section('_', 0, 0));
            browser.setQuery(query);
            m_loginDeadline = QDateTime::currentMSecsSinceEpoch()
                + qBound(5, data["expires_in"].toInt(), 600) * 1000;
            m_loginTimer.start(qBound(5, data["interval"].toInt(), 30) * 1000);
            emit browserLogin(browser, code);
        },
        false);
}

void OrchestrionClient::cancelSignIn() {
    m_loginTimer.stop();
    m_deviceCode.clear();
    ++m_generation;
}
void OrchestrionClient::pollLogin() {
    if (QDateTime::currentMSecsSinceEpoch() >= m_loginDeadline) {
        cancelSignIn();
        emit error(tr("Sign-in timed out. Start again."));
        return;
    }
    m_loginTimer.stop();
    if (m_backend == interstice) {
        request("/auth/confirm", { { "client_id", m_deviceCode } }, true, [this](const QJsonObject& data) {
            if (data["status"] == "not-found") { m_loginTimer.start(); return; }
            if (data["status"] != "authorized" || data["token"].toString().isEmpty()) {
                cancelSignIn(); emit error(tr("Connection denied or code expired. Start sign-in again.")); return;
            }
            m_token = data["token"].toString().toUtf8();
            BaronCredentials::save(m_root, m_token); m_deviceCode.clear();
            emit authenticated(); account(); models();
        }, false);
        return;
    }
    request(
        "/api/krita/device/poll", { { "device_code", m_deviceCode } }, true,
        [this](const QJsonObject& data) {
            const auto issue = data["error"].toString();
            if (issue == "authorization_pending") {
                m_loginTimer.start();
                return;
            }
            if (issue == "slow_down") {
                m_loginTimer.start(m_loginTimer.interval() + 5000);
                return;
            }
            const auto token = data["access_token"].toString();
            if (!issue.isEmpty() || !token.startsWith("ork_krita_")) {
                cancelSignIn();
                emit error(tr("Connection denied or code expired. Start sign-in again."));
                return;
            }
            m_token = token.toUtf8();
            BaronCredentials::save(m_root, m_token);
            m_deviceCode.clear();
            emit authenticated();
            account();
            models();
        },
        false);
}
void OrchestrionClient::signOut() {
    m_reconnect.stop();
    m_socket.close();
    if (!m_token.isEmpty() && m_backend == orchestrion)
        request("/api/krita/logout", {}, true, [](const QJsonObject&) { });
    cancelSignIn();
    m_token.clear();
    BaronCredentials::clear(m_root);
    m_connected = false;
    m_jobTimer.stop();
    m_pollingJob = false;
    emit accountReady({});
}
void OrchestrionClient::account() {
    if (m_backend == comfyui) { emit accountReady({ { "name", "ComfyUI" } }); return; }
    if (m_backend == interstice) {
        request("/user?plugin_version=1.53.0", {}, false, [this](const auto& data) { emit accountReady(data); }); return;
    }
    request("/api/krita/account", {}, false, [this](const auto& data) { emit accountReady(data); });
}
void OrchestrionClient::restoreLogin(bool connect) {
    const auto saved = BaronCredentials::load(m_root);
    if (m_backend == comfyui) {
        const auto connection = QJsonDocument::fromJson(saved).object();
        m_token = connection["token"].toString().toUtf8();
        if (m_externalQuery.isEmpty()) m_externalQuery = connection["query"].toString().toUtf8();
        if (connect && !saved.isEmpty()) signIn();
        return;
    }
    m_token = saved;
    if (!m_token.isEmpty()) {
        account();
        models();
    }
}
void OrchestrionClient::catalogRequest(const QString& path,
    std::function<void(const QJsonObject&, const QString&)> done, bool refresh, int cacheSeconds) {
    if (m_backend != orchestrion || !signedIn() || m_root.isEmpty()) {
        done({}, tr("Model information is unavailable for this connection")); return;
    }
    const int generation = m_generation;
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/baron/model-information";
    const QByteArray identity = m_root.toEncoded() + m_token + path.toUtf8();
    const QString file = m_transport == &m_network ? directory + "/" + QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex()) + ".json" : QString();
    auto download = [this, path, done, generation, directory, file](const QJsonObject& cached) {
        if (generation != m_generation) { done({}, tr("Connection changed")); return; }
        if (!cached.isEmpty()) { done(cached, {}); return; }
        const QUrl url(m_root.toString(QUrl::FullyEncoded) + path);
        if (url.host() != m_root.host() || url.scheme() != m_root.scheme() || url.port() != m_root.port()) {
            done({}, tr("The server returned an invalid address")); return;
        }
        QNetworkRequest request(url);
        request.setTransferTimeout(45000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setRawHeader("Authorization", "Bearer " + m_token);
        auto reply = m_transport->get(request);
        m_requests.append(reply);
        auto bytes = QSharedPointer<QByteArray>::create();
        connect(reply, &QNetworkReply::readyRead, reply, [reply, bytes] {
            bytes->append(reply->readAll());
            if (bytes->size() > 16 * 1024 * 1024) reply->abort();
        });
        QTimer::singleShot(45000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
        connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, generation, directory, file, done] {
            m_requests.removeAll(reply);
            bytes->append(reply->readAll());
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && bytes->size() <= 16 * 1024 * 1024;
            reply->deleteLater();
            if (generation != m_generation) { done({}, tr("Connection changed")); return; }
            // Optional catalogue failures must never stop a running generation.
            if (!ok) { done({}, tr("No author information is available (%1)").arg(status)); return; }
            const auto body = *bytes;
            BackgroundWork::run(this, [body, directory, file] {
                const auto data = QJsonDocument::fromJson(body).object();
                if (!data.isEmpty() && data.value("success") != false && !file.isEmpty() && QDir().mkpath(directory)) {
                    QSaveFile output(file);
                    if (output.open(QIODevice::WriteOnly)) { output.write(QJsonDocument(data).toJson(QJsonDocument::Compact)); output.commit(); }
                    const auto files = QDir(directory).entryInfoList({"*.json"}, QDir::Files, QDir::Time);
                    qint64 size = 0;
                    for (const auto& entry : files) { size += entry.size(); if (size > 64 * 1024 * 1024) QFile::remove(entry.absoluteFilePath()); }
                }
                return data;
            }, [this, done, generation](const QJsonObject& data) {
                if (generation != m_generation) { done({}, tr("Connection changed")); return; }
                done(data, data.isEmpty() || data.value("success") == false ? tr("No author information is available") : QString());
            }, BackgroundWork::history());
        });
    };
    if (file.isEmpty() || refresh) { download({}); return; }
    BackgroundWork::run(this, [file, cacheSeconds] {
        if (QFileInfo(file).lastModified().secsTo(QDateTime::currentDateTime()) > cacheSeconds) return QJsonObject();
        QFile input(file);
        if (!input.open(QIODevice::ReadOnly) || input.size() > 16 * 1024 * 1024) return QJsonObject();
        return QJsonDocument::fromJson(input.readAll()).object();
    }, download);
}
void OrchestrionClient::modelMetadata(const QString& name, const QString& kind,
    std::function<void(const QJsonObject&, const QString&)> done, bool refresh) {
    const QString path = "/api/krita/model-metadata?name=" + QString::fromLatin1(QUrl::toPercentEncoding(name))
        + "&kind=" + QString(kind == "lora" ? "lora" : "checkpoint");
    catalogRequest(path, [done](const QJsonObject& data, const QString& error) { done(data.value("data").toObject(), error); }, refresh);
}
void OrchestrionClient::models() {
    if (m_backend == comfyui) {
        request("/baron/native/models", {}, false, [this](const auto& data) { emit modelsReady(data); }); return;
    }
    if (m_backend == interstice) {
        request("/plugin/resources", {}, false, [this](const auto& data) { cloudCatalog(data); }); return;
    }
    request("/api/krita/models", {}, false, [this](const auto& data) {
        request("/krita/connection/object_info", {}, false, [this, data](const auto& info) {
            const auto choices = [&info](const QString& node, const QString& field) {
                const auto values = info[node].toObject()["input"].toObject()["required"].toObject()[field].toArray();
                return values.isEmpty() ? QJsonArray() : values.first().toArray();
            };
            QJsonObject resourceModels;
            const QList<QPair<QString, QStringList>> tileNames {
                { "sd15", { "control_v11f1e_sd15_tile", "control_lora_rank128_v11f1e_sd15_tile" } },
                { "sdxl", { "xinsirtile", "tile-sdxl", "ttplanetsdxlcontrolnet", "ttplanet_sdxl_controlnet_tile_realistic", "ttplanet_controlnet_tile_realistic" } },
                { "illu", { "noob-sdxl-controlnet-tile", "noobaixlcontrolnet_epstile" } },
                { "flux", { "flux.1-dev-controlnet-upscale" } },
                { "zimage", { "z-image-turbo-fun-controlnet-tile" } }
            };
            for (const auto& family : tileNames)
                for (const auto& value : choices(family.first == "zimage" ? "ModelPatchLoader" : "ControlNetLoader",
                         family.first == "zimage" ? "name" : "control_net_name"))
                    for (const auto& pattern : family.second)
                        if (value.toString().contains(pattern, Qt::CaseInsensitive))
                            resourceModels[(family.first == "zimage" ? "model_patch-blur-" : "controlnet-blur-") + family.first] = value;
            auto catalog = data;
            catalog["resources"] = QJsonObject { { "upscalers", choices("UpscaleModelLoader", "model_name") }, { "resources", resourceModels },
                { "vae", choices("VAELoader", "vae_name") },
                { "a1111_prompt", info.contains("smZ CLIPTextEncode") }, { "a1111_gpu_noise", info.contains("smZ Settings") } };
            emit modelsReady(catalog);
        });
    });
}
void OrchestrionClient::cloudCatalog(const QJsonObject& data) {
    m_cloudResources = data;
    QJsonArray items;
    const auto checkpoints = data["checkpoints"].toObject();
    for (auto it = checkpoints.begin(); it != checkpoints.end(); ++it) {
        const auto model = it.value().toObject();
        QString title = model["filename"].toString(it.key());
        if (title.endsWith(".safetensors")) title.chop(12);
        items.append(QJsonObject { { "name", it.key() }, { "title", title }, { "kind", model["format"].toString("checkpoint") },
            { "architecture", model["arch"] }, { "family", model["arch"] } });
    }
    for (const auto& value : data["loras"].toArray()) {
        const auto name = value.toString();
        if (!name.isEmpty()) items.append(QJsonObject { { "name", name }, { "title", name }, { "kind", "lora" } });
    }
    emit modelsReady({ { "items", items }, { "resources", data } });
}
void OrchestrionClient::pollCloudJob() {
    m_pollingJob = true;
    request("/status/" + QString::fromUtf8(QUrl::toPercentEncoding(m_promptId)), {}, true, [this](const QJsonObject& data) {
        m_pollingJob = false;
        const auto state = data["status"].toString().toLower();
        if (state == "in_queue") { emit jobRunning(false); return; }
        if (state == "in_progress") {
            emit jobRunning(true);
            emit progressChanged(qBound(0.0, data["output"].toObject()["progress"].toDouble(.09), .99));
            return;
        }
        m_jobTimer.stop();
        if (state == "completed") receiveCloudImages(data["output"].toObject()["images"].toObject());
        else if (state == "cancelled") emit cancelled();
        else emit error(data["error"].toString(tr("Generation failed on the server. No result was applied.")));
    });
}
void OrchestrionClient::receiveCloudImages(const QJsonObject& data) {
    m_cloudOffsets = data["offsets"].toArray();
    if (m_cloudOffsets.isEmpty() || m_cloudOffsets.size() > 4) { emit error(tr("No images in the server result")); return; }
    auto ready = [this](const QByteArray& bytes) {
        if (bytes.isEmpty() || bytes.size() > 128 * 1024 * 1024) { emit error(tr("Could not download the result")); return; }
        int previous = -1;
        for (const auto& value : m_cloudOffsets) {
            const int offset = value.toInt(-1);
            if (!value.isDouble() || value.toDouble() != offset || offset <= previous
                || offset >= bytes.size() || (previous == -1 && offset != 0)) {
                emit error(tr("The server returned an invalid response")); return;
            }
            previous = offset;
        }
        m_cloudImages = bytes;
        QJsonArray images;
        for (int i = 0; i < m_cloudOffsets.size(); ++i) images.append(QJsonObject { { "cloud_index", i } });
        emit jobReady({ { "outputs", QJsonObject { { "cloud", QJsonObject { { "images", images } } } } } });
    };
    if (data["base64"].isString()) {
        if (data["base64"].toString().size() > 180 * 1024 * 1024) { emit error(tr("Could not download the result")); return; }
        ready(QByteArray::fromBase64(data["base64"].toString().toLatin1())); return;
    }
    const QUrl url(data["url"].toString());
    if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()) {
        emit error(tr("The server returned an invalid address")); return;
    }
    QNetworkRequest req(url);
    req.setTransferTimeout(60000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto reply = m_transport->get(req);
    m_requests.append(reply);
    QTimer::singleShot(60000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::readyRead, reply, [reply] { if (reply->bytesAvailable() > 128 * 1024 * 1024) reply->abort(); });
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, ready, generation] {
        m_requests.removeAll(reply);
        const auto bytes = reply->readAll(); const auto failure = reply->error(); reply->deleteLater();
        if (generation != m_generation) return;
        if (failure != QNetworkReply::NoError) { emit error(tr("Could not download the result")); return; }
        ready(bytes);
    });
}
void OrchestrionClient::quote(const QJsonObject& prompt) {
    if (m_backend != orchestrion) { emit quoted({ { "available", false } }); return; }
    request("/api/krita/quote", { { "prompt", prompt } }, true,
        [this](const auto& data) { emit quoted(data); });
}
void OrchestrionClient::prepare(const QJsonObject& input) {
    if (m_backend == interstice) {
        ++m_generation;
        m_promptId.clear(); m_jobTimer.stop(); m_pollingJob = false;
        m_cancelRequested = false; m_cloudWorker.clear(); m_cloudImages.clear(); m_cloudOffsets = {};
        QString issue;
        const auto work = CloudWorkflow::prepare(input, m_cloudResources, &issue);
        if (!issue.isEmpty()) { emit error(issue); return; }
        emit prepared({ { "prompt", work }, { "metadata", CloudWorkflow::metadata(work) } });
        return;
    }
    openProgress();
    ++m_generation;
    m_promptId.clear();
    m_cancelRequested = false;
    m_jobTimer.stop();
    m_pollingJob = false;
    request(m_backend == comfyui ? "/baron/native/prepare" : "/api/krita/native/prepare", input, true, [this](const auto& data) {
        if (data["prompt"].toObject().isEmpty() || (m_backend == orchestrion && (!data["coins"].isDouble()
            || data["coins"].toDouble() < 0))) {
            emit error(tr("The server returned an invalid response"));
            return;
        }
        emit prepared(data);
    });
}
void OrchestrionClient::submit(const QJsonObject& graph, bool front) {
    if (m_backend == interstice) {
        m_submitting = true; m_cancelRequested = false; m_cloudWorkflow = graph;
        const auto images = graph["image_data"].toObject();
        if (images["base64"].toString().size() <= 4096) { sendCloudWorkflow(graph); return; }
        const auto bytes = QByteArray::fromBase64(images["base64"].toString().toLatin1());
        if (bytes.isEmpty() || bytes.size() > 64 * 1024 * 1024) {
            emit error(tr("Could not upload the input images")); return;
        }
        request("/upload/image", {}, true, [this, bytes, images, graph](const QJsonObject& data) {
            if (m_cancelRequested) { m_submitting = false; emit cancelled(); return; }
            const QUrl url(data["url"].toString());
            if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty()
                || data["object"].toString().isEmpty()) {
                emit error(tr("The server returned an invalid address")); return;
            }
            QNetworkRequest upload(url);
            upload.setTransferTimeout(60000);
            upload.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
            upload.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
            auto reply = m_transport->put(upload, bytes);
            const int generation = m_generation;
            connect(reply, &QNetworkReply::finished, this, [this, reply, graph, images, data, generation] {
                const auto failure = reply->error();
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                reply->deleteLater();
                if (generation != m_generation) return;
                if (m_cancelRequested) { m_submitting = false; emit cancelled(); return; }
                if (failure != QNetworkReply::NoError || status < 200 || status >= 300) {
                    emit error(tr("Could not upload the input images")); return;
                }
                auto workflow = graph;
                workflow["image_data"] = QJsonObject { { "s3_object", data["object"] }, { "offsets", images["offsets"] } };
                sendCloudWorkflow(workflow);
            });
        });
        return;
    }
    m_submitting = true;
    m_progressGraph = graph;
    m_completedNodes.clear();
    m_progressNode.clear();
    m_progress = -1;
    openProgress();
    request("/krita/connection/prompt",
        { { "prompt", graph }, { "front", front },
            { "client_id", m_clientId } },
        true, [this](const QJsonObject& data) {
            m_submitting = false;
            const auto id = data["prompt_id"].toString();
            if (id.isEmpty()) {
                emit error(tr("The server did not accept the generation"));
                return;
            }
            resume(id);
            emit submitted(id);
            if (m_cancelRequested)
                cancelJob();
        });
}
void OrchestrionClient::sendCloudWorkflow(const QJsonObject& workflow) {
        request("/generate", { { "input", QJsonObject { { "workflow", workflow },
            { "clientInfo", "krita-ai-diffusion 1.53.0 (Baron native Android port)" },
            { "options", QJsonObject { { "useWebpCompression", true } } } } } }, true,
            [this](const QJsonObject& data) {
                m_submitting = false;
                m_cloudWorker = data["worker_id"].toString();
                const auto id = data["id"].toString();
                if (id.isEmpty()) { emit error(tr("The server did not accept the generation")); return; }
                resume(id); emit submitted(id);
                if (data["user"].isObject()) emit accountReady(data["user"].toObject());
                if (m_cancelRequested) cancelJob();
            });
}
void OrchestrionClient::cancelJob() {
    m_cancelRequested = true;
    if (m_submitting) {
        return;
    }
    if (m_cancelling) return;
    ++m_generation;
    m_jobTimer.stop();
    m_reconnect.stop();
    m_socket.close();
    m_pollingJob = false;
    abortRequests();
    if (m_promptId.isEmpty()) {
        finishCancellation();
        return;
    }
    m_cancelling = true;
    const auto id = m_promptId;
    if (m_backend == interstice) {
        request("/cancel/" + QString::fromUtf8(QUrl::toPercentEncoding(m_cloudWorker)) + "/" + QString::fromUtf8(QUrl::toPercentEncoding(id)),
            {}, true, [this, id](const QJsonObject&) {
                if (m_promptId != id) return;
                finishCancellation();
            });
        return;
    }
    request("/krita/connection/queue", {}, false, [this, id](const QJsonObject& data) {
        bool running = false;
        for (const auto& entry : data["queue_running"].toArray())
            if (entry.toArray().size() > 1 && entry.toArray().at(1).toString() == id)
                running = true;
        request(running ? "/krita/connection/interrupt" : "/krita/connection/queue",
            running ? QJsonObject { { "prompt_id", id } }
                    : QJsonObject { { "delete", QJsonArray { id } } },
            true, [this, id, running](const QJsonObject&) {
                if (m_promptId != id)
                    return;
                if (running) { finishCancellation(); return; }
                request("/krita/connection/queue", {}, false, [this, id](const QJsonObject& queue) {
                    if (m_promptId != id) return;
                    for (const auto& entry : queue["queue_running"].toArray()) {
                        if (entry.toArray().size() > 1 && entry.toArray().at(1).toString() == id) {
                            request("/krita/connection/interrupt", {{ "prompt_id", id }}, true,
                                [this, id](const QJsonObject&) { if (m_promptId == id) finishCancellation(); });
                            return;
                        }
                    }
                    for (const auto& entry : queue["queue_pending"].toArray()) {
                        if (entry.toArray().size() > 1 && entry.toArray().at(1).toString() == id) {
                            emit error(tr("Server cancellation was not confirmed. The task may still be queued."));
                            return;
                        }
                    }
                    finishCancellation();
                });
            });
    });
}
void OrchestrionClient::abortRequests() {
    const auto requests = m_requests;
    for (auto reply : requests)
        if (reply && !reply->isFinished()) reply->abort();
}
void OrchestrionClient::finishCancellation() {
    m_jobTimer.stop();
    m_reconnect.stop();
    m_promptId.clear();
    m_socket.close();
    ++m_generation;
    m_pollingJob = false;
    m_submitting = m_cancelling = m_cancelRequested = false;
    m_cloudImages.clear(); m_cloudOffsets = {}; m_cloudWorkflow = {};
    m_progressGraph = {}; m_completedNodes.clear();
    emit cancelled();
}
void OrchestrionClient::resume(const QString& id) {
    ++m_generation;
    m_promptId = id;
    m_pollingJob = false;
    m_missingJobPolls = 0;
    m_jobTimer.start();
    openProgress();
}
void OrchestrionClient::openProgress() {
    if (m_backend == interstice || !signedIn() || m_root.isEmpty()
        || m_socket.state() == QAbstractSocket::ConnectedState
        || m_socket.state() == QAbstractSocket::ConnectingState)
        return;
    auto url = serviceUrl("/krita/connection/ws");
    url.setScheme(url.scheme() == "https" ? "wss" : "ws");
    QUrlQuery query(url);
    query.addQueryItem("clientId", m_clientId);
    url.setQuery(query);
    QNetworkRequest request(url);
    if (!m_token.isEmpty()) request.setRawHeader("Authorization", "Bearer " + m_token);
    m_socket.open(request);
}
void OrchestrionClient::receiveProgress(const QJsonObject& message) {
    const auto data = message["data"].toObject();
    if (m_promptId.isEmpty() || data["prompt_id"].toString() != m_promptId)
        return;
    const auto type = message["type"].toString();
    if (type == "execution_start") {
        emit jobRunning(true);
        return;
    }
    double fraction = 0;
    if (type == "executing") {
        if (!m_progressNode.isEmpty())
            m_completedNodes.insert(m_progressNode);
        m_progressNode = data["node"].toString();
    } else if (type == "execution_cached") {
        for (auto node : data["nodes"].toArray())
            m_completedNodes.insert(node.toString());
    } else if (type == "progress") {
        const auto node = data["node"].toString();
        if (!node.isEmpty())
            m_progressNode = node;
        if (data["max"].toDouble() <= 0)
            return;
        fraction = qBound(0.0, data["value"].toDouble() / data["max"].toDouble(), 1.0);
    } else
        return;
    double total = 0, completed = 0;
    for (auto it = m_progressGraph.begin(); it != m_progressGraph.end(); ++it) {
        const auto node = it.value().toObject();
        const auto inputs = node["inputs"].toObject();
        const double weight = node["class_type"].toString().contains("Sampler")
            ? qMax(1.0, inputs["steps"].toDouble(20)) : 1;
        total += weight;
        if (m_completedNodes.contains(it.key()))
            completed += weight;
        else if (it.key() == m_progressNode)
            completed += weight * fraction;
    }
    if (total <= 0)
        return;
    m_progress = qMax(m_progress, qBound(0.0, completed / total, .99));
    emit jobRunning(true);
    emit progressChanged(m_progress);
}
void OrchestrionClient::pollJob() {
    if (m_pollingJob || m_cancelRequested)
        return;
    if (m_backend == interstice) { pollCloudJob(); return; }
    m_pollingJob = true;
    request("/krita/connection/history/" + QString::fromUtf8(QUrl::toPercentEncoding(m_promptId)),
        {}, false, [this](const QJsonObject& data) {
            const auto job = data[m_promptId].toObject();
            if (job.isEmpty()) {
                const auto id = m_promptId;
                request("/krita/connection/queue", {}, false, [this, id](const QJsonObject& queue) {
                    if (id != m_promptId)
                        return;
                    m_pollingJob = false;
                    bool running = false, pending = false;
                    for (auto value : queue["queue_running"].toArray()) {
                        const auto entry = value.toArray();
                        running |= entry.size() > 1 && entry.at(1).toString() == id;
                    }
                    for (auto value : queue["queue_pending"].toArray()) {
                        const auto entry = value.toArray();
                        pending |= entry.size() > 1 && entry.at(1).toString() == id;
                    }
                    if (running || pending) m_missingJobPolls = 0;
                    else if (++m_missingJobPolls >= 3) {
                        emit error(tr("The job is no longer in the server queue or history. No new generation was started."));
                        return;
                    }
                    emit jobRunning(running);
                });
                return;
            }
            m_pollingJob = false;
            m_missingJobPolls = 0;
            m_jobTimer.stop();
            m_reconnect.stop();
            m_socket.close();
            emit jobReady(job);
        });
}
void OrchestrionClient::fetchImage(const QJsonObject& descriptor, int index) {
    if (m_backend == interstice) {
        const auto position = descriptor["cloud_index"].toInt(-1);
        if (position < 0 || position >= m_cloudOffsets.size()) { emit error(tr("Could not download the result")); return; }
        const int start = m_cloudOffsets[position].toInt(-1);
        const int end = position + 1 < m_cloudOffsets.size() ? m_cloudOffsets[position + 1].toInt(-1) : m_cloudImages.size();
        if (start < 0 || end <= start || end > m_cloudImages.size()) { emit error(tr("Could not download the result")); return; }
        emit imageReady(m_cloudImages.mid(start, end - start), index); return;
    }
    QUrl url = serviceUrl("/krita/connection/view");
    QUrlQuery query(url);
    for (const auto& key : { "filename", "subfolder", "type" })
        query.addQueryItem(key, descriptor[key].toString());
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setTransferTimeout(60000);
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!m_token.isEmpty()) req.setRawHeader("Authorization", "Bearer " + m_token);
    auto reply = m_transport->get(req);
    m_requests.append(reply);
    QTimer::singleShot(60000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 128 * 1024 * 1024)
            reply->abort();
    });
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, generation] {
        m_requests.removeAll(reply);
        const auto bytes = reply->readAll();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (status != 200 || reply->error() != QNetworkReply::NoError
            || bytes.size() > 128 * 1024 * 1024) {
            emit error(tr("Could not download the result"));
            return;
        }
        emit imageReady(bytes, index);
    });
}
