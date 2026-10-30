#include "snow_shot/account/accountui.h"
#include "snow_shot/account/membersession.h"
#include "snow_shot/presentation/components/settingscustomwidget.h"
#include "widgets/button.h"
#include "widgets/checkbox.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QResource>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QScrollArea>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

static void initializeMemberAssets() {
    Q_INIT_RESOURCE(snow_shot_member_assets);
}

namespace snow_shot::account {
#ifdef Q_OS_MACOS
void applyAccountWindowMaterial(QWidget* window);
#endif
namespace {
class LoginArtwork final : public QWidget {
  public:
    explicit LoginArtwork(QWidget* parent)
        : QWidget(parent), m_light(QStringLiteral(":/snow-shot/account/login-alpine-light.png")),
          m_dark(QStringLiteral(":/snow-shot/account/login-alpine-dark.png")) {
        setObjectName(QStringLiteral("memberLoginArtwork"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
        const QPixmap& image = dark ? m_dark : m_light;
        setProperty("artworkAppearance", dark ? QStringLiteral("dark") : QStringLiteral("light"));
        painter.fillRect(rect(), dark ? QColor("#171b22") : QColor("#f4f8fc"));
        const QSize imageSize = image.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        const QRect target(
            QPoint((width() - imageSize.width()) / 2, (height() - imageSize.height()) / 2),
            imageSize);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(target, image);
    }
    void changeEvent(QEvent* event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::PaletteChange)
            update();
    }

  private:
    QPixmap m_light;
    QPixmap m_dark;
};

class AccountUi final : public QObject {
  public:
    explicit AccountUi(QApplication& application, std::function<void()> onLocked)
        : QObject(&application), m_onLocked(std::move(onLocked)), m_session(this) {
        initializeMemberAssets();
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
        });
    }

    MemberSession& session() {
        return m_session;
    }

    bool requireSignIn() {
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
        m_unlocked = m_session.signedIn();
        return m_unlocked;
    }

