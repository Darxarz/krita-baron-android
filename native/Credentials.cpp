// SPDX-License-Identifier: GPL-3.0-or-later
#include "Credentials.h"
#include "ServiceAddress.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#ifdef Q_OS_ANDROID
#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#endif
namespace {
QString credentialKey(const QUrl& root) {
    return QString("encryptedLogin/" + QCryptographicHash::hash(root.toEncoded(), QCryptographicHash::Sha256).toHex());
}
QString transform(const QString& method, const QString& value) {
#ifdef Q_OS_ANDROID
    const auto input = QAndroidJniObject::fromString(value);
    const auto output = QAndroidJniObject::callStaticObjectMethod("org/baron/krita/Credentials",
        method.toUtf8().constData(), "(Ljava/lang/String;)Ljava/lang/String;",
        input.object<jstring>());
    QAndroidJniEnvironment environment;
    if (environment->ExceptionCheck()) {
        environment->ExceptionClear();
        return {};
    }
    return output.isValid() ? output.toString() : QString();
#else
    Q_UNUSED(method);
    Q_UNUSED(value);
    return {};
#endif
}
}
QByteArray BaronCredentials::load(const QUrl& root) {
    QSettings settings("BaronEdition", "Orchestrion");
    QList<QUrl> addresses { root };
    if (root == QUrl("https://orchestrion.ink")) {
        addresses.append(QUrl("https://orchestrion.su"));
        addresses.append(QUrl("https://orchestrion.su:443"));
    }
    QStringList candidates;
    for (const auto& address : addresses)
        candidates.append(settings.value(credentialKey(address)).toString());
    candidates.append(settings.value("encryptedLogin").toString());
    for (const auto& encrypted : candidates) {
        if (encrypted.isEmpty()) continue;
        const auto raw = transform("unseal", encrypted);
        const auto data = QJsonDocument::fromJson(raw.toUtf8()).object();
        const QUrl saved(data["website"].toString());
        if (saved != root && BaronService::migrate(saved) != root) continue;
        const auto token = data["token"].toString().toUtf8();
        if (token.isEmpty()) continue;
        if (saved != root) save(root, token);
        return token;
    }
    return {};
}
bool BaronCredentials::save(const QUrl& root, const QByteArray& token) {
    const auto data = QJsonDocument(
        QJsonObject { { "website", root.toString() }, { "token", QString::fromUtf8(token) } })
                          .toJson(QJsonDocument::Compact);
    const auto encrypted = transform("seal", QString::fromUtf8(data));
    if (encrypted.isEmpty())
        return false;
    QSettings settings("BaronEdition", "Orchestrion");
    settings.setValue(credentialKey(root), encrypted);
    return true;
}
void BaronCredentials::clear(const QUrl& root) {
    QSettings settings("BaronEdition", "Orchestrion");
    if (root.isEmpty()) { settings.remove("encryptedLogin"); return; }
    settings.remove(credentialKey(root));
    if (root == QUrl("https://orchestrion.ink")) {
        settings.remove(credentialKey(QUrl("https://orchestrion.su")));
        settings.remove(credentialKey(QUrl("https://orchestrion.su:443")));
    }
    const auto legacy = QJsonDocument::fromJson(transform("unseal", settings.value("encryptedLogin").toString()).toUtf8()).object();
    if (BaronService::migrate(QUrl(legacy["website"].toString())) == root) settings.setValue("encryptedLogin", QString());
}
