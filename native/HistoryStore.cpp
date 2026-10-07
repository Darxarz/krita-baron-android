// SPDX-License-Identifier: GPL-3.0-or-later
#include "HistoryStore.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace {
QByteArray digest(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}
bool writeFile(const QString& name, const QByteArray& data) {
    QSaveFile file(name);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
QByteArray readFile(const QString& name) {
    QFile file(name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}
HistoryStore::HistoryStore(const QString& document, const QString& cacheDirectory) {
    const QString root = cacheDirectory.isEmpty()
        ? QString(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history")
        : cacheDirectory;
    m_cache = root + "/" + QString::fromLatin1(digest(document.toUtf8()));
}
QString HistoryStore::imageKey(int slot) {
    return QString("ai_diffusion/result%1.webp").arg(slot);
}
bool HistoryStore::load(const Read& read) {
    m_images.clear();
    m_thumbnails.clear();
    m_recentImages.clear();
    m_recentId.clear();
    m_nextSlot = 0;
    m_error.clear();
    auto bytes = read("ai_diffusion/ui.json");
    if (bytes.isEmpty())
        bytes = read("ai_diffusion/ui");
    m_baseHash = digest(bytes);
    m_bases = { m_baseHash };
    m_state = QJsonDocument::fromJson(bytes).object();
    if (!m_state.isEmpty() && m_state["version"].toInt() != 1) {
        m_error = "Unsupported history version";
        return false;
    }
    const auto journal = QJsonDocument::fromJson(readFile(m_cache + "/manifest.json")).object();
    bool recover = journal["schema"].toInt() == 1
        && (journal["base"].toString().toLatin1() == m_baseHash
            || journal["latest"].toString().toLatin1() == m_baseHash
            || journal["bases"].toArray().contains(QString::fromLatin1(m_baseHash)));
    if (recover) {
        for (auto value : journal["state"].toObject()["history"].toArray()) {
            const auto key = QString::number(value.toObject()["slot"].toInt(-1));
            const auto hash = journal["blobs"].toObject()[key].toString();
            if (hash.size() != 64 || hash.contains(QRegularExpression("[^a-f0-9]"))
                || digest(readFile(m_cache + "/" + hash + ".bin")) != hash.toLatin1()) {
                recover = false;
                break;
            }
        }
    }
    if (recover) {
        m_state = journal["state"].toObject();
        for (auto base : journal["bases"].toArray())
            m_bases.insert(base.toString().toLatin1());
    }
    for (auto value : entries()) {
        const auto entry = value.toObject();
        const int slot = entry["slot"].toInt(-1);
        if (slot < 0)
            continue;
        auto blob = read(imageKey(slot));
        if (blob.isEmpty())
            blob = read(QString("ai_diffusion/result%1").arg(slot));
        if (recover) {
            const auto hash = journal["blobs"].toObject()[QString::number(slot)].toString();
            if (hash.size() == 64 && !hash.contains(QRegularExpression("[^a-f0-9]"))) {
                const auto cached = readFile(m_cache + "/" + hash + ".bin");
                if (digest(cached) == hash.toLatin1())
                    blob = cached;
            }
        }
        m_images[slot] = blob;
        m_nextSlot = qMax(m_nextSlot, slot + 1);
    }
    return true;
}
QJsonArray HistoryStore::entries() const { return m_state["history"].toArray(); }
QRect HistoryStore::bounds(const QJsonObject& entry) {
    const auto data = entry["params"].toObject()["bounds"].toArray();
    return data.size() == 4 ? QRect(data[0].toInt(), data[1].toInt(), data[2].toInt(), data[3].toInt())
                            : QRect();
}
QJsonObject HistoryStore::settings(const QJsonObject& entry) {
    const auto params = entry["params"].toObject();
    const auto meta = params["metadata"].toObject();
    auto result = meta["baron"].toObject()["settings"].toObject();
    if (result.isEmpty()) {
        result = { { "prompt", meta["prompt"] }, { "negative", meta["negative_prompt"] },
            { "model", meta["checkpoint"] }, { "strength", meta["strength"].toDouble(1) },
            { "steps", meta["steps"].toInt(20) }, { "cfg", meta["guidance"].toDouble(4) },
            { "sampler", meta["sampler"] } };
    }
    result["seed"] = QString::number(quint64(params["seed"].toDouble()));
    result["fixed_seed"] = true;
    return result;
}
bool HistoryStore::pack(QJsonObject& entry, const QList<QImage>& images) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QJsonArray offsets;
    for (const auto& image : images) {
        offsets.append(double(buffer.pos()));
        // The Python reader auto-detects the image format, including PNG in .webp annotations.
        if (image.isNull() || !image.save(&buffer, "PNG")) {
            m_error = "Could not encode history image";
            return false;
        }
    }
    entry["offsets"] = offsets;
    m_images[entry["slot"].toInt()] = bytes;
    return true;
}
QString HistoryStore::append(const QList<QImage>& images, const QRect& area,
    const QJsonObject& settings, const QImage& selection) {
    return appendPrepared(prepare(images, [&selection] {
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        if (!selection.isNull()) selection.save(&buffer, "PNG");
        return bytes;
    }()), area, settings);
}
QString HistoryStore::appendPrepared(const Prepared& prepared, const QRect& area,
    const QJsonObject& settings) {
    if (!prepared.error.isEmpty() || prepared.bytes.isEmpty() || area.isEmpty()) {
        m_error = prepared.error;
        return {};
    }
    const auto& selection = prepared.selection;
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject extra { { "settings", settings },
        { "time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate) } };
    if (!selection.isNull())
        extra["selection_mask"] = prepared.selectionText;
    QJsonObject metadata { { "prompt", settings["prompt"] },
        { "negative_prompt", settings["negative"] }, { "strength", settings["strength"] },
        { "checkpoint", settings["model"] }, { "steps", settings["steps"] },
        { "guidance", settings["cfg"] }, { "sampler", settings["sampler"] }, { "baron", extra } };
    QJsonObject params { { "bounds", QJsonArray { area.x(), area.y(), area.width(), area.height() } },
        { "name", settings["prompt"].toString() }, { "metadata", metadata },
        { "seed", settings["seed"].toVariant().toDouble() }, { "has_mask", !selection.isNull() } };
    QJsonObject entry { { "id", id }, { "slot", m_nextSlot++ }, { "params", params },
        { "kind", "diffusion" }, { "in_use", QJsonObject() } };
    entry["offsets"] = prepared.offsets;
    m_images[entry["slot"].toInt()] = prepared.bytes;
    m_recentId = id;
    m_recentImages = prepared.images;
    for (int i = 0; i < prepared.thumbnails.size(); ++i)
        m_thumbnails[id + ":" + QString::number(i) + ":96"] = prepared.thumbnails[i];
    auto history = entries();
    history.append(entry);
    m_state["history"] = history;
    m_state["version"] = 1;
    m_state["baron_history_unlimited"] = true;
    return id;
}
QImage HistoryStore::decode(const QByteArray& bytes, const QSize& scaled) const {
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 67108864)
        return {};
    if (!scaled.isEmpty())
        reader.setScaledSize(size.scaled(scaled, Qt::KeepAspectRatio));
    return reader.read();
}
QImage HistoryStore::image(const QString& id, int index, const QSize& scaled) const {
    const QString cacheKey = id + ":" + QString::number(index) + ":" + QString::number(scaled.width());
    if (!scaled.isEmpty() && m_thumbnails.contains(cacheKey)) return m_thumbnails.value(cacheKey);
    if (scaled.isEmpty() && id == m_recentId && index >= 0 && index < m_recentImages.size())
        return m_recentImages[index];
    for (auto value : entries()) {
        const auto entry = value.toObject();
        if (entry["id"] != id)
            continue;
        const auto offsets = entry["offsets"].toArray();
        const auto bytes = m_images.value(entry["slot"].toInt());
        if (index < 0 || index >= offsets.size())
            return {};
        const qint64 start = offsets[index].toDouble(-1);
        const qint64 end = index + 1 < offsets.size() ? offsets[index + 1].toDouble(-1) : bytes.size();
        if (start < 0 || end <= start || end > bytes.size())
            return {};
        const auto result = decode(bytes.mid(start, end - start), scaled);
        if (!scaled.isEmpty()) m_thumbnails[cacheKey] = result;
        return result;
    }
    return {};
}
void HistoryStore::mark(const QString& id, int index, bool favorite, bool applied) {
    auto history = entries();
    for (int i = 0; i < history.size(); ++i) {
        auto entry = history[i].toObject();
        if (entry["id"] != id)
            continue;
        auto used = entry["in_use"].toObject();
        used[QString::number(index)] = applied;
        entry["in_use"] = used;
        auto params = entry["params"].toObject();
        auto meta = params["metadata"].toObject();
        auto extra = meta["baron"].toObject();
        auto favorites = extra["favorites"].toObject();
        favorites[QString::number(index)] = favorite;
        extra["favorites"] = favorites;
        meta["baron"] = extra;
        params["metadata"] = meta;
        entry["params"] = params;
        history[i] = entry;
    }
    m_state["history"] = history;
}
bool HistoryStore::remove(const QString& id, int index) {
    auto history = entries();
    for (int i = 0; i < history.size(); ++i) {
        auto entry = history[i].toObject();
        if (entry["id"] != id)
            continue;
        const int count = entry["offsets"].toArray().size();
        if (index < 0 || index >= count)
            return false;
        if (count == 1) {
            history.removeAt(i);
            m_removedSlots.insert(entry["slot"].toInt());
            m_images.remove(entry["slot"].toInt());
        } else {
            QList<QImage> images;
            QJsonObject used, favorites;
            auto params = entry["params"].toObject();
            auto meta = params["metadata"].toObject();
            auto extra = meta["baron"].toObject();
            for (int j = 0; j < count; ++j) {
                if (j == index)
                    continue;
                images.append(image(id, j));
                const auto key = QString::number(images.size() - 1);
                used[key] = entry["in_use"].toObject()[QString::number(j)].toBool();
                favorites[key] = extra["favorites"].toObject()[QString::number(j)].toBool();
            }
            if (!pack(entry, images))
                return false;
            extra["favorites"] = favorites;
            meta["baron"] = extra;
            params["metadata"] = meta;
            entry["params"] = params;
            entry["in_use"] = used;
            history[i] = entry;
        }
        m_state["history"] = history;
        return true;
    }
    return false;
}
void HistoryStore::clear() {
    for (auto it = m_images.begin(); it != m_images.end(); ++it)
        m_removedSlots.insert(it.key());
    m_state["history"] = QJsonArray();
    m_images.clear();
    m_thumbnails.clear();
    m_recentImages.clear();
    m_recentId.clear();
}
void HistoryStore::saveAsync(QObject* owner, const Write& write, std::function<void(QString)> done) {
    m_state["version"] = 1;
    m_bases.insert(digest(QJsonDocument(m_state).toJson(QJsonDocument::Compact)));
    if (write) {
        for (auto it = m_images.begin(); it != m_images.end(); ++it) write(imageKey(it.key()), it.value());
        write("ai_diffusion/ui.json", QJsonDocument(m_state).toJson(QJsonDocument::Compact));
        for (auto slot : m_removedSlots) {
            write(imageKey(slot), {});
            write(QString("ai_diffusion/result%1").arg(slot), {});
        }
    }
    auto snapshot = *this;
    snapshot.m_recentImages.clear();
    snapshot.m_thumbnails.clear();
    const auto epoch = ++*m_saveEpoch;
    m_removedSlots.clear();
    BackgroundWork::run(owner, [snapshot, epoch]() mutable {
        if (snapshot.m_saveEpoch->load() != epoch) return QString();
        snapshot.save(); return snapshot.error();
    }, std::move(done), BackgroundWork::history());
}
HistoryStore::Prepared HistoryStore::prepare(const QList<QImage>& images,
    const QByteArray& selection, const QString& mode, const QImage& source) {
    Prepared result;
    result.images = images;
    result.selection = QImage::fromData(selection);
    result.selectionText = QString::fromLatin1(selection.toBase64());
    if (mode == "background") {
        if (images.size() < 2 || images[0].size() != images[1].size() || images[0].size() != source.size()) {
            result.error = "The server returned an invalid background mask";
            return result;
        }
        auto foreground = images[0].convertToFormat(QImage::Format_ARGB32);
        const auto mask = images[1].convertToFormat(QImage::Format_RGB32);
        const auto original = source.convertToFormat(QImage::Format_ARGB32);
        result.mask = QImage(mask.size(), QImage::Format_Grayscale8);
        for (int y = 0; y < mask.height(); ++y) {
            auto dest = reinterpret_cast<QRgb*>(foreground.scanLine(y));
            const auto colors = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
            const auto alpha = reinterpret_cast<const QRgb*>(original.constScanLine(y));
            auto line = result.mask.scanLine(y);
            for (int x = 0; x < mask.width(); ++x) {
                line[x] = qGray(colors[x]) * qAlpha(alpha[x]) / 255;
                dest[x] = qRgba(qRed(dest[x]), qGreen(dest[x]), qBlue(dest[x]), line[x]);
            }
        }
        result.images = {foreground};
    }
    QBuffer buffer(&result.bytes);
    buffer.open(QIODevice::WriteOnly);
    for (const auto& image : result.images) {
        result.offsets.append(double(buffer.pos()));
        if (image.isNull() || !image.save(&buffer, "PNG")) {
            result.error = "Could not encode history image"; return result;
        }
        result.thumbnails.append(image.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    if (!result.images.isEmpty())
        result.preview = result.images.first().scaled(640, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return result;
}
bool HistoryStore::save(const Write& write) {
    m_error.clear();
    m_state["version"] = 1;
    const auto bytes = QJsonDocument(m_state).toJson(QJsonDocument::Compact);
    m_bases.insert(digest(bytes));
    if (write) {
        for (auto it = m_images.begin(); it != m_images.end(); ++it)
            write(imageKey(it.key()), it.value());
        write("ai_diffusion/ui.json", bytes);
        for (auto slot : m_removedSlots) {
            write(imageKey(slot), {});
            write(QString("ai_diffusion/result%1").arg(slot), {});
        }
        m_removedSlots.clear();
    }
    QJsonObject blobs;
    if (!QDir().mkpath(m_cache)) {
        m_error = "Could not create history cache";
        return false;
    }
    for (auto it = m_images.begin(); it != m_images.end(); ++it) {
        const auto hash = digest(it.value());
        const QString path = m_cache + "/" + QString::fromLatin1(hash) + ".bin";
        if ((!QFile::exists(path) && !writeFile(path, it.value())) || it.value().isEmpty()) {
            m_error = "Could not save history image";
            return false;
        }
        blobs[QString::number(it.key())] = QString::fromLatin1(hash);
    }
    QJsonArray bases;
    for (const auto& hash : m_bases)
        bases.append(QString::fromLatin1(hash));
    const QJsonObject journal { { "schema", 1 }, { "state", m_state }, { "blobs", blobs }, { "bases", bases },
        { "base", QString::fromLatin1(m_baseHash) }, { "latest", QString::fromLatin1(digest(bytes)) } };
    if (!writeFile(m_cache + "/manifest.json", QJsonDocument(journal).toJson(QJsonDocument::Compact))) {
        m_error = "Could not save history manifest";
        return false;
    }
    const auto files = QDir(m_cache).entryList({ "*.bin" }, QDir::Files);
    for (const auto& file : files) {
        bool used = false;
        for (auto value : blobs)
            used |= file == value.toString() + ".bin";
        if (!used)
            QDir(m_cache).remove(file);
    }
    return true;
}
