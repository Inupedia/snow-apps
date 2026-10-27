#ifndef SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H
#define SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H

// macOS control-layer shell. Upstream widgets keep their paint and layout.
// The only upstream edits are installLiquidGlassShell() in main.cpp and the
// two paintEvent early-returns for toolbar panels that have no Q_OBJECT, so a
// later upstream merge does not have to touch main-window, history, or platform code.

#include "snow_shot/presentation/styles/thememanager.h"
#include "widgets/context_menu.h"
#include "widgets/date_picker.h"
#include "widgets/message.h"
#include "widgets/modal.h"
#include "widgets/navigation_menu.h"
#include "widgets/notification.h"
#include "widgets/popover.h"
#include "widgets/select.h"
#include "widgets/tooltip.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPointer>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace snow_shot::presentation::styles {

inline QColor liquidGlassPopupColor(bool dark) {
    // Opaque control-layer fill. A translucent popup lets the page text show
    // through the settings flyout, and a translucent toolbar lets the desktop
    // show through its buttons.
    return dark ? QColor(35, 39, 48) : QColor(250, 251, 253);
}

inline QColor liquidGlassChromeColor(bool dark) {
    return liquidGlassPopupColor(dark);
}

inline QColor liquidGlassHighlightColor(bool dark) {
    return dark ? QColor(255, 255, 255, 31) : QColor(255, 255, 255, 191);
}

inline bool& liquidGlassShellEnabled() {
    static bool enabled = false;
    return enabled;
}

inline bool themeIsDark() {
    return ThemeManager::instance().themeColorScheme().appearance == ThemeAppearance::Dark;
}

inline bool paintLiquidGlassToolbar(QWidget* widget, qreal radius) {
    if (!liquidGlassShellEnabled() || widget == nullptr) {
        return false;
    }
    const bool dark = themeIsDark();
    const qreal glassRadius = std::max(0.0, radius * 2.0);
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(widget->rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(Qt::NoPen);
    painter.setBrush(liquidGlassChromeColor(dark));
    painter.drawRoundedRect(QRectF(widget->rect()), glassRadius, glassRadius);
    QPainterPath edge;
    const qreal edgeRadius = std::max(0.0, glassRadius - 0.5);
    edge.addRoundedRect(QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5), edgeRadius,
                        edgeRadius);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(liquidGlassHighlightColor(dark), 1.0));
    painter.drawPath(edge);
    return true;
}

inline bool paintLiquidGlassHud(QWidget* widget, qreal radius) {
    if (!liquidGlassShellEnabled() || widget == nullptr) {
        return false;
    }
    const qreal glassRadius = std::max(0.0, radius * 2.0);
    const QRectF panelRect = QRectF(widget->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainter painter(widget);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(liquidGlassPopupColor(true));
    painter.drawRoundedRect(panelRect, glassRadius, glassRadius);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));
    painter.drawRoundedRect(panelRect, glassRadius, glassRadius);
    return true;
}

inline void applyOpaqueSurface(QWidget* widget, const QColor& color) {
    if (widget == nullptr || !color.isValid() || color.alpha() == 0) {
        return;
    }
    QColor fill = color;
    fill.setAlpha(255);
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, fill);
    palette.setColor(QPalette::Base, fill);
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
    widget->update();
}

inline QWidget* findByClassName(QWidget* root, const char* className) {
    if (root == nullptr) {
        return nullptr;
    }
    const QList<QWidget*> widgets = root->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (widget->inherits(className)) {
            return widget;
        }
    }
    return nullptr;
}

inline void applySidebarGlass(QWidget* sidebar) {
    if (sidebar == nullptr) {
        return;
    }
    // The menu paints its own surface. The empty area under the last item has
    // to use that same color. Row fills stay on the menu's own tokens.
    const QColor menuSurface = sidebar->palette().color(QPalette::Base);
    applyOpaqueSurface(sidebar, menuSurface);
    for (QObject* child : sidebar->children()) {
        auto* scroll = qobject_cast<QAbstractScrollArea*>(child);
        if (scroll == nullptr) {
            continue;
        }
        applyOpaqueSurface(scroll, menuSurface);
        applyOpaqueSurface(scroll->viewport(), menuSurface);
    }
}

