#include "snow_shot/account/accountui.h"
#include "snow_shot/account/credentialstore.h"
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace snow_shot::account {
QByteArray CredentialStore::read(const QString&) {
    return {};
}
bool CredentialStore::write(const QString&, const QByteArray&) {
    return true;
}
bool CredentialStore::remove(const QString&) {
    return true;
}
} // namespace snow_shot::account
namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
} // namespace
int main(int argc, char** argv) {
    QApplication application(argc, argv);
    const bool dark = application.arguments().contains(QStringLiteral("--dark"));
    if (dark) {
        QPalette palette = application.palette();
        palette.setColor(QPalette::Window, QColor(35, 39, 48));
        palette.setColor(QPalette::WindowText, Qt::white);
        application.setPalette(palette);
    }
    bool lockedCallback = false;
    snow_shot::account::installAccountUi(application, [&] { lockedCallback = true; });
    QTimer::singleShot(0, &application, [&] {
        QDialog* dialog = nullptr;
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == QStringLiteral("memberAccountDialog"))
                dialog = qobject_cast<QDialog*>(widget);
        }
        require(dialog && dialog->isVisible(),
                "Standalone login must be visible before application access");
        require(dialog->parentWidget() == nullptr,
                "Login gate must not sit above a functional main window");
        auto* submit = dialog->findChild<QPushButton*>(QStringLiteral("memberSubmit"));
        require(submit && !submit->isEnabled(), "Empty credentials must not submit");
        for (auto* button : dialog->findChildren<QPushButton*>())
            require(button->text() != QStringLiteral("Not now"),
                    "Mandatory login must not have a skip action");
        auto* telephone = dialog->findChild<QLineEdit*>(QStringLiteral("memberTelephone"));
        auto* password = dialog->findChild<QLineEdit*>(QStringLiteral("memberPassword"));
        require(telephone && password && password->echoMode() == QLineEdit::Password,
                "Login must expose labeled fields and obscure the password");
        telephone->setText(QStringLiteral("test-user"));
        password->setText(QStringLiteral("test-password"));
        require(submit->isEnabled(), "Complete credentials must enable submission");
        dialog->grab().save(dark ? QStringLiteral("/tmp/inushot-login-dark.png")
                                 : QStringLiteral("/tmp/inushot-login-light.png"));
        dialog->resize(420, 240);
        application.processEvents();
        auto* scroll = dialog->findChild<QScrollArea*>();
        require(scroll && scroll->verticalScrollBar()->maximum() > 0,
                "Short login windows must keep their form reachable by scrolling");
        dialog->reject();
    });
    require(!snow_shot::account::requireMemberSignIn(),
            "Closing login must not grant application access");
    require(!lockedCallback, "A never-authenticated session must not claim an unlocked controller");
    std::cout << "Mandatory account UI tests passed\n";
}
