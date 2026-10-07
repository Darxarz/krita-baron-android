// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QSet>
#include <functional>
#include <atomic>
#include <memory>

class HistoryStore {
public:
    struct Prepared {
        QList<QImage> images, thumbnails;
        QImage selection, mask, preview;
        QByteArray bytes;
        QJsonArray offsets;
        QString error, selectionText;
    };
    static Prepared prepare(const QList<QImage>& images, const QByteArray& selection = {},
        const QString& mode = {}, const QImage& source = {});
    QString appendPrepared(const Prepared& prepared, const QRect& bounds, const QJsonObject& settings);
    using Read = std::function<QByteArray(const QString&)>;
    using Write = std::function<void(const QString&, const QByteArray&)>;
    explicit HistoryStore(const QString& document, const QString& cacheDirectory = {});
    bool load(const Read& read);
    bool save(const Write& write = {});
    void saveAsync(QObject* owner, const Write& write, std::function<void(QString)> done);
    QString append(const QList<QImage>& images, const QRect& bounds,
        const QJsonObject& settings, const QImage& selection = {});
    QImage cachedThumbnail(const QString& id, int index, int size) const {
        return m_thumbnails.value(id + ":" + QString::number(index) + ":" + QString::number(size));
    }
    void cacheThumbnail(const QString& id, int index, int size, const QImage& image) {
        m_thumbnails[id + ":" + QString::number(index) + ":" + QString::number(size)] = image;
    }
    QImage image(const QString& id, int index, const QSize& scaled = {}) const;
    void mark(const QString& id, int index, bool favorite, bool applied);
    bool remove(const QString& id, int index);
    void clear();
    QJsonArray entries() const;
    void setDocumentSettings(const QJsonObject& settings);
    QString error() const { return m_error; }
    static QString imageKey(int slot);
    static QRect bounds(const QJsonObject& entry);
    static QJsonObject settings(const QJsonObject& entry);

private:
    QImage decode(const QByteArray& bytes, const QSize& scaled = {}) const;
    bool pack(QJsonObject& entry, const QList<QImage>& images);
    QString m_cache, m_error;
    QByteArray m_baseHash;
    QSet<QByteArray> m_bases;
    QJsonObject m_state;
    QMap<int, QByteArray> m_images;
    mutable QMap<QString, QImage> m_thumbnails;
    QString m_recentId;
    QList<QImage> m_recentImages;
    std::shared_ptr<std::atomic<quint64>> m_saveEpoch = std::make_shared<std::atomic<quint64>>(0);
    int m_nextSlot = 0;
    QSet<int> m_removedSlots;
};
