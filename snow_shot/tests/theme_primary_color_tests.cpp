#include "snow_shot/presentation/globalshortcutmanager.h"
#include "snow_shot/presentation/languagemanager.h"
#include "snow_shot/presentation/settings/settingsbackend.h"
#include "snow_shot/presentation/settings/settingscatalog.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/settingsadapters.h"
#include "theme/theme_manager.h"

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void requireUpdateAppearance() {
    const auto& resolved = adqt::theme::ThemeManager::instance().globalResolvedTheme();
    const auto& colors = resolved.theme.palette;
    const auto& metrics = resolved.theme.metrics;
    const QJsonObject appearance =
        snow_shot::presentation::styles::ThemeManager::instance().updateProgressAppearance();
    require(appearance.size() == 16, "updater appearance contains the complete token contract");
    const auto rgb = [](const QColor& color) { return static_cast<int>(color.rgb() & 0xffffffU); };
    const QColor background = resolved.semantic.surfaceElevated;
    const auto composite = [&background, &rgb](const QColor& color) {
        const float alpha = color.alphaF();
        QColor mixed;
        mixed.setRedF(color.redF() * alpha + background.redF() * (1.0F - alpha));
        mixed.setGreenF(color.greenF() * alpha + background.greenF() * (1.0F - alpha));
        mixed.setBlueF(color.blueF() * alpha + background.blueF() * (1.0F - alpha));
        return rgb(mixed);
    };
    require(appearance.value(QStringLiteral("background")).toInt() ==
                    rgb(background) &&
                appearance.value(QStringLiteral("text")).toInt() == composite(colors.colorText) &&
                appearance.value(QStringLiteral("textSecondary")).toInt() ==
                    composite(colors.colorTextSecondary) &&
                appearance.value(QStringLiteral("textTertiary")).toInt() ==
                    composite(colors.colorTextTertiary) &&
                appearance.value(QStringLiteral("fillSecondary")).toInt() ==
                    composite(colors.colorFillSecondary) &&
                appearance.value(QStringLiteral("fillTertiary")).toInt() ==
                    composite(colors.colorFillTertiary),
            "updater flattens translucent tokens over its elevated surface");
    require(appearance.value(QStringLiteral("primary")).toInt() == rgb(colors.colorPrimary) &&
                appearance.value(QStringLiteral("primaryBackground")).toInt() ==
                    rgb(colors.colorPrimaryBg) &&
                appearance.value(QStringLiteral("border")).toInt() ==
                    composite(colors.colorBorderSecondary) &&
                appearance.value(QStringLiteral("success")).toInt() == rgb(colors.colorSuccess) &&
                appearance.value(QStringLiteral("error")).toInt() == rgb(colors.colorError),
            "updater inherits resolved accent and semantic colors");
    require(appearance.value(QStringLiteral("fontFamily")).toString() ==
                    QApplication::font().family() &&
                appearance.value(QStringLiteral("fontSize")).toInt() == qRound(metrics.fontSize) &&
                appearance.value(QStringLiteral("smallFontSize")).toInt() ==
                    qRound(metrics.fontSizeSM) &&
                appearance.value(QStringLiteral("borderRadius")).toInt() ==
                    qRound(metrics.borderRadiusLG) &&
                appearance.value(QStringLiteral("motion")).toBool() == resolved.config.motion,
            "updater inherits application typography, popup radius, and motion preference");
}

