#include "snow_shot/presentation/components/icons/snowshoticons.h"

#include "icon_renderer.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool containsVisiblePixels(const QImage& image) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                return true;
            }
        }
    }
    return false;
}

void applicationIconUsesGeneratedArtworkAtEveryScale() {
    const auto icon = snow_shot::presentation::icons::custom::app::ApplicationIcon();
    require(icon.isValid(), "application icon must have a valid descriptor");
    const auto svg = icon.descriptor()->svg;
    require(svg.find("<image") != std::string_view::npos &&
                svg.find("data:image/png;base64,") != std::string_view::npos,
            "the InuShot application icon must preserve its generated full-color artwork");

    for (const qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0, 3.0}) {
        adqt::icons::IconRenderRequest request;
        request.logicalSize = QSize(16, 16);
        request.devicePixelRatio = scale;
        const QPixmap pixmap = adqt::icons::renderIconPixmap(icon, request);
        require(pixmap.size() == QSize(qRound(16 * scale), qRound(16 * scale)) &&
                    pixmap.devicePixelRatio() == scale,
                "caption icon must provide physical pixels for the window's display scale");
        const QImage image = pixmap.toImage();
        require(containsVisiblePixels(image), "InuShot icon must render visible artwork");
    }
}

void titleBarIconKeepsItsTransparentArtwork() {
    namespace icons = snow_shot::presentation::icons::custom;
    const auto icon = icons::app::ApplicationTitleBarIcon();
    require(icon.isValid(), "title bar icon must have a valid descriptor");
    const auto svg = icon.descriptor()->svg;
    require(svg.find("#FFFFFF") == std::string_view::npos,
            "the title bar icon must not carry the application icon's white background");

    for (const qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0, 3.0}) {
        adqt::icons::IconRenderRequest request;
        request.logicalSize = QSize(16, 16);
        request.devicePixelRatio = scale;
        const QImage image = adqt::icons::renderIconPixmap(icon, request).toImage();
        bool containsWhiteBackdrop = false;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor color = image.pixelColor(x, y);
                containsWhiteBackdrop |= color.alpha() >= 250 && color.red() >= 240 &&
                                         color.green() >= 240 && color.blue() >= 240;
            }
        }
        require(!containsWhiteBackdrop,
                "the title bar icon must render without an opaque white backdrop");
        require(containsVisiblePixels(image), "the InuShot title bar icon must remain visible");
    }
}

void installedApplicationIconKeepsTheGeneratedArtwork() {
    namespace icons = snow_shot::presentation::icons::custom;
    QApplication::setWindowIcon(adqt::icons::makeIcon(icons::app::ApplicationIcon()));
    const QIcon installedIcon = QApplication::windowIcon();
    require(!installedIcon.isNull(), "QApplication did not retain the InuShot icon");

    for (const int size : {16, 20, 24, 32, 40, 48, 64}) {
        const QPixmap pixmap = installedIcon.pixmap(QSize(size, size));
        require(!pixmap.isNull() && pixmap.size() == QSize(size, size),
                "application icon should provide every taskbar raster size");
        require(containsVisiblePixels(pixmap.toImage()),
                "application icon should render at taskbar sizes");
    }
}

#ifdef Q_OS_WIN
void executableIconResourceKeepsTheGeneratedArtwork() {
    HMODULE module = GetModuleHandleW(nullptr);
    require(module != nullptr, "application icon test could not resolve its module");
    for (const int size : {16, 32, 48}) {
        HICON icon = static_cast<HICON>(
            LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
        require(icon != nullptr, "InuShot executable did not contain its application icon");
        const QImage image = QImage::fromHICON(icon);
        DestroyIcon(icon);
        require(!image.isNull() && containsVisiblePixels(image),
                "embedded InuShot application icon should render visible artwork");
    }
}
#endif

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        applicationIconUsesGeneratedArtworkAtEveryScale();
        titleBarIconKeepsItsTransparentArtwork();
        installedApplicationIconKeepsTheGeneratedArtwork();
#ifdef Q_OS_WIN
        executableIconResourceKeepsTheGeneratedArtwork();
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
