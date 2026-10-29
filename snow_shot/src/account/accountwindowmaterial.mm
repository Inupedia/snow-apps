#include <QWidget>
#include <QGuiApplication>
#import <AppKit/AppKit.h>

namespace snow_shot::account {
void applyAccountWindowMaterial(QWidget* widget) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;
    NSView* view = reinterpret_cast<NSView*>(widget->winId());
    NSWindow* window = view.window;
    if (!window)
        return;
    window.titlebarAppearsTransparent = YES;
    window.backgroundColor = NSColor.windowBackgroundColor;
    // Use the system's title-bar material; form fields retain an opaque reading surface.
    window.titleVisibility = NSWindowTitleHidden;
    auto* material = [[NSVisualEffectView alloc] initWithFrame:view.frame];
    material.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    material.material = NSVisualEffectMaterialUnderWindowBackground;
    material.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    material.state = NSVisualEffectStateFollowsWindowActiveState;
    [view.superview addSubview:material positioned:NSWindowBelow relativeTo:view];
    [material release];
}
} // namespace snow_shot::account
