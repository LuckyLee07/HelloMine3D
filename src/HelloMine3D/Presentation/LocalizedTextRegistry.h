#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class ResourcePackResolver;

struct LocalizedTextSource
{
    std::string name;
    std::string content;
};

/// Immutable semantic-text catalogue. The default en-US catalogue is
/// authoritative; unknown locales and incomplete translations safely fall
/// back to it while recording bounded development diagnostics.
class LocalizedTextRegistry
{
  public:
    static constexpr std::size_t MaxSourceBytes = 256 * 1024;
    static constexpr std::size_t MaxLocales = 16;
    static constexpr std::size_t MaxKeysPerLocale = 1024;
    static constexpr std::size_t MaxTextBytes = 1024;
    static constexpr std::size_t MaxDiagnostics = 128;

    void freeze(std::vector<LocalizedTextSource> sources,
                bool requireKeyParity = true);
    void freezeFromResourceView(const ResourcePackResolver& resolver);

    bool isFrozen() const noexcept;
    bool hasLocale(const std::string& locale) const noexcept;
    bool hasKey(const std::string& locale,
                const std::string& key) const noexcept;
    std::vector<std::string> keys(const std::string& locale) const;

    std::string lookup(const std::string& locale,
                       const std::string& key) const;
    const std::vector<std::string>& diagnostics() const noexcept;

    static bool isCanonicalLocale(const std::string& value) noexcept;
    static bool isCanonicalKey(const std::string& value) noexcept;

  private:
    using Catalogue = std::unordered_map<std::string, std::string>;

    void diagnose(const std::string& message) const;

    std::unordered_map<std::string, Catalogue> m_catalogues;
    mutable std::vector<std::string> m_diagnostics;
    bool m_frozen = false;
};

LocalizedTextRegistry& runtimeLocalizedTextRegistry();
void ensureRuntimeLocalizedTextRegistry();