inline void applyNavigationPopupGlass(QWidget* window, bool skinActive) {
    if (window == nullptr) {
        return;
    }
    const QColor lightPopup = liquidGlassPopupColor(false);
    const QColor darkPopup = liquidGlassPopupColor(true);
    const QList<QWidget*> widgets = window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        // moc reports the namespaced class name, so inherits("AdNavigationMenu") misses it.
        auto* menu = qobject_cast<adqt::widgets::AdNavigationMenu*>(widget);
        if (menu == nullptr) {
            continue;
        }
        // Scheme tokens win over the shared slot. The sidebar skin path only
        // resets the shared slot, so the inline group has to live on the scheme.
        QColor submenu = menu->resolvedColorTokens().itemBackground;
        submenu.setAlpha(255);
        const bool paintSubmenu = !skinActive && submenu.isValid();
        auto tokens = menu->componentTokens();
        const bool popupReady = tokens.colors.light.popupBackground == lightPopup &&
                                tokens.colors.dark.popupBackground == darkPopup;
        const bool submenuReady =
            !paintSubmenu || (tokens.colors.light.subMenuItemBackground == submenu &&
                              tokens.colors.dark.subMenuItemBackground == darkPopup);
        if (popupReady && submenuReady) {
            continue;
        }
        tokens.colors.light.popupBackground = lightPopup;
        tokens.colors.dark.popupBackground = darkPopup;
        if (paintSubmenu) {
            tokens.colors.light.subMenuItemBackground = submenu;
            tokens.colors.dark.subMenuItemBackground = darkPopup;
        } else if (skinActive) {
            tokens.colors.light.subMenuItemBackground.reset();
            tokens.colors.dark.subMenuItemBackground.reset();
        }
        menu->setComponentTokens(tokens);
    }
}

inline void applyModalGlass() {
    const QColor fill = liquidGlassPopupColor(themeIsDark());
    QList<adqt::widgets::AdModal*> modals;
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        modals.append(top->findChildren<adqt::widgets::AdModal*>());
    }
    for (adqt::widgets::AdModal* modal : modals) {
        if (modal == nullptr) {
            continue;
        }
        auto tokens = modal->componentTokens();
        if (tokens.contentBg == fill && tokens.headerBg == fill && tokens.bodyBg == fill &&
            tokens.footerBg == fill) {
            continue;
        }
        // One opaque control surface. The dialog body keeps its own fields,
        // and the footer stays in the panel layout.
        tokens.contentBg = fill;
        tokens.headerBg = fill;
        tokens.bodyBg = fill;
        tokens.footerBg = fill;
        modal->setComponentTokens(tokens);
    }
}

template <typename T>
void collectControlObjects(QList<T*>& out) {
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        out.append(top->findChildren<T*>());
    }
}

inline void applyDetachedControlGlass() {
    const QColor fill = liquidGlassPopupColor(themeIsDark());

    QList<adqt::widgets::AdTooltip*> tooltips;
    collectControlObjects(tooltips);
    for (adqt::widgets::AdTooltip* tooltip : tooltips) {
        auto tokens = tooltip->componentTokens();
        if (tokens.popupBg == fill) {
            continue;
        }
        tokens.popupBg = fill;
        tooltip->setComponentTokens(tokens);
    }

    QList<adqt::widgets::AdPopover*> popovers;
    collectControlObjects(popovers);
    for (adqt::widgets::AdPopover* popover : popovers) {
        if (popover->backgroundColor() == fill) {
            continue;
        }
        popover->setBackgroundColor(fill);
    }

    QList<adqt::widgets::AdDatePicker*> datePickers;
    collectControlObjects(datePickers);
    for (adqt::widgets::AdDatePicker* picker : datePickers) {
        auto tokens = picker->componentTokens();
        if (tokens.panelBackground == fill) {
            continue;
        }
        tokens.panelBackground = fill;
        picker->setComponentTokens(tokens);
    }

    QList<adqt::widgets::AdDateRangePicker*> dateRanges;
    collectControlObjects(dateRanges);
    for (adqt::widgets::AdDateRangePicker* picker : dateRanges) {
        auto tokens = picker->componentTokens();
        if (tokens.panelBackground == fill) {
            continue;
        }
        tokens.panelBackground = fill;
        picker->setComponentTokens(tokens);
    }

    QList<adqt::widgets::AdMessage*> messages;
    collectControlObjects(messages);
    for (adqt::widgets::AdMessage* message : messages) {
        auto tokens = message->componentTokens();
        if (tokens.contentBg == fill) {
            continue;
        }
        tokens.contentBg = fill;
        message->setComponentTokens(tokens);
    }

    QList<adqt::widgets::AdNotification*> notifications;
    collectControlObjects(notifications);
    for (adqt::widgets::AdNotification* notification : notifications) {
        auto tokens = notification->componentTokens();
        if (tokens.backgroundColor == fill) {
            continue;
        }
        tokens.backgroundColor = fill;
        notification->setComponentTokens(tokens);
    }

    // Widget menus only. macOS keeps the native menu when that mode is on, and
    // the token does not replace it.
    QList<adqt::widgets::AdContextMenu*> menus;
    collectControlObjects(menus);
    for (adqt::widgets::AdContextMenu* menu : menus) {
        auto tokens = menu->componentTokens();
        if (tokens.background.has_value() && tokens.background.value() == fill) {
            continue;
        }
        tokens.background = fill;
        menu->setComponentTokens(tokens);
    }
}

