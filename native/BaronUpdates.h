// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QCryptographicHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSaveFile>
#include <QUrl>
#include <memory>

class BaronUpdates : public QObject {
    Q_OBJECT
public:
    enum State { idle, checking, latest, available, downloading, ready, failed };
    explicit BaronUpdates(QObject* parent = nullptr, QNetworkAccessManager* network = nullptr);
    static QUrl feedUrl();
    static int installedVersionCode();
    static QString version();
    static QJsonObject validatePackage(const QJsonObject& manifest, const QUrl& feed);
    void check();
    void download();
    void install();
    void cancel();
    State state() const { return m_state; }
    QString latestVersion() const { return m_package["version"].toString(); }
    QString downloadedFile() const { return m_path; }
Q_SIGNALS:
    void changed();
    void message(const QString& text);
    void progress(qint64 received, qint64 total);

private:
    void fail();
    void setState(State state);
    QNetworkAccessManager* m_network;
    QJsonObject m_package;
    State m_state = idle;
    QNetworkReply* m_reply = nullptr;
    QString m_path;
    std::unique_ptr<QSaveFile> m_file;
    QCryptographicHash m_hash { QCryptographicHash::Sha256 };
    qint64 m_received = 0;
};
