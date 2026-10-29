#pragma once
#include <functional>
class QApplication;
namespace snow_shot::account {
void installAccountUi(QApplication& application, std::function<void()> onLocked);
bool requireMemberSignIn();
} // namespace snow_shot::account
