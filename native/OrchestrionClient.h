// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <functional>

class OrchestrionClient : public QObject {
    Q_OBJECT
public:
    explicit OrchestrionClient(QObject* parent = nullptr);
    static QUrl validateRoot(const QString& text, QString* error = nullptr);
    bool setRoot(const QString& text);
    QUrl root() const { return m_root; }
    bool signedIn() const { return !m_token.isEmpty(); }
    void signIn();
    void cancelSignIn();
    void signOut();
    void account();
    void restoreLogin();
    void models();
    void prepare(const QJsonObject& input);
    void submit(const QJsonObject& graph);
    void resume(const QString& promptId);
    void fetchImage(const QJsonObject& descriptor, int index);
Q_SIGNALS:
    void error(const QString& message);
    void browserLogin(const QUrl& url, const QString& code);
    void authenticated();
    void accountReady(const QJsonObject& account);
    void modelsReady(const QJsonObject& catalog);
    void prepared(const QJsonObject& data);
    void submitted(const QString& promptId);
    void jobReady(const QJsonObject& data);
    void imageReady(const QByteArray& png, int index);

private:
    using Callback = std::function<void(const QJsonObject&)>;
    void request(
        const QString& path, const QJsonObject& body, bool post, Callback done, bool auth = true);
    void pollLogin();
    void pollJob();
    QNetworkAccessManager m_network;
    QUrl m_root;
    QByteArray m_token;
    QString m_deviceCode;
    QString m_promptId;
    QTimer m_loginTimer, m_jobTimer;
    qint64 m_loginDeadline = 0;
    int m_generation = 0;
    bool m_pollingJob = false;
};
