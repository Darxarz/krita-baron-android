// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QSet>
#include <QPointer>
#include <functional>
class QNetworkReply;

class OrchestrionClient : public QObject {
    Q_OBJECT
public:
    enum Backend { orchestrion, comfyui, interstice };
    Q_ENUM(Backend)
    explicit OrchestrionClient(QObject* parent = nullptr, QNetworkAccessManager* transport = nullptr);
    ~OrchestrionClient() override;
    static QUrl validateRoot(const QString& text, QString* error = nullptr);
    bool setRoot(const QString& text);
    QUrl root() const { return m_root; }
    bool signedIn() const { return m_backend == comfyui ? m_connected : !m_token.isEmpty(); }
    Backend backend() const { return m_backend; }
    void setBackend(Backend backend);
    void setAccessToken(const QByteArray& token);
    void copyConnection(const OrchestrionClient& other);
    void signIn();
    void cancelSignIn();
    void signOut();
    void account();
    void restoreLogin(bool connect = true);
    void models();
    void modelMetadata(const QString& name, const QString& kind,
        std::function<void(const QJsonObject&, const QString&)> done, bool refresh = false);
    void quote(const QJsonObject& prompt);
    void translatePrompt(const QString& text, const QString& mode, std::function<void(const QJsonObject&)> done,
        const QString& model = {});
    void promptOrganizerCapabilities(std::function<void(const QJsonObject&)> done);
    void promptOrganizerLabels(const QJsonArray& fragments, const QString& family, std::function<void(const QJsonObject&)> done);
    virtual void prepare(const QJsonObject& input);
    virtual void submit(const QJsonObject& graph, bool front = false);
    virtual void cancelJob();
    virtual void resume(const QString& promptId);
    virtual void fetchImage(const QJsonObject& descriptor, int index);
    void receiveProgress(const QJsonObject& message);
Q_SIGNALS:
    void error(const QString& message);
    void browserLogin(const QUrl& url, const QString& code);
    void authenticated();
    void accountReady(const QJsonObject& account);
    void modelsReady(const QJsonObject& catalog);
    void prepared(const QJsonObject& data);
    void quoted(const QJsonObject& data);
    void submitted(const QString& promptId);
    void jobReady(const QJsonObject& data);
    void imageReady(const QByteArray& png, int index);
    void cancelled();
    void jobRunning(bool running);
    void progressChanged(double progress);

private:
    using Callback = std::function<void(const QJsonObject&)>;
    void catalogRequest(const QString& path, std::function<void(const QJsonObject&, const QString&)> done,
        bool refresh = false, int cacheSeconds = 86400);
    void request(
        const QString& path, const QJsonObject& body, bool post, Callback done, bool auth = true);
    void pollLogin();
    void pollJob();
    void finishCancellation();
    void abortRequests();
    void openProgress();
    QUrl serviceUrl(const QString& path) const;
    void pollCloudJob();
    void receiveCloudImages(const QJsonObject& data);
    void sendCloudWorkflow(const QJsonObject& workflow);
    void cloudCatalog(const QJsonObject& data);
    QString transportPath(const QString& path) const;
    Backend m_backend = orchestrion;
    bool m_connected = false;
    QJsonObject m_cloudResources, m_cloudWorkflow;
    QByteArray m_cloudImages;
    QJsonArray m_cloudOffsets;
    QString m_cloudWorker;
    QByteArray m_externalQuery;
    QNetworkAccessManager m_network;
    QNetworkAccessManager* m_transport;
    QUrl m_root;
    QByteArray m_token;
    QString m_deviceCode;
    QString m_promptId;
    QTimer m_loginTimer, m_jobTimer;
    qint64 m_loginDeadline = 0;
    int m_generation = 0;
    bool m_pollingJob = false;
    bool m_submitting = false, m_cancelRequested = false;
    bool m_cancelling = false;
    int m_missingJobPolls = 0;
    QList<QPointer<QNetworkReply>> m_requests;
    QWebSocket m_socket;
    QTimer m_reconnect;
    QString m_clientId, m_progressNode;
    QJsonObject m_progressGraph;
    QSet<QString> m_completedNodes;
    double m_progress = -1;
    int m_reconnectDelay = 2000;
};
