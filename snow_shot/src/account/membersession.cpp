#include "snow_shot/account/membersession.h"
#include "snow_shot/account/credentialstore.h"
#include "snow_shot/account/memberidentity.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace snow_shot::account {
MemberSession::MemberSession(QObject* parent) : QObject(parent), m_network(this) {
    restore();
}

bool MemberSession::signedIn() const {
    return !m_session.value(QStringLiteral("tokenValue")).toString().isEmpty();
}

QString MemberSession::telephone() const {
    return m_session.value(QStringLiteral("telephone")).toString();
}

void MemberSession::restore() {
    const auto previous = m_session.value(QStringLiteral("tokenValue"));
    m_session = QJsonDocument::fromJson(CredentialStore::read(QStringLiteral("member"))).object();
    const auto issuedAt =
        QDateTime::fromString(m_session.value(QStringLiteral("issuedAt")).toString(), Qt::ISODate);
    const auto age = issuedAt.secsTo(QDateTime::currentDateTimeUtc());
    if (!m_session.isEmpty() &&
        (m_session.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
         m_session.value(QStringLiteral("userId")).toDouble() <= 0 || !signedIn() ||
         !issuedAt.isValid() || age < -300 || age >= 6 * 60 * 60)) {
        CredentialStore::remove(QStringLiteral("member"));
        m_session = {};
    }
    if (previous != m_session.value(QStringLiteral("tokenValue"))) {
        m_subscribed = false;
        m_subscriptionKnown = false;
    }
}

void MemberSession::post(const QString& path, const QJsonObject& body, bool authenticated,
                         std::function<void(const QJsonObject&, bool)> completion) {
    const QString base = qEnvironmentVariable("SNOW_SHOT_MEMBER_API_BASE_URL",
                                              QStringLiteral(SNOW_SHOT_MEMBER_API_BASE_URL));
    QNetworkRequest request{QUrl(base + path)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    if (authenticated)
        request.setRawHeader("auth-token",
                             m_session.value(QStringLiteral("tokenValue")).toString().toUtf8());
    auto* reply = m_network.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, completion = std::move(completion)] {
        const auto payload = QJsonDocument::fromJson(reply->readAll()).object();
        // Diagnostic codes only. Never log response bodies, credentials, or request headers.
        qInfo("Member API response: HTTP %d, application %d, network %d",
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
              payload.value(QStringLiteral("httpCode")).toInt(), static_cast<int>(reply->error()));
        const bool unauthorized =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401 ||
            payload.value(QStringLiteral("httpCode")).toInt() == 401;
        const bool success =
            reply->error() == QNetworkReply::NoError &&
            payload.value(QStringLiteral("httpCode")).toInt() == 200 &&
            payload.value(QStringLiteral("status")).toString() == QStringLiteral("success");
        completion(success ? payload : QJsonObject{}, unauthorized);
        reply->deleteLater();
    });
}

