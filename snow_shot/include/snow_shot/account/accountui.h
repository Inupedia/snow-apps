#pragma once
#include <functional>
class QApplication;
class QWidget;
class SettingsCustomWidget;
namespace snow_shot::account {
void installAccountUi(QApplication& application, std::function<void()> onLocked);
bool requireMemberSignIn();
SettingsCustomWidget* createMemberAccountSettingsWidget(QWidget* parent);
} // namespace snow_shot::account
