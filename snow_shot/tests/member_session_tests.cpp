#include "snow_shot/account/credentialstore.h"
#include "snow_shot/account/membersession.h"
#include <QCoreApplication>
#include <QDateTime>
#include "snow_shot/account/memberidentity.h"
#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
QHash<QString, QByteArray> credentials;
bool writable = true;
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
bool waitFor(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}
QByteArray success(const QJsonObject& data) {
    return QJsonDocument(QJsonObject{{"httpCode", 200}, {"status", "success"}, {"data", data}})
        .toJson(QJsonDocument::Compact);
}
void respond(QTcpSocket* socket, const QByteArray& body) {
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
    socket->disconnectFromHost();
}
} // namespace
namespace snow_shot::account {
QByteArray CredentialStore::read(const QString& account) {
    return credentials.value(account);
}
bool CredentialStore::write(const QString& account, const QByteArray& value) {
    if (!writable)
        return false;
    credentials[account] = value;
    return true;
}
bool CredentialStore::remove(const QString& account) {
    credentials.remove(account);
    return true;
}
} // namespace snow_shot::account
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTcpServer server;
    require(server.listen(QHostAddress::LocalHost), "HTTP fixture must listen");
    qputenv("SNOW_SHOT_MEMBER_API_BASE_URL",
            "http://127.0.0.1:" + QByteArray::number(server.serverPort()));
    int logins = 0;
    int subscriptions = 0;
    bool holdLogin = false;
    bool unauthorized = false;
    bool rejectOnce = false;
    QTcpSocket* held = nullptr;
    const auto loginResult =
        success({{"tokenValue", "test-token"},
                 {"user", QJsonObject{{"id", 42}, {"telephone", "test-user"}}}});
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto* socket = server.nextPendingConnection();
        auto buffer = std::make_shared<QByteArray>();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer] {
            *buffer += socket->readAll();
            const auto end = buffer->indexOf("\r\n\r\n");
            if (end < 0)
                return;
            int length = 0;
            for (const auto& header : buffer->left(end).split('\n')) {
                if (header.toLower().startsWith("content-length:"))
                    length = header.mid(15).trimmed().toInt();
            }
            if (buffer->size() < end + 4 + length)
                return;
            const auto body = QJsonDocument::fromJson(buffer->mid(end + 4)).object();
            if (buffer->startsWith("POST /api/member/user/login ")) {
                ++logins;
                require(body.value("telephone") == "test-user" &&
                            body.value("password") == "test-password",
                        "Login must preserve contract fields");
                require(!body.contains("serviceId"), "Login must not send serviceId");
                if (holdLogin)
                    held = socket;
                else
                    respond(socket, loginResult);
            } else {
                ++subscriptions;
                require(buffer->toLower().contains("auth-token: test-token"),
                        "Subscription must use auth-token header");
                require(body.value("serviceId") == 11 && body.value("userId") == 42,
                        "Subscription must use capture identity");
                if (unauthorized) {
                    respond(socket, "{\"httpCode\":401,\"status\":\"error\"}");
                    if (rejectOnce)
                        unauthorized = false;
                } else
                    respond(socket, success({{"status", true}}));
            }
            buffer->clear();
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    });
    using snow_shot::account::MemberSession;
    require(snow_shot::account::memberAppCode == QLatin1StringView("inu-capture"),
            "Application identity must be inu-capture");
    MemberSession session;
    session.login("test-user", "test-password", false);
    session.login("test-user", "test-password", false);
    require(waitFor([&] { return subscriptions == 1 && !session.busy(); }),
            "Login and subscription must complete");
    require(logins == 1 && session.signedIn() && session.subscribed(),
            "Duplicate submission must be ignored");
    require(!credentials.contains("member-secret"), "Remember off must not retain a password");
    const auto persisted = QJsonDocument::fromJson(credentials.value("member")).object();
    require(persisted.value("schemaVersion") == 1 && !persisted.contains("password") &&
                !persisted.contains("mac"),
            "Family session must not store password or MAC");
    MemberSession restored;
    require(restored.signedIn() && restored.telephone() == "test-user",
            "New instance must restore family session");
    unauthorized = true;
    session.refresh();
    require(waitFor([&] { return !session.busy(); }), "Unauthorized refresh must finish");
    require(!session.signedIn() && !credentials.contains("member"),
            "401 without secret must clear session");
    unauthorized = false;
    holdLogin = true;
    session.login("test-user", "test-password", true);
    require(waitFor([&] { return held != nullptr; }), "Delayed login must be received");
    session.logout();
    respond(held, loginResult);
    require(waitFor([&] { return !session.busy(); }), "Logout must end loading");
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    require(!session.signedIn() && !credentials.contains("member"),
            "Late login must not undo logout");
    holdLogin = false;
    session.login("test-user", "test-password", true);
    require(waitFor([&] { return session.signedIn() && session.subscribed() && !session.busy(); }),
            "Remembered login must complete");
    require(credentials.contains("member-secret"), "Remember must use the separate secret slot");
    const int beforeRenew = logins;
    unauthorized = true;
    rejectOnce = true;
    session.refresh();
    require(waitFor([&] {
                return logins == beforeRenew + 1 && !session.busy() && session.subscribed();
            }),
            "401 must renew remembered credentials once and retry the subscription");
    unauthorized = true;
    rejectOnce = false;
    const int beforePermanentFailure = logins;
    session.refresh();
    require(waitFor([&] { return !session.busy() && !session.signedIn(); }),
            "Repeated unauthorized responses must return to signed-out state");
    require(logins == beforePermanentFailure + 1, "Silent renewal must not loop indefinitely");
    unauthorized = false;
    writable = false;
    int errors = 0;
    QObject::connect(&session, &MemberSession::failed, &app, [&](const QString&) { ++errors; });
    session.login("test-user", "test-password", false);
    require(waitFor([&] { return errors > 0; }), "Credential write failure must be reported");
    require(!session.signedIn(), "Failed secure persistence must not report successful login");
    credentials["member"] = "{\"schemaVersion\":99,\"tokenValue\":\"bad\",\"userId\":1}";
    MemberSession invalid;
    require(!invalid.signedIn() && !credentials.contains("member"),
            "Unsupported family schema must be rejected");
    auto expired = persisted;
    expired["issuedAt"] =
        QDateTime::currentDateTimeUtc().addSecs(-6 * 60 * 60 - 1).toString(Qt::ISODate);
    credentials["member"] = QJsonDocument(expired).toJson(QJsonDocument::Compact);
    MemberSession expiredSession;
    require(!expiredSession.signedIn() && !credentials.contains("member"),
            "Expired sessions must not unlock the application");
    std::cout << "Member session tests passed\n";
}
