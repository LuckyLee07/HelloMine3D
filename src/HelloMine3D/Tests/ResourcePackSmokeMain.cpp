#include "../Util/ResourcePackResolver.h"
#include "../Ogre/StartupResourcePreflight.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include "../World/Block/BlockTextureCoordinates.h"
#include "../World/Environment/AtmosphereShaderContract.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
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

    std::string pngHeader(std::uint32_t width, std::uint32_t height)
    {
        std::string value(24, '\0');
        const unsigned char signature[8] = {
            0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au};
        for (std::size_t index = 0; index < 8; ++index)
        {
            value[index] = static_cast<char>(signature[index]);
        }
        value[11] = 13;
        value[12] = 'I';
        value[13] = 'H';
        value[14] = 'D';
        value[15] = 'R';
        const auto writeDimension = [&value](std::size_t offset,
                                             std::uint32_t dimension)
        {
            value[offset] = static_cast<char>((dimension >> 24u) & 0xffu);
            value[offset + 1] =
                static_cast<char>((dimension >> 16u) & 0xffu);
            value[offset + 2] =
                static_cast<char>((dimension >> 8u) & 0xffu);
            value[offset + 3] = static_cast<char>(dimension & 0xffu);
        };
        writeDimension(16, width);
        writeDimension(20, height);
        return value;
    }

    std::string terrainShaderInterface()
    {
        return
            "uniform float atlasPixels;\n"
            "uniform float tilePixels;\n"
            "uniform float tilesPerRow;\n"
            "uniform float colourSaturation;\n"
            "uniform float greenSuppression;\n"
            "uniform float greenRedShift;\n"
            "uniform float toneGamma;\n";
    }

    std::string terrainProfile(
        const std::string &overrides = "")
    {
        std::map<std::string, std::string> values = {
            {"atlas_texture", "media/textures/DefaultPack.png"},
            {"atlas_pixels", "256"},
            {"tile_pixels", "16"},
            {"tiles_per_row", "16"},
            {"colour_saturation", "0.62"},
            {"green_suppression", "0.22"},
            {"green_red_shift", "0.07"},
            {"tone_gamma", "1.05"},
        };
        std::istringstream changes(overrides);
        std::string change;
        while (std::getline(changes, change))
        {
            const std::size_t separator = change.find('=');
            if (separator != std::string::npos)
            {
                values[change.substr(0, separator)] =
                    change.substr(separator + 1);
            }
        }
        std::ostringstream output;
        output << "# HelloMine3D terrain material parameters v1\n";
        for (const char *key : {
                 "atlas_texture", "atlas_pixels", "tile_pixels",
                 "tiles_per_row", "colour_saturation",
                 "green_suppression", "green_red_shift", "tone_gamma"})
        {
            output << key << '=' << values[key] << '\n';
        }
        return output.str();
    }

    const std::vector<ResourcePackRequirement> &requirements()
    {
        static const std::vector<ResourcePackRequirement> value = {
            {"audio", "media/audio/Base.audio"},
            {"audio-sample", "media/audio/samples/ui-click.wav"},
            {"atlas-layout", "media/materials/Base.terrain-atlas"},
            {"block", "media/blocks/Stone.block"},
            {"enemy", "media/enemies/Base.enemy"},
            {"food", "media/foods/Base.food"},
            {"font", "media/fonts/rs.ttf"},
            {"license", "media/fonts/NotoSansSC-OFL.txt"},
            {"license", "media/audio/samples/LICENSE-HelloMine3D-Audio.txt"},
            {"license", "media/music/tracks/LICENSE-HelloMine3D-Music.txt"},
            {"license", "media/textures/LICENSE-HelloMine3D-Textures.txt"},
            {"material-profile", "media/materials/Base.terrain-material"},
            {"music", "media/music/Base.music"},
            {"music-track", "media/music/tracks/quiet-horizons.wav"},
            {"objective", "media/objectives/Base.objective"},
            {"presentation-font", "media/fonts/NotoSansSC-VF.ttf"},
            {"recipe", "media/recipes/Base.recipe"},
            {"smelting", "media/smelting/Base.smelting"},
            {"text", "media/text/en-US.text"},
            {"text", "media/text/zh-CN.text"},
            {"tool", "media/tools/Base.tool"},
            {"resource-script", "media/ogre/Test.material"},
            {"resource-script", "media/ogre/HelloMine3D.program"},
            {"runtime-template", "bin/resource-packs.txt"},
            {"shader", "media/ogre/Test.vert"},
            {"shader", "media/ogre/HelloMine3DActor.frag"},
            {"shader", "media/ogre/HelloMine3DActor.vert"},
            {"shader", "media/ogre/HelloMine3DFlora.vert"},
            {"shader", "media/ogre/HelloMine3DSkybox.frag"},
            {"shader", "media/ogre/HelloMine3DTerrain.frag"},
            {"shader", "media/ogre/HelloMine3DTerrain.vert"},
            {"shader", "media/ogre/HelloMine3DWater.frag"},
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

    TerrainMaterialParameters loadTerrainFixture(
        const fs::path &root, const std::string &profile,
        std::uint32_t width = 256, std::uint32_t height = 256,
        const std::string &shader = terrainShaderInterface())
    {
        writeFile(root / "media/materials/Base.terrain-material",
                  profile);
        writeFile(root / "media/textures/DefaultPack.png",
                  pngHeader(width, height));
        writeFile(root / "media/ogre/HelloMine3DTerrain.frag", shader);
        ResourcePackResolver resolver;
        resolver.freeze(root.string(), requirements(), {});
        return loadTerrainMaterialParameters(
            resolver.resolve(TerrainMaterialParameters::LogicalPath),
            [&resolver](const std::string &logicalPath)
            {
                return resolver.resolve(logicalPath);
            });
    }

    void writeAtmosphereFixture(const fs::path &root)
    {
        writeFile(root / "media/ogre/HelloMine3D.program",
            "param_named fogSunwardColour float3\n"
            "param_named fogDirectionalStrength float\n"
            "param_named cloudLayerEnabled float\n"
            "param_named cloudBaseHeight float\n"
            "param_named cloudThickness float\n"
            "param_named cloudHorizontalScale float\n"
            "param_named cloudVelocity float2\n"
            "param_named cloudMaxDistance float\n"
            "param_named_auto cameraPosition camera_position\n"
            "param_named_auto globalTime time 1.0\n"
            "param_named_auto legacyTime time_0_x 1.0\n");
        writeFile(root / "media/ogre/HelloMine3DSkybox.frag",
            "uniform vec3 fogSunwardColour;\n"
            "uniform float fogDirectionalStrength;\n"
            "uniform float cloudLayerEnabled;\n"
            "uniform float cloudBaseHeight;\n"
            "uniform float cloudThickness;\n"
            "uniform float cloudHorizontalScale;\n"
            "uniform vec2 cloudVelocity;\n"
            "uniform float cloudMaxDistance;\n"
            "uniform vec3 cameraPosition;\n"
            "uniform float globalTime;\n"
            "uniform float legacyTime;\n"
            "void sampleLegacyClouds() {}\n"
            "void sampleBoundedCloudLayer() {}\n");
        writeFile(root / "media/ogre/HelloMine3DTerrain.vert",
            "out vec3 terrainWorldPosition;\nuniform mat4 world;\n");
        writeFile(root / "media/ogre/HelloMine3DFlora.vert",
            "out vec3 terrainWorldPosition;\nuniform mat4 world;\n");
        writeFile(root / "media/ogre/HelloMine3DTerrain.frag",
            terrainShaderInterface() +
            "in vec3 terrainWorldPosition;\n"
            "uniform vec3 fogSunwardColour;\n"
            "uniform vec3 sunDirection;\n"
            "uniform float fogDirectionalStrength;\n"
            "uniform vec3 cameraPosition;\n"
            "vec3 directionalFogColour() {}\n");
        writeFile(root / "media/ogre/HelloMine3DWater.frag",
            "uniform vec3 fogSunwardColour;\n"
            "uniform float fogDirectionalStrength;\n"
            "vec3 directionalFogColour() {}\n");
        writeFile(root / "media/ogre/HelloMine3DActor.vert",
            "out vec3 actorWorldPosition;\nuniform mat4 world;\n");
        writeFile(root / "media/ogre/HelloMine3DActor.frag",
            "in vec3 actorWorldPosition;\n"
            "uniform vec3 fogSunwardColour;\n"
            "uniform vec3 sunDirection;\n"
            "uniform float fogDirectionalStrength;\n"
            "uniform vec3 cameraPosition;\n"
            "vec3 directionalFogColour() {}\n");
    }

    void caseAtmosphereShaderContract()
    {
        {
            const fs::path root = freshRoot("v10c-atmosphere-valid");
            writeAtmosphereFixture(root);
            ResourcePackResolver resolver;
            resolver.freeze(root.string(), requirements(), {});
            bool passed = false;
            try
            {
                validateAtmosphereShaderContract(resolver);
                passed = true;
            }
            catch (...)
            {
            }
            check("V10C/atmosphere-shader-interface-valid", passed);
        }
        {
            const fs::path root = freshRoot(
                "v10c-atmosphere-invalid-pack");
            writeAtmosphereFixture(root);
            const fs::path pack = createPack(
                root, "invalid-atmosphere", "Invalid Atmosphere", 1,
                {{"media/ogre/HelloMine3DSkybox.frag",
                  "uniform float cloudLayerEnabled;\n"}});
            ResourcePackResolver resolver;
            resolver.freeze(root.string(), requirements(), {pack.string()});
            check("V10C/reject-atmosphere-interface-drift",
                  throwsContaining(
                      [&]
                      {
                          validateAtmosphereShaderContract(resolver);
                      },
                      "missing interface declaration"));
        }
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
        std::map<std::string, int> categoryOccurrences;
        for (const ResourcePackRequirement &requirement : requirements())
        {
            if (requirement.category == "runtime-template" ||
                requirement.category == "atlas-layout" ||
                requirement.category == "audio" ||
                requirement.category == "audio-sample" ||
                requirement.category == "music" ||
                requirement.category == "music-track" ||
                requirement.category == "enemy" ||
                requirement.category == "food" ||
                requirement.category == "objective" ||
                requirement.category == "presentation-font" ||
                requirement.category == "recipe" ||
                requirement.category == "smelting" ||
                requirement.category == "text" ||
                requirement.category == "license" ||
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
            const int occurrence = ++categoryOccurrences[requirement.category];
            const std::string checkId =
                "X2/override-class-" + requirement.category +
                (occurrence == 1
                     ? ""
                     : "-" + fs::path(requirement.logicalPath)
                                   .filename().string());
            check(checkId,
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
            const fs::path root = freshRoot("enemy-override");
            const fs::path pack = createPack(
                root, "enemy", "Enemy Override", 1,
                {{"media/enemies/Base.enemy", "override\n"}});
            check("N4/reject-unversioned-enemy-override",
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
            const fs::path root = freshRoot("food-override");
            const fs::path pack = createPack(
                root, "food", "Food Override", 1,
                {{"media/foods/Base.food", "override\n"}});
            check("N3/reject-unversioned-food-override",
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
            const fs::path root = freshRoot("objective-override");
            const fs::path pack = createPack(
                root, "objective", "Objective Override", 1,
                {{"media/objectives/Base.objective", "override\n"}});
            check("N1/reject-unversioned-objective-override",
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
            const fs::path root = freshRoot("smelting-override");
            const fs::path pack = createPack(
                root, "smelting", "Smelting Override", 1,
                {{"media/smelting/Base.smelting", "override\n"}});
            check("N2/reject-unversioned-smelting-override",
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
            const fs::path root = freshRoot("audio-sample-override");
            const fs::path pack = createPack(
                root, "audio-sample", "Audio Sample Override", 1,
                {{"media/audio/samples/ui-click.wav", "override\n"}});
            check("N12B/reject-unversioned-audio-sample-override",
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
            const fs::path root = freshRoot("text-override");
            const fs::path pack = createPack(
                root, "text", "Text Override", 1,
                {{"media/text/en-US.text", "override\n"}});
            check("N7A/reject-unversioned-text-override",
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
            const fs::path root = freshRoot("music-override");
            const fs::path pack = createPack(
                root, "music", "Music Override", 1,
                {{"media/music/Base.music", "override\n"}});
            check("N12C/reject-unversioned-music-override",
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
            const fs::path root = freshRoot("music-track-override");
            const fs::path pack = createPack(
                root, "music-track", "Music Track Override", 1,
                {{"media/music/tracks/quiet-horizons.wav",
                  "override\n"}});
            check("N12C/reject-unversioned-music-track-override",
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
            const fs::path root = freshRoot("presentation-font-override");
            const fs::path pack = createPack(
                root, "font", "Presentation Font Override", 1,
                {{"media/fonts/NotoSansSC-VF.ttf", "override\n"}});
            check("N12A/reject-unversioned-presentation-font-override",
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
            const fs::path root = freshRoot("font-license-override");
            const fs::path pack = createPack(
                root, "license", "Font License Override", 1,
                {{"media/fonts/NotoSansSC-OFL.txt", "override\n"}});
            check("N12A/reject-unversioned-font-license-override",
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

        const fs::path sampleRoot = freshRoot("optional-audio-sample");
        const fs::path sample =
            sampleRoot / "media/audio/samples/ui-click.wav";
        fs::remove(sample);
        ResourcePackResolver sampleResolver;
        bool sampleFrozen = false;
        bool samplePreflightPassed = false;
        try
        {
            sampleResolver.freeze(sampleRoot.string(), requirements(), {});
            sampleFrozen = sampleResolver.isFrozen();
            std::vector<StartupResourceRequirement> startupRequirements;
            for (const ResourcePackRequirement &requirement : requirements())
            {
                startupRequirements.push_back(
                    {requirement.category, requirement.logicalPath});
            }
            validateStartupResources(sampleRoot.string(),
                                     startupRequirements);
            samplePreflightPassed = true;
        }
        catch (...)
        {
        }
        check("N12B/missing-sample-keeps-resource-view-and-preflight-loadable",
              sampleFrozen && samplePreflightPassed &&
                  sampleResolver.resolve(
                      "media/audio/samples/ui-click.wav") ==
                      sample.generic_string());
    }

    void caseTerrainMaterialProfile()
    {
        {
            const fs::path root = freshRoot("terrain-material-valid");
            const TerrainMaterialParameters profile = loadTerrainFixture(
                root, terrainProfile());
            const auto first = BlockTextureCoordinates::get(0, 0, profile);
            const auto last = BlockTextureCoordinates::get(15, 15, profile);
            check("V10B1/default-profile-is-pixel-compatible",
                  profile.atlasPixels == 256 &&
                      profile.tilePixels == 16 &&
                      profile.tilesPerRow == 16 &&
                      std::abs(profile.colourSaturation - 0.62f) < 0.000001f &&
                      std::abs(profile.greenSuppression - 0.22f) < 0.000001f &&
                      std::abs(profile.greenRedShift - 0.07f) < 0.000001f &&
                      std::abs(profile.toneGamma - 1.05f) < 0.000001f &&
                      std::abs(first[0] - 0.060546875f) < 0.000001f &&
                      std::abs(first[2] - 0.001953125f) < 0.000001f &&
                      std::abs(last[0] - 0.998046875f) < 0.000001f &&
                      std::abs(last[2] - 0.939453125f) < 0.000001f);
            check("V10B1/profile-bounds-atlas-tiles",
                  profile.containsTile(0, 0) &&
                      profile.containsTile(15, 15) &&
                      !profile.containsTile(-1, 0) &&
                      !profile.containsTile(16, 0));
        }
        {
            const fs::path root = freshRoot("terrain-material-scaled");
            const TerrainMaterialParameters profile = loadTerrainFixture(
                root,
                terrainProfile("atlas_pixels=512\ntile_pixels=32"),
                512, 512);
            const auto first = BlockTextureCoordinates::get(0, 0, profile);
            check("V10B1/scaled-atlas-uses-profile-pixel-centres",
                  profile.atlasPixels == 512 &&
                      profile.tilePixels == 32 &&
                      profile.tilesPerRow == 16 &&
                      std::abs(first[0] - 0.0615234375f) < 0.000001f &&
                      std::abs(first[2] - 0.0009765625f) < 0.000001f);
        }
        {
            const fs::path root = freshRoot("terrain-material-missing-key");
            std::string profile = terrainProfile();
            const std::string line = "tone_gamma=1.05\n";
            profile.erase(profile.find(line), line.size());
            check("V10B1/reject-missing-parameter",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(root, profile);
                      },
                      "key 'tone_gamma' is missing"));
        }
        {
            const fs::path root = freshRoot("terrain-material-duplicate");
            check("V10B1/reject-duplicate-parameter",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root, terrainProfile() +
                                        "tone_gamma=1.05\n");
                      },
                      "key 'tone_gamma' is duplicated"));
        }
        {
            const fs::path root = freshRoot("terrain-material-unknown");
            check("V10B1/reject-unknown-parameter",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root, terrainProfile() + "legacy_tint=1\n");
                      },
                      "key 'legacy_tint' is unknown"));
        }
        {
            const fs::path root = freshRoot("terrain-material-division");
            check("V10B1/reject-noninteger-atlas-division",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root,
                              terrainProfile("tile_pixels=15"));
                      },
                      "atlas_pixels must be evenly divisible"));
        }
        {
            const fs::path root = freshRoot("terrain-material-row-count");
            check("V10B1/reject-inconsistent-row-count",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root,
                              terrainProfile("tiles_per_row=8"));
                      },
                      "tiles_per_row must equal"));
        }
        {
            const fs::path root = freshRoot("terrain-material-colour-range");
            check("V10B1/reject-colour-parameter-out-of-range",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root,
                              terrainProfile("green_suppression=1.1"));
                      },
                      "green_suppression' is outside [0, 1]"));
        }
        {
            const fs::path root = freshRoot("terrain-material-gamma-range");
            check("V10B1/reject-gamma-out-of-range",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root, terrainProfile("tone_gamma=0.49"));
                      },
                      "tone_gamma' is outside [0.5, 2]"));
        }
        {
            const fs::path root = freshRoot("terrain-material-size");
            check("V10B1/reject-atlas-size-mismatch",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root, terrainProfile(), 128, 256);
                      },
                      "expected 256x256 pixels, got 128x256"));
        }
        {
            const fs::path root = freshRoot("terrain-material-shader");
            check("V10B1/reject-shader-interface-drift",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainFixture(
                              root, terrainProfile(), 256, 256,
                              "uniform float atlasPixels;\n");
                      },
                      "missing interface declaration"));
        }
        {
            const fs::path root = freshRoot("terrain-material-missing-atlas");
            writeFile(root / "media/materials/Base.terrain-material",
                      terrainProfile());
            writeFile(root / "media/ogre/HelloMine3DTerrain.frag",
                      terrainShaderInterface());
            fs::remove(root / "media/textures/DefaultPack.png");
            check("V10B1/reject-missing-atlas",
                  throwsContaining(
                      [&]
                      {
                          loadTerrainMaterialParameters(
                              (root /
                               "media/materials/Base.terrain-material")
                                  .generic_string(),
                              [&root](const std::string &logicalPath)
                              {
                                  return (root / fs::path(logicalPath))
                                      .generic_string();
                              });
                      },
                      "Missing terrain atlas resource"));
        }
        {
            const fs::path root = freshRoot("terrain-material-override");
            writeFile(root / "media/materials/Base.terrain-material",
                      terrainProfile());
            writeFile(root / "media/textures/DefaultPack.png",
                      pngHeader(256, 256));
            writeFile(root / "media/ogre/HelloMine3DTerrain.frag",
                      terrainShaderInterface());
            const fs::path pack = createPack(
                root, "terrain", "Terrain Profile", 1,
                {{TerrainMaterialParameters::LogicalPath,
                  terrainProfile("colour_saturation=0.5")}});
            ResourcePackResolver resolver;
            resolver.freeze(root.string(), requirements(),
                            {pack.string()});
            const TerrainMaterialParameters profile =
                loadTerrainMaterialParameters(
                    resolver.resolve(
                        TerrainMaterialParameters::LogicalPath),
                    [&resolver](const std::string &logicalPath)
                    {
                        return resolver.resolve(logicalPath);
                    });
            check("V10B1/resource-pack-profile-override-is-effective",
                  std::abs(profile.colourSaturation - 0.5f) < 0.000001f &&
                      resolver.resolve(
                          TerrainMaterialParameters::LogicalPath) ==
                          (pack /
                           TerrainMaterialParameters::LogicalPath)
                              .generic_string());
        }
        {
            const fs::path root = freshRoot("terrain-atlas-layout-missing");
            fs::remove(root / "media/materials/Base.terrain-atlas");
            ResourcePackResolver resolver;
            check("V10B2/reject-missing-atlas-layout",
                  throwsContaining(
                      [&]
                      {
                          resolver.freeze(root.string(), requirements(), {});
                      },
                      "Missing or empty effective atlas-layout resource"));
        }
        {
            const fs::path root = freshRoot("terrain-atlas-layout-override");
            const fs::path pack = createPack(
                root, "layout", "Layout Override", 1,
                {{"media/materials/Base.terrain-atlas", "override\n"}});
            ResourcePackResolver resolver;
            check("V10B2/resource-pack-cannot-desynchronise-atlas-layout",
                  throwsContaining(
                      [&]
                      {
                          resolver.freeze(root.string(), requirements(),
                                          {pack.string()});
                      },
                      "stale or unsupported override"));
        }
    }

    void caseOptionalPresentationFont()
    {
        const fs::path root = freshRoot("optional-presentation-font");
        const fs::path font = root / "media/fonts/NotoSansSC-VF.ttf";
        fs::remove(font);
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
        check("N12A/missing-presentation-font-keeps-view-loadable",
              frozen && resolver.isFrozen() &&
                  resolver.resolve("media/fonts/NotoSansSC-VF.ttf") ==
                      font.generic_string());

        bool preflightPassed = false;
        try
        {
            std::vector<StartupResourceRequirement> startupRequirements;
            for (const ResourcePackRequirement& requirement : requirements())
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
        check("N12A/missing-presentation-font-keeps-preflight-loadable",
              preflightPassed);
    }

    void caseOptionalMusic()
    {
        for (const std::string logicalPath : {
                 std::string("media/music/Base.music"),
                 std::string("media/music/tracks/quiet-horizons.wav")})
        {
            const fs::path root = freshRoot(
                logicalPath.find("Base.music") != std::string::npos
                    ? "optional-music-definition"
                    : "optional-music-track");
            const fs::path missing = root / fs::path(logicalPath);
            fs::remove(missing);
            ResourcePackResolver resolver;
            bool frozen = false;
            bool preflightPassed = false;
            try
            {
                resolver.freeze(root.string(), requirements(), {});
                frozen = resolver.isFrozen();
                std::vector<StartupResourceRequirement> startupRequirements;
                for (const ResourcePackRequirement &requirement : requirements())
                {
                    startupRequirements.push_back(
                        {requirement.category, requirement.logicalPath});
                }
                validateStartupResources(root.string(),
                                         startupRequirements);
                preflightPassed = true;
            }
            catch (...)
            {
            }
            check(logicalPath.find("Base.music") != std::string::npos
                      ? "N12C/missing-music-definition-keeps-startup-loadable"
                      : "N12C/missing-music-track-keeps-startup-loadable",
                  frozen && preflightPassed &&
                      resolver.resolve(logicalPath) == missing.generic_string());
        }
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
    std::cout << std::unitbuf;
    caseNoPackAndPrecedence();
    caseEveryAllowedClass();
    caseInvalidPacks();
    caseTerrainMaterialProfile();
    caseAtmosphereShaderContract();
    caseOptionalAudio();
    caseOptionalMusic();
    caseOptionalPresentationFont();
    caseFrozenManifest();
    std::cout << "[RESOURCE_PACK_TEST] checks=" << checks
              << " failures=" << failures << '\n';
    std::cout << "[RESOURCE_PACK_TEST] status="
              << (failures == 0 ? "PASS" : "FAIL") << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
