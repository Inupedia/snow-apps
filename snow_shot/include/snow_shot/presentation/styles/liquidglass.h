#ifndef SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H
#define SNOW_SHOT_PRESENTATION_STYLES_LIQUIDGLASS_H

// macOS control-layer shell. Upstream widgets keep their paint and layout.
// The only upstream edits are installLiquidGlassShell() in main.cpp and the
// two paintEvent early-returns for toolbar panels that have no Q_OBJECT, so a
// later upstream merge does not have to touch main-window, history, or platform code.

#include "snow_shot/app/supportedpinfiles.h"
#include "snow_shot/storage/settingsadapters.h"
#include "snow_shot/presentation/components/applicationsearchwidget.h"
#include "snow_shot/presentation/components/icons/snowshoticons.h"
#include "snow_shot/presentation/components/icons/third_party_icons.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "theme/theme_manager.h"
#include "widgets/context_menu.h"
#include "widgets/date_picker.h"
#include "widgets/message.h"
#include "widgets/modal.h"
#include "widgets/navigation_menu.h"
#include "widgets/notification.h"
#include "widgets/popover.h"
#include "widgets/popconfirm.h"
#include "widgets/select.h"
#include "widgets/tooltip.h"
#include "widgets/button.h"
#include "widgets/detail/pointer_region.h"
#include "icon_renderer.h"

#include "widgets/control_scale.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
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
#include <QScreen>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <optional>

