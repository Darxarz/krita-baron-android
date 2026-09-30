// SPDX-License-Identifier: GPL-3.0-or-later
#include "BaronPanel.h"
#include "OrchestrionClient.h"
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QToolButton>
class ClientTest : public QObject {
    Q_OBJECT
private slots:
    void fullScreenGallerySearchAndSelection() {
        BaronPanel panel(nullptr);
        panel.resize(520, 840);
        panel.show();
        auto client = panel.findChild<OrchestrionClient*>();
        QVERIFY(client);
        client->modelsReady({ { "items",
            QJsonArray { QJsonObject { { "name", "qwen.safetensors" }, { "title", "Qwen Edit" },
                             { "family", "Qwen" }, { "kind", "checkpoint" } },
                QJsonObject { { "name", "krea.safetensors" }, { "title", "Krea Turbo" },
                    { "family", "Krea" }, { "kind", "diffusion_model" } },
                QJsonObject { { "name", "identity.safetensors" }, { "title", "Identity LoRA" },
                    { "family", "Krea" }, { "kind", "lora" } } } } });
        int sections = 0;
        for (auto button : panel.findChildren<QToolButton*>())
            if (button->isCheckable()) {
                ++sections;
                QVERIFY(!button->isChecked());
            }
        QVERIFY(sections >= 5);
        auto choose = panel.findChild<QPushButton*>("chooseModelButton");
        QVERIFY(choose);
        bool inspected = false;
        QTimer::singleShot(100, &panel, [&] {
            auto dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto gallery = dialog->findChild<QListWidget*>("modelGallery");
            auto filter = dialog->findChild<QLineEdit*>("modelSearchField");
            inspected = dialog->isFullScreen() && gallery && filter;
            if (inspected) {
                filter->setText("krea");
                inspected = gallery->item(0)->isHidden() && !gallery->item(1)->isHidden()
                    && gallery->item(2)->isHidden();
                gallery->itemClicked(gallery->item(1));
            } else
                dialog->reject();
        });
        choose->click();
        QVERIFY(inspected);
        bool selected = false;
        for (auto combo : panel.findChildren<QComboBox*>())
            selected = selected || combo->currentData().toString() == "krea.safetensors";
        QVERIFY(selected);
        QVERIFY(panel.findChild<QListWidget*>("modelGallery"));
        QCOMPARE(panel.palette().color(QPalette::Window),
            QApplication::palette().color(QPalette::Window));
    }
    void websiteAddresses_data() {
        QTest::addColumn<QString>("address");
        QTest::addColumn<bool>("valid");
        QTest::newRow("https") << "https://orchestrion.su" << true;
        QTest::newRow("lan") << "http://192.168.50.191:3000" << true;
        QTest::newRow("lan-v6") << "http://[fd00::1]:3000" << true;
        QTest::newRow("localhost") << "http://127.0.0.1:3000" << true;
        QTest::newRow("public-http") << "http://orchestrion.su" << false;
        QTest::newRow("credentials") << "https://user:secret@orchestrion.su" << false;
        QTest::newRow("query-token") << "https://orchestrion.su?token=secret" << false;
        QTest::newRow("path-token") << "https://orchestrion.su/krita/secret" << false;
        QTest::newRow("fragment") << "https://orchestrion.su#token" << false;
        QTest::newRow("file") << "file:///tmp/private" << false;
    }
    void websiteAddresses() {
        QFETCH(QString, address);
        QFETCH(bool, valid);
        QCOMPARE(!OrchestrionClient::validateRoot(address).isEmpty(), valid);
    }
    void pngPreservesTransparency() {
        QImage source(3, 2, QImage::Format_ARGB32);
        source.fill(qRgba(30, 80, 120, 17));
        auto restored = QImage::fromData(BaronPanel::png(source));
        QCOMPARE(restored.size(), source.size());
        QCOMPARE(qAlpha(restored.pixel(0, 0)), 17);
    }
    void refusesExternalLoginAddress() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll();
                const QByteArray body
                    = R"({"device_code":"abc","user_code":"123","verification_uri":"https://foreign.example/login","expires_in":600})";
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        OrchestrionClient client;
        QVERIFY(client.setRoot(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        QSignalSpy error(&client, &OrchestrionClient::error),
            browser(&client, &OrchestrionClient::browserLogin);
        client.signIn();
        QTRY_COMPARE(error.size(), 1);
        QCOMPARE(browser.size(), 0);
        QVERIFY(!client.signedIn());
    }
    void deviceLoginPrepareAndDownload() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int authorized = 0;
        QImage source(8, 8, QImage::Format_ARGB32);
        source.fill(qRgba(80, 120, 160, 70));
        const auto image = BaronPanel::png(source);
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                QByteArray bytes
                    = socket->property("requestBytes").toByteArray() + socket->readAll();
                socket->setProperty("requestBytes", bytes);
                int boundary = bytes.indexOf("\r\n\r\n");
                if (boundary < 0 || socket->property("replied").toBool())
                    return;
                int length = 0;
                for (auto line : bytes.left(boundary).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        length = line.mid(15).trimmed().toInt();
                if (bytes.size() < boundary + 4 + length)
                    return;
                socket->setProperty("replied", true);
                const auto path = bytes.split(' ').value(1);
                if (!path.contains("/device/")) {
                    QVERIFY(bytes.contains("Authorization: Bearer ork_krita_test"));
                    ++authorized;
                }
                QByteArray response;
                if (path.contains("/device/start"))
                    response
                        = R"({"device_code":"abc","user_code":"AB12345678","verification_uri":"/connect/krita","expires_in":600,"interval":5})";
                else if (path.contains("/device/poll"))
                    response = R"({"access_token":"ork_krita_test"})";
                else if (path.contains("/account"))
                    response = R"({"name":"Test artist","coins":42})";
                else if (path.contains("/models"))
                    response = R"({"items":[]})";
                else if (path.contains("/native/prepare"))
                    response
                        = R"({"prompt":{"1":{"class_type":"SaveImage","inputs":{}}},"coins":7})";
                else if (path.contains("/prompt"))
                    response = R"({"prompt_id":"test-job"})";
                else if (path.contains("/history/"))
                    response
                        = R"({"test-job":{"status":{"status_str":"success"},"outputs":{"1":{"images":[{"filename":"result.png","type":"output","subfolder":""}]}}}})";
                else if (path.contains("/view?"))
                    response = image;
                else
                    QFAIL("Unexpected native client request");
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(response.size())
                    + "\r\nConnection: close\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        OrchestrionClient client;
        QVERIFY(client.setRoot(QString("http://127.0.0.1:%1").arg(server.serverPort())));
        QSignalSpy login(&client, &OrchestrionClient::authenticated),
            prepared(&client, &OrchestrionClient::prepared),
            submitted(&client, &OrchestrionClient::submitted),
            ready(&client, &OrchestrionClient::jobReady),
            downloaded(&client, &OrchestrionClient::imageReady),
            errors(&client, &OrchestrionClient::error);
        client.signIn();
        QTRY_COMPARE_WITH_TIMEOUT(login.size(), 1, 7000);
        client.prepare({ { "mode", "generate" } });
        QTRY_COMPARE(prepared.size(), 1);
        client.submit(prepared.first().first().toJsonObject()["prompt"].toObject());
        QTRY_COMPARE(submitted.size(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 4000);
        const auto descriptor = ready.first()
                                    .first()
                                    .toJsonObject()["outputs"]
                                    .toObject()["1"]
                                    .toObject()["images"]
                                    .toArray()
                                    .first()
                                    .toObject();
        client.fetchImage(descriptor, 0);
        QTRY_COMPARE(downloaded.size(), 1);
        QCOMPARE(QImage::fromData(downloaded.first().first().toByteArray()).pixel(0, 0),
            source.pixel(0, 0));
        QVERIFY(authorized >= 6);
        QCOMPARE(errors.size(), 0);
    }
};
QTEST_MAIN(ClientTest)
#include "ClientTest.moc"
