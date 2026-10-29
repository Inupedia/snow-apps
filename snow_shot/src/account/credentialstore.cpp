#include "snow_shot/account/credentialstore.h"

#ifdef Q_OS_MACOS
#include <Security/Security.h>
#import <LocalAuthentication/LocalAuthentication.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#endif

namespace snow_shot::account {
#ifdef Q_OS_MACOS
namespace {
CFMutableDictionaryRef queryFor(const QString& account) {
    auto query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    const auto serviceBytes = (QStringLiteral("YuntuSoftware/") + account).toUtf8();
    const auto accountBytes = account.toUtf8();
    auto service =
        CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(serviceBytes.constData()),
                                serviceBytes.size(), kCFStringEncodingUTF8, false);
    auto name =
        CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(accountBytes.constData()),
                                accountBytes.size(), kCFStringEncodingUTF8, false);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, name);
    CFRelease(service);
    CFRelease(name);
    return query;
}
} // namespace
#endif

QByteArray CredentialStore::read(const QString& account) {
#ifdef Q_OS_MACOS
    auto query = queryFor(account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    // An inaccessible shared item must fall back to the login window rather
    // than blocking startup behind an OS authentication prompt.
    LAContext* context = [[LAContext alloc] init];
    context.interactionNotAllowed = YES;
    CFDictionarySetValue(query, kSecUseAuthenticationContext, reinterpret_cast<CFTypeRef>(context));
    [context release];
    CFTypeRef result = nullptr;
    const auto status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || !result)
        return {};
    auto data = static_cast<CFDataRef>(result);
    QByteArray bytes(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), CFDataGetLength(data));
    CFRelease(result);
    return bytes;
#elif defined(Q_OS_WIN)
    const auto target = (QStringLiteral("YuntuSoftware/") + account).toStdWString();
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential))
        return {};
    QByteArray bytes(reinterpret_cast<const char*>(credential->CredentialBlob),
                     credential->CredentialBlobSize);
    CredFree(credential);
    return bytes;
#else
    Q_UNUSED(account);
    return {};
#endif
}

bool CredentialStore::write(const QString& account, const QByteArray& value) {
#ifdef Q_OS_MACOS
    auto query = queryFor(account);
    auto data =
        CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(value.constData()), value.size());
    auto attributes = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kSecValueData, data);
    auto status = SecItemUpdate(query, attributes);
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(query, kSecValueData, data);
        CFDictionarySetValue(query, kSecAttrAccessible,
                             kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
        status = SecItemAdd(query, nullptr);
    }
    CFRelease(attributes);
    CFRelease(data);
    CFRelease(query);
    return status == errSecSuccess;
#elif defined(Q_OS_WIN)
    auto target = (QStringLiteral("YuntuSoftware/") + account).toStdWString();
    auto name = account.toStdWString();
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target.data();
    credential.UserName = name.data();
    credential.CredentialBlobSize = static_cast<DWORD>(value.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(value.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    return CredWriteW(&credential, 0) != FALSE;
#else
    Q_UNUSED(account);
    Q_UNUSED(value);
    return false;
#endif
}

bool CredentialStore::remove(const QString& account) {
#ifdef Q_OS_MACOS
    auto query = queryFor(account);
    const auto status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
#elif defined(Q_OS_WIN)
    const auto target = (QStringLiteral("YuntuSoftware/") + account).toStdWString();
    return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;
#else
    Q_UNUSED(account);
    return false;
#endif
}
} // namespace snow_shot::account
