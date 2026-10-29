#include "snow_shot/account/accountui.h"
#include "snow_shot/account/membersession.h"

#include <QApplication>
#include "widgets/checkbox.h"
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QEventLoop>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QScrollArea>
#include <QScreen>
#include <QStyleHints>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace snow_shot::account {
#ifdef Q_OS_MACOS
void applyAccountWindowMaterial(QWidget* window);
#endif
namespace {
class AccountUi final : public QObject {
  public:
    explicit AccountUi(QApplication& application, std::function<void()> onLocked)
        : QObject(&application), m_onLocked(std::move(onLocked)), m_session(this) {
        application.setQuitOnLastWindowClosed(false);
        application.installEventFilter(this);
        auto* validationTimer = new QTimer(this);
        validationTimer->setInterval(60000);
        connect(validationTimer, &QTimer::timeout, this, [this] {
            if (m_session.signedIn())
                m_session.refresh();
        });
        validationTimer->start();
        connect(&m_session, &MemberSession::changed, this, [this] {
            if (m_unlocked && !m_session.signedIn()) {
                m_unlocked = false;
                QTimer::singleShot(0, this, [this] { m_onLocked(); });
            }
            if (m_entry)
                m_entry->setText(m_session.signedIn()
                                     ? QCoreApplication::translate("MemberAccount", "Account")
                                     : QCoreApplication::translate("MemberAccount", "Sign in"));
        });
    }

    bool requireSignIn() {
        m_gate = true;
        if (!m_session.signedIn()) {
            show();
            auto* dialog = m_dialog.data();
            QEventLoop loop;
            connect(dialog, &QDialog::finished, &loop, &QEventLoop::quit);
            loop.exec();
            if (m_dialog) {
                m_dialog->hide();
                m_dialog->deleteLater();
                m_dialog = nullptr;
            }
        }
        m_gate = false;
        m_unlocked = m_session.signedIn();
        return m_unlocked;
    }

