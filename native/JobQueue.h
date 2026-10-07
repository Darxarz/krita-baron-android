// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "OrchestrionClient.h"
#include "HistoryStore.h"
#include <QImage>
#include <QMap>
#include <QSharedPointer>

class JobQueue : public QObject {
    Q_OBJECT
public:
    enum State { preparing, submitting, queued, running, downloading, failed, cancelled, finished };
    static constexpr int max_jobs = 128;
    static constexpr int max_batch = 64;
    struct Job {
        QString id, promptId, issue;
        QJsonObject context;
        QJsonObject input;
        QImage source;
        QMap<int, QImage> images;
        HistoryStore::Prepared result;
        QSet<int> decoding;
        qint64 decodingBytes = 0;
        quint64 resultEpoch = 0;
        OrchestrionClient* client = nullptr;
        State state = preparing;
        int expectedImages = 0;
        double progress = -1;
        qint64 requestBytes = 0;
        bool started = false, cancelPending = false;
    };
    using Factory = std::function<OrchestrionClient*(QObject*)>;
    explicit JobQueue(
        OrchestrionClient* connection, QObject* parent = nullptr, Factory factory = {});
    QString start(
        const QJsonObject& input, const QJsonObject& context, const QImage& source, bool front);
    QSharedPointer<Job> job(const QString& id) const { return m_jobs.value(id); }
    QList<QSharedPointer<Job>> jobs() const { return m_jobs.values(); }
    void cancel(const QString& id);
    void retry(const QString& id);
    void dismiss(const QString& id);
    int activeCount() const;
    static bool active(State state) { return state <= downloading; }
Q_SIGNALS:
    void changed(const QString& id);
    void completed(const QString& id);
    void estimate(const QString& id, double coins);
    void error(const QString& message);

private:
    void setState(const QSharedPointer<Job>& job, State state);
    void fail(const QSharedPointer<Job>& job, const QString& issue);
    qint64 memoryBytes() const;
    void pump();
    OrchestrionClient* m_connection;
    Factory m_factory;
    QMap<QString, QSharedPointer<Job>> m_jobs;
    QList<QString> m_waiting;
    bool m_pumping = false;
};