void requireThemeConfigConversions() {
    namespace styles = snow_shot::presentation::styles;
    auto& theme = styles::ThemeManager::instance();
    for (const auto appearance : {styles::ThemeAppearance::Light, styles::ThemeAppearance::Dark}) {
        for (const auto preset : {styles::ThemePreset::Default, styles::ThemePreset::Compact}) {
            styles::ThemeStyleConfig config;
            config.appearance = appearance;
            config.preset = preset;
            config.colorPrimary = QColor("#13c2c2");
            config.colorSuccess = QColor("#389e0d");
            config.colorWarning = QColor("#d48806");
            config.colorError = QColor("#cf1322");
            config.colorLink = QColor("#531dab");
            config.presetColors = {{QStringLiteral(" BLUE "), QColor("#123456")},
                                   {QStringLiteral("Pink"), QColor("#654321")},
                                   {QStringLiteral("lime"), QColor()},
                                   {QStringLiteral("unknown"), QColor("#ffffff")}};
            config.appFont.setFamily(QStringLiteral("Theme conversion test family"));
            config.fontSize = 17;
            config.lineWidth = preset == styles::ThemePreset::Compact ? 0 : 3;
            config.borderRadius = 9;
            config.sizeUnit = 5;
            config.sizeStep = 3;
            config.controlHeight = 31;
            config.motionUnit = appearance == styles::ThemeAppearance::Dark ? 0.0 : 0.1;

            auto expected = adqt::theme::defaultThemeConfig(
                appearance == styles::ThemeAppearance::Dark ? adqt::theme::ThemeScheme::Dark
                                                            : adqt::theme::ThemeScheme::Light,
                preset == styles::ThemePreset::Compact ? adqt::theme::ThemeDensity::Compact
                                                       : adqt::theme::ThemeDensity::Comfortable);
            if (appearance == styles::ThemeAppearance::Dark) {
                expected.surfaceOverrides.colorBgBase = QColor("#111D2B");
                expected.surfaceOverrides.colorBgLayout = QColor("#111F2E");
                expected.surfaceOverrides.colorBgContainer = QColor("#182A3D");
                expected.surfaceOverrides.colorBgContainerDisabled = QColor("#1B2F43");
                expected.surfaceOverrides.colorBgElevated = QColor("#20374B");
                expected.surfaceOverrides.colorFillSecondary = QColor("#2B4359");
                expected.surfaceOverrides.colorFillTertiary = QColor("#233A50");
                expected.surfaceOverrides.colorFillQuaternary = QColor("#1D3246");
                expected.surfaceOverrides.colorBorder = QColor("#36516A");
                expected.surfaceOverrides.colorBorderDisabled = QColor("#2B4359");
                expected.surfaceOverrides.colorBorderSecondary = QColor("#2B4359");
            } else {
                expected.surfaceOverrides.colorBgBase = QColor("#F5F9FE");
                expected.surfaceOverrides.colorBgLayout = QColor("#EAF3FC");
                expected.surfaceOverrides.colorBgContainer = QColor("#F5FAFF");
                expected.surfaceOverrides.colorBgContainerDisabled = QColor("#EAF3FA");
                expected.surfaceOverrides.colorBgElevated = QColor("#FCFDFF");
                expected.surfaceOverrides.colorFillSecondary = QColor("#E4EFFA");
                expected.surfaceOverrides.colorFillTertiary = QColor("#EDF5FC");
                expected.surfaceOverrides.colorFillQuaternary = QColor("#F4F8FD");
                expected.surfaceOverrides.colorBorder = QColor("#C9DCEC");
                expected.surfaceOverrides.colorBorderDisabled = QColor("#D8E7F4");
                expected.surfaceOverrides.colorBorderSecondary = QColor("#D8E7F4");
            }
            expected.primary = config.colorPrimary;
            expected.success = config.colorSuccess;
            expected.warning = config.colorWarning;
            expected.error = config.colorError;
            expected.link = config.colorLink;
            expected.blue = QColor("#123456");
            expected.pink = QColor("#654321");
            expected.appFont = config.appFont;
            expected.fontSize = 17.0;
            expected.lineWidth = static_cast<double>(config.lineWidth);
            expected.borderRadius = 9.0;
            expected.sizeUnit = 5.0;
            expected.sizeStep = 3.0;
            expected.controlHeight = 31.0;
            expected.motion = config.motionUnit > 0.0;

            const auto resolved = adqt::theme::makeResolvedTheme(expected);
            const auto scheme = styles::generateThemeColorScheme(config);
            require(scheme.appearance == appearance &&
                        scheme.map.colorPrimary == resolved.values.colorPrimary &&
                        scheme.map.colorSuccess == resolved.values.colorSuccess &&
                        scheme.map.colorWarning == resolved.values.colorWarning &&
                        scheme.map.colorError == resolved.values.colorError &&
                        scheme.map.colorInfo == resolved.values.colorInfo &&
                        scheme.map.colorLink == resolved.values.colorLink &&
                        scheme.map.presetColorHover.value(QStringLiteral("blue")) ==
                            resolved.values.blue &&
                        scheme.map.presetColorHover.value(QStringLiteral("pink")) ==
                            resolved.values.pink &&
                        scheme.map.presetColorHover.value(QStringLiteral("lime")) ==
                            resolved.values.lime,
                    "standalone schemes preserve valid overrides, normalized presets and defaults");
            require(scheme.metricMap.control.lineWidth == (config.lineWidth == 0 ? 1 : 3) &&
                        scheme.metricMap.font.fontSize == 17 &&
                        (scheme.metricMap.control.motionDurationFastMs > 0) == expected.motion,
                    "scheme metrics preserve minimum visible borders and motion preferences");

            theme.setThemeStyleConfig(config);
            require(adqt::theme::ThemeManager::instance().config() == expected,
                    "live themes and standalone schemes use the same input conversion");
            theme.setThemePreset(preset == styles::ThemePreset::Compact
                                     ? styles::ThemePreset::Default
                                     : styles::ThemePreset::Compact);
            require(adqt::theme::ThemeManager::instance().config().lineWidth == expected.lineWidth,
                    "live theme round trips preserve zero-width borders");
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "create isolated theme storage");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "create executable directory");
    namespace storage = snow_shot::storage;
    namespace settings = snow_shot::presentation::settings;
    namespace styles = snow_shot::presentation::styles;
    auto& appStorage = storage::ApplicationStorage::instance();
    require(appStorage.initialize({executable, temporary.path(), 60000}).success,
            "initialize theme storage");
    snow_shot::presentation::LanguageManager::instance().initialize();
    const storage::InterfaceSettings preferences;
    require(preferences.themePrimaryColor() == QColor("#1677ff"), "default primary is blue");
    const QColor saved("#722ed1");
    require(preferences.setThemePrimaryColor(saved), "save primary before theme initialization");
    auto& theme = styles::ThemeManager::instance();
    theme.initialize(application);
    require(adqt::theme::ThemeManager::instance().config().primary == saved,
            "startup applies saved primary color");
    requireUpdateAppearance();
    snow_shot::presentation::GlobalShortcutManager shortcuts;
    settings::BuiltInSettingsBackend backend(shortcuts);
    const auto binding = settings::SettingsColorBinding::ThemePrimaryColor;
    const QColor custom("#13c2c2");
    int changes = 0;
    QObject::connect(&theme, &styles::ThemeManager::themeChanged, &application,
                     [&changes] { ++changes; });
    require(backend.applyColorValue(binding, custom) && backend.colorValue(binding) == custom &&
                preferences.themePrimaryColor() == custom && changes == 1 &&
                adqt::theme::ThemeManager::instance().config().primary == custom,
            "editing primary persists and updates the live theme");
    requireUpdateAppearance();
    require(appStorage.flushNow().success, "flush custom theme preference");
    const storage::ConfigurationStore reloaded(
        QDir(appStorage.configurationDirectory()).filePath(QStringLiteral("config.json")), true,
        false);
    require(storage::colorFromRgbaString(
                reloaded.value(QStringLiteral("interface/theme_primary_color")).toString()) ==
                custom,
            "custom primary color survives a configuration reload");
    require(!backend.applyColorValue(binding, QColor()) &&
                preferences.themePrimaryColor() == custom,
            "invalid colors leave the preference unchanged");
    for (auto mode :
         {styles::ThemeMode::Dark, styles::ThemeMode::Light, styles::ThemeMode::FollowSystem}) {
        theme.setThemeMode(mode);
        require(adqt::theme::ThemeManager::instance().config().primary == custom,
                "appearance changes preserve primary color");
        requireUpdateAppearance();
    }
    const QString customFont = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    require(theme.setAppFontFamily(customFont), "configure updater font through app preference");
    requireUpdateAppearance();
    require(theme.updateProgressAppearance().value(QStringLiteral("fontFamily")).toString() ==
                customFont,
            "updater receives the configured application font");
    require(backend.resetSection(settings::SettingsSectionReset::GeneralSettings) &&
                preferences.themePrimaryColor() == QColor("#1677ff") &&
                adqt::theme::ThemeManager::instance().config().primary == QColor("#1677ff"),
            "general reset restores the default primary color immediately");
    theme.setThemeMode(styles::ThemeMode::Light);
    requireUpdateAppearance();
    const QJsonObject defaultAppearance = theme.updateProgressAppearance();
    require(defaultAppearance.value(QStringLiteral("background")).toInt() == 0xffffff &&
                defaultAppearance.value(QStringLiteral("primary")).toInt() == 0x1677ff &&
                defaultAppearance.value(QStringLiteral("fontSize")).toInt() == 14 &&
                defaultAppearance.value(QStringLiteral("smallFontSize")).toInt() == 12 &&
                defaultAppearance.value(QStringLiteral("borderRadius")).toInt() == 8,
            "default updater matches Ant's comfortable notification and small progress tokens");
    const QFont previousFont = QApplication::font();
    const QString oversizedFamily(64 * 1024, QLatin1Char('A'));
    QFont oversizedFont = previousFont;
    oversizedFont.setFamily(oversizedFamily);
    QApplication::setFont(oversizedFont);
    const QJsonObject boundedAppearance = theme.updateProgressAppearance();
    QApplication::setFont(previousFont);
    require(boundedAppearance.value(QStringLiteral("fontFamily")).toString() ==
                    oversizedFamily.left(63) &&
                QJsonDocument(boundedAppearance).toJson(QJsonDocument::Compact).size() < 2048,
            "imported font names are bounded before updater protocol serialization");
    auto motionConfig = adqt::theme::ThemeManager::instance().config();
    motionConfig.motion = false;
    adqt::theme::ThemeManager::instance().setConfig(motionConfig);
    requireUpdateAppearance();
    require(!theme.updateProgressAppearance().value(QStringLiteral("motion")).toBool(),
            "updater respects disabled Ant motion");
    requireThemeConfigConversions();
    require(appStorage.flushNow().success, "flush theme preferences");
    appStorage.shutdown();
    return 0;
}