inline void applyWindowGroupSwitcherGlass() {
    const QColor fill = liquidGlassPopupColor(themeIsDark());
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        if (top->objectName() != QStringLiteral("windowGroupSwitcherPopup")) {
            continue;
        }
        QPalette palette = top->palette();
        if (palette.color(QPalette::Window) == fill) {
            continue;
        }
        palette.setColor(QPalette::Window, fill);
        top->setPalette(palette);
        const QList<QAbstractItemView*> views = top->findChildren<QAbstractItemView*>();
        for (QAbstractItemView* view : views) {
            QPalette viewPalette = view->palette();
            viewPalette.setColor(QPalette::Window, fill);
            view->setPalette(viewPalette);
        }
    }
}

inline void applySelectPopupGlass(QWidget* popup) {
    if (popup == nullptr) {
        return;
    }
    const QColor fill = liquidGlassPopupColor(themeIsDark());
    QWidget* window = popup->window();
    if (window == nullptr) {
        return;
    }
    const QList<adqt::widgets::AdSelect*> selects = window->findChildren<adqt::widgets::AdSelect*>();
    for (adqt::widgets::AdSelect* select : selects) {
        auto tokens = select->componentTokens();
        if (tokens.colors.popupBackground == fill) {
            continue;
        }
        tokens.colors.popupBackground = fill;
        select->setComponentTokens(tokens);
    }
}

inline bool activeSkinCoversChrome(QWidget* window) {
    if (window->findChild<QWidget*>(QStringLiteral("mainWindowSkinBackground")) == nullptr) {
        return false;
    }
    // The skin widget exists while a skin is selected. Content autofill turns
    // off only after the frame is showing, which is the same moment the skin
    // covers the chrome.
    auto* mainWindow = qobject_cast<QMainWindow*>(window);
    QWidget* central = mainWindow != nullptr ? mainWindow->centralWidget() : nullptr;
    return central != nullptr && !central->autoFillBackground();
}

inline void syncLiquidGlassMainWindow(QWidget* window) {
    if (!liquidGlassShellEnabled() || window == nullptr) {
        return;
    }
    // Keep the window opaque. A translucent top-level surface does not clear the
    // previous frame, so collapsing the sidebar leaves the old tabs and labels
    // painted on top of the new layout.
    if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
        window->setAttribute(Qt::WA_TranslucentBackground, false);
    }
    const bool skinActive = activeSkinCoversChrome(window);
    auto* sidebar = findByClassName(window, "SidebarWidget");
    applyNavigationPopupGlass(window, skinActive);
    if (skinActive) {
        return;
    }
    applySidebarGlass(sidebar);
    const auto& map = ThemeManager::instance().themeColorScheme().map;
    const QColor sidebarFill = sidebar != nullptr ? sidebar->palette().color(QPalette::Window)
                                                  : map.colorBgLayout;
    // Title follows the navigation surface. The content header follows the
    // card, so the empty area under a short page is not a third color.
    applyOpaqueSurface(findByClassName(window, "TitleBarWidget"), sidebarFill);
    applyOpaqueSurface(window->findChild<QWidget*>(QStringLiteral("mainContentHeader")),
                       map.colorBgContainer);
}

inline void syncOpenMainWindows() {
    if (!liquidGlassShellEnabled()) {
        return;
    }
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget* window : windows) {
        if (window->inherits("QMainWindow") && findByClassName(window, "TitleBarWidget") != nullptr) {
            syncLiquidGlassMainWindow(window);
        }
    }
    applyDetachedControlGlass();
    applyModalGlass();
    applyWindowGroupSwitcherGlass();
}