  private:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == m_dialog && event->type() == QEvent::Resize && m_artwork)
            m_artwork->setVisible(m_dialog->width() >= 780);
        if ((watched == m_dialog || watched == qApp) &&
            event->type() == QEvent::ApplicationPaletteChange && m_dialog && m_style) {
            m_style();
            if (m_artwork)
                m_artwork->update();
        }
        if (event->type() == QEvent::Show && !m_window && watched->inherits("MainWindow")) {
            auto* window = qobject_cast<QMainWindow*>(watched);
            if (!window || !window->centralWidget())
                return false;
            m_window = window;
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
        dialog->resize(1040, 680);
        const auto style = [dialog] {
            const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
            const auto surface = dark ? QStringLiteral("#171b22") : QStringLiteral("#fafbfd");
            const auto ink = dark ? QStringLiteral("#f5f5f7") : QStringLiteral("#1d1d1f");
            const auto field = dark ? QStringLiteral("#1e232c") : QStringLiteral("#ffffff");
            const auto border = dark ? QStringLiteral("#697586") : QStringLiteral("#8996a8");
            dialog->setStyleSheet(
                QStringLiteral(
                    "QDialog#memberAccountDialog {background:%1; color:%2;}"
                    "QWidget#memberFormSurface,QScrollArea#memberFormScroll {background:%1;}"
                    "QLabel,QCheckBox {color:%2; background:transparent; font-size:14px;}"
                    "QLabel#memberBrand {font-size:22px; font-weight:600;}"
                    "QLabel#memberHeading {font-size:30px; font-weight:600;}"
                    "QLabel#memberDescription {color:%7; font-size:14px;}"
                    "QLineEdit {background:%3; color:%2; border:1px solid %4; border-radius:10px; "
                    "padding:10px;}"
                    "QLineEdit:focus {border:2px solid #007aff; padding:9px;}"
                    "QPushButton {color:%2; background:%3; border:1px solid %4; "
                    "border-radius:10px; padding:10px 16px;}"
                    "QPushButton:focus {border:2px solid #007aff; padding:9px 15px;}"
                    "QPushButton#memberSubmit {font-size:15px; font-weight:600;}"
                    "QPushButton:default {color:%8; background:%9; border-color:%9;}"
                    "QPushButton:default:hover {background:%10;}"
                    "QPushButton:disabled {color:%4; background:%1;}"
                    "QLabel#memberError {color:%5; border-radius:8px; padding:10px; "
                    "background:%6;}")
                    .arg(surface, ink, field, border,
                         dark ? QStringLiteral("#ff929b") : QStringLiteral("#c40012"),
                         dark ? QStringLiteral("#46232a") : QStringLiteral("#fff0f1"),
                         dark ? QStringLiteral("#abb5c4") : QStringLiteral("#596579"),
                         dark ? QStringLiteral("#152238") : QStringLiteral("#ffffff"),
                         dark ? QStringLiteral("#accdff") : QStringLiteral("#006be0"),
                         dark ? QStringLiteral("#c4dcff") : QStringLiteral("#005bc4")));
            if (auto* links = dialog->findChild<QLabel*>(QStringLiteral("memberLinks"))) {
                links->setText(links->property("sourceHtml")
                                   .toString()
                                   .replace(QStringLiteral("<a href="),
                                            QStringLiteral("<a style=\"color:%1\" href=")
                                                .arg(dark ? QStringLiteral("#accdff")
                                                          : QStringLiteral("#005fcc"))));
            }
            dialog->update();
        };
        style();
        m_style = style;
        auto* outerLayout = new QHBoxLayout(dialog);
        outerLayout->setContentsMargins(0, 0, 0, 0);
        outerLayout->setSpacing(0);
        m_artwork = new LoginArtwork(dialog);
        outerLayout->addWidget(m_artwork, 55);
        m_artwork->setVisible(dialog->width() >= 780);
        auto* scroll = new QScrollArea(dialog);
        scroll->setObjectName(QStringLiteral("memberFormScroll"));
        scroll->setMinimumWidth(360);
        scroll->setMaximumWidth(490);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget(scroll);
        content->setObjectName(QStringLiteral("memberFormSurface"));
        content->setMaximumWidth(490);
        scroll->setAlignment(Qt::AlignHCenter);
        scroll->setWidget(content);
        outerLayout->addWidget(scroll, 45);
        auto* layout = new QVBoxLayout(content);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setContentsMargins(44, 40, 44, 32);
        layout->setSpacing(16);
        layout->addStretch(1);
        auto* brand = new QLabel(QCoreApplication::translate("MemberAccount", "InuShot"), content);
        brand->setObjectName(QStringLiteral("memberBrand"));
        layout->addWidget(brand);
        layout->addSpacing(16);
        auto* title =
            new QLabel(QCoreApplication::translate("MemberAccount", "Welcome to InuShot"), dialog);
        title->setObjectName(QStringLiteral("memberHeading"));
        QFont font = title->font();
        font.setPixelSize(30);
        font.setBold(true);
        title->setFont(font);
        title->setWordWrap(true);
        layout->addWidget(title);
        auto* description =
            new QLabel(QCoreApplication::translate(
                           "MemberAccount", "Sign in with your Yuntu account to use InuShot."),
                       dialog);
        description->setObjectName(QStringLiteral("memberDescription"));
        description->setWordWrap(true);
        layout->addWidget(description);
        layout->addSpacing(12);

        auto* telephone = new QLineEdit(dialog);
        telephone->setAccessibleName(QCoreApplication::translate("MemberAccount", "Phone number"));
        telephone->setInputMethodHints(Qt::ImhDialableCharactersOnly);
        auto* password = new QLineEdit(dialog);
        password->setAccessibleName(QCoreApplication::translate("MemberAccount", "Password"));
        password->setEchoMode(QLineEdit::Password);
        telephone->setMinimumHeight(44);
        password->setMinimumHeight(44);
        telephone->setObjectName(QStringLiteral("memberTelephone"));
        password->setObjectName(QStringLiteral("memberPassword"));
        auto* form = new QFormLayout;
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setVerticalSpacing(16);
        form->addRow(QCoreApplication::translate("MemberAccount", "Phone number"), telephone);
        form->addRow(QCoreApplication::translate("MemberAccount", "Password"), password);
        layout->addLayout(form);
        auto* remember = new adqt::widgets::AdCheckbox(
            QCoreApplication::translate("MemberAccount", "Remember sign-in on this device"),
            dialog);
        remember->setComponentTokenResolver(
            [](const adqt::widgets::AdCheckbox::ComponentTokenContext& context) {
                const bool dark = QApplication::palette().color(QPalette::Window).lightness() < 128;
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
        error->setAccessibleName(QCoreApplication::translate("MemberAccount", "Sign-in status"));
        error->hide();
        layout->addWidget(error);
        auto* submit =
            new QPushButton(QCoreApplication::translate("MemberAccount", "Sign in"), dialog);
        submit->setObjectName(QStringLiteral("memberSubmit"));
        submit->setDefault(true);
        submit->setMinimumHeight(44);
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

        auto* links = new QLabel(
            QCoreApplication::translate(
                "MemberAccount",
                "<a href=\"https://www.yuntudata.com.cn/register\">Create an account</a> · <a "
                "href=\"https://www.yuntudata.com.cn/privacy\">Privacy</a> · <a "
                "href=\"https://www.yuntudata.com.cn/terms\">Terms</a>"),
            dialog);
        links->setObjectName(QStringLiteral("memberLinks"));
        links->setProperty("sourceHtml", links->text());
        links->setWordWrap(true);
        links->setOpenExternalLinks(true);
        links->setTextInteractionFlags(Qt::TextBrowserInteraction);
        layout->addWidget(links);
        layout->addStretch(1);
        style();
        dialog->show();
#ifdef Q_OS_MACOS
        applyAccountWindowMaterial(dialog);
#endif
    }
    std::function<void()> m_onLocked;
    bool m_unlocked = false;
    std::function<void()> m_style;
    MemberSession m_session;
    QPointer<QMainWindow> m_window;
    QPointer<QDialog> m_dialog;
    QPointer<QWidget> m_artwork;
};
class MemberSettingsWidget final : public SettingsCustomWidget {
  public:
    MemberSettingsWidget(MemberSession* session, QWidget* parent)
        : SettingsCustomWidget(parent), m_session(session) {
        setObjectName(QStringLiteral("memberAccountSettings"));
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 8, 0, 8);
        m_telephone = new QLabel(this);
        m_telephone->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(m_telephone, 1);
        m_logout = new adqt::widgets::AdButton(this);
        m_logout->setObjectName(QStringLiteral("memberSignOut"));
        layout->addWidget(m_logout);
        if (m_session) {
            connect(m_session, &MemberSession::changed, this, [this] { retranslateUi(); });
            connect(m_logout, &adqt::widgets::AdButton::clicked, this,
                    [this] { m_session->logout(); });
        }
        retranslateUi();
    }
    void retranslateUi() override {
        m_telephone->setText(m_session && m_session->signedIn()
                                 ? m_session->telephone()
                                 : QCoreApplication::translate("MemberAccount", "Sign in"));
        m_logout->setText(QCoreApplication::translate("MemberAccount", "Sign out"));
        m_logout->setAccessibleDescription(
            QCoreApplication::translate("MemberAccount", "Sign out of Yuntu apps on this device"));
        m_logout->setEnabled(m_session && m_session->signedIn());
    }
    void applyTheme(const snow_shot::presentation::styles::ThemeColorScheme& scheme) override {
        auto palette = m_telephone->palette();
        palette.setColor(QPalette::WindowText, scheme.map.colorText);
        m_telephone->setPalette(palette);
        auto font = m_telephone->font();
        font.setPixelSize(scheme.metricAlias.fontSize);
        font.setWeight(QFont::Normal);
        m_telephone->setFont(font);
    }

  private:
    QPointer<MemberSession> m_session;
    QLabel* m_telephone;
    adqt::widgets::AdButton* m_logout;
};
AccountUi* accountUi = nullptr;

} // namespace

void installAccountUi(QApplication& application, std::function<void()> onLocked) {
    accountUi = new AccountUi(application, std::move(onLocked));
}
SettingsCustomWidget* createMemberAccountSettingsWidget(QWidget* parent) {
    return new MemberSettingsWidget(accountUi ? &accountUi->session() : nullptr, parent);
}
bool requireMemberSignIn() {
    return accountUi && accountUi->requireSignIn();
}
} // namespace snow_shot::account
