// SPDX-License-Identifier: GPL-3.0-or-later
#include "JobQueue.h"
#include "BackgroundWork.h"
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QSaveFile>
#include <QUuid>
#include <QScopedValueRollback>

JobQueue::JobQueue(OrchestrionClient* connection, QObject* parent, Factory factory)
    : QObject(parent)
    , m_connection(connection)
    , m_factory(factory) {
}
qint64 JobQueue::memoryBytes() const {
    qint64 bytes = 0;
    for (const auto& job : m_jobs) {
        bytes += job->requestBytes + job->source.sizeInBytes() + job->decodingBytes;
        for (const auto& image : job->images)
            bytes += image.sizeInBytes();
    }
    return bytes;
}
void JobQueue::setState(const QSharedPointer<Job>& job, State state) {
    job->state = state;
    emit changed(job->id);
    pump();
}
int JobQueue::activeCount() const {
    int count = 0;
    for (const auto& job : m_jobs)
        count += active(job->state);
    return count;
}
void JobQueue::pump() {
    if (m_pumping) return;
    QScopedValueRollback<bool> pumping(m_pumping, true);
    int inFlight = 0;
    for (const auto& job : m_jobs)
        inFlight += job->started && (job->state == preparing || job->state == submitting);
    while (inFlight < 4 && !m_waiting.isEmpty()) {
        const auto job = m_jobs.value(m_waiting.takeFirst());
        if (!job || job->state != preparing) continue;
        job->started = true;
        const auto input = job->input;
        job->input = {};
        job->client->prepare(input);
        inFlight = 0;
        for (const auto& current : m_jobs)
            inFlight += current->started && (current->state == preparing || current->state == submitting);
    }
}
void JobQueue::fail(const QSharedPointer<Job>& job, const QString& issue) {
    ++job->resultEpoch;
    job->issue = issue;
    job->requestBytes = 0;
    job->input = {};
    job->images.clear();
    setState(job, failed);
}
QString JobQueue::start(
    const QJsonObject& input, const QJsonObject& context, const QImage& source, bool front) {
    for (const auto& job : m_jobs.values())
        if ((job->state == cancelled && !job->cancelPending && job->issue.isEmpty()) || job->state == finished)
            dismiss(job->id);
    const auto bytes = QJsonDocument(input).toJson(QJsonDocument::Compact).size();
    if (activeCount() >= max_jobs) {
        emit error(tr("The queue is full. Wait for a result or remove a stopped job."));
        return {};
    }
    if (memoryBytes() + bytes + source.sizeInBytes() > 256 * 1024 * 1024) {
        emit error(tr("Queued images exceed the memory limit. Queue fewer large images or remove stopped jobs."));
        return {};
    }
    auto job = QSharedPointer<Job>::create();
    job->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job->context = context;
    job->source = source;
    job->requestBytes = bytes;
    job->input = input;
    job->client = m_factory ? m_factory(this) : new OrchestrionClient(this);
    job->client->copyConnection(*m_connection);
    m_jobs.insert(job->id, job);
    connect(job->client, &OrchestrionClient::prepared, this,
        [this, job, front](const QJsonObject& data) {
            if (job->state != preparing)
                return;
            job->requestBytes = 0;
            auto settings = job->context["settings"].toObject();
            const auto metadata = data["metadata"].toObject();
            for (auto it = metadata.begin(); it != metadata.end(); ++it)
                if (it.key() == "prompt_final" || it.key() == "negative_prompt_final")
                    settings[it.key()] = it.value();
            job->context["settings"] = settings;
            if (data["coins"].isDouble()) emit estimate(job->id, data["coins"].toDouble());
            if (QSettings("BaronEdition", "Orchestrion").value("debug_dump_workflow", false).toBool()) {
                const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
                QDir().mkpath(directory);
                QSaveFile file(directory + "/workflow.json");
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(QJsonDocument(data["prompt"].toObject()).toJson());
                    file.commit();
                }
            }
            setState(job, submitting);
            job->client->submit(data["prompt"].toObject(), front);
        });
    connect(job->client, &OrchestrionClient::submitted, this, [this, job](const QString& id) {
        job->promptId = id;
        if (job->state == cancelled) return;
        if (job->state != submitting) return;
        setState(job, queued);
    });
    connect(job->client, &OrchestrionClient::jobRunning, this, [this, job](bool isRunning) {
        if (job->state == queued || job->state == running)
            setState(job, isRunning ? running : queued);
    });
    connect(job->client, &OrchestrionClient::progressChanged, this, [this, job](double value) {
        if (!active(job->state) || job->state == downloading)
            return;
        job->progress = qMax(job->progress, qBound(0.0, value, .99));
        setState(job, running);
    });
    connect(job->client, &OrchestrionClient::cancelled, this, [this, job] {
        job->cancelPending = false;
        job->issue.clear();
        job->promptId.clear();
        job->images.clear();
        job->source = {};
        job->requestBytes = 0;
        setState(job, cancelled);
    });
    connect(job->client, &OrchestrionClient::error, this, [this, job](const QString& issue) {
        if (job->state == cancelled && job->cancelPending) {
            job->cancelPending = false;
            job->issue = tr("Cancelled locally; server cancellation could not be confirmed: %1").arg(issue);
            emit changed(job->id);
            emit error(job->issue);
            return;
        }
        if (active(job->state))
            fail(job, issue);
    });
    connect(job->client, &OrchestrionClient::jobReady, this, [this, job](const QJsonObject& data) {
        if (!active(job->state) || job->state == downloading)
            return;
        if (data["status"].toObject()["status_str"] == "error") {
            job->promptId.clear();
            fail(job, tr("Generation failed on the server. No result was applied."));
            return;
        }
        QList<QJsonObject> descriptors;
        const auto outputs = data["outputs"].toObject();
        for (auto it = outputs.begin(); it != outputs.end(); ++it)
            for (auto value : it.value().toObject()["images"].toArray())
                descriptors.append(value.toObject());
        if (descriptors.isEmpty() || descriptors.size() > 4) {
            fail(job, tr("No images in the server result"));
            return;
        }
        ++job->resultEpoch;
        job->decoding.clear();
        job->expectedImages = descriptors.size();
        job->images.clear();
        setState(job, downloading);
        for (int i = 0; i < descriptors.size(); ++i)
            job->client->fetchImage(descriptors[i], i);
    });
    connect(job->client, &OrchestrionClient::imageReady, this,
        [this, job](const QByteArray& bytes, int index) {
            if (job->state != downloading || index < 0 || index >= job->expectedImages)
                return;
            QBuffer buffer;
            buffer.setData(bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer);
            const auto size = reader.size();
            if (!size.isValid() || qint64(size.width()) * size.height() > 67108864
                || memoryBytes() + qint64(size.width()) * size.height() * 4 > 512 * 1024 * 1024) {
                fail(job, tr("Could not decode the result image"));
                return;
            }
            if (job->decoding.contains(index) || job->images.contains(index)) return;
            const auto reservation = qint64(size.width()) * size.height() * 4;
            job->decoding.insert(index);
            job->decodingBytes += reservation;
            const auto epoch = job->resultEpoch;
            BackgroundWork::run(this, [bytes] {
                return QImage::fromData(bytes);
            }, [this, job, index, epoch, reservation](const QImage& image) {
                job->decodingBytes -= reservation;
                if (job->state != downloading || epoch != job->resultEpoch || m_jobs.value(job->id) != job) return;
                job->decoding.remove(index);
                if (image.isNull()) { fail(job, tr("Could not decode the result image")); return; }
                job->images[index] = image;
                if (job->images.size() != job->expectedImages) return;
                const auto images = job->images.values();
                const auto context = job->context;
                const auto source = job->source;
                BackgroundWork::run(this, [images, context, source] {
                    return HistoryStore::prepare(images, QByteArray::fromBase64(
                        context["selection_mask"].toString().toLatin1()), context["mode"].toString(), source);
                }, [this, job, epoch](const HistoryStore::Prepared& result) {
                    if (job->state != downloading || epoch != job->resultEpoch || m_jobs.value(job->id) != job) return;
                    if (!result.error.isEmpty()) { fail(job, tr("Could not prepare the result image: %1").arg(result.error)); return; }
                    job->result = result;
                    setState(job, finished);
                    emit completed(job->id);
                    job->source = {};
                    job->images.clear();
                    job->client->deleteLater();
                    m_jobs.remove(job->id);
                });
            });
        });
    emit changed(job->id);
    if (front) m_waiting.prepend(job->id);
    else m_waiting.append(job->id);
    pump();
    return job->id;
}
void JobQueue::cancel(const QString& id) {
    const auto job = m_jobs.value(id);
    if (!job || job->cancelPending || (job->state == cancelled && job->issue.isEmpty())) return;
    if (active(job->state) || job->state == failed || job->state == cancelled) {
        ++job->resultEpoch;
        job->cancelPending = job->started;
        job->issue.clear();
        job->input = {};
        job->images.clear();
        job->source = {};
        job->requestBytes = 0;
        m_waiting.removeAll(id);
        setState(job, cancelled);
        if (!job->cancelPending) return;
        job->client->cancelJob();
    }
}
void JobQueue::retry(const QString& id) {
    const auto job = m_jobs.value(id);
    if (!job || job->state != failed || job->promptId.isEmpty())
        return;
    if (activeCount() >= max_jobs) {
        emit error(tr("The queue is full. Wait for a result or remove a stopped job."));
        return;
    }
    ++job->resultEpoch;
    job->issue.clear();
    job->images.clear();
    setState(job, queued);
    job->client->resume(job->promptId);
}
void JobQueue::dismiss(const QString& id) {
    const auto job = m_jobs.value(id);
    if (!job || active(job->state) || job->cancelPending)
        return;
    job->client->deleteLater();
    m_jobs.remove(id);
    emit changed(id);
}
