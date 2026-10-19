// SPDX-License-Identifier: GPL-3.0-or-later
#include "OrchestrionInbox.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QSharedPointer>
#include <QSysInfo>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>

const char* OrchestrionInbox::annotationKey = "ai_diffusion/orchestrion_inbox";

namespace {
constexpr int fullRefreshMs = 30 * 60 * 1000;

QString deviceName() {
    const auto host = QSysInfo::machineHostName();
    return (host.isEmpty() || host == "localhost" ? "Krita · " + QSysInfo::prettyProductName() : host).left(120);
}
QString itemPath(const QString& id, const QString& action = {}) {
    const QString base = "/api/krita/inbox/" + QString::fromUtf8(QUrl::toPercentEncoding(id));
    return action.isEmpty() ? base : QString(base + "/" + action);
}
}

OrchestrionInbox::OrchestrionInbox(QObject* parent, QNetworkAccessManager* transport)
    : QObject(parent), m_transport(transport ? transport : &m_network) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &OrchestrionInbox::poll);
}

OrchestrionInbox::~OrchestrionInbox() {
    for (auto reply : m_requests) {
        if (!reply) continue;
        disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished()) reply->abort();
    }
}

QString OrchestrionInbox::deviceId() {
    QSettings settings("BaronEdition", "Orchestrion");
    auto id = settings.value("inboxDeviceId").toString();
    if (id.isEmpty()) {
        id = "krita-android-" + QUuid::createUuid().toString(QUuid::Id128);
        settings.setValue("inboxDeviceId", id);
    }
    return id;
}

QString OrchestrionInbox::returnKey(const QString& id, const QByteArray& png) {
    // Same picture for the same item -> same key: a retry never creates a duplicate version.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(id.toUtf8());
    hash.addData(QByteArray(1, '\0'));
    hash.addData(png);
    return QString::fromLatin1(hash.result().toHex().left(48));
}

bool OrchestrionInbox::validItem(const QJsonObject& item) {
    static const QRegularExpression idPattern("^(?!\\.+$)[A-Za-z0-9._~-]{1,128}$");
    static const QRegularExpression hexPattern("^[0-9a-f]{64}$");
    const auto id = item["id"].toString();
    const auto bytes = item["bytes"].toDouble();
    return idPattern.match(id).hasMatch()
        && item["imageUrl"].toString() == "/api/krita/inbox/" + id + "/image"
        && hexPattern.match(item["sha256"].toString().toLower()).hasMatch()
        && bytes > 0 && bytes <= maxBytes;
}

void OrchestrionInbox::start(const QUrl& root, const QByteArray& token) {
    if (root == m_root && token == m_token && (m_enabled || !m_requests.isEmpty()))
        return;
    stop();
    if (root.isEmpty() || token.isEmpty())
        return;
    m_root = root;
    m_token = token;
    checkFeatures();
}

void OrchestrionInbox::checkFeatures() {
    const int generation = m_generation;
    send("GET", "/api/krita/info", {}, {}, {}, 15000,
        [this, generation](int status, const QByteArray&, const QJsonObject& json) {
            if (status == 200) {
                if (!json["features"].toArray().contains("inbox:1"))
                    return; // this website has no inbox
                m_enabled = true;
                emit changed();
                poll();
            } else if (status != 401 && status != 403) {
                QTimer::singleShot(60000, this, [this, generation] {
                    if (generation == m_generation && !m_enabled) checkFeatures();
                });
            }
        });
}

void OrchestrionInbox::stop() {
    ++m_generation;
    m_timer.stop();
    for (auto reply : m_requests) {
        if (!reply) continue;
        disconnect(reply, nullptr, this, nullptr);
        if (!reply->isFinished()) reply->abort();
        reply->deleteLater();
    }
    m_requests.clear();
    m_poll = nullptr;
    m_items.clear();
    m_handled.clear();
    m_opening.clear();
    m_cursor.clear();
    m_error.clear();
    m_root = QUrl();
    m_token.clear();
    m_delay = 0;
    m_lastFull = 0;
    m_enabled = false;
    emit changed();
}

QList<QJsonObject> OrchestrionInbox::items() const {
    QList<QJsonObject> result;
    for (const auto& item : m_items)
        if (item["status"].toString() != "expired") result.append(item);
    std::sort(result.begin(), result.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a["createdAt"].toString() > b["createdAt"].toString();
    });
    return result;
}

int OrchestrionInbox::unopenedCount() const {
    return int(std::count_if(m_items.begin(), m_items.end(),
        [](const QJsonObject& item) { return item["status"].toString() == "new"; }));
}