  private:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_dialog && event->type() == QEvent::ApplicationPaletteChange && m_style)
            m_style();
        if (event->type() == QEvent::LanguageChange && watched == m_window && m_entry) {
            m_entry->setText(m_session.signedIn()
                                 ? QCoreApplication::translate("MemberAccount", "Account")
                                 : QCoreApplication::translate("MemberAccount", "Sign in"));
        }
        if (event->type() == QEvent::Show && !m_window && watched->inherits("MainWindow")) {
            auto* window = qobject_cast<QMainWindow*>(watched);
            if (!window || !window->centralWidget())
                return false;
            m_window = window;
            auto* layout = qobject_cast<QVBoxLayout*>(window->centralWidget()->layout());
            if (layout) {
                auto* row = new QWidget(window);
                auto* controls = new QHBoxLayout(row);
                controls->setContentsMargins(20, 4, 20, 4);
                controls->addStretch();
                m_entry =
                    new QPushButton(QCoreApplication::translate("MemberAccount", "Sign in"), row);
                controls->addWidget(m_entry);
                layout->insertWidget(1, row);
                connect(m_entry, &QPushButton::clicked, this, [this] { show(); });
            }
            QTimer::singleShot(0, this, [this] { m_session.refresh(); });
        } else if (event->type() == QEvent::ApplicationActivate) {
            m_session.refresh();
        }
        return QObject::eventFilter(watched, event);
    }

    void show() {
        if (m_dialog) {
            m_dialog->raise();
            m_dialog->activateWindow();
            return;
        }
        auto* dialog = new QDialog(m_window);
        m_dialog = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QCoreApplication::translate("MemberAccount", "Yuntu account"));
        dialog->setWindowModality(Qt::WindowModal);
        dialog->setObjectName(QStringLiteral("memberAccountDialog"));
        dialog->setMinimumWidth(360);
        dialog->resize(m_gate ? 760 : 440, 560);
        const auto style = [dialog] {
            const bool dark = dialog->palette().color(QPalette::Window).lightness() < 128;
            const auto surface = dark ? QStringLiteral("#232730") : QStringLiteral("#fafbfd");
            const auto ink = dark ? QStringLiteral("#f5f5f7") : QStringLiteral("#1d1d1f");
            const auto field = dark ? QStringLiteral("#2b2f38") : QStringLiteral("#ffffff");
            const auto border = dark ? QStringLiteral("#9aa0ac") : QStringLiteral("#6e7076");
            dialog->setStyleSheet(
                QStringLiteral(
                    "QDialog#memberAccountDialog {background:%1; color:%2;}"
                    "QLabel,QCheckBox {color:%2; background:transparent;}"
                    "QLineEdit {background:%3; color:%2; border:1px solid %4; border-radius:10px; "
                    "padding:10px;}"
                    "QLineEdit:focus {border:2px solid #007aff; padding:9px;}"
                    "QPushButton {color:%2; background:%3; border:1px solid %4; "
                    "border-radius:10px; padding:10px 16px;}"
                    "QPushButton:focus {border:2px solid #007aff; padding:9px 15px;}"
                    "QPushButton:default {color:white; background:#006be0; border-color:#006be0;}"
                    "QPushButton:default:hover {background:#005bc4;}"
                    "QPushButton:disabled {color:%4; background:%1;}"
                    "QLabel#memberError {color:%5; border-radius:8px; padding:10px; "
                    "background:%6;}")
                    .arg(surface, ink, field, border,
                         dark ? QStringLiteral("#ff929b") : QStringLiteral("#c40012"),
                         dark ? QStringLiteral("#46232a") : QStringLiteral("#fff0f1")));
        };
        style();
        m_style = style;
        auto* outerLayout = new QVBoxLayout(dialog);
        outerLayout->setContentsMargins(0, 0, 0, 16);
        auto* scroll = new QScrollArea(dialog);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget(scroll);
        content->setMaximumWidth(480);
        scroll->setAlignment(Qt::AlignHCenter);
        scroll->setWidget(content);
        outerLayout->addWidget(scroll);
        auto* layout = new QVBoxLayout(content);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setContentsMargins(32, 24, 32, 24);
        layout->setSpacing(16);
        layout->setAlignment(Qt::AlignTop);
        auto* title =
            new QLabel(QCoreApplication::translate("MemberAccount", "Welcome to InuShot"), dialog);
        QFont font = title->font();
        font.setPixelSize(26);
        font.setBold(true);
        title->setFont(font);
        title->setWordWrap(true);
        layout->addWidget(title);
        auto* description =
            new QLabel(QCoreApplication::translate(
                           "MemberAccount", "Sign in with your Yuntu account to use InuShot."),
                       dialog);
        description->setWordWrap(true);
        layout->addWidget(description);

        if (m_session.signedIn()) {
            layout->addWidget(new QLabel(m_session.telephone(), dialog));
            auto* state = new QLabel(dialog);
            const auto update = [this, state] {
                state->setText(
                    m_session.busy()
                        ? QCoreApplication::translate("MemberAccount", "Checking subscription…")
                    : !m_session.subscriptionKnown()
                        ? QCoreApplication::translate(
                              "MemberAccount", "Subscription status is temporarily unavailable.")
                    : m_session.subscribed()
                        ? QCoreApplication::translate("MemberAccount", "Subscription active")
                        : QCoreApplication::translate("MemberAccount", "Free account"));
            };
            connect(&m_session, &MemberSession::changed, dialog, update);
            update();
            layout->addWidget(state);
            auto* logout =
                new QPushButton(QCoreApplication::translate(
                                    "MemberAccount", "Sign out of Yuntu apps on this device"),
                                dialog);
            logout->setAutoDefault(false);
            layout->addWidget(logout);
            connect(logout, &QPushButton::clicked, dialog, [this, dialog] {
                m_session.logout();
                m_dialog = nullptr;
                dialog->close();
            });
        } else {
            auto* telephone = new QLineEdit(dialog);
            telephone->setAccessibleName(
                QCoreApplication::translate("MemberAccount", "Phone number"));
            telephone->setInputMethodHints(Qt::ImhDialableCharactersOnly);
            auto* password = new QLineEdit(dialog);
            password->setAccessibleName(QCoreApplication::translate("MemberAccount", "Password"));
            password->setEchoMode(QLineEdit::Password);
            telephone->setObjectName(QStringLiteral("memberTelephone"));
            password->setObjectName(QStringLiteral("memberPassword"));
            auto* form = new QFormLayout;
            form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            form->setVerticalSpacing(12);
            form->addRow(QCoreApplication::translate("MemberAccount", "Phone number"), telephone);
            form->addRow(QCoreApplication::translate("MemberAccount", "Password"), password);
            layout->addLayout(form);
            auto* remember = new adqt::widgets::AdCheckbox(
                QCoreApplication::translate("MemberAccount", "Remember sign-in on this device"),
                dialog);
            remember->setComponentTokenResolver(
                [dialog](const adqt::widgets::AdCheckbox::ComponentTokenContext& context) {
                    const bool dark = dialog->palette().color(QPalette::Window).lightness() < 128;
                    adqt::widgets::AdCheckbox::ComponentTokens tokens;
                    tokens.colors.textColor = QColor(dark ? "#f5f5f7" : "#1d1d1f");
                    tokens.colors.indicatorBorderColor = QColor(dark ? "#9aa0ac" : "#6e7076");
                    tokens.colors.indicatorFillColor =
                        context.checked ? QColor("#006be0") : QColor(dark ? "#2b2f38" : "#ffffff");
                    tokens.colors.indicatorMarkColor = QColor(Qt::white);
                    tokens.colors.focusRingColor = QColor("#007aff");
                    return tokens;
                });
            layout->addWidget(remember);
            auto* error = new QLabel(dialog);
            error->setObjectName(QStringLiteral("memberError"));
            error->setWordWrap(true);
            error->setAccessibleName(
                QCoreApplication::translate("MemberAccount", "Sign-in status"));
            error->hide();
            layout->addWidget(error);
            auto* submit =
                new QPushButton(QCoreApplication::translate("MemberAccount", "Sign in"), dialog);
            submit->setObjectName(QStringLiteral("memberSubmit"));
            submit->setDefault(true);
            layout->addWidget(submit);
            const auto update = [this, telephone, password, remember, submit] {
                const bool busy = m_session.busy();
                telephone->setEnabled(!busy);
                password->setEnabled(!busy);
                remember->setEnabled(!busy);
                submit->setEnabled(!busy && !telephone->text().trimmed().isEmpty() &&
                                   !password->text().isEmpty());
                submit->setText(busy ? QCoreApplication::translate("MemberAccount", "Signing in…")
                                     : QCoreApplication::translate("MemberAccount", "Sign in"));
            };
            connect(telephone, &QLineEdit::textChanged, dialog, update);
            connect(password, &QLineEdit::textChanged, dialog, update);
            connect(&m_session, &MemberSession::changed, dialog, [this, dialog, update] {
                update();
                if (m_session.signedIn())
                    dialog->accept();
            });
            connect(&m_session, &MemberSession::failed, dialog, [error](const QString& message) {
                error->setText(message);
                error->show();
            });
            connect(submit, &QPushButton::clicked, dialog,
                    [this, telephone, password, remember, error] {
                        error->hide();
                        m_session.login(telephone->text().trimmed(), password->text(),
                                        remember->isChecked());
                    });
            update();
            telephone->setFocus();
        }
        auto* links = new QLabel(
            QCoreApplication::translate(
                "MemberAccount",
                "<a href=\"https://www.yuntudata.com.cn/register\">Create an account</a> · <a "
                "href=\"https://www.yuntudata.com.cn/privacy\">Privacy</a> · <a "
                "href=\"https://www.yuntudata.com.cn/terms\">Terms</a>"),
            dialog);
        links->setObjectName(QStringLiteral("memberLinks"));
        QPalette linkPalette = links->palette();
        const bool darkLinks = dialog->palette().color(QPalette::Window).lightness() < 128;
        linkPalette.setColor(QPalette::Link, QColor(darkLinks ? "#79baff" : "#005fcc"));
        linkPalette.setColor(QPalette::LinkVisited, linkPalette.color(QPalette::Link));
        links->setPalette(linkPalette);
        links->setText(links->text().replace(QStringLiteral("<a href="),
                                             QStringLiteral("<a style=\"color:%1\" href=")
                                                 .arg(linkPalette.color(QPalette::Link).name())));
        links->setWordWrap(true);
        links->setOpenExternalLinks(true);
        links->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(links);
        auto* buttons = new QDialogButtonBox(dialog);
        auto* skip = buttons->addButton(QCoreApplication::translate("MemberAccount", "Not now"),
                                        QDialogButtonBox::RejectRole);
        skip->setAutoDefault(false);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        buttons->setContentsMargins(32, 0, 32, 0);
        if (m_gate) {
            delete buttons;
        } else {
            outerLayout->addWidget(buttons);
        }
        dialog->show();
#ifdef Q_OS_MACOS
        applyAccountWindowMaterial(dialog);
#endif
    }
    std::function<void()> m_onLocked;
    bool m_gate = false;
    bool m_unlocked = false;
    std::function<void()> m_style;
    MemberSession m_session;
    QPointer<QMainWindow> m_window;
    QPointer<QPushButton> m_entry;
    QPointer<QDialog> m_dialog;
};
AccountUi* accountUi = nullptr;
} // namespace

void installAccountUi(QApplication& application, std::function<void()> onLocked) {
    accountUi = new AccountUi(application, std::move(onLocked));
}
bool requireMemberSignIn() {
    return accountUi && accountUi->requireSignIn();
}
} // namespace snow_shot::account
