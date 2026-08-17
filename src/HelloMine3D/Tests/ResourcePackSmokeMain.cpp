#include "../Util/ResourcePackResolver.h"
#include "../Ogre/StartupResourcePreflight.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    int checks = 0;
    int failures = 0;

    void check(const std::string &id, bool passed,
               const std::string &detail = "")
    {
        ++checks;
        if (!passed)
        {
            ++failures;
        }
        std::cout << "[RESOURCE_PACK_TEST] "
                  << (passed ? "PASS " : "FAIL ") << id;
        if (!detail.empty())
        {
            std::cout << " :: " << detail;
        }
        std::cout << '\n';
    }

    void writeFile(const fs::path &path, const std::string &content)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << content;
    }

    const std::vector<ResourcePackRequirement> &requirements()
    {
        static const std::vector<ResourcePackRequirement> value = {
            {"audio", "media/audio/Base.audio"},
            {"block", "media/blocks/Stone.block"},
            {"font", "media/fonts/rs.ttf"},
            {"recipe", "media/recipes/Base.recipe"},
            {"tool", "media/tools/Base.tool"},
            {"resource-script", "media/ogre/Test.material"},
            {"runtime-template", "bin/resource-packs.txt"},
            {"shader", "media/ogre/Test.vert"},
            {"shape", "media/shapes/Cross.shape"},
            {"texture", "media/textures/DefaultPack.png"},
        };
        return value;
    }

    fs::path freshRoot(const std::string &name)
    {
        const fs::path root = fs::current_path() / "bin" /
                              "validation_runs" / "resource_packs" / name;
        std::error_code error;
        fs::remove_all(root, error);
        for (const ResourcePackRequirement &requirement : requirements())
        {
            writeFile(root / fs::path(requirement.logicalPath),
                      "base:" + requirement.logicalPath + "\n");
        }
        return root;
    }

    fs::path createPack(
        const fs::path &root, const std::string &directory,
        const std::string &name, int format,
        const std::vector<std::pair<std::string, std::string>> &overrides)
    {
        const fs::path pack = root / "packs" / directory;
        writeFile(pack / "pack.meta",
                  "# HelloMine3D resource pack v1\nname=" + name +
                      "\nformat=" + std::to_string(format) + "\n");
        for (const auto &overrideFile : overrides)
        {
            writeFile(pack / fs::path(overrideFile.first),
                      overrideFile.second);
        }
        return pack;
    }

    bool throwsContaining(const std::function<void()> &action,
                          const std::string &expected)
    {
        try
        {
            action();
        }
        catch (const std::exception &error)
        {
            return std::string(error.what()).find(expected) !=
                   std::string::npos;
        }
        return false;
    }

    void caseNoPackAndPrecedence()
    {
        const fs::path root = freshRoot("precedence");
        ResourcePackResolver base;
        base.freeze(root.string(), requirements(), {});
        check("X1/no-pack-uses-base",
              base.packs().empty() && base.overrideCount() == 0 &&
                  base.resolve("media/blocks/Stone.block") ==
                      (root / "media/blocks/Stone.block").generic_string());

        const fs::path low = createPack(
            root, "low", "Low Priority", 1,
            {{"media/textures/DefaultPack.png", "low\n"}});
        const fs::path high = createPack(
            root, "high", "High Priority", 1,
            {{"media/textures/DefaultPack.png", "high\n"},
             {"media/ogre/Test.vert", "high shader\n"}});
        ResourcePackResolver resolver;
        resolver.freeze(root.string(), requirements(),
                        {high.string(), low.string()});
        check("X1/enabled-order-is-deterministic",
              resolver.resolve("media/textures/DefaultPack.png") ==
                  (high / "media/textures/DefaultPack.png")
                      .generic_string());
        check("X1/unoverridden-resource-falls-back",
              resolver.resolve("media/fonts/rs.ttf") ==
                  (root / "media/fonts/rs.ttf").generic_string());
        const std::vector<std::string> ogreDirectories =
            resolver.resourceDirectories("media/ogre");
        check("X2/ogre-locations-follow-effective-order",
              ogreDirectories.size() == 2 &&
                  ogreDirectories.front() ==
                      (root / "media/ogre").generic_string() &&
                  ogreDirectories.back() ==
                      (high / "media/ogre").generic_string());
    }

    void caseEveryAllowedClass()
    {
        const fs::path root = freshRoot("classes");
        for (const ResourcePackRequirement &requirement : requirements())
        {
            if (requirement.category == "runtime-template" ||
                requirement.category == "audio" ||
                requirement.category == "recipe" ||
                requirement.category == "tool")
            {
                continue;
            }
            const std::string directory = "only-" + requirement.category;
            const fs::path pack = createPack(
                root, directory, "Only " + requirement.category, 1,
                {{requirement.logicalPath, "override\n"}});
            ResourcePackResolver resolver;
            resolver.freeze(root.string(), requirements(), {pack.string()});
            const bool selected =
                resolver.resolve(requirement.logicalPath) ==
                (pack / fs::path(requirement.logicalPath)).generic_string();
            const bool fallback =
                resolver.resolve("bin/resource-packs.txt") ==
                (root / "bin/resource-packs.txt").generic_string();
            check("X2/override-class-" + requirement.category,
                  selected && fallback);
        }
    }

    void caseInvalidPacks()
    {
        {
            const fs::path root = freshRoot("bad-version");
            const fs::path pack = createPack(
                root, "bad", "Bad Version", 2,
                {{"media/blocks/Stone.block", "override\n"}});
            check("X1/reject-incompatible-version",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "Incompatible resource-pack format"));
        }
        {
            const fs::path root = freshRoot("traversal");
            check("X1/reject-relative-path-escape",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {"../escape"});
                      },
                      "must not contain path separators or traversal"));
        }
        {
            const fs::path root = freshRoot("stale");
            const fs::path pack = createPack(
                root, "stale", "Stale Override", 1,
                {{"media/textures/not-in-manifest.png", "stale\n"}});
            check("X3/reject-stale-override",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "stale or unsupported override"));
        }
        {
            const fs::path root = freshRoot("empty");
            const fs::path pack = createPack(
                root, "empty", "Empty Override", 1,
                {{"media/fonts/rs.ttf", ""}});
            check("X2/reject-empty-override-with-owner",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "Empty Override' override 'media/fonts/rs.ttf' is empty"));
        }
        {
            const fs::path root = freshRoot("duplicate-name");
            const fs::path first = createPack(
                root, "one", "Same Name", 1,
                {{"media/fonts/rs.ttf", "one\n"}});
            const fs::path second = createPack(
                root, "two", "Same Name", 1,
                {{"media/shapes/Cross.shape", "two\n"}});
            check("X1/reject-duplicate-pack-name",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {first.string(), second.string()});
                      },
                      "Duplicate enabled resource-pack name"));
        }
        {
            const fs::path root = freshRoot("recipe-override");
            const fs::path pack = createPack(
                root, "recipe", "Recipe Override", 1,
                {{"media/recipes/Base.recipe", "override\n"}});
            check("G1/reject-unversioned-recipe-override",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "stale or unsupported override"));
        }
        {
            const fs::path root = freshRoot("tool-override");
            const fs::path pack = createPack(
                root, "tool", "Tool Override", 1,
                {{"media/tools/Base.tool", "override\n"}});
            check("G3/reject-unversioned-tool-override",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "stale or unsupported override"));
        }
        {
            const fs::path root = freshRoot("audio-override");
            const fs::path pack = createPack(
                root, "audio", "Audio Override", 1,
                {{"media/audio/Base.audio", "override\n"}});
            check("G5/reject-unversioned-audio-override",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "stale or unsupported override"));
        }
        {
            const fs::path root = freshRoot("missing-base");
            fs::remove(root / "media/ogre/Test.vert");
            check("X2/reject-missing-effective-resource",
                  throwsContaining(
                      [&]
                      {
                          ResourcePackResolver resolver;
                          resolver.freeze(root.string(), requirements(), {});
                      },
                      "Missing or empty effective shader resource"));
        }
    }

    void caseOptionalAudio()
    {
        const fs::path root = freshRoot("optional-audio");
        const fs::path audio = root / "media/audio/Base.audio";
        fs::remove(audio);
        ResourcePackResolver resolver;
        bool frozen = false;
        try
        {
            resolver.freeze(root.string(), requirements(), {});
            frozen = true;
        }
        catch (...)
        {
        }
        check("G5/missing-audio-keeps-effective-view-loadable",
              frozen && resolver.isFrozen() &&
                  resolver.resolve("media/audio/Base.audio") ==
                      audio.generic_string());

        bool preflightPassed = false;
        try
        {
            std::vector<StartupResourceRequirement> startupRequirements;
            for (const ResourcePackRequirement &requirement : requirements())
            {
                startupRequirements.push_back(
                    {requirement.category, requirement.logicalPath});
            }
            validateStartupResources(root.string(), startupRequirements);
            preflightPassed = true;
        }
        catch (...)
        {
        }
        check("G5/missing-audio-does-not-fail-startup-preflight",
              preflightPassed);
    }

    void caseFrozenManifest()
    {
        const fs::path root = freshRoot("manifest");
        const fs::path pack = createPack(
            root, "one", "Manifest Pack", 1,
            {{"media/blocks/Stone.block", "override\n"}});
        ResourcePackResolver resolver;
        resolver.freeze(root.string(), requirements(), {pack.string()});
        const std::string manifest = resolver.effectiveManifest();
        std::vector<std::string> entries;
        std::string line;
        std::istringstream input(manifest);
        while (std::getline(input, line))
        {
            if (!line.empty() && line.front() != '#')
            {
                entries.push_back(line);
            }
        }
        check("X3/effective-manifest-sorted-and-owned",
              std::is_sorted(entries.begin(), entries.end()) &&
                  manifest.find("block|media/blocks/Stone.block|Manifest Pack\n") !=
                      std::string::npos &&
                  manifest.find("font|media/fonts/rs.ttf|base\n") !=
                      std::string::npos);
        check("X2/effective-view-freezes-once",
              throwsContaining(
                  [&]
                  {
                      resolver.freeze(root.string(), requirements(), {});
                  },
                  "already frozen"));
    }
}

int main()
{
    caseNoPackAndPrecedence();
    caseEveryAllowedClass();
    caseInvalidPacks();
    caseOptionalAudio();
    caseFrozenManifest();
    std::cout << "[RESOURCE_PACK_TEST] checks=" << checks
              << " failures=" << failures << '\n';
    std::cout << "[RESOURCE_PACK_TEST] status="
              << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