void OrchestrionInbox::send(const QString& method, const QString& path, const QByteArray& body,
    const QByteArray& contentType, const QList<QPair<QByteArray, QByteArray>>& headers, int timeout, Done done) {
    const QUrl relative(path);
    QUrl url = m_root;
    url.setPath(m_root.path() + relative.path(QUrl::FullyDecoded), QUrl::DecodedMode);
    url.setQuery(relative.query());
    if (m_root.isEmpty() || url.host() != m_root.host() || url.scheme() != m_root.scheme()
        || url.port() != m_root.port()) {
        done(0, {}, {});
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Authorization", "Bearer " + m_token);
    if (!contentType.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    for (const auto& header : headers) request.setRawHeader(header.first, header.second);
    request.setTransferTimeout(timeout);
    QNetworkReply* reply = method == "POST" ? m_transport->post(request, body)
        : method == "DELETE" ? m_transport->deleteResource(request)
        : m_transport->get(request);
    m_requests.append(reply);
    if (path.startsWith("/api/krita/inbox?") || path == "/api/krita/inbox") m_poll = reply;
    QTimer::singleShot(timeout + 5000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    auto data = QSharedPointer<QByteArray>::create();
    connect(reply, &QNetworkReply::readyRead, this, [reply, data] {
        data->append(reply->readAll());
        if (data->size() > maxBytes + 1024 * 1024) reply->abort();
    });
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, done, generation] {
        m_requests.removeAll(reply);
        if (m_poll == reply) m_poll = nullptr;
        data->append(reply->readAll());
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status < 400) status = 0;
        reply->deleteLater();
        if (generation != m_generation) return;
        const auto json = QJsonDocument::fromJson(*data).object();
        done(status, *data, json);
    });
}

QString OrchestrionInbox::message(int status, const QJsonObject& error, QString* code) const {
    *code = error["code"].toString();
    if (*code == "expired")
        return tr("This picture is no longer in the website inbox (it expired after 7 days)");
    if (*code == "not_found")
        return tr("This picture was removed from the website inbox");
    if (*code == "already_opened")
        return tr("This picture is already open on %1")
            .arg(error["openedBy"].toObject()["deviceName"].toString("?"));
    if (*code == "too_large")
        return tr("The picture is too large for the website (limit %1 MB)")
            .arg(qint64(error["maxBytes"].toDouble(maxBytes)) / (1024 * 1024));
    if (*code == "too_many_pixels")
        return tr("The picture is too large for the website (more than 8192×8192 pixels)");
    if (status == 401 || status == 403) {
        *code = "auth";
        return tr("Your website login expired. Sign in again.");
    }
    if (status >= 300 && status < 400) {
        *code = "redirect";
        return tr("The server redirected this request. Check the website address.");
    }
    if (code->isEmpty()) *code = "network";
    const auto text = error["error"].toString();
    return text.isEmpty() ? tr("Could not reach the website (%1)").arg(status) : text;
}

void OrchestrionInbox::schedule(int milliseconds) {
    if (m_enabled) m_timer.start(milliseconds);
}

void OrchestrionInbox::retry() {
    m_cursor.clear(); // re-read everything after any problem
    m_delay = qMin(120000, m_delay ? m_delay * 2 : m_minDelay);
    schedule(m_delay);
}

void OrchestrionInbox::poll() {
    if (!m_enabled || m_poll) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFull > fullRefreshMs) m_cursor.clear();
    const bool full = m_cursor.isEmpty();
    QString path = "/api/krita/inbox";
    if (!full) {
        QUrlQuery query;
        query.addQueryItem("since", m_cursor);
        query.addQueryItem("wait", "25");
        path += "?" + query.toString(QUrl::FullyEncoded);
    }
    send("GET", path, {}, {}, {}, full ? 30000 : 55000,
        [this, full, now](int status, const QByteArray&, const QJsonObject& json) {
            if (status == 200 && json.contains("cursor")) {
                if (full) m_lastFull = now;
                apply(json, full);
                m_error.clear();
                m_delay = 0;
                m_cursor = json["cursor"].toString();
                openNew();
                schedule(m_cursor.isEmpty() ? 25000 : 0);
                return;
            }
            QString code;
            const auto text = message(status, json, &code);
            if (code == "auth") {
                m_error = text;
                m_enabled = false;
                emit failed(text, code, {});
                emit changed();
                return;
            }
            retry();
        });
}

void OrchestrionInbox::apply(const QJsonObject& data, bool full) {
    if (full) m_items.clear();
    for (const auto& value : data["items"].toArray()) {
        const auto item = value.toObject();
        if (!validItem(item)) continue;
        if (item["status"].toString() == "expired") m_items.remove(item["id"].toString());
        else m_items.insert(item["id"].toString(), item);
    }
    for (const auto& removed : data["removed"].toArray())
        m_items.remove(removed.toString());
    emit changed();
}

