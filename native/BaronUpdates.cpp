// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronUpdates.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrlQuery>
#ifdef Q_OS_ANDROID
#include <QAndroidJniObject>
#include <QtAndroid>
#include <QAndroidJniEnvironment>
#endif

QUrl BaronUpdates::feedUrl() {
    return QUrl("https://orchestrion.su/baron-updates/stable.json");
}
int BaronUpdates::installedVersionCode() {
#ifdef Q_OS_ANDROID
    const auto code = QAndroidJniObject::callStaticMethod<jint>("org/baron/krita/Updates", "installedCode",
        "(Landroid/content/Context;)I", QtAndroid::androidActivity().object<jobject>());
    QAndroidJniEnvironment environment;
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
        return -1;
    }
    return code;
#else
    return BARON_VERSION_CODE;
#endif
}
QString BaronUpdates::version() {
#ifdef Q_OS_ANDROID
    const auto name = QAndroidJniObject::callStaticObjectMethod("org/baron/krita/Updates",
        "installedName", "(Landroid/content/Context;)Ljava/lang/String;",
        QtAndroid::androidActivity().object<jobject>());
    QAndroidJniEnvironment environment;
    if (environment->ExceptionCheck()) environment->ExceptionClear();
    else if (name.isValid()) {
        const auto value = name.toString();
        const auto separator = value.lastIndexOf("-baron.");
        if (separator >= 0) return value.mid(separator + 7);
    }
#endif
    return QStringLiteral(BARON_VERSION);
}
BaronUpdates::BaronUpdates(QObject* parent, QNetworkAccessManager* network)
    : QObject(parent)
    , m_network(network ? network : new QNetworkAccessManager(this)) {
}
QJsonObject BaronUpdates::validatePackage(const QJsonObject& manifest, const QUrl& feed) {
    const auto p = manifest["android"].toObject();
    const QUrl url(p["url"].toString());
    const auto size = p["bytes"].toDouble();
    const auto code = p["version_code"].toDouble();
    if (manifest["schema"].toInt() != 1 || manifest["edition"] != "baron"
        || manifest["channel"] != "stable" || url.scheme() != "https" || url.host() != feed.host()
        || url.port(443) != feed.port(443) || !url.userInfo().isEmpty() || url.hasQuery()
        || url.hasFragment() || !url.path().startsWith("/baron-updates/releases/")
        || !url.path().endsWith(".apk") || url.path().split('/').contains("..")
        || p["package"] != "org.krita.baron.debug" || size <= 0 || size > 512 * 1024 * 1024
        || size != qint64(size) || code <= 0 || code > 2100000000 || code != int(code)
        || !QRegularExpression("^[a-f0-9]{64}$").match(p["sha256"].toString()).hasMatch()
        || !QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$")
            .match(p["version"].toString())
            .hasMatch())
        return {};
    return p;
}
void BaronUpdates::setState(State state) {
    m_state = state;
    emit changed();
}
void BaronUpdates::fail() {
    setState(failed);
    emit message(tr("Update failed"));
}
void BaronUpdates::check() {
    if (m_reply)
        return;
    setState(checking);
    emit message(tr("Checking for updates..."));
    auto url = feedUrl();
    QUrlQuery query;
    query.addQueryItem("t", QString::number(QDateTime::currentSecsSinceEpoch()));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Cache-Control", "no-cache");
    request.setTransferTimeout(15000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    m_reply = m_network->get(request);
    auto reply = m_reply;
    reply->setReadBufferSize(65537);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 65536)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_reply = nullptr;
        const auto bytes = reply->readAll();
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && bytes.size() <= 65536;
        reply->deleteLater();
        m_package = ok ? validatePackage(QJsonDocument::fromJson(bytes).object(), feedUrl())
                       : QJsonObject();
        if (m_package.isEmpty()) {
            fail();
            return;
        }
        const auto installed = installedVersionCode();
        if (installed <= 0) {
            fail();
            return;
        }
        setState(m_package["version_code"].toInt() > installed ? available : latest);
        emit message(m_state == available ? tr("New version available: %1").arg(latestVersion())
                                          : version());
    });
}
void BaronUpdates::download() {
    if (m_reply || m_package.isEmpty()
        || m_package["version_code"].toInt() <= installedVersionCode())
        return;
    QString directory;
#ifdef Q_OS_ANDROID
    directory = QAndroidJniObject::callStaticObjectMethod("org/baron/krita/Updates", "directory",
        "(Landroid/content/Context;)Ljava/lang/String;",
        QtAndroid::androidActivity().object<jobject>())
                    .toString();
#else
    directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/baron-updates";
#endif
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        fail();
        return;
    }
    m_path = directory + "/update-" + m_package["sha256"].toString() + ".apk";
    m_file = std::make_unique<QSaveFile>(m_path);
    if (!m_file->open(QIODevice::WriteOnly)) {
        fail();
        return;
    }
    m_hash.reset();
    m_received = 0;
    QNetworkRequest request(QUrl(m_package["url"].toString()));
    request.setTransferTimeout(60000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    m_reply = m_network->get(request);
    auto reply = m_reply;
    reply->setReadBufferSize(64 * 1024);
    setState(downloading);
    emit message(tr("Downloading package..."));
    auto read = [this, reply] {
        const auto chunk = reply->readAll();
        if (chunk.isEmpty())
            return;
        m_received += chunk.size();
        if (m_received > qint64(m_package["bytes"].toDouble())
            || m_file->write(chunk) != chunk.size()) {
            reply->abort();
            return;
        }
        m_hash.addData(chunk);
        emit progress(m_received, qint64(m_package["bytes"].toDouble()));
    };
    connect(reply, &QNetworkReply::readyRead, this, read);
    connect(reply, &QNetworkReply::finished, this, [this, reply, read] {
        read();
        m_reply = nullptr;
        const bool ok = reply->error() == QNetworkReply::NoError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200
            && m_received == qint64(m_package["bytes"].toDouble())
            && QString::fromLatin1(m_hash.result().toHex()) == m_package["sha256"].toString();
        reply->deleteLater();
        if (!ok || !m_file->commit()) {
            m_file->cancelWriting();
            m_file.reset();
            fail();
            return;
        }
        m_file.reset();
        setState(ready);
        emit message(tr("Downloaded. Save your work, then install the update."));
    });
}
void BaronUpdates::cancel() {
    if (m_reply)
        m_reply->abort();
}
void BaronUpdates::install() {
    if (m_state != ready)
        return;
#ifdef Q_OS_ANDROID
    const auto path = QAndroidJniObject::fromString(m_path);
    const int result = QAndroidJniObject::callStaticMethod<jint>("org/baron/krita/Updates",
        "install", "(Landroid/app/Activity;Ljava/lang/String;I)I",
        QtAndroid::androidActivity().object<jobject>(), path.object<jstring>(),
        m_package["version_code"].toInt());
    if (result == 1)
        emit message(
            tr("Allow updates from Krita in Android settings, then press Install update again."));
    else if (result != 0)
        fail();
#else
    emit message(tr("APK installation is available on Android."));
#endif
}
