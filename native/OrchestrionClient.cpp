// SPDX-License-Identifier: GPL-3.0-or-later
#include "OrchestrionClient.h"
#include "Credentials.h"
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QUuid>

OrchestrionClient::OrchestrionClient(QObject* parent)
    : QObject(parent) {
    m_loginTimer.setInterval(5000);
    m_jobTimer.setInterval(2000);
    connect(&m_loginTimer, &QTimer::timeout, this, &OrchestrionClient::pollLogin);
    connect(&m_jobTimer, &QTimer::timeout, this, &OrchestrionClient::pollJob);
    connect(this, &OrchestrionClient::error, this, [this] {
        m_pollingJob = false;
        m_jobTimer.stop();
        m_loginTimer.stop();
    });
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
    const auto url = validateRoot(text, &message);
    if (url.isEmpty()) {
        emit error(message);
        return false;
    }
    if (url != m_root) {
        cancelSignIn();
        m_token.clear();
        m_jobTimer.stop();
        m_pollingJob = false;
        m_promptId.clear();
        ++m_generation;
    }
    m_root = url;
    return true;
}

void OrchestrionClient::request(
    const QString& path, const QJsonObject& body, bool post, Callback done, bool auth) {
    if (m_root.isEmpty()) {
        emit error(tr("Choose an Orchestrion website first"));
        return;
    }
    if (auth && m_token.isEmpty()) {
        emit error(tr("Sign in required"));
        return;
    }
    const QUrl url = m_root.resolved(QUrl(path));
    if (url.host() != m_root.host() || url.scheme() != m_root.scheme()
        || url.port() != m_root.port()) {
        emit error(tr("The server returned an invalid address"));
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(60000);
    if (auth)
        req.setRawHeader("Authorization", "Bearer " + m_token);
    auto reply = post ? m_network.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact))
                      : m_network.get(req);
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
        const auto bytes = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto failure = reply->error();
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (status == 401) {
            m_token.clear();
            BaronCredentials::clear();
            m_jobTimer.stop();
            emit error(tr("Your login expired. Sign in again."));
            return;
        }
        if (status >= 300 && status < 400) {
            emit error(tr("The server redirected this request. Check the website address."));
            return;
        }
        if (status < 200 || status >= 300 || failure != QNetworkReply::NoError) {
            emit error(
                tr("Orchestrion request failed (%1). Check your connection and server integration.")
                    .arg(status));
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

void OrchestrionClient::signIn() {
    cancelSignIn();
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
    if (!m_token.isEmpty())
        request("/api/krita/logout", {}, true, [](const QJsonObject&) { });
    cancelSignIn();
    m_token.clear();
    BaronCredentials::clear();
    m_jobTimer.stop();
    m_pollingJob = false;
    emit accountReady({});
}
void OrchestrionClient::account() {
    request("/api/krita/account", {}, false, [this](const auto& data) { emit accountReady(data); });
}
void OrchestrionClient::restoreLogin() {
    m_token = BaronCredentials::load(m_root);
    if (!m_token.isEmpty()) {
        account();
        models();
    }
}
void OrchestrionClient::models() {
    request("/api/krita/models", {}, false, [this](const auto& data) { emit modelsReady(data); });
}
void OrchestrionClient::prepare(const QJsonObject& input) {
    ++m_generation;
    m_jobTimer.stop();
    m_pollingJob = false;
    request("/api/krita/native/prepare", input, true, [this](const auto& data) {
        if (data["prompt"].toObject().isEmpty() || !data["coins"].isDouble()
            || data["coins"].toDouble() < 0) {
            emit error(tr("The server returned an invalid response"));
            return;
        }
        emit prepared(data);
    });
}
void OrchestrionClient::submit(const QJsonObject& graph) {
    request("/krita/connection/prompt",
        { { "prompt", graph },
            { "client_id", QUuid::createUuid().toString(QUuid::WithoutBraces) } },
        true, [this](const QJsonObject& data) {
            const auto id = data["prompt_id"].toString();
            if (id.isEmpty()) {
                emit error(tr("The server did not accept the generation"));
                return;
            }
            resume(id);
            emit submitted(id);
        });
}
void OrchestrionClient::resume(const QString& id) {
    ++m_generation;
    m_promptId = id;
    m_pollingJob = false;
    m_jobTimer.start();
}
void OrchestrionClient::pollJob() {
    if (m_pollingJob)
        return;
    m_pollingJob = true;
    request("/krita/connection/history/" + QString::fromUtf8(QUrl::toPercentEncoding(m_promptId)),
        {}, false, [this](const QJsonObject& data) {
            m_pollingJob = false;
            const auto job = data[m_promptId].toObject();
            if (job.isEmpty())
                return;
            m_jobTimer.stop();
            emit jobReady(job);
            account();
        });
}
void OrchestrionClient::fetchImage(const QJsonObject& descriptor, int index) {
    QUrl url = m_root.resolved(QUrl("/krita/connection/view"));
    QUrlQuery query;
    for (const auto& key : { "filename", "subfolder", "type" })
        query.addQueryItem(key, descriptor[key].toString());
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setTransferTimeout(60000);
    req.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Authorization", "Bearer " + m_token);
    auto reply = m_network.get(req);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 128 * 1024 * 1024)
            reply->abort();
    });
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, generation] {
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
