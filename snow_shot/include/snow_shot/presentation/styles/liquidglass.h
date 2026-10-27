#ifndef SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H
#define SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H

// macOS control-layer shell. Upstream widgets keep their paint and layout.
// The only upstream edits are installLiquidGlassShell() in main.cpp and the
// two paintEvent early-returns for toolbar panels that have no Q_OBJECT, so a
// later upstream merge does not have to touch main-window, history, or platform code.

#include "snow_shot/presentation/styles/thememanager.h"

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
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace snow_shot::presentation::styles {

inline QColor liquidGlassChromeColor(bool dark) {
    return dark ? QColor(35, 39, 48, 209) : QColor(250, 251, 253, 199);
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
    // The desktop floating toolbar sits on top of other apps. A translucent fill
    // lets their text show through the buttons.
    if (widget->window()->objectName() == QStringLiteral("floatingToolbarWindow")) {
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
    painter.setBrush(QColor(22, 24, 28, 184));
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
    // to use that same color. Menu popups reuse the item background, so those
    // tokens stay untouched.
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
    if (activeSkinCoversChrome(window)) {
        return;
    }
    auto* sidebar = findByClassName(window, "SidebarWidget");
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
                if (name == QStringLiteral("screenshotHistoryPage")) {
                    dockManagementPagination(widget, QStringLiteral("screenshotHistoryPagination"),
                                             QStringLiteral("screenshotHistoryContent"));
                } else if (name == QStringLiteral("pinnedWindowManagementPage")) {
                    dockManagementPagination(widget, QStringLiteral("pinnedManagementPagination"),
                                             QStringLiteral("pageContent"));
                } else if (widget->isWindow() && widget->inherits("QMainWindow") &&
                           findByClassName(widget, "TitleBarWidget") != nullptr) {
                    syncLiquidGlassMainWindow(widget);
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
