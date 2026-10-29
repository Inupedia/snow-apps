#include "screenshottoolpalettestylepresets.h"

#include "snow_shot/presentation/components/icons/snowshoticons.h"
#include "snow_shot/presentation/components/icons/third_party_icons.h"
#include "snow_shot/presentation/screenshotdefaultstyles.h"
#include "snow_shot/storage/settingsadapters.h"

namespace snow_shot::presentation::style_presets {
namespace {
namespace third_party_icons = snow_shot::presentation::icons::third_party::outlined;

} // namespace

QVector<QColor> strokeColors() {
    return snow_shot::storage::ScreenshotColorPresetSettings{}.strokeColors();
}

QVector<QColor> shapeFillColors() {
    return snow_shot::storage::ScreenshotColorPresetSettings{}.fillColors();
}

QVector<QColor> textColors() {
    return strokeColors();
}

QVector<QColor> textFillColors() {
    return shapeFillColors();
}

const QVector<double>& shapeStrokeWidths() {
    static const QVector<double> values{
        snow_shot::presentation::screenshotCanvasStyleDefaults().rectangle.strokeWidth,
        4.0,
        8.0,
    };
    return values;
}

const QVector<double>& strokePresetWidths() {
    static const QVector<double> values{2.0, 4.0, 8.0};
    return values;
}

const QVector<double>& sizePresetValues() {
    static const QVector<double> values{24.0, 30.0, 42.0, 54.0};
    return values;
}

const QStringList& sizePresetLabels() {
    static const QStringList labels{
        QStringLiteral("S"),
        QStringLiteral("M"),
        QStringLiteral("L"),
        QStringLiteral("XL"),
    };
    return labels;
}

adqt::icons::IconRef sizePresetIcon(int index) {
    switch (index) {
    case 0:
        return third_party_icons::FontSizeSmall();
    case 1:
        return third_party_icons::FontSizeMedium();
    case 2:
        return third_party_icons::FontSizeLarge();
    case 3:
        return third_party_icons::FontSizeVeryLarge();
    default:
        return third_party_icons::FontSizeMedium();
    }
}

const QVector<double>& fontSizes() {
    // The font size presets intentionally use the same S/M/L/XL values as the
    // pen width presets; both render through sizePresetIcon/sizePresetLabels.
    return sizePresetValues();
}

const QVector<double>& watermarkFontSizes() {
    static const QVector<double> values{12.0, 16.0, 24.0, 30.0};
    return values;
}

ScreenshotToolPaletteTranslationText sizePresetTooltip(const char* pattern, int index,
                                                       double value) {
    return ScreenshotToolPaletteTranslationText(pattern)
        .arg(sizePresetLabels().value(index))
        .arg(value, 0, 'g', 3);
}

} // namespace snow_shot::presentation::style_presets