void OrchestrionInbox::openNew() {
    if (!m_autoOpen) return;
    QList<QJsonObject> pending;
    for (const auto& item : m_items)
        if (item["status"].toString() == "new" && !m_handled.contains(item["id"].toString()))
            pending.append(item);
    std::sort(pending.begin(), pending.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a["createdAt"].toString() < b["createdAt"].toString();
    });
    for (const auto& item : pending)
        openItem(item["id"].toString(), false, true);
}

void OrchestrionInbox::open(const QString& id, bool force) {
    openItem(id, force, false);
}

void OrchestrionInbox::openItem(const QString& id, bool force, bool automatic) {
    if (!m_enabled || !m_items.contains(id) || m_opening.contains(id)) return;
    m_handled.insert(id);
    m_opening.insert(id);
    const QJsonObject body { { "deviceId", deviceId() }, { "deviceName", deviceName() }, { "force", force } };
    send("POST", itemPath(id, "ack"), QJsonDocument(body).toJson(QJsonDocument::Compact), "application/json", {},
        30000, [this, id, automatic](int status, const QByteArray&, const QJsonObject& json) {
            const auto item = json["item"].toObject();
            if (status == 200 && validItem(item) && item["id"].toString() == id) {
                m_items.insert(id, item);
                emit changed();
                download(item, automatic);
                return;
            }
            m_opening.remove(id);
            QString code;
            const auto text = message(status, json, &code);
            if (!(automatic && code == "already_opened")) emit failed(text, code, id);
        });
}

void OrchestrionInbox::download(const QJsonObject& item, bool automatic) {
    Q_UNUSED(automatic);
    const auto id = item["id"].toString();
    send("GET", item["imageUrl"].toString(), {}, {}, {}, 300000,
        [this, item, id](int status, const QByteArray& bytes, const QJsonObject& json) {
            if (status != 200) {
                m_opening.remove(id);
                QString code;
                emit failed(message(status, json, &code), code, id);
                return;
            }
            const int generation = m_generation;
            const auto origin = m_root.toString(QUrl::FullyEncoded);
            BackgroundWork::run(this, [item, bytes] {
                const bool intact = bytes.size() == qint64(item["bytes"].toDouble())
                    && QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()
                        == item["sha256"].toString().toLower().toLatin1();
                return intact ? QImage::fromData(bytes) : QImage();
            }, [this, item, id, generation, origin](const QImage& picture) {
                if (generation != m_generation) return;
                m_opening.remove(id);
                if (picture.isNull()) {
                    emit failed(tr("The picture from the website arrived damaged (checksum mismatch). Try again."),
                        "checksum", id);
                    return;
                }
                const QJsonObject info { { "id", id }, { "title", item["title"].toString() },
                    { "source", item["source"].toString() }, { "returnTarget", item["returnTarget"].toObject() },
                    { "origin", origin } };
                emit pictureReady(item, picture, QJsonDocument(info).toJson(QJsonDocument::Compact));
            });
        });
}

void OrchestrionInbox::returnPicture(const QByteArray& annotation, const QImage& picture) {
    const auto info = QJsonDocument::fromJson(annotation).object();
    const auto id = info["id"].toString();
    if (!m_enabled) {
        emit failed(tr("Not connected to the website"), "offline", id);
        return;
    }
    if (id.isEmpty() || info["origin"].toString() != m_root.toString(QUrl::FullyEncoded)) {
        emit failed(tr("This document was opened from a different website"), "origin", id);
        return;
    }
    if (picture.isNull() || qint64(picture.width()) * picture.height() > maxPixels) {
        emit failed(tr("The picture is too large for the website (more than 8192×8192 pixels)"),
            "too_many_pixels", id);
        return;
    }
    const int generation = m_generation;
    const QSize size = picture.size();
    BackgroundWork::run(this, [picture] {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        picture.convertToFormat(QImage::Format_ARGB32).save(&buffer, "PNG");
        return png;
    }, [this, id, size, generation](const QByteArray& png) {
        if (generation != m_generation) return;
        if (png.size() > maxBytes) {
            emit failed(tr("The picture is too large for the website (limit %1 MB)").arg(maxBytes / (1024 * 1024)),
                "too_large", id);
            return;
        }
        QUrlQuery query;
        query.addQueryItem("width", QString::number(size.width()));
        query.addQueryItem("height", QString::number(size.height()));
        send("POST", itemPath(id, "return") + "?" + query.toString(QUrl::FullyEncoded), png, "image/png",
            { { "Idempotency-Key", returnKey(id, png).toLatin1() } }, 300000,
            [this, id](int status, const QByteArray&, const QJsonObject& json) {
                if (status == 200 && json["ok"].toBool()) {
                    if (m_items.contains(id)) {
                        auto item = m_items[id];
                        item["status"] = "returned";
                        item["version"] = json["version"];
                        m_items.insert(id, item);
                        emit changed();
                    }
                    emit returned(json);
                    return;
                }
                QString code;
                emit failed(message(status, json, &code), code, id);
            });
    });
}
