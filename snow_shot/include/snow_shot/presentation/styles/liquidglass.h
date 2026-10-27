#ifndef SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H
#define SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H

// macOS control-layer shell. Widgets keep their own paint and theme code.
// installLiquidGlassShell() turns the shell on once. syncLiquidGlassMainWindow()
// runs after the main window has applied its own theme or skin.

#include "snow_shot/presentation/components/maincontentheaderwidget.h"
#include "snow_shot/presentation/components/sidebarwidget.h"
#include "snow_shot/presentation/components/titlebarwidget.h"
#include "snow_shot/presentation/mainwindowskinwidget.h"
#include "snow_shot/presentation/styles/thememanager.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QWidget>

#include <algorithm>

#ifdef Q_OS_MACOS
#include "snow_shot/platform/macos/applicationactivation.h"
#endif

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

inline void installLiquidGlassShell() {
#ifdef Q_OS_MACOS
    liquidGlassShellEnabled() = true;
#else
    liquidGlassShellEnabled() = false;
#endif
}

inline bool themeIsDark() {
    return ThemeManager::instance().themeColorScheme().appearance == ThemeAppearance::Dark;
}

inline QColor liquidGlassSurfaceFill(const QColor& original) {
    if (!liquidGlassShellEnabled() || !original.isValid()) {
        return original;
    }
    return liquidGlassChromeColor(themeIsDark());
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

inline void applySidebarGlass(SidebarWidget* sidebar) {
    if (sidebar == nullptr) {
        return;
    }
    // The menu paints its own surface. The empty area under the last item has
    // to use that same color. A separate chrome tint reads as a hole, and
    // menu popups reuse the item background, so those tokens stay untouched.
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

inline void syncLiquidGlassMainWindow(QWidget* window) {
#ifndef Q_OS_MACOS
    Q_UNUSED(window);
#else
    if (window == nullptr) {
        return;
    }
    const bool dark = themeIsDark();
    const auto* skin = window->findChild<MainWindowSkinWidget*>();
    const bool skinActive = skin != nullptr && skin->skinActive();
    // Keep the window opaque. A translucent top-level surface does not clear the
    // previous frame, so collapsing the sidebar leaves the old tabs and labels
    // painted on top of the new layout.
    if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
        window->setAttribute(Qt::WA_TranslucentBackground, false);
    }
    snow_shot::platform::macos::setMainWindowGlassBackdropEnabled(window, false, dark);
    if (skinActive) {
        return;
    }
    auto* sidebar = window->findChild<SidebarWidget*>();
    applySidebarGlass(sidebar);
    const auto& map = ThemeManager::instance().themeColorScheme().map;
    const QColor sidebarFill = sidebar != nullptr ? sidebar->palette().color(QPalette::Window)
                                                  : map.colorBgLayout;
    // Title follows the navigation surface. The content header follows the
    // card, so the empty area under a short page is not a third color.
    applyOpaqueSurface(window->findChild<TitleBarWidget*>(), sidebarFill);
    applyOpaqueSurface(window->findChild<MainContentHeaderWidget*>(), map.colorBgContainer);
#endif
}

} // namespace snow_shot::presentation::styles

#endif
