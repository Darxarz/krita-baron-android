// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

namespace BackgroundWork {
inline QThreadPool* images() {
    static auto pool = new QThreadPool;
    static const bool configured = [] { pool->setMaxThreadCount(2); return true; }();
    Q_UNUSED(configured);
    return pool;
}
inline QThreadPool* history() {
    static auto pool = new QThreadPool;
    static const bool configured = [] { pool->setMaxThreadCount(1); return true; }();
    Q_UNUSED(configured);
    return pool;
}
template<class Work, class Done>
void run(QObject* owner, Work work, Done done, QThreadPool* pool = images()) {
    using Result = decltype(work());
    auto watcher = new QFutureWatcher<Result>(owner);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, owner,
        [watcher, done] { const auto result = watcher->result(); watcher->deleteLater(); done(result); });
    watcher->setFuture(QtConcurrent::run(pool, std::move(work)));
}
}
