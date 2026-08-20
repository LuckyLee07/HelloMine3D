#include "RuntimeConfig.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
struct ParsedRuntimeConfig {
    Config config;
    int version = 0;
    bool needsMigration = false;
};

[[noreturn]] void fail(const std::string &path, const std::string &key,
                       const std::string &detail)
{
    throw std::runtime_error("Invalid runtime config '" + path +
                             "': key '" + key + "' " + detail + ".");
}

void requireEnd(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    input >> std::ws;
    if (!input.eof()) {
        fail(path, key, "contains trailing data");
    }
}

int readInteger(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    int value = 0;
    if (!(input >> value)) {
        fail(path, key, "must contain an integer");
    }
    return value;
}

float readFloat(const std::string &path, const std::string &key,
                std::istringstream &input)
{
    float value = 0.f;
    if (!(input >> value) || !std::isfinite(value)) {
        fail(path, key, "must contain a finite number");
    }
    return value;
}

void validateVolume(const char *name, float value)
{
    if (!std::isfinite(value) || value < 0.f || value > 1.f) {
        throw std::runtime_error(std::string(name) +
                                 " must be between 0.0 and 1.0");
    }
}

ParsedRuntimeConfig parseRuntimeConfig(const std::string &path,
                                       bool allowLegacy)
{
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Unable to read runtime config '" + path +
                                 "'.");
    }

    ParsedRuntimeConfig parsed;
    bool hasVersion = false;
    bool usesVersionTwoKey = false;
    std::set<std::string> seenKeys;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }

        std::istringstream values(line);
        std::string key;
        if (!(values >> key)) {
            continue;
        }
        if (!seenKeys.insert(key).second) {
            fail(path, key, "is duplicated at line " +
                                std::to_string(lineNumber));
        }

        if (key == "settings_version") {
            const int version = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (version != LegacyRuntimeSettingsFormatVersion &&
                version != RuntimeSettingsFormatVersion) {
                fail(path, key, "uses unsupported version " +
                                    std::to_string(version));
            }
            parsed.version = version;
            hasVersion = true;
        }
        else if (key == "renderdistance") {
            parsed.config.renderDistance = readInteger(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "fullscreen") {
            const int fullscreen = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (fullscreen != 0 && fullscreen != 1) {
                fail(path, key, "must be 0 or 1");
            }
            parsed.config.isFullscreen = fullscreen != 0;
        }
        else if (key == "windowsize") {
            parsed.config.windowX = readInteger(path, key, values);
            parsed.config.windowY = readInteger(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "fov") {
            parsed.config.fov = readInteger(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "mousesensitivity") {
            parsed.config.mouseSensitivity = readFloat(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "invertmousey") {
            const int inverted = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (inverted != 0 && inverted != 1) {
                fail(path, key, "must be 0 or 1");
            }
            parsed.config.invertMouseY = inverted != 0;
        }
        else if (key == "mastervolume") {
            parsed.config.masterVolume = readFloat(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "uivolume") {
            parsed.config.uiVolume = readFloat(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "effectsvolume") {
            parsed.config.effectsVolume = readFloat(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "ambientvolume") {
            parsed.config.ambientVolume = readFloat(path, key, values);
            requireEnd(path, key, values);
        }
        else if (key == "uiscale") {
            parsed.config.uiScale = readFloat(path, key, values);
            requireEnd(path, key, values);
            usesVersionTwoKey = true;
        }
        else if (key == "audiocaptions") {
            const int enabled = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (enabled != 0 && enabled != 1) {
                fail(path, key, "must be 0 or 1");
            }
            parsed.config.audioCaptions = enabled != 0;
            usesVersionTwoKey = true;
        }
        else if (key == "actionhints") {
            const int enabled = readInteger(path, key, values);
            requireEnd(path, key, values);
            if (enabled != 0 && enabled != 1) {
                fail(path, key, "must be 0 or 1");
            }
            parsed.config.showActionHints = enabled != 0;
            usesVersionTwoKey = true;
        }
        else if (key == "seed") {
            std::string seedText;
            if (!(values >> seedText)) {
                fail(path, key, "must be an integer or 'random'");
            }
            requireEnd(path, key, values);
            if (seedText == "random") {
                parsed.config.worldSeed.reset();
            }
            else {
                std::istringstream seedValue(seedText);
                parsed.config.worldSeed = readInteger(path, key, seedValue);
                requireEnd(path, key, seedValue);
            }
        }
        else {
            bool bindingMatched = false;
            for (std::size_t actionIndex = 0;
                 actionIndex < GameplayActionCount; ++actionIndex) {
                const auto action =
                    static_cast<GameplayAction>(actionIndex);
                if (key != gameplayActionConfigKey(action)) {
                    continue;
                }
                std::string token;
                GameplayKey binding = GameplayKey::W;
                if (!(values >> token) ||
                    !tryParseGameplayKey(token, binding)) {
                    fail(path, key, "contains an unknown key binding");
                }
                requireEnd(path, key, values);
                parsed.config.inputBindings.set(action, binding);
                bindingMatched = true;
                usesVersionTwoKey = true;
                break;
            }
            if (!bindingMatched) {
                fail(path, key, "is unknown at line " +
                                    std::to_string(lineNumber));
            }
        }
    }

    if (!hasVersion && !allowLegacy) {
        fail(path, "settings_version", "is required");
    }
    if (hasVersion && parsed.version == LegacyRuntimeSettingsFormatVersion &&
        usesVersionTwoKey) {
        fail(path, "settings_version",
             "version 1 cannot contain version 2 settings");
    }
    parsed.needsMigration =
        !hasVersion || parsed.version < RuntimeSettingsFormatVersion;
    try {
        validateUserSettings(parsed.config);
    }
    catch (const std::exception &exception) {
        throw std::runtime_error("Invalid runtime config '" + path +
                                 "': " + exception.what() + ".");
    }
    return parsed;
}

std::vector<char> serializeRuntimeConfig(const Config &config)
{
    std::ostringstream output;
    output << std::setprecision(9)
           << "settings_version " << RuntimeSettingsFormatVersion << '\n'
           << "renderdistance " << config.renderDistance << '\n'
           << "fullscreen " << (config.isFullscreen ? 1 : 0) << '\n'
           << "windowsize " << config.windowX << ' ' << config.windowY
           << '\n'
           << "fov " << config.fov << '\n'
           << "mousesensitivity " << config.mouseSensitivity << '\n'
           << "invertmousey " << (config.invertMouseY ? 1 : 0) << '\n'
           << "mastervolume " << config.masterVolume << '\n'
           << "uivolume " << config.uiVolume << '\n'
           << "effectsvolume " << config.effectsVolume << '\n'
           << "ambientvolume " << config.ambientVolume << '\n'
           << "uiscale " << config.uiScale << '\n'
           << "audiocaptions " << (config.audioCaptions ? 1 : 0) << '\n'
           << "actionhints " << (config.showActionHints ? 1 : 0) << '\n';
    for (std::size_t actionIndex = 0;
         actionIndex < GameplayActionCount; ++actionIndex) {
        const auto action = static_cast<GameplayAction>(actionIndex);
        output << gameplayActionConfigKey(action) << ' '
               << gameplayKeyToken(config.inputBindings.get(action)) << '\n';
    }
    output << "seed ";
    if (config.worldSeed.has_value()) {
        output << *config.worldSeed;
    }
    else {
        output << "random";
    }
    output << '\n';
    const std::string text = output.str();
    return std::vector<char>(text.begin(), text.end());
}
} // namespace

void validateUserSettings(const UserSettings &settings)
{
    if (settings.renderDistance < 1 || settings.renderDistance > 32) {
        throw std::runtime_error("render distance must be between 1 and 32");
    }
    if (settings.windowX < 640 || settings.windowX > 7680 ||
        settings.windowY < 480 || settings.windowY > 4320) {
        throw std::runtime_error(
            "window size must be between 640x480 and 7680x4320");
    }
    if (settings.fov < 45 || settings.fov > 120) {
        throw std::runtime_error("FOV must be between 45 and 120 degrees");
    }
    if (!std::isfinite(settings.mouseSensitivity) ||
        settings.mouseSensitivity < 0.005f ||
        settings.mouseSensitivity > 1.f) {
        throw std::runtime_error(
            "mouse sensitivity must be between 0.005 and 1.0");
    }
    validateVolume("master volume", settings.masterVolume);
    validateVolume("UI volume", settings.uiVolume);
    validateVolume("effects volume", settings.effectsVolume);
    validateVolume("ambient volume", settings.ambientVolume);
    if (!std::isfinite(settings.uiScale) || settings.uiScale < 0.75f ||
        settings.uiScale > 1.75f) {
        throw std::runtime_error("UI scale must be between 0.75 and 1.75");
    }
    std::string bindingError;
    if (!validateGameplayInputBindings(settings.inputBindings,
                                       bindingError)) {
        throw std::runtime_error("invalid gameplay bindings: " +
                                 bindingError);
    }
}

Config loadRuntimeConfig(const std::string &path)
{
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError)) {
        if (existsError) {
            throw std::runtime_error("Unable to inspect runtime config '" +
                                     path + "'.");
        }
        Config defaults;
        std::string error;
        if (!saveRuntimeConfig(path, defaults, &error)) {
            throw std::runtime_error("Unable to write runtime config '" +
                                     path + "': " + error + ".");
        }
        return defaults;
    }

    ParsedRuntimeConfig parsed = parseRuntimeConfig(path, true);
    if (parsed.needsMigration) {
        std::string error;
        if (!saveRuntimeConfig(path, parsed.config, &error)) {
            throw std::runtime_error("Unable to migrate runtime config '" +
                                     path + "': " + error + ".");
        }
    }
    return parsed.config;
}

bool saveRuntimeConfig(const std::string &path, const Config &config,
                       std::string *error,
                       const StorageTransactionOptions &options)
{
    try {
        validateUserSettings(config);
        const std::filesystem::path parent =
            std::filesystem::path(path).parent_path();
        std::error_code directoryError;
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, directoryError);
            if (directoryError) {
                throw std::runtime_error(
                    "cannot create config directory: " +
                    directoryError.message());
            }
        }

        StorageTransactionMetrics metrics;
        const bool published = StorageTransaction::publish(
            path, serializeRuntimeConfig(config),
            [](const std::string &candidate, std::string &validationError) {
                try {
                    (void)parseRuntimeConfig(candidate, false);
                    return true;
                }
                catch (const std::exception &exception) {
                    validationError = exception.what();
                    return false;
                }
            },
            options, &metrics);
        if (!published) {
            throw std::runtime_error(metrics.error.empty()
                                         ? "atomic publication failed"
                                         : metrics.error);
        }
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    catch (const std::exception &exception) {
        if (error != nullptr) {
            *error = exception.what();
        }
        return false;
    }
}

void RuntimeSettingsSession::begin(const UserSettings &settings) noexcept
{
    m_original = settings;
    m_draft = settings;
    m_open = true;
}

bool RuntimeSettingsSession::isOpen() const noexcept
{
    return m_open;
}

UserSettings &RuntimeSettingsSession::draft() noexcept
{
    return m_draft;
}

const UserSettings &RuntimeSettingsSession::draft() const noexcept
{
    return m_draft;
}

void RuntimeSettingsSession::restoreDefaults() noexcept
{
    if (m_open) {
        m_draft = UserSettings();
    }
}

void RuntimeSettingsSession::cancel() noexcept
{
    m_draft = m_original;
    m_open = false;
}

bool RuntimeSettingsSession::prepareApply(
    RuntimeSettingsApplyPlan &plan, std::string &error) const noexcept
{
    if (!m_open) {
        error = "settings session is not open";
        return false;
    }
    try {
        validateUserSettings(m_draft);
        plan.settings = m_draft;
        plan.restartRequired =
            m_draft.windowX != m_original.windowX ||
            m_draft.windowY != m_original.windowY ||
            m_draft.isFullscreen != m_original.isFullscreen;
        plan.renderDistanceChanged =
            m_draft.renderDistance != m_original.renderDistance;
        error.clear();
        return true;
    }
    catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}

void RuntimeSettingsSession::acceptApplied() noexcept
{
    if (!m_open) {
        return;
    }
    m_original = m_draft;
    m_open = false;
}
