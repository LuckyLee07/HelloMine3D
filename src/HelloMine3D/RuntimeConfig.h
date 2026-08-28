#ifndef RUNTIMECONFIG_H_INCLUDED
#define RUNTIMECONFIG_H_INCLUDED

#include <string>

#include "Config.h"
#include "World/Storage/StorageTransaction.h"

constexpr int RuntimeSettingsFormatVersion = 6;
constexpr int PreviousRuntimeSettingsFormatVersion = 5;
constexpr int DirectionalShadowRuntimeSettingsFormatVersion = 5;
constexpr int MusicRuntimeSettingsFormatVersion = 4;
constexpr int LocaleRuntimeSettingsFormatVersion = 3;
constexpr int AccessibilityRuntimeSettingsFormatVersion = 2;
constexpr int LegacyRuntimeSettingsFormatVersion = 1;

struct RuntimeSettingsApplyPlan {
    UserSettings settings;
    bool restartRequired = false;
    bool renderDistanceChanged = false;
    bool directionalShadowQualityChanged = false;
    bool postProcessingQualityChanged = false;
};

class RuntimeSettingsSession {
  public:
    void begin(const UserSettings &settings) noexcept;
    bool isOpen() const noexcept;
    UserSettings &draft() noexcept;
    const UserSettings &draft() const noexcept;
    void restoreDefaults() noexcept;
    void cancel() noexcept;
    bool prepareApply(RuntimeSettingsApplyPlan &plan,
                      std::string &error) const noexcept;
    void acceptApplied() noexcept;

  private:
    UserSettings m_original;
    UserSettings m_draft;
    bool m_open = false;
};

/// Throws std::runtime_error when a user-controlled setting is out of range.
void validateUserSettings(const UserSettings &settings);

/// Loads the per-user runtime configuration, writing documented defaults when
/// the file does not exist and atomically migrating legacy unversioned files.
Config loadRuntimeConfig(const std::string &path);

/// Atomically publishes a fully validated versioned runtime configuration.
bool saveRuntimeConfig(
    const std::string &path, const Config &config,
    std::string *error = nullptr,
    const StorageTransactionOptions &options = {});

#endif // RUNTIMECONFIG_H_INCLUDED