// Moves a pagination bar out of the scrolling page content by object name, so
// the history and pinned pages do not need a layout edit.
inline void dockManagementPagination(QWidget* page, const QString& paginationName,
                                     const QString& contentName) {
    if (page == nullptr || page->property("snowGlassPagerDocked").toBool()) {
        return;
    }
    auto* pagination = page->findChild<QWidget*>(paginationName);
    auto* content = page->findChild<QWidget*>(contentName);
    auto* contentLayout =
        content != nullptr ? qobject_cast<QVBoxLayout*>(content->layout()) : nullptr;
    if (pagination == nullptr || content == nullptr || contentLayout == nullptr ||
        !content->isAncestorOf(pagination)) {
        return;
    }
    QWidget* viewport = content->parentWidget();
    QWidget* scroll = viewport != nullptr ? viewport->parentWidget() : nullptr;
    QWidget* container = scroll != nullptr ? scroll->parentWidget() : nullptr;
    auto* containerLayout =
        container != nullptr ? qobject_cast<QVBoxLayout*>(container->layout()) : nullptr;
    if (containerLayout == nullptr) {
        return;
    }
    const QMargins margins = contentLayout->contentsMargins();
    contentLayout->removeWidget(pagination);
    contentLayout->setContentsMargins(margins.left(), margins.top(), margins.right(), 0);
    auto* footer = new QWidget(container);
    footer->setAutoFillBackground(false);
    footer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* footerLayout = new QVBoxLayout(footer);
    footerLayout->setContentsMargins(margins.left(), 14, margins.right(), margins.bottom());
    footerLayout->setSpacing(0);
    pagination->setParent(footer);
    footerLayout->addWidget(pagination);
    containerLayout->addWidget(footer, 0);
    page->setProperty("snowGlassPagerDocked", true);
}

class LiquidGlassShellHooks final : public QObject {
  public:
    explicit LiquidGlassShellHooks(QObject* parent) : QObject(parent) {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->installEventFilter(this);
        }
        QObject::connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
                         [](const ThemeColorScheme&) { syncOpenMainWindows(); },
                         Qt::QueuedConnection);
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (liquidGlassShellEnabled() && event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(watched)) {
                const QString name = widget->objectName();
                if (name == QStringLiteral("ad-modal-overlay")) {
                    applyModalGlass();
                } else if (name == QStringLiteral("adselect-popup")) {
                    applySelectPopupGlass(widget);
                } else if (name == QStringLiteral("adtooltip-popup") ||
                           name == QStringLiteral("adtooltip-surface") ||
                           name == QStringLiteral("adpopover-popup") ||
                           name == QStringLiteral("adpopover-surface") ||
                           name == QStringLiteral("addatepicker-popup") ||
                           name == QStringLiteral("addaterangepicker-popup") ||
                           name == QStringLiteral("ad-message-notice") ||
                           name == QStringLiteral("ad-notification-notice") ||
                           name == QStringLiteral("ad-context-menu")) {
                    applyDetachedControlGlass();
                } else if (name == QStringLiteral("windowGroupSwitcherPopup")) {
                    applyWindowGroupSwitcherGlass();
                } else if (name == QStringLiteral("screenshotHistoryPage")) {
                    dockManagementPagination(widget, QStringLiteral("screenshotHistoryPagination"),
                                             QStringLiteral("screenshotHistoryContent"));
                } else if (name == QStringLiteral("pinnedWindowManagementPage")) {
                    dockManagementPagination(widget, QStringLiteral("pinnedManagementPagination"),
                                             QStringLiteral("pageContent"));
                } else if (widget->isWindow() && widget->inherits("QMainWindow") &&
                           findByClassName(widget, "TitleBarWidget") != nullptr) {
                    syncLiquidGlassMainWindow(widget);
                    // Show can run before a later token reset. Apply again after
                    // the window finishes its own show handling.
                    QTimer::singleShot(0, widget, [window = QPointer<QWidget>(widget)] {
                        if (window != nullptr) {
                            syncLiquidGlassMainWindow(window.data());
                        }
                    });
                } else if (qobject_cast<adqt::widgets::AdNavigationMenu*>(widget) != nullptr ||
                           widget->inherits("SidebarWidget")) {
                    QWidget* window = widget->window();
                    if (window != nullptr && window->inherits("QMainWindow")) {
                        syncLiquidGlassMainWindow(window);
                    }
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

inline void installLiquidGlassShell() {
#ifndef Q_OS_MACOS
    liquidGlassShellEnabled() = false;
#else
    liquidGlassShellEnabled() = true;
    if (qApp == nullptr) {
        return;
    }
    // Not a child of qApp: a function-local static outlives QApplication and
    // would be deleted twice if the application owned it.
    static LiquidGlassShellHooks hooks(nullptr);
    (void)hooks;
#endif
}

} // namespace snow_shot::presentation::styles

#endif