namespace snow_shot::presentation::styles {

inline QColor liquidGlassPopupColor(bool dark) {
    // Opaque control-layer fill. A translucent popup lets the page text show
    // through the settings flyout, and a translucent toolbar lets the desktop
    // show through its buttons.
    return dark ? QColor(32, 55, 75) : QColor(246, 250, 255);
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

inline bool canvasToolbarUsesGlassPlate(const QString& name) {
    return name == QStringLiteral("imageSplitCanvasToolbar") ||
           name == QStringLiteral("imageEditingCanvasToolbar") ||
           name == QStringLiteral("collageImageToolbar") ||
           name == QStringLiteral("collagePdfCanvasToolbar") ||
           name == QStringLiteral("collagePdfImageToolbar") ||
           name == QStringLiteral("screenRecordingWebcamOrientationToolbar") ||
           name == QStringLiteral("permissionGuide");
}

inline void paintOpaqueControlPlate(QPainter& painter, const QRectF& rect, qreal radius,
                                    const QColor& border, const QColor& fill = QColor()) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(fill.isValid() ? fill : liquidGlassPopupColor(themeIsDark()));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

// Matches screenshot_action_toolbar::PanelRadius. The collapsed recording tab
// has no Q_OBJECT, and its screen edge is the only side that stays square.
inline constexpr int kRecordingEdgeRadius = 8;

inline bool recordingEdgeIsOnLeft(const QWidget* widget) {
    const QScreen* screen = widget != nullptr ? widget->screen() : nullptr;
    if (widget == nullptr || screen == nullptr) {
        return false;
    }
    return widget->mapToGlobal(QPoint(0, 0)).x() <= screen->geometry().left() + 1;
}

inline void paintRecordingEdgeControl(QPainter& painter, const QRectF& rect, bool onLeft, bool down,
                                     bool hover, const adqt::theme::ThemeMapToken& theme) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const qreal scale = rect.width() / 20.0;
    const qreal radius = std::min<qreal>(
        adqt::widgets::scaleControlMetric(kRecordingEdgeRadius, scale), rect.height() / 2.0);
    const QRectF body = rect.adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath surface;
    if (onLeft) {
        surface.moveTo(body.topLeft());
        surface.lineTo(body.right() - radius, body.top());
        surface.quadTo(body.topRight(), QPointF(body.right(), body.top() + radius));
        surface.lineTo(body.right(), body.bottom() - radius);
        surface.quadTo(body.bottomRight(), QPointF(body.right() - radius, body.bottom()));
        surface.lineTo(body.bottomLeft());
    } else {
        surface.moveTo(body.topRight());
        surface.lineTo(body.left() + radius, body.top());
        surface.quadTo(body.topLeft(), QPointF(body.left(), body.top() + radius));
        surface.lineTo(body.left(), body.bottom() - radius);
        surface.quadTo(body.bottomLeft(), QPointF(body.left() + radius, body.bottom()));
        surface.lineTo(body.bottomRight());
    }
    surface.closeSubpath();
    const QColor fill = down          ? theme.colorFillSecondary
                        : hover       ? theme.colorPrimaryBg
                                      : liquidGlassPopupColor(themeIsDark());
    painter.setBrush(fill);
    painter.setPen(QPen(hover ? theme.colorPrimary : theme.colorBorderSecondary, 1));
    painter.drawPath(surface);
    const QPointF center(rect.center());
    const qreal direction = onLeft ? 1.0 : -1.0;
    QPainterPath chevron;
    chevron.moveTo(center + QPointF(-direction * 2 * scale, -4 * scale));
    chevron.lineTo(center + QPointF(direction * 2 * scale, 0));
    chevron.lineTo(center + QPointF(-direction * 2 * scale, 4 * scale));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(hover ? theme.colorPrimary : theme.colorTextSecondary, 1.7 * scale,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(chevron);
}

inline bool paintRecordingEdgeControl(QWidget* widget, const adqt::theme::ThemeMapToken& theme) {
    auto* button = qobject_cast<QAbstractButton*>(widget);
    QPainter painter(widget);
    paintRecordingEdgeControl(painter, QRectF(widget->rect()), recordingEdgeIsOnLeft(widget),
                             button != nullptr && button->isDown(), widget->underMouse(), theme);
    return true;
}

inline void paintQrPopoverBubble(QPainter& painter, const QRectF& rect, bool onLeft, qreal arrowY,
                                 const QColor& border) {
    // The bubble and its arrow are one shape. The text and buttons are children
    // and keep painting on top of this fill.
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QRectF box = rect.adjusted(12, 12, -12, -12);
    if (box.width() <= 0 || box.height() <= 0) {
        return;
    }
    const qreal y = std::clamp(arrowY, box.top() + 16, box.bottom() - 16);
    const qreal x = onLeft ? box.left() : box.right();
    QPainterPath shape;
    shape.addRoundedRect(box, 8, 8);
    QPainterPath arrow;
    arrow.moveTo(x, y - 8);
    arrow.lineTo(x + (onLeft ? -8.0 : 8.0), y);
    arrow.lineTo(x, y + 8);
    arrow.closeSubpath();
    painter.setPen(QPen(border, 1));
    painter.setBrush(liquidGlassPopupColor(themeIsDark()));
    painter.drawPath(shape.united(arrow));
}

inline bool qrPopoverArrow(const QWidget* body, bool* onLeft, qreal* arrowY) {
    const QWidget* popover = body != nullptr ? body->window() : nullptr;
    const QScreen* screen = popover != nullptr ? popover->screen() : nullptr;
    if (popover == nullptr || screen == nullptr || onLeft == nullptr || arrowY == nullptr) {
        return false;
    }
    const QPoint popoverGlobal = popover->mapToGlobal(QPoint(0, 0));
    const QRect available = screen->availableGeometry();
    const int popoverWidth = popover->width();
    const int popoverHeight = popover->height();
    const QWidget* matched = nullptr;
    int bestDistance = 12;
    bool matchedOnLeft = true;
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (QWidget* top : tops) {
        const QList<QWidget*> markers =
            top->findChildren<QWidget*>(QStringLiteral("screenshotQrMarker"));
        for (const QWidget* marker : markers) {
            if (marker == nullptr || !marker->isVisible()) {
                continue;
            }
            const QRect anchor(marker->mapToGlobal(QPoint(0, 0)), marker->size());
            const int preferredX = anchor.right() + 1 + 8 - 12;
            const bool arrowOnLeft = preferredX + popoverWidth <= available.right() + 1;
            int x = arrowOnLeft ? preferredX : anchor.left() - 8 - popoverWidth + 12;
            const int maxX = std::max(available.left(), available.right() + 1 - popoverWidth);
            const int maxY = std::max(available.top(), available.bottom() + 1 - popoverHeight);
            const int y =
                std::clamp(anchor.center().y() - popoverHeight / 2, available.top(), maxY);
            x = std::clamp(x, available.left(), maxX);
            const int distance = (QPoint(x, y) - popoverGlobal).manhattanLength();
            if (distance <= bestDistance) {
                bestDistance = distance;
                matched = marker;
                matchedOnLeft = arrowOnLeft;
            }
        }
    }
    if (matched == nullptr) {
        return false;
    }
    const QRect anchor(matched->mapToGlobal(QPoint(0, 0)), matched->size());
    *onLeft = matchedOnLeft;
    *arrowY = anchor.center().y() - popoverGlobal.y();
    return true;
}

inline bool floatingToolbarDropReady(const QDropEvent* event) {
    if (event == nullptr || !event->possibleActions().testFlag(Qt::CopyAction)) {
        return false;
    }
    const QMimeData* mime = event->mimeData();
    if (mime == nullptr) {
        return false;
    }
    if (mime->hasImage() || mime->hasText() || mime->hasHtml()) {
        return true;
    }
    for (const QString& format : mime->formats()) {
        if (format.startsWith(QLatin1String("image/"))) {
            return true;
        }
    }
    if (!mime->hasUrls()) {
        return false;
    }
    const QStringList extensions = snow_shot::app::supportedPinFileExtensions();
    for (const QUrl& url : mime->urls()) {
        if (url.isLocalFile() &&
            extensions.contains(QFileInfo(url.toLocalFile()).suffix(), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

inline void syncFloatingToolbarDropReady(QWidget* surface, bool ready) {
    if (surface == nullptr) {
        return;
    }
    const QList<QWidget*> buttons =
        surface->findChildren<QWidget*>(QStringLiteral("floatingToolbarSnowflake"));
    for (QWidget* button : buttons) {
        if (button->property("snowGlassDropReady").toBool() == ready) {
            continue;
        }
        button->setProperty("snowGlassDropReady", ready);
        button->update();
    }
}

inline void noteFloatingToolbarDrag(QObject* watched, QEvent* event) {
    auto* surface = qobject_cast<QWidget*>(watched);
    if (surface == nullptr || event == nullptr) {
        return;
    }
    const QString name = surface->objectName();
    if (name != QStringLiteral("floatingToolbarIconWindow") &&
        name != QStringLiteral("floatingToolbarWindow")) {
        return;
    }
    const QEvent::Type type = event->type();
    bool ready = false;
    if (type == QEvent::DragEnter || type == QEvent::DragMove) {
        ready = floatingToolbarDropReady(static_cast<const QDropEvent*>(event));
    } else if (type != QEvent::DragLeave && type != QEvent::Drop && type != QEvent::Hide) {
        return;
    }
    syncFloatingToolbarDropReady(surface, ready);
}

inline void paintFloatingSnowflake(QPainter& painter, const QRectF& rect, bool down, bool hover,
                                   bool focus, bool dropReady,
                                   const adqt::theme::ThemeMapToken& theme) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.save();
    painter.translate(rect.topLeft());
    painter.scale(rect.width() / 48.0, rect.height() / 48.0);
    painter.setPen(QPen(focus ? theme.colorPrimary : theme.colorBorderSecondary, 1));
    painter.setBrush(down ? theme.colorFillSecondary : liquidGlassPopupColor(themeIsDark()));
    painter.drawRoundedRect(QRectF(0.5, 0.5, 47, 47), 12, 12);
    if (hover) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme.colorFillQuaternary);
        painter.drawRoundedRect(QRectF(1, 1, 46, 46), 12, 12);
    }
    if (dropReady) {
        QColor glow = theme.colorSuccess;
        glow.setAlphaF(0.14F);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawRoundedRect(QRectF(1, 1, 46, 46), 12, 12);
        QColor ring = theme.colorSuccess;
        ring.setAlphaF(0.7F);
        painter.setPen(QPen(ring, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(2.5, 2.5, 43, 43), 10, 10);
    }
    painter.translate(24, 24);
    if (dropReady) {
        painter.scale(1.08, 1.08);
    }
    painter.setPen(QPen(dropReady ? theme.colorSuccess : theme.colorPrimary, 1.8, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    for (int arm = 0; arm < 6; ++arm) {
        painter.drawLine(QPointF(0, 0), QPointF(0, -14));
        painter.drawLine(QPointF(0, -8.5), QPointF(-4, -11));
        painter.drawLine(QPointF(0, -8.5), QPointF(4, -11));
        painter.rotate(60);
    }
    painter.restore();
}

inline bool overlayControlUsesDarkGlass(const QString& name) {
    // Pinned edit and close stay on their original plate. These controls sit on
    // a picture or a capture, so the resting plate uses the dark control color.
    return name == QStringLiteral("screenshotPinnedClickThroughExitButton") ||
           name == QStringLiteral("screenshotPinnedClickThroughMoveButton") ||
           name == QStringLiteral("screenRecordingRegionDragHandle") ||
           name == QStringLiteral("collageReplaceImage") ||
           name == QStringLiteral("collageDeleteImage") ||
           name == QStringLiteral("collageFitToCanvas");
}

inline void paintOverlayControlPlate(QPainter& painter, const QRectF& rect) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter.setBrush(liquidGlassPopupColor(true));
    painter.drawEllipse(rect.adjusted(0.5, 0.5, -0.5, -0.5));
}

inline bool paintOverlayControlButton(QWidget* widget) {
    auto* button = qobject_cast<adqt::widgets::AdButton*>(widget);
    if (button == nullptr || !button->isEnabled() || button->isDown() ||
        adqt::widgets::detail::widgetHovered(button)) {
        return false;
    }
    const adqt::icons::IconRef icon = button->iconRef();
    if (!adqt::icons::isValid(icon)) {
        return false;
    }
    QPainter painter(widget);
    paintOverlayControlPlate(painter, QRectF(widget->rect()));
    const QSize iconSize = button->iconSize().isValid() && !button->iconSize().isEmpty()
                               ? button->iconSize()
                               : QSize(18, 18);
    const QRectF iconRect(widget->width() / 2.0 - iconSize.width() / 2.0,
                          widget->height() / 2.0 - iconSize.height() / 2.0, iconSize.width(),
                          iconSize.height());
    adqt::icons::IconRenderRequest request;
    request.logicalSize = iconSize;
    request.devicePixelRatio = widget->devicePixelRatioF();
    adqt::icons::paintIcon(&painter, icon, iconRect, request);
    return true;
}

inline void paintWebcamResizeHandle(QPainter& painter, const QRectF& rect, bool down, bool hover) {
    // This handle sits on the camera picture, so it uses the dark control color.
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QColor fill = down    ? QColor(28, 32, 40)
                        : hover ? QColor(48, 52, 62)
                                : liquidGlassPopupColor(true);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter.setBrush(fill);
    painter.drawEllipse(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.save();
    painter.translate(rect.center());
    painter.scale(rect.width() / 32.0, rect.height() / 32.0);
    painter.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(-6, 6), QPointF(6, -6));
    painter.drawLine(QPointF(0, -6), QPointF(6, -6));
    painter.drawLine(QPointF(6, -6), QPointF(6, 0));
    painter.drawLine(QPointF(-6, 0), QPointF(-6, 6));
    painter.drawLine(QPointF(-6, 6), QPointF(0, 6));
    painter.restore();
}

struct RecordingStatusGlass {
    QString text;
    bool microphone = false;
    bool speaker = false;
};

inline std::optional<RecordingStatusGlass> recordingStatusFromAccessibleName(const QString& name) {
    const auto text = [](const char* source) {
        return QCoreApplication::translate("RecordingStatusPrompt", source);
    };
    const QString microphoneOn = text("Microphone enabled");
    const QString microphoneOff = text("Microphone disabled");
    const QString speakerOn = text("Speaker enabled");
    const QString speakerOff = text("Speaker disabled");
    for (const char* formatSource : {"GIF", "WebP", "MP4"}) {
        const QString format = text(formatSource);
        for (const bool microphone : {false, true}) {
            for (const bool speaker : {false, true}) {
                const QString candidate =
                    text("Format: %1; %2; %3")
                        .arg(format, microphone ? microphoneOn : microphoneOff,
                             speaker ? speakerOn : speakerOff);
                if (candidate == name) {
                    return RecordingStatusGlass{format, microphone, speaker};
                }
            }
        }
    }
    return std::nullopt;
}

inline void paintRecordingStatusPlate(QPainter& painter, const QRectF& rect) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter.setBrush(liquidGlassPopupColor(true));
    painter.drawRoundedRect(rect, 6, 6);
}

inline bool shortcutHintUsesMouse(const QString& shortcut) {
    const QString mouseWheel =
        QCoreApplication::translate("ScreenshotShortcutHintsWidget", "mouse wheel");
    return !mouseWheel.isEmpty() && shortcut.contains(mouseWheel);
}

inline void paintShortcutHintPlate(QPainter& painter, const QRectF& rect) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(QColor(255, 255, 255, 70), 1));
    painter.setBrush(liquidGlassPopupColor(true));
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
}

inline bool paintShortcutHints(QWidget* widget) {
    const QStringList lines =
        widget->accessibleName().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return false;
    }
    const qreal opacity = std::clamp(
        snow_shot::storage::ScreenshotUiSettings().shortcutHintOpacity() / 100.0, 0.0, 1.0);
    QPainter painter(widget);
    painter.setOpacity(opacity);
    paintShortcutHintPlate(painter, QRectF(widget->rect()));
    painter.setFont(widget->font());
    const QFontMetrics metrics(widget->font());
    const QColor contentColor(Qt::white);
    constexpr int padding = 16;
    constexpr int lineHeight = 22;
    constexpr int rowSpacing = 16;
    constexpr int colonGapLeft = 2;
    constexpr int colonGapRight = 8;
    constexpr int chipPadding = 16;
    constexpr int chipRadius = 6;
    constexpr int iconSize = 14;
    constexpr int iconGap = 8;
    int rowTop = padding;
    namespace custom = snow_shot::presentation::icons::third_party::outlined;
    for (const QString& line : lines) {
        const qsizetype ascii = line.indexOf(QLatin1Char(':'));
        const qsizetype fullWidth = line.indexOf(QChar(0xFF1A));
        const qsizetype separator = ascii < 0      ? fullWidth
                                    : fullWidth < 0 ? ascii
                                                    : std::min(ascii, fullWidth);
        const QString label = separator < 0 ? line : line.left(separator);
        const QString shortcut = separator < 0 ? QString() : line.mid(separator + 1).trimmed();
        const int labelWidth = metrics.horizontalAdvance(label);
        painter.setPen(QColor(255, 255, 255, 184));
        painter.drawText(QRect(padding, rowTop, labelWidth, lineHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, label);
        int chipLeft = padding + labelWidth;
        if (separator >= 0) {
            const QString colon = line.mid(separator, 1);
            const int colonWidth = metrics.horizontalAdvance(colon);
            painter.drawText(QRect(chipLeft + colonGapLeft, rowTop, colonWidth, lineHeight),
                             Qt::AlignLeft | Qt::AlignVCenter, colon);
            chipLeft += colonGapLeft + colonWidth + colonGapRight;
        }
        if (!shortcut.isEmpty()) {
            const int textWidth = metrics.horizontalAdvance(shortcut);
            const int chipWidth = chipPadding * 2 + iconSize + iconGap + textWidth;
            const QRectF chipRect(chipLeft, rowTop - 2, chipWidth, lineHeight + 4);
            painter.setPen(QPen(contentColor, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(chipRect.adjusted(0.5, 0.5, -0.5, -0.5), chipRadius,
                                    chipRadius);
            const QRect iconRect(chipLeft + chipPadding,
                                 rowTop + (lineHeight - iconSize) / 2, iconSize, iconSize);
            adqt::icons::paintIcon(&painter,
                                   shortcutHintUsesMouse(shortcut)
                                       ? custom::WheelMouse(adqt::icons::IconColors::primary(
                                             contentColor))
                                       : custom::Keyboard(adqt::icons::IconColors::primary(
                                             contentColor)),
                                   iconRect);
            painter.setPen(contentColor);
            painter.drawText(QRect(iconRect.right() + 1 + iconGap, rowTop, textWidth, lineHeight),
                             Qt::AlignLeft | Qt::AlignVCenter, shortcut);
        }
        rowTop += lineHeight + rowSpacing;
    }
    return true;
}

inline bool paintRecordingStatusPrompt(QWidget* widget) {
    const std::optional<RecordingStatusGlass> status =
        recordingStatusFromAccessibleName(widget->accessibleName());
    if (!status.has_value()) {
        return false;
    }
    QPainter painter(widget);
    paintRecordingStatusPlate(painter, QRectF(widget->rect()));
    painter.setFont(widget->font());
    painter.setPen(Qt::white);
    const int textWidth = widget->fontMetrics().horizontalAdvance(status->text);
    painter.drawText(QRect(12, 0, textWidth, widget->height()), Qt::AlignCenter, status->text);
    const auto colors = ThemeManager::instance().themeColorScheme().map;
    const auto microphoneColor = adqt::icons::IconColors::primary(
        status->microphone ? colors.colorSuccess : colors.colorError);
    const auto speakerColor =
        adqt::icons::IconColors::primary(status->speaker ? colors.colorSuccess : colors.colorError);
    constexpr int iconSize = 20;
    const QRect microphone(12 + textWidth + 12, (widget->height() - iconSize) / 2, iconSize,
                           iconSize);
    namespace custom = snow_shot::presentation::icons::third_party::outlined;
    adqt::icons::paintIcon(&painter,
                           status->microphone ? custom::RecordingMicrophone(microphoneColor)
                                              : custom::RecordingMicrophoneOff(microphoneColor),
                           microphone);
    adqt::icons::paintIcon(&painter,
                           status->speaker ? custom::Sound(speakerColor)
                                           : custom::RecordingSpeakerOff(speakerColor),
                           microphone.translated(iconSize + 12, 0));
    return true;
}

inline bool paintNamedControlPlate(QWidget* widget) {
    if (!liquidGlassShellEnabled() || widget == nullptr) {
        return false;
    }
    const QString name = widget->objectName();
    const auto theme = adqt::theme::ThemeManager::instance().resolveTheme(widget);
    if (overlayControlUsesDarkGlass(name)) {
        return paintOverlayControlButton(widget);
    }
    if (name == QStringLiteral("screenRecordingStatusPrompt")) {
        return paintRecordingStatusPrompt(widget);
    }
    if (name == QStringLiteral("screenshotShortcutHints")) {
        return paintShortcutHints(widget);
    }
    if (name == QStringLiteral("screenshotPinnedClickThroughOpacityEditor")) {
        // This pill sits on the pinned image, same as the capture readout, so it
        // stays on the dark control color instead of the window theme.
        QPainter painter(widget);
        paintOpaqueControlPlate(painter, QRectF(widget->rect()), widget->height() / 2.0,
                                QColor(255, 255, 255, 70), liquidGlassPopupColor(true));
        return true;
    }
    if (name == QStringLiteral("recordingToolbarAutoHideControl")) {
        return paintRecordingEdgeControl(widget, theme);
    }
    if (name == QStringLiteral("screenRecordingWebcamResize")) {
        auto* button = qobject_cast<QAbstractButton*>(widget);
        QPainter painter(widget);
        paintWebcamResizeHandle(painter, QRectF(widget->rect()),
                                button != nullptr && button->isDown(), widget->underMouse());
        return true;
    }
    if (name == QStringLiteral("floatingToolbarSnowflake")) {
        auto* button = qobject_cast<QAbstractButton*>(widget);
        QPainter painter(widget);
        paintFloatingSnowflake(painter, QRectF(widget->rect()),
                               button != nullptr && button->isDown(), widget->underMouse(),
                               widget->hasFocus(), widget->property("snowGlassDropReady").toBool(),
                               theme);
        return true;
    }
    if (name == QStringLiteral("screenshotQrPopoverBody")) {
        bool onLeft = true;
        qreal arrowY = 0;
        if (!qrPopoverArrow(widget, &onLeft, &arrowY)) {
            return false;
        }
        QPainter painter(widget);
        paintQrPopoverBubble(painter, QRectF(widget->rect()), onLeft, arrowY,
                             theme.colorBorderSecondary);
        return true;
    }
    if (!canvasToolbarUsesGlassPlate(name)) {
        return false;
    }
    const qreal radius = name == QStringLiteral("permissionGuide")
                             ? 11.0
                             : static_cast<qreal>(theme.borderRadiusSM);
    QPainter painter(widget);
    paintOpaqueControlPlate(painter, QRectF(widget->rect()), radius, theme.colorBorderSecondary);
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

inline QString canvasStatusReadoutStyleSheet() {
    // Scale, opacity, and preview chips sit on the picture. Keep the text and
    // padding, and replace the translucent black plate with the dark control color.
    const QColor fill = liquidGlassPopupColor(true);
    return QStringLiteral("QLabel { color: white; background-color: rgb(%1, %2, %3); "
                           "padding: 3px 6px; border-radius: 4px; }")
        .arg(fill.red())
        .arg(fill.green())
        .arg(fill.blue());
}

inline void applyCanvasStatusReadoutGlass(QWidget* widget) {
    if (!liquidGlassShellEnabled() || widget == nullptr ||
        !widget->inherits("CanvasStatusReadout")) {
        return;
    }
    const QString style = canvasStatusReadoutStyleSheet();
    if (widget->styleSheet() == style) {
        return;
    }
    widget->setStyleSheet(style);
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

    // A confirmation refresh writes its own popover color and clears a color set
    // on the inner popover. The resolver is the color that refresh keeps.
    QList<adqt::widgets::AdPopconfirm*> confirmations;
    collectControlObjects(confirmations);
    for (adqt::widgets::AdPopconfirm* confirmation : confirmations) {
        if (confirmation->property("snowGlassPopconfirm").toBool()) {
            continue;
        }
        confirmation->setProperty("snowGlassPopconfirm", true);
        confirmation->setSemanticStyleResolver([](const adqt::widgets::AdPopconfirm::StyleContext&) {
            adqt::widgets::AdPopconfirm::SemanticStyles styles;
            styles.container.backgroundColor = liquidGlassPopupColor(themeIsDark());
            return styles;
        });
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

inline bool selectIsGlobalSearch(const QWidget* select) {
    for (const QWidget* cursor = select; cursor != nullptr; cursor = cursor->parentWidget()) {
        if (cursor->objectName() == QStringLiteral("globalTopSearchBar")) {
            return true;
        }
    }
    return false;
}

inline void applySearchPopupGlass() {
    // The search field stores the menu color and paints every result row with
    // it. One theme pass replaces that color and leaves the field itself alone.
    const QColor fill = liquidGlassPopupColor(themeIsDark());
    QList<ApplicationSearchWidget*> searches;
    collectControlObjects(searches);
    if (searches.isEmpty()) {
        return;
    }
    auto scheme = ThemeManager::instance().themeColorScheme();
    scheme.map.colorBgElevated = fill;
    for (ApplicationSearchWidget* search : searches) {
        if (search == nullptr) {
            continue;
        }
        auto* select = search->findChild<adqt::widgets::AdSelect*>();
        if (select != nullptr && select->componentTokens().colors.popupBackground == fill) {
            continue;
        }
        search->applyTheme(scheme);
    }
}

inline void applySelectPopupGlass(QWidget* popup) {
    Q_UNUSED(popup);
    // Tool-window menus are parented to nothing, so they are not children of the
    // window that owns the select. Walk every top-level widget instead.
    const QColor fill = liquidGlassPopupColor(themeIsDark());
    QList<adqt::widgets::AdSelect*> selects;
    collectControlObjects(selects);
    for (adqt::widgets::AdSelect* select : selects) {
        if (select == nullptr || selectIsGlobalSearch(select)) {
            continue;
        }
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
    applySelectPopupGlass(nullptr);
    applySearchPopupGlass();
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
        if (liquidGlassShellEnabled() &&
            (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
             event->type() == QEvent::DragLeave || event->type() == QEvent::Drop ||
             event->type() == QEvent::Hide)) {
            noteFloatingToolbarDrag(watched, event);
        }
        if (liquidGlassShellEnabled() && event->type() == QEvent::Paint) {
            if (auto* widget = qobject_cast<QWidget*>(watched)) {
                if (paintNamedControlPlate(widget)) {
                    return true;
                }
            }
        }
        if (liquidGlassShellEnabled() && event->type() == QEvent::Show) {
            if (auto* widget = qobject_cast<QWidget*>(watched)) {
                applyCanvasStatusReadoutGlass(widget);
                const QString name = widget->objectName();
                if (name == QStringLiteral("ad-modal-overlay")) {
                    applyModalGlass();
                } else if (name == QStringLiteral("adselect-popup")) {
                    applySelectPopupGlass(widget);
                    applySearchPopupGlass();
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
