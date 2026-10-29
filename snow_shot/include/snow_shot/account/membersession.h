#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

namespace snow_shot::account {
class MemberSession final : public QObject {
    Q_OBJECT
  public:
    explicit MemberSession(QObject* parent = nullptr);
    void login(const QString& telephone, const QString& password, bool remember);
    void refresh();
    void logout();
    bool signedIn() const;
    bool busy() const {
        return m_busy;
    }
    bool subscribed() const {
        return m_subscribed;
    }
    bool subscriptionKnown() const {
        return m_subscriptionKnown;
    }
    QString telephone() const;
  signals:
    void changed();
    void failed(const QString& message);

  private:
    void post(const QString& path, const QJsonObject& body, bool authenticated,
              std::function<void(const QJsonObject&, bool)> completion);
    void restore();
    QNetworkAccessManager m_network;
    QJsonObject m_session;
    bool m_busy = false;
    bool m_subscribed = false;
    bool m_subscriptionKnown = false;
    bool m_silentAttempted = false;
    quint64 m_generation = 0;
};
} // namespace snow_shot::account