void MemberSession::login(const QString& telephone, const QString& password, bool remember) {
    if (m_busy)
        return;
    m_busy = true;
    const auto generation = ++m_generation;
    emit changed();
    QString mac;
    for (const auto& interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            !interface.hardwareAddress().isEmpty()) {
            mac = interface.hardwareAddress();
            break;
        }
    }
    post(
        QStringLiteral("/api/member/user/login"),
        {{QStringLiteral("telephone"), telephone},
         {QStringLiteral("password"), password},
         {QStringLiteral("mac"), mac},
         {QStringLiteral("locale"), QLocale().bcp47Name()}},
        false, [this, telephone, password, remember, generation](const QJsonObject& result, bool) {
            if (generation != m_generation)
                return;
            m_busy = false;
            const auto data = result.value(QStringLiteral("data")).toObject();
            const auto user = data.value(QStringLiteral("user")).toObject();
            if (data.value(QStringLiteral("tokenValue")).toString().isEmpty() ||
                user.value(QStringLiteral("id")).toDouble() <= 0) {
                emit changed();
                emit failed(tr(
                    "Unable to sign in. Check your credentials and connection, then try again."));
                return;
            }
            QJsonObject session{
                {QStringLiteral("schemaVersion"), 1},
                {QStringLiteral("tokenValue"), data.value(QStringLiteral("tokenValue"))},
                {QStringLiteral("userId"), user.value(QStringLiteral("id"))},
                {QStringLiteral("telephone"),
                 user.value(QStringLiteral("telephone")).toString(telephone)},
                {QStringLiteral("issuedAt"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
            if (!CredentialStore::write(QStringLiteral("member"),
                                        QJsonDocument(session).toJson(QJsonDocument::Compact))) {
                emit changed();
                emit failed(tr("Unable to save your session in the system credential store."));
                return;
            }
            if (remember) {
                const QJsonObject secret{{QStringLiteral("schemaVersion"), 1},
                                         {QStringLiteral("telephone"), telephone},
                                         {QStringLiteral("password"), password}};
                if (!CredentialStore::write(QStringLiteral("member-secret"),
                                            QJsonDocument(secret).toJson(QJsonDocument::Compact))) {
                    CredentialStore::remove(QStringLiteral("member-secret"));
                    emit failed(tr("Signed in, but automatic sign-in could not be saved."));
                }
            } else {
                CredentialStore::remove(QStringLiteral("member-secret"));
            }
            m_session = session;
            m_subscribed = false;
            m_subscriptionKnown = false;
            emit changed();
            refresh();
        });
}

void MemberSession::refresh() {
    if (m_busy)
        return;
    restore();
    if (!signedIn()) {
        m_subscribed = false;
        const auto secret =
            QJsonDocument::fromJson(CredentialStore::read(QStringLiteral("member-secret")))
                .object();
        if (!m_silentAttempted && secret.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
            !secret.value(QStringLiteral("password")).toString().isEmpty()) {
            m_silentAttempted = true;
            login(secret.value(QStringLiteral("telephone")).toString(),
                  secret.value(QStringLiteral("password")).toString(), true);
            return;
        }
        emit changed();
        return;
    }
    m_busy = true;
    emit changed();
    const auto generation = m_generation;
    const auto token = m_session.value(QStringLiteral("tokenValue"));
    post(QStringLiteral("/api/member/subscription/querySubscriptInfo"),
         {{QStringLiteral("userId"), m_session.value(QStringLiteral("userId"))},
          {QStringLiteral("serviceId"), memberServiceId}},
         true, [this, token, generation](const QJsonObject& result, bool unauthorized) {
             if (generation != m_generation)
                 return;
             m_busy = false;
             if (token != m_session.value(QStringLiteral("tokenValue")))
                 return;
             if (unauthorized) {
                 const auto secret =
                     QJsonDocument::fromJson(CredentialStore::read(QStringLiteral("member-secret")))
                         .object();
                 const bool canRenew = !m_silentAttempted;
                 logout();
                 m_silentAttempted = true;
                 if (canRenew && secret.value(QStringLiteral("schemaVersion")).toInt() == 1 &&
                     !secret.value(QStringLiteral("password")).toString().isEmpty())
                     login(secret.value(QStringLiteral("telephone")).toString(),
                           secret.value(QStringLiteral("password")).toString(), true);
                 return;
             }
             if (!result.isEmpty()) {
                 m_silentAttempted = false;
                 m_subscribed = result.value(QStringLiteral("data"))
                                    .toObject()
                                    .value(QStringLiteral("status"))
                                    .toBool();
                 m_subscriptionKnown = true;
             }
             emit changed();
         });
}

void MemberSession::logout() {
    ++m_generation;
    m_busy = false;
    CredentialStore::remove(QStringLiteral("member"));
    CredentialStore::remove(QStringLiteral("member-secret"));
    m_session = {};
    m_subscribed = false;
    m_subscriptionKnown = false;
    m_silentAttempted = false;
    emit changed();
}
} // namespace snow_shot::account
