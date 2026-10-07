// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QCache>
#include <QImage>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <functional>

class ModelThumbnails : public QObject {
    Q_OBJECT
public:
    explicit ModelThumbnails(QObject* parent = nullptr, QNetworkAccessManager* network = nullptr,
        const QString& cacheDirectory = {});
    static QUrl previewUrl(const QUrl& url);
    static QUrl redirectUrl(const QUrl& source, const QUrl& target);
    using Callback = std::function<void(const QImage&)>;
    void load(const QUrl& url, Callback done);

private:
    void fetch(const QUrl& url, const QString& cacheKey, int redirects, Callback done);
    void complete(const QString& key, const QImage& image);
    QNetworkAccessManager* m_network;
    QString m_directory;
    QHash<QString, QList<Callback>> m_pending;
    QCache<QString, QImage> m_cache { 8192 };
};
