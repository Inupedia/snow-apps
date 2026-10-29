#pragma once

#include <QByteArray>
#include <QString>

namespace snow_shot::account {
// Family-wide, current-user OS credentials. No plaintext fallback is permitted.
class CredentialStore {
  public:
    static QByteArray read(const QString& account);
    static bool write(const QString& account, const QByteArray& value);
    static bool remove(const QString& account);
};
} // namespace snow_shot::account
