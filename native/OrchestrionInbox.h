// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <functional>
class QNetworkReply;

// Pictures sent from the Orchestrion website with "Send to Krita" (GET /api/krita/inbox),
// opened here as new documents and returned with POST /api/krita/inbox/:id/return.
class OrchestrionInbox : public QObject {
    Q_OBJECT
public:
    static constexpr qint64 maxBytes = 64ll * 1024 * 1024;
    static constexpr qint64 maxPixels = 8192ll * 8192;
    static const char* annotationKey;

    explicit OrchestrionInbox(QObject* parent = nullptr, QNetworkAccessManager* transport = nullptr);
    ~OrchestrionInbox() override;
    // Starts polling when the website advertises the inbox:1 feature. An empty token stops it.
    void start(const QUrl& root, const QByteArray& token);
    void stop();
    bool enabled() const { return m_enabled; }
    QString lastError() const { return m_error; }
    QList<QJsonObject> items() const; // newest first, without expired items
    int unopenedCount() const;
    void open(const QString& id, bool force = false);
    // info is the annotation stored on the opened document
    void returnPicture(const QByteArray& info, const QImage& picture);
    void setAutoOpen(bool enabled) { m_autoOpen = enabled; }
    void setRetryDelay(int milliseconds) { m_minDelay = milliseconds; }
    static QString deviceId();
    static QString returnKey(const QString& id, const QByteArray& png);
    static bool validItem(const QJsonObject& item);

Q_SIGNALS:
    void changed();
    // Downloaded, size and SHA-256 verified; info must be stored on the new document.
    void pictureReady(const QJsonObject& item, const QImage& picture, const QByteArray& info);
    void returned(const QJsonObject& result);
    // code: already_opened, expired, not_found, checksum, too_large, too_many_pixels, auth, origin...
    void failed(const QString& message, const QString& code, const QString& id);

private:
    using Done = std::function<void(int status, const QByteArray& body, const QJsonObject& json)>;
    void send(const QString& method, const QString& path, const QByteArray& body,
        const QByteArray& contentType, const QList<QPair<QByteArray, QByteArray>>& headers,
        int timeout, Done done);
    void poll();
    void schedule(int milliseconds);
    void retry();
    void apply(const QJsonObject& data, bool full);
    void openNew();
    void openItem(const QString& id, bool force, bool automatic);
    void download(const QJsonObject& item, bool automatic);
    void checkFeatures();
    QString message(int status, const QJsonObject& error, QString* code) const;
    QNetworkAccessManager m_network;
    QNetworkAccessManager* m_transport;
    QUrl m_root;
    QByteArray m_token;
    QString m_cursor, m_error;
    QHash<QString, QJsonObject> m_items;
    QSet<QString> m_handled, m_opening;
    QList<QPointer<QNetworkReply>> m_requests;
    QPointer<QNetworkReply> m_poll;
    QTimer m_timer;
    qint64 m_lastFull = 0;
    int m_delay = 0, m_minDelay = 5000, m_generation = 0;
    bool m_enabled = false, m_autoOpen = true;
};
