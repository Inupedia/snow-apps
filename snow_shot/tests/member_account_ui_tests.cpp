#include "snow_shot/account/accountui.h"
#include "snow_shot/account/credentialstore.h"
#include "snow_shot/presentation/components/settingscustomwidget.h"
#include <QAbstractButton>
#include <memory>
#include <QApplication>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QPixmap>
#include <QLabel>
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
    auto accountSection = std::unique_ptr<SettingsCustomWidget>(
        snow_shot::account::createMemberAccountSettingsWidget(nullptr));
    auto* signOut = accountSection->findChild<QAbstractButton*>(QStringLiteral("memberSignOut"));
    require(signOut && signOut->text() == QStringLiteral("Sign out"),
            "Settings user section must expose an inline sign-out action");
    require(!signOut->isEnabled(), "Signed-out settings must not offer an active sign-out action");
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
        auto* artwork = dialog->findChild<QWidget*>(QStringLiteral("memberLoginArtwork"));
        require(artwork && artwork->isVisible(), "Wide login must display its artwork");
        require(!QPixmap(QStringLiteral(":/snow-shot/account/login-alpine-light.png")).isNull() &&
                    !QPixmap(QStringLiteral(":/snow-shot/account/login-alpine-dark.png")).isNull(),
                "Both theme artwork assets must be bundled");
        require(artwork->property("artworkAppearance").toString() ==
                    (dark ? QStringLiteral("dark") : QStringLiteral("light")),
                "Login artwork must match the initial application appearance");
        const QPalette originalPalette = application.palette();
        QPalette switchedPalette = originalPalette;
        switchedPalette.setColor(QPalette::Window, dark ? QColor("#fafbfd") : QColor("#171b22"));
        application.setPalette(switchedPalette);
        application.processEvents();
        const QImage switched = dialog->grab().toImage();
        require(artwork->property("artworkAppearance").toString() ==
                    (dark ? QStringLiteral("light") : QStringLiteral("dark")),
                "Visible login artwork must switch when application appearance changes");
        const QColor formPixel = switched.pixelColor(switched.width() - 8, switched.height() / 2);
        require((formPixel.lightness() < 128) != dark,
                "The form surface must switch together with artwork");
        auto* links = dialog->findChild<QLabel*>(QStringLiteral("memberLinks"));
        require(links && links->text().contains(dark ? QStringLiteral("#005fcc")
                                                     : QStringLiteral("#accdff")),
                "Link contrast must update on appearance changes");
        require(telephone->text() == QStringLiteral("test-user") &&
                    password->text() == QStringLiteral("test-password"),
                "Switching appearance must preserve in-progress credentials");
        application.setPalette(originalPalette);
        application.processEvents();
        dialog->resize(420, 240);
        application.processEvents();
        require(!artwork->isVisible(), "Narrow login must prioritize the usable form");
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
