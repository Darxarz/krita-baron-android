// SPDX-License-Identifier: GPL-3.0-or-later
#include "ModelThumbnails.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QImageReader>
#include <QNetworkReply>
#include <QSharedPointer>

namespace {
bool allowed(const QUrl& url) {
    return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty()
        && url.port(443) == 443
        && (url.host() == "image.civitai.com" || url.host() == "blobs-b2.civitai.com");
}
constexpr int max_bytes = 5 * 1024 * 1024;
QString cacheFile(const QString& directory, const QString& key) {
    return directory + "/" + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".png";
}
QImage decode(const QByteArray& bytes) {
    QBuffer buffer;
    buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 16777216) return {};
    reader.setScaledSize(size.scaled(160, 160, Qt::KeepAspectRatio));
    return reader.read();
}
}

ModelThumbnails::ModelThumbnails(QObject* parent, QNetworkAccessManager* network, const QString& cacheDirectory)
    : QObject(parent)
    , m_network(network ? network : new QNetworkAccessManager(this)) {
    m_directory = !cacheDirectory.isEmpty() ? cacheDirectory : network ? QString()
        : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/baron/model-thumbnails";
}

QUrl ModelThumbnails::previewUrl(const QUrl& url) {
    if (!allowed(url))
        return {};
    QUrl result = url;
    result.setFragment({});
    if (result.host() == "image.civitai.com") {
        auto parts = result.path().split('/');
        // Civitai's image route: /account/image-id/transform/filename.
        if (parts.size() == 5 && !parts[1].isEmpty() && !parts[2].isEmpty()
            && !parts[4].isEmpty()) {
            parts[3] = "width=320,quality=85,format=jpeg";
            result.setPath(parts.join('/'));
        }
    }
    return result;
}

QUrl ModelThumbnails::redirectUrl(const QUrl& source, const QUrl& target) {
    const auto result = source.resolved(target);
    return allowed(result) ? result : QUrl();
}

void ModelThumbnails::load(const QUrl& url, Callback done) {
    const auto preview = previewUrl(url);
    if (preview.isEmpty()) {
        done({});
        return;
    }
    const auto key = preview.toString();
    if (const auto cached = m_cache.object(key)) {
        done(*cached);
        return;
    }
    const bool pending = m_pending.contains(key);
    m_pending[key].append(std::move(done));
    if (pending) return;
    auto download = [this, preview, key](const QImage& cached) {
        if (!cached.isNull()) { complete(key, cached); return; }
        fetch(preview, key, 0, [this, key](const QImage& image) { complete(key, image); });
    };
    if (m_directory.isEmpty()) { download({}); return; }
    const auto file = cacheFile(m_directory, key);
    BackgroundWork::run(this, [file] {
        QFile input(file);
        if (!input.open(QIODevice::ReadOnly)) return QImage();
        const auto image = input.size() <= max_bytes ? decode(input.readAll()) : QImage();
        input.close();
        if (image.isNull()) QFile::remove(file);
        return image;
    }, download);
}

void ModelThumbnails::complete(const QString& key, const QImage& image) {
    if (!image.isNull()) m_cache.insert(key, new QImage(image), int((image.sizeInBytes() + 1023) / 1024));
    const auto callbacks = m_pending.take(key);
    for (const auto& done : callbacks) done(image);
}

void ModelThumbnails::fetch(
    const QUrl& url, const QString& cacheKey, int redirects, Callback done) {
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setRawHeader("Accept", "image/jpeg,image/png,image/webp");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto reply = m_network->get(request);
    reply->setReadBufferSize(64 * 1024);
    auto bytes = QSharedPointer<QByteArray>::create();
    connect(reply, &QNetworkReply::readyRead, reply, [reply, bytes] {
        bytes->append(reply->readAll());
        if (bytes->size() > max_bytes)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, cacheKey, redirects, done] {
        bytes->append(reply->readAll());
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const auto next = redirectUrl(reply->url(), target);
        const bool ok = reply->error() == QNetworkReply::NoError;
        reply->deleteLater();
        if (ok && status >= 300 && status < 400 && !target.isEmpty() && !next.isEmpty()
            && redirects < 3 && bytes->size() <= max_bytes) {
            fetch(next, cacheKey, redirects + 1, done);
            return;
        }
        if (!ok || status < 200 || status >= 300 || bytes->size() > max_bytes) { done({}); return; }
        const auto directory = m_directory;
        const auto body = *bytes;
        BackgroundWork::run(this, [body, directory, cacheKey] {
            const auto image = decode(body);
            if (!image.isNull() && !directory.isEmpty() && QDir().mkpath(directory)) {
                QSaveFile output(cacheFile(directory, cacheKey));
                if (output.open(QIODevice::WriteOnly) && image.save(&output, "PNG")) output.commit();
                // Prune only after downloads; cache hits never walk the directory.
                static quint64 writes = 0;
                if ((writes++ % 64) == 0) {
                    const auto files = QDir(directory).entryInfoList({"*.png"}, QDir::Files, QDir::Time);
                    qint64 size = 0;
                    for (const auto& file : files) {
                        size += file.size();
                        if (size > 256 * 1024 * 1024) QFile::remove(file.absoluteFilePath());
                    }
                }
            }
            return image;
        }, done, BackgroundWork::history());
    });
}
