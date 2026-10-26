#ifndef SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H
#define SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H

// macOS control-layer shell. Widgets keep their own paint and theme code.
// installLiquidGlassShell() turns the shell on once. syncLiquidGlassMainWindow()
// runs after the main window has applied its own theme or skin.

#include "snow_shot/presentation/components/contentcardwidget.h"
#include "snow_shot/presentation/components/maincontentheaderwidget.h"
#include "snow_shot/presentation/components/sidebarwidget.h"
#include "snow_shot/presentation/components/titlebarwidget.h"
#include "snow_shot/presentation/mainwindowskinwidget.h"
#include "snow_shot/presentation/styles/thememanager.h"

#include "widgets/navigation_menu.h"

#include <QColor>
#include <QFrame>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QScrollArea>
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
    painter.setBrush(QColor(22, 24, 28, 184));
    painter.drawRoundedRect(panelRect, glassRadius, glassRadius);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1.0));
    painter.drawRoundedRect(panelRect, glassRadius, glassRadius);
    return true;
}

inline void openWidgetToGlass(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->setAutoFillBackground(false);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->update();
}

inline void applyChromePalette(QWidget* widget, bool dark) {
    if (widget == nullptr) {
        return;
    }
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, liquidGlassChromeColor(dark));
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
    widget->update();
}

inline void clearOverlayFill(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, Qt::transparent);
    widget->setPalette(palette);
    widget->setAutoFillBackground(false);
}

inline void applySidebarGlass(SidebarWidget* sidebar, bool dark) {
    if (sidebar == nullptr) {
        return;
    }
    auto* menu = sidebar->findChild<adqt::widgets::AdNavigationMenu*>();
    QColor solid;
    if (menu != nullptr) {
        auto tokens = menu->componentTokens();
        tokens.colors.shared.itemBackground.reset();
        tokens.colors.shared.subMenuItemBackground.reset();
        menu->setComponentTokens(tokens);
        solid = adqt::widgets::AdNavigationMenu::resolveColorTokens(menu).itemBackground;
        tokens = menu->componentTokens();
        tokens.colors.shared.itemBackground = QColor(Qt::transparent);
        tokens.colors.shared.subMenuItemBackground = QColor(Qt::transparent);
        menu->setComponentTokens(tokens);
        clearOverlayFill(menu);
    }
    for (auto* scroll : sidebar->findChildren<QScrollArea*>()) {
        clearOverlayFill(scroll);
        clearOverlayFill(scroll->viewport());
    }
    clearOverlayFill(sidebar->findChild<QFrame*>(QStringLiteral("sidebarCollapseTrigger")));
    QPalette palette = sidebar->palette();
    palette.setColor(QPalette::Window, liquidGlassChromeColor(dark));
    if (solid.isValid() && solid.alpha() == 255) {
        palette.setColor(QPalette::Base, solid);
    }
    sidebar->setPalette(palette);
    sidebar->setAutoFillBackground(true);
    sidebar->update();
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
    if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
        window->setAttribute(Qt::WA_TranslucentBackground);
    }
    snow_shot::platform::macos::setMainWindowGlassBackdropEnabled(window, !skinActive, dark);

    if (auto* mainWindow = qobject_cast<QMainWindow*>(window); mainWindow != nullptr) {
        openWidgetToGlass(mainWindow->centralWidget());
    }
    auto* sidebar = window->findChild<SidebarWidget*>();
    if (sidebar != nullptr) {
        openWidgetToGlass(sidebar->parentWidget());
    }
    if (auto* card = window->findChild<ContentCardWidget*>(); card != nullptr) {
        openWidgetToGlass(card->parentWidget());
        if (card->parentWidget() != nullptr) {
            openWidgetToGlass(card->parentWidget()->parentWidget());
        }
    }
    if (skinActive) {
        return;
    }
    applyChromePalette(window->findChild<TitleBarWidget*>(), dark);
    applyChromePalette(window->findChild<MainContentHeaderWidget*>(), dark);
    applySidebarGlass(sidebar, dark);
#endif
}

} // namespace snow_shot::presentation::styles

#endif
