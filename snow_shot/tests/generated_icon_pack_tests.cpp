#include "snow_shot/presentation/components/icons/snowshoticons.h"
#include "snow_shot/presentation/components/icons/third_party_icons.h"

#include "icon_renderer.h"
#include "antd_icons.h"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QPixmap>

#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

QPixmap render(const adqt::icons::IconRef& icon, const QSize& size, qreal dpr = 1.0) {
    adqt::icons::IconRenderRequest request;
    request.logicalSize = size;
    request.devicePixelRatio = dpr;
    return adqt::icons::renderIconPixmap(icon, request);
}

QRect alphaBounds(const QImage& image) {
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                bounds |= QRect(x, y, 1, 1);
            }
        }
    }
    return bounds;
}

bool containsColor(const QImage& image, const QColor& expected) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            if (actual.alpha() >= 192 && std::abs(actual.red() - expected.red()) <= 2 &&
                std::abs(actual.green() - expected.green()) <= 2 &&
                std::abs(actual.blue() - expected.blue()) <= 2) {
                return true;
            }
        }
    }
    return false;
}

void everySnowShotEntryRenders() {
    namespace icons = snow_shot::presentation::icons::custom;
    adqt::icons::IconRenderer renderer;
    const auto registered = icons::registerWith(renderer);
    require(registered.ok(), "InuShot product asset pack registration should succeed");
    const adqt::icons::IconPack* pack = icons::pack().staticPack();
    require(pack != nullptr && pack->entryCount == 5,
            "InuShot pack should contain the five brand, app, and cursor assets");

    adqt::icons::IconRenderRequest request;
    request.logicalSize = QSize(32, 32);
    request.devicePixelRatio = 1.25;
    for (std::size_t index = 0; index < pack->entryCount; ++index) {
        const auto icon = icons::pack().icon(index);
        require(icon.isValid(), "every InuShot product asset should create an icon reference");
        const QPixmap pixmap = renderer.renderIconPixmap(icon, request);
        require(!pixmap.isNull() && pixmap.size() == QSize(40, 40) &&
                    qFuzzyCompare(pixmap.devicePixelRatio(), 1.25),
                "every InuShot product asset should render at fractional DPR");
        require(!alphaBounds(pixmap.toImage()).isEmpty(),
                "every InuShot product asset should have visible artwork");
    }
}

void productBrandingRetainsItsColorModels() {
    namespace icons = snow_shot::presentation::icons::custom;
    const QColor tint(0, 166, 90);
    const auto logo = icons::brand::SnowShotLogo(adqt::icons::IconColors::primary(tint));
    const auto logoMetadata = adqt::icons::describeIcon(logo);
    require(logoMetadata.key.pack == QStringLiteral("snow-shot") &&
                logoMetadata.colorModel == adqt::icons::IconColorModel::Monochrome &&
                containsColor(render(logo, QSize(190, 34)).toImage(), tint),
            "the product wordmark remains a tintable brand asset");

    const auto miniLogo = icons::brand::SnowShotMiniLogo(adqt::icons::IconColors::primary(tint));
    const auto miniLogoMetadata = adqt::icons::describeIcon(miniLogo);
    require(miniLogoMetadata.colorModel == adqt::icons::IconColorModel::Monochrome &&
                containsColor(render(miniLogo, QSize(106, 17)).toImage(), tint),
            "the compact product wordmark remains a tintable brand asset");

    const auto applicationIcon = icons::app::ApplicationIcon();
    const auto applicationMetadata = adqt::icons::describeIcon(applicationIcon);
    require(applicationMetadata.colorModel == adqt::icons::IconColorModel::FullColor &&
                !alphaBounds(render(applicationIcon, QSize(64, 64)).toImage()).isEmpty(),
            "the application icon retains its full-color product artwork");
}

void iconUsesAntDesign(const adqt::icons::IconRef& icon) {
    const auto metadata = adqt::icons::describeIcon(icon);
    require(metadata.key.pack == QStringLiteral("antd") &&
                metadata.colorModel == adqt::icons::IconColorModel::Monochrome,
            "product UI aliases should resolve to monochrome Ant Design icons");

    for (const QColor color : {QColor("#262626"), QColor("#f0f0f0"), QColor("#1677ff")}) {
        const auto tinted = icon.withColors(adqt::icons::IconColors::primary(color));
        for (const int size : {16, 20, 24, 32}) {
            for (const qreal dpr : {1.0, 1.25, 2.0}) {
                const QPixmap pixmap = render(tinted, QSize(size, size), dpr);
                const QImage image = pixmap.toImage();
                const QRect bounds = alphaBounds(image);
                require(!pixmap.isNull() && !bounds.isEmpty(),
                        "Ant Design aliases should render visible artwork at toolbar sizes");
                require(containsColor(image, color),
                        "Ant Design aliases should follow the requested theme color");
            }
        }
    }
}

void productIconAliasesUseTheBundledAntDesignPack() {
    namespace icons = snow_shot::presentation::icons::third_party::outlined;
    const std::array<adqt::icons::IconRef, 16> iconsToCheck = {icons::AirDrop(),
                                                               icons::GlobalMouseNav(),
                                                               icons::PinToScreen(),
                                                               icons::RecordScreen(),
                                                               icons::TextRecognition(),
                                                               icons::OcrTranslate(),
                                                               icons::ApplicationShortcutsNav(),
                                                               icons::ScreenshotFeature(),
                                                               icons::ColorPicker(),
                                                               icons::ImageEditing(),
                                                               icons::ImageSplitting(),
                                                               icons::OpenSpreadsheetFile(),
                                                               icons::RevealInFolder(),
                                                               icons::FlipHorizontal(),
                                                               icons::BrushPressureSensitive(),
                                                               icons::SelectionRadius()};
    for (const auto& icon : iconsToCheck) {
        iconUsesAntDesign(icon);
    }
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        namespace icons = snow_shot::presentation::icons::third_party::outlined;
        if (application.arguments().contains(QStringLiteral("--all-entries-only"))) {
            everySnowShotEntryRenders();
            return 0;
        }
        if (application.arguments().contains(QStringLiteral("--airdrop-only"))) {
            iconUsesAntDesign(icons::AirDrop());
            return 0;
        }
        if (application.arguments().contains(QStringLiteral("--border-fill-only"))) {
            iconUsesAntDesign(icons::FillBorder());
            return 0;
        }
        if (application.arguments().contains(QStringLiteral("--reveal-only"))) {
            iconUsesAntDesign(icons::RevealInFolder());
            return 0;
        }
        if (application.arguments().contains(QStringLiteral("--image-splitting-only"))) {
            iconUsesAntDesign(icons::ImageSplitting());
            return 0;
        }

        everySnowShotEntryRenders();
        productBrandingRetainsItsColorModels();
        productIconAliasesUseTheBundledAntDesignPack();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
