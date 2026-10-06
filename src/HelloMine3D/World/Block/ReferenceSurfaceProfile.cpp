#include "ReferenceSurfaceProfile.h"
#include "TerrainTextureArray.h"
#include "../../Util/ResourcePackResolver.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace {
std::string trim(std::string value)
{
    const auto space = [](unsigned char c) { return std::isspace(c); };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), space).base(), value.end());
    return value;
}
[[noreturn]] void fail(const std::string &path, const std::string &why)
{
    throw std::runtime_error("Invalid reference surface profile '" + path + "': " + why + ".");
}
constexpr const char *Names[] = {"colour", "normal", "surface"};
constexpr const char *Textures[] = {
    "media/textures/ReferenceColour64.hmt",
    "media/textures/ReferenceNormal64.hmt",
    "media/textures/ReferenceSurface64.hmt"};
}
ReferenceSurfaceProfile loadReferenceSurfaceProfile(
    const std::string &path, const TerrainResourceResolver &resolve)
{
    if (!resolve) throw std::invalid_argument("Reference surface resolver must be callable.");
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input,line) ||
        trim(line) != "# HelloMine3D reference surface profile v1")
        fail(path,"unsupported or missing header");
    std::set<std::string> keys{"edge","mips"};
    for (const auto *name : Names)
    {
        keys.insert(std::string(name)+"_texture");
        keys.insert(std::string(name)+"_hash");
    }
    std::map<std::string,std::string> values;
    while (std::getline(input,line))
    {
        line=trim(line);
        if (line.empty() || line.front()=='#') continue;
        const auto separator=line.find('=');
        if (separator==std::string::npos) fail(path,"expected key=value");
        const auto key=trim(line.substr(0,separator));
        const auto value=trim(line.substr(separator+1));
        if (!keys.count(key) || value.empty()) fail(path,"unknown/empty key '"+key+"'");
        if (!values.emplace(key,value).second) fail(path,"duplicate key '"+key+"'");
    }
    for (const auto &key : keys)
        if (!values.count(key)) fail(path,"missing key '"+key+"'");
    if (values["edge"]!="64" || values["mips"]!="7")
        fail(path,"requires 64-pixel, 256-layer, seven-mip arrays");
    ReferenceSurfaceProfile result;
    for (unsigned channel=0;channel<3;++channel)
    {
        const std::string prefix=Names[channel];
        if (values[prefix+"_texture"]!=Textures[channel])
            fail(path,"unsupported path for "+prefix);
        const auto &hash=values[prefix+"_hash"];
        if (hash.size()!=16 || !std::all_of(hash.begin(),hash.end(),
            [](unsigned char c){return std::isxdigit(c)!=0;}))
            fail(path,"hash must be exactly sixteen hexadecimal digits: "+prefix);
        result.textures[channel]=Textures[channel];
        result.contentHashes[channel]=std::stoull(hash,nullptr,16);
        const auto array=TerrainTextureArray::load(resolve(result.textures[channel]));
        if (array.edge!=ReferenceSurfaceProfile::Edge ||
            array.layers!=ReferenceSurfaceProfile::Layers ||
            array.mipCount!=ReferenceSurfaceProfile::Mips)
            fail(path,"layer/mip mismatch: "+prefix);
        if (array.contentHash!=result.contentHashes[channel])
            fail(path,"profile/content identity mismatch: "+prefix);
        if (channel==1)
        {
            for (std::size_t i=0;i<array.rgba.size();i+=4)
            {
                const float x=float(array.rgba[i])/127.5f-1.f;
                const float y=float(array.rgba[i+1])/127.5f-1.f;
                const float z=float(array.rgba[i+2])/127.5f-1.f;
                const float squared=x*x+y*y+z*z;
                if (squared<.96f || squared>1.04f || z<=0.f || array.rgba[i+3]!=255)
                    fail(path,"normal mip must contain normalized positive-Z vectors");
            }
        }
        if (channel==2)
        {
            for (std::size_t i=0;i<array.rgba.size();i+=4)
                if (array.rgba[i]<51 || array.rgba[i+3]!=255)
                    fail(path,"surface requires roughness >= .2 and reserved alpha=1");
        }
    }
    return result;
}
void RuntimeReferenceSurfaceProfile::freezeFromResourceView(const ResourcePackResolver &resolver)
{
    if (m_frozen) throw std::logic_error("Reference surface profile is already frozen.");
    const auto owner=[&resolver](const std::string &path) -> std::optional<std::string> {
        for (const auto &resource:resolver.effectiveResources())
            if (resource.logicalPath==path) return resource.packName;
        return std::nullopt;
    };
    const auto profileOwner=owner(ReferenceSurfaceProfile::LogicalPath);
    std::size_t declared=profileOwner ? 1u : 0u;
    for (const char *texture:Textures) declared+=owner(texture).has_value();
    if (!declared) {
        // Exact old-format distributions declare none of this independent
        // interface. Missing members of a declared new set remain errors.
        m_selectionReason="complete-legacy-resource-set";
        m_frozen=true;
        return;
    }
    if (!profileOwner || declared!=4u)
        fail(ReferenceSurfaceProfile::LogicalPath,"partial declared surface interface");
    for (const char *texture : Textures)
        if (owner(texture)!=profileOwner)
            fail(ReferenceSurfaceProfile::LogicalPath,"all three channels and profile must share one resource owner");
    auto parsed=loadReferenceSurfaceProfile(resolver.resolve(ReferenceSurfaceProfile::LogicalPath),
        [&resolver](const std::string &path){return resolver.resolve(path);});
    const auto priority=[&resolver](const std::string& name) {
        if (name.empty()) return std::size_t(0);
        const auto& packs=resolver.packs();
        for (std::size_t i=0;i<packs.size();++i)
            if (packs[i].name==name) return packs.size()-i;
        throw std::logic_error("Effective surface owner is absent from frozen resource packs.");
    };
    const auto surfacePriority=priority(*profileOwner);
    bool legacyOverride=false;
    // A higher-priority old-format colour/profile/shader override must remain
    // visible as one legacy set. A complete explicit new set can override a
    // lower-priority legacy set; its own four members are still inseparable.
    for (const char* path:{TerrainMaterialParameters::LogicalPath,
                           TerrainMaterialParameters::DefaultArrayLogicalPath,
                           TerrainMaterialParameters::DefaultAtlasLogicalPath,
                           TerrainMaterialParameters::TerrainShaderLogicalPath,
                           "media/ogre/HelloMine3DTerrainShadow.frag",
                           "media/ogre/HelloMine3D.program",
                           "media/ogre/HelloMine3D.material"}) {
        const auto legacyOwner=owner(path);
        if (legacyOwner && priority(*legacyOwner)>surfacePriority) legacyOverride=true;
    }
    m_profile=std::move(parsed);
    m_available=true;
    m_legacyOverride=legacyOverride;
    m_selectionReason=legacyOverride?"higher-priority-legacy-resource-set":"coherent-surface-resource-set";
    m_frozen=true;
}

RuntimeReferenceSurfaceProfile &runtimeReferenceSurfaceProfile()
{
    static RuntimeReferenceSurfaceProfile profile;
    return profile;
}
