#include "../../src/HelloMine3D/World/Block/ReferenceSurfaceProfile.h"
#include "../../src/HelloMine3D/World/Block/TerrainTextureArray.h"
#include "../../src/HelloMine3D/Util/ResourcePackResolver.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
namespace fs=std::filesystem;
int checks=0,failures=0;
std::string read(const fs::path &p) {
    std::ifstream in(p,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot read "+p.string());
    return {std::istreambuf_iterator<char>(in),{}};
}
void write(const fs::path &p,const std::string &s) {
    fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<s;
    if(!out) throw std::runtime_error("Cannot write "+p.string());
}
void check(const std::string &name,bool pass) {
    ++checks;failures+=!pass;
    std::cout<<"[SURFACE_PROFILE] "<<(pass?"PASS ":"FAIL ")<<name<<'\n';
}
void rejects(const std::string &name,const std::function<void()> &fn,const std::string &message) {
    try { fn();check(name,false); }
    catch(const std::exception &e){check(name,std::string(e.what()).find(message)!=std::string::npos);}
}
std::string replace(std::string source,const std::string &a,const std::string &b) {
    auto at=source.find(a);if(at==std::string::npos) throw std::runtime_error("Missing fixture token "+a);
    source.replace(at,a.size(),b);return source;
}
std::string checksum(std::string &data) {
    std::uint64_t hash=UINT64_C(14695981039346656037);
    for(std::size_t i=36;i<data.size();++i) hash=(hash^static_cast<unsigned char>(data[i]))*UINT64_C(1099511628211);
    for(unsigned i=0;i<8;++i) data[28+i]=char((hash>>(i*8))&255u);
    std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return out.str();
}
}
int main(int argc,char **argv) {
try {
    if(argc!=3) throw std::runtime_error("Usage: profile-test ROOT NEW_EVIDENCE");
    const fs::path root=fs::absolute(argv[1]),output=fs::absolute(argv[2]);
    if(fs::exists(output)) throw std::runtime_error("Evidence already exists");
    fs::create_directories(output);
    const auto base=read(root/ReferenceSurfaceProfile::LogicalPath);
    const std::vector<ResourcePackRequirement> requirements{
        {"material-profile",ReferenceSurfaceProfile::LogicalPath},
        {"texture","media/textures/ReferenceColour64.hmt"},
        {"texture","media/textures/ReferenceNormal64.hmt"},
        {"texture","media/textures/ReferenceSurface64.hmt"}};
    for(const auto &r:requirements) write(output/r.logicalPath,read(root/r.logicalPath));
    const auto resolve=[&output](const std::string &p){return (output/p).string();};
    const auto parsed=loadReferenceSurfaceProfile((output/ReferenceSurfaceProfile::LogicalPath).string(),resolve);
    check("production-three-channel-load",parsed.textures.size()==3);
    std::size_t bytes=0;
    for(unsigned i=0;i<3;++i) {
        const auto data=TerrainTextureArray::load(resolve(parsed.textures[i]));
        bytes+=data.rgba.size();
        check("production-channel-"+std::to_string(i)+"-layer-mip-identity",data.edge==64&&data.layers==256&&
              data.mipCount==7&&data.contentHash==parsed.contentHashes[i]);
    }
    check("explicit-payload-budget",bytes==16776192);
    const fs::path fixture=output/"fault.profile";
    const auto fault=[&](const std::string &text){write(fixture,text);loadReferenceSurfaceProfile(fixture.string(),resolve);};
    rejects("header-version",[&]{fault(replace(base,"v1","v2"));},"unsupported or missing header");
    rejects("duplicate",[&]{fault(base+"edge=64\n");},"duplicate key");
    rejects("unknown",[&]{fault(base+"arbitrary=1\n");},"unknown/empty key");
    rejects("empty",[&]{fault(replace(base,"edge=64","edge="));},"unknown/empty key");
    rejects("missing",[&]{fault(replace(base,"mips=7\n",""));},"missing key");
    rejects("oversize",[&]{fault(replace(base,"edge=64","edge=128"));},"requires 64-pixel");
    rejects("wrong-mip",[&]{fault(replace(base,"mips=7","mips=6"));},"requires 64-pixel");
    rejects("path",[&]{fault(replace(base,"ReferenceColour64","OtherColour64"));},"unsupported path");
    const auto hashAt=base.find("colour_hash=")+12;
    const auto originalHash=base.substr(hashAt,16);
    rejects("hex",[&]{fault(replace(base,originalHash,"zzzzzzzzzzzzzzzz"));},"hexadecimal");
    rejects("identity",[&]{fault(replace(base,originalHash,"0000000000000000"));},"identity mismatch");
    const auto colour=read(output/parsed.textures[0]);
    for(const auto &item:std::vector<std::pair<std::string,std::string>>{
        {"truncated",colour.substr(0,colour.size()-1)},{"trailing",colour+"x"},
        {"bad-checksum",replace(colour.substr(0,36), "HMTARRAY", "HMTARRAY")+std::string(colour.size()-36,'x')}}) {
        write(output/parsed.textures[0],item.second);
        rejects(item.first,[&]{fault(base);},item.first=="bad-checksum"?"checksum mismatch":"payload size mismatch");
    }
    write(output/parsed.textures[0],colour);
    const auto normal=read(output/parsed.textures[1]);
    auto badNormal=normal;badNormal[36]=badNormal[37]=badNormal[38]=0;
    auto normalHash=checksum(badNormal);
    const auto normalAt=base.find("normal_hash=")+12;
    write(output/parsed.textures[1],badNormal);
    rejects("renormalization-negative",[&]{fault(replace(base,base.substr(normalAt,16),normalHash));},
            "normalized positive-Z");
    write(output/parsed.textures[1],normal);
    const auto surface=read(output/parsed.textures[2]);
    auto badSurface=surface;badSurface[36]=0;
    auto surfaceHash=checksum(badSurface);
    const auto surfaceAt=base.find("surface_hash=")+13;
    write(output/parsed.textures[2],badSurface);
    rejects("roughness-negative",[&]{fault(replace(base,base.substr(surfaceAt,16),surfaceHash));},"roughness >= .2");
    write(output/parsed.textures[2],surface);
    ResourcePackResolver resolver;resolver.freeze(output.string(),requirements,{});
    RuntimeReferenceSurfaceProfile frozen;frozen.freezeFromResourceView(resolver);
    check("freeze-base",frozen.frozen()&&frozen.parameters().contentHashes==parsed.contentHashes);
    rejects("double-freeze",[&]{frozen.freezeFromResourceView(resolver);},"already frozen");
    const auto pack=output/"packs/partial";
    write(pack/"pack.meta","# HelloMine3D resource pack v1\nname=PartialSurface\nformat=1\n");
    write(pack/parsed.textures[1],normal);
    ResourcePackResolver partial;partial.freeze(output.string(),requirements,{pack.string()});
    RuntimeReferenceSurfaceProfile rejected;
    rejects("partial-owner",[&]{rejected.freezeFromResourceView(partial);},"one resource owner");
    check("failed-freeze-not-published",!rejected.frozen());
    for(const auto &r:requirements) write(pack/r.logicalPath,read(output/r.logicalPath));
    ResourcePackResolver coherent;coherent.freeze(output.string(),requirements,{pack.string()});
    RuntimeReferenceSurfaceProfile whole;whole.freezeFromResourceView(coherent);
    check("coherent-pack",whole.frozen()&&whole.parameters().contentHashes==parsed.contentHashes);
    check("all-effective-owners-match",coherent.overrideCount()==4);
    check("base-surface-available",frozen.available()&&frozen.usableWithEffectiveTerrain());
    check("coherent-new-pack-available",whole.available()&&whole.usableWithEffectiveTerrain());
    const std::vector<ResourcePackRequirement> legacyRequirements{
        {"material-profile",TerrainMaterialParameters::LogicalPath},
        {"texture",TerrainMaterialParameters::DefaultArrayLogicalPath},
        {"texture",TerrainMaterialParameters::DefaultAtlasLogicalPath},
        {"shader",TerrainMaterialParameters::TerrainShaderLogicalPath},
        {"shader","media/ogre/HelloMine3DTerrainShadow.frag"},
        {"resource-script","media/ogre/HelloMine3D.program"},
        {"resource-script","media/ogre/HelloMine3D.material"}};
    for(const auto &r:legacyRequirements) write(output/r.logicalPath,read(root/r.logicalPath));
    ResourcePackResolver absent;absent.freeze(output.string(),legacyRequirements,{});
    RuntimeReferenceSurfaceProfile oldOnly;oldOnly.freezeFromResourceView(absent);
    check("complete-old-interface-absence",oldOnly.frozen()&&!oldOnly.available()&&
          !oldOnly.usableWithEffectiveTerrain()&&oldOnly.selectionReason()=="complete-legacy-resource-set");
    auto partialRequirements=legacyRequirements;partialRequirements.push_back(requirements[0]);
    ResourcePackResolver declaredPartial;declaredPartial.freeze(output.string(),partialRequirements,{});
    RuntimeReferenceSurfaceProfile missingChannels;
    rejects("declared-profile-without-channels",[&]{missingChannels.freezeFromResourceView(declaredPartial);},"partial declared");
    partialRequirements=legacyRequirements;partialRequirements.push_back(requirements[2]);
    ResourcePackResolver orphan;orphan.freeze(output.string(),partialRequirements,{});
    RuntimeReferenceSurfaceProfile missingProfile;
    rejects("declared-channel-without-profile",[&]{missingProfile.freezeFromResourceView(orphan);},"partial declared");
    auto allRequirements=requirements;allRequirements.insert(allRequirements.end(),legacyRequirements.begin(),legacyRequirements.end());
    const auto oldPack=output/"packs/old";
    write(oldPack/"pack.meta","# HelloMine3D resource pack v1\nname=CompleteOldVisuals\nformat=1\n");
    for(const auto &r:legacyRequirements) if(r.category!="resource-script") write(oldPack/r.logicalPath,read(output/r.logicalPath));
    ResourcePackResolver oldOverride;oldOverride.freeze(output.string(),allRequirements,{oldPack.string()});
    RuntimeReferenceSurfaceProfile oldSelected;oldSelected.freezeFromResourceView(oldOverride);
    RuntimeTerrainMaterialProfile oldTerrain;oldTerrain.freezeFromResourceView(oldOverride);oldTerrain.freezeRenderingMode(true,true);
    check("complete-old-v2-overrides-stays-standard-array",oldTerrain.usesTextureArray());
    check("complete-old-v2-array-not-replaced-by-base-surface",oldSelected.available()&&
          !oldSelected.usableWithEffectiveTerrain()&&oldSelected.selectionReason()=="higher-priority-legacy-resource-set");
    ResourcePackResolver newAboveOld;newAboveOld.freeze(output.string(),allRequirements,{pack.string(),oldPack.string()});
    RuntimeReferenceSurfaceProfile newSelected;newSelected.freezeFromResourceView(newAboveOld);
    check("explicit-new-set-above-old-array",newSelected.available()&&newSelected.usableWithEffectiveTerrain());
    ResourcePackResolver oldAboveNew;oldAboveNew.freeze(output.string(),allRequirements,{oldPack.string(),pack.string()});
    RuntimeReferenceSurfaceProfile oldPriority;oldPriority.freezeFromResourceView(oldAboveNew);
    check("first-pack-wins-old-above-complete-new",oldPriority.available()&&!oldPriority.usableWithEffectiveTerrain());
    write(pack/ReferenceSurfaceProfile::LogicalPath,replace(base,originalHash,"0000000000000000"));
    ResourcePackResolver hiddenBad;hiddenBad.freeze(output.string(),allRequirements,{oldPack.string(),pack.string()});
    RuntimeReferenceSurfaceProfile hiddenRejected;
    rejects("bad-new-set-still-rejected-below-old-override",[&]{hiddenRejected.freezeFromResourceView(hiddenBad);},"identity mismatch");
    check("bad-hidden-set-not-published",!hiddenRejected.frozen());
    std::cout<<"[SURFACE_PROFILE] checks="<<checks<<" failures="<<failures
             <<" scope=production-parser-data-coherence-and-independent-faults\n";
    return failures?1:0;
} catch(const std::exception &e) {std::cerr<<"[SURFACE_PROFILE] "<<e.what()<<'\n';return 2;}
}
