#include "FloraWindCapture.h"
#include "../World/World.h"
#include "../Util/ResourcePackResolver.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include <Ogre.h>
#include <OgreAutoParamDataSource.h>
#include <OgreGL3PlusPrerequisites.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <GLSL/OgreGLSLShader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
using Fields = std::vector<std::pair<std::string, std::string>>;
constexpr std::size_t OpLimit = 16u * 1024u * 1024u;
constexpr std::size_t PacketLimit = 256u * 1024u * 1024u;
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("Flora wind capture: " + message);
}
std::string q(const std::string& s) {
    std::ostringstream o; o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else if (c < 32) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else o << c;
    }
    return o.str() + '"';
}
template<class T> std::string n(T v) { require(std::isfinite(static_cast<double>(v)),"nonfinite JSON number"); std::ostringstream o; o << std::setprecision(10) << +v; return o.str(); }
std::string b(bool v) { return v ? "true" : "false"; }
std::string object(const Fields& f) { std::string s="{"; for(const auto& x:f) { if(s.size()>1)s+=',';s+=q(x.first)+':'+x.second; } return s+'}'; }
std::string array(const std::vector<std::string>& a) { std::string s="[";for(const auto& x:a){if(s.size()>1)s+=',';s+=x;}return s+']'; }
template<class T, std::size_t N> std::string numbers(const std::array<T,N>& a) { std::vector<std::string> s;for(auto v:a)s.push_back(n(v));return array(s); }
std::string xyz(glm::ivec3 p) { return array({n(p.x),n(p.y),n(p.z)}); }
std::string matrix(const Ogre::Matrix4& m) {
    std::vector<std::string> a; for(int r=0;r<4;++r)for(int c=0;c<4;++c){require(std::isfinite(m[r][c]),"nonfinite matrix");a.push_back(n(m[r][c]));}return array(a);
}
double difference(const Ogre::Matrix4& a,const Ogre::Matrix4& b) {
    double d=0;for(int r=0;r<4;++r)for(int c=0;c<4;++c)d=std::max(d,std::abs(double(a[r][c])-b[r][c]));return d;
}
Ogre::Matrix4 fromColumn(const std::array<float,16>& a) {
    Ogre::Matrix4 m;for(int r=0;r<4;++r)for(int c=0;c<4;++c)m[r][c]=a[c*4+r];return m;
}
std::vector<std::string> errors() {
    std::vector<std::string> a;for(int i=0;i<32;++i){const auto e=glGetError();if(e==GL_NO_ERROR)break;a.push_back(n(e));}return a;
}
// Only these states are mutated: COPY_READ, active unit (query unit0), PACK/PBO,
// READ_FRAMEBUFFER and the read selector of its actual draw object. All other
// snapshots are comparison-only, including the currently active VAO's EBO.
struct ReadState {
    GLint copy=0,active=0,packBuffer=0,readFbo=0,drawFbo=0,read=0,drawRead=0,drawBuffer=0;
    GLint program=0,pipeline=0,vao=0,ebo=0,arrayBuffer=0;
    std::array<GLint,8> pack{};
    std::array<GLint,4> viewport{};
    const std::array<GLenum,8> names{{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_IMAGE_HEIGHT,
        GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_IMAGES,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST}};
    bool restored=false;
    ReadState() {
        glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);
        for(std::size_t i=0;i<names.size();++i)glGetIntegerv(names[i],&pack[i]);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);
        glGetIntegerv(GL_READ_BUFFER,&read);glGetIntegerv(GL_DRAW_BUFFER0,&drawBuffer);
        if(drawFbo!=readFbo){glBindFramebuffer(GL_READ_FRAMEBUFFER,drawFbo);glGetIntegerv(GL_READ_BUFFER,&drawRead);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);}else drawRead=read;
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING,&pipeline);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&ebo);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);glGetIntegerv(GL_VIEWPORT,viewport.data());
    }
    void tightPack() {glBindBuffer(GL_PIXEL_PACK_BUFFER,0);for(auto name:names)glPixelStorei(name,name==GL_PACK_ALIGNMENT?1:0);}
    void restore() noexcept {
        if(restored)return;
        glBindBuffer(GL_COPY_READ_BUFFER,copy);glActiveTexture(active);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,packBuffer);for(std::size_t i=0;i<names.size();++i)glPixelStorei(names[i],pack[i]);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,drawFbo);glReadBuffer(drawRead);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);glReadBuffer(read);restored=true;
    }
    bool same(const ReadState& s) const noexcept {
        return copy==s.copy&&active==s.active&&packBuffer==s.packBuffer&&pack==s.pack&&readFbo==s.readFbo&&drawFbo==s.drawFbo&&read==s.read&&drawRead==s.drawRead&&drawBuffer==s.drawBuffer&&program==s.program&&pipeline==s.pipeline&&vao==s.vao&&ebo==s.ebo&&arrayBuffer==s.arrayBuffer&&viewport==s.viewport;
    }
    bool verify() {restore();ReadState now;const bool ok=same(now);now.restore();return ok;}
    ~ReadState(){restore();}
};
std::size_t cpuBytes(const PackedTerrainRenderBatch& cpu) {
    require(cpu.vertices.size()<=OpLimit/sizeof(TerrainRenderVertex)&&cpu.indices.size()<=OpLimit/sizeof(std::uint32_t),"CPU part exceeds raw bound");
    const auto size=cpu.vertices.size()*sizeof(TerrainRenderVertex)+cpu.indices.size()*sizeof(std::uint32_t);require(size<=OpLimit,"combined CPU buffers exceed 16MiB");return size;
}
std::string partJson(const FloraWindCapture::PartState& p) {return object({{"section",xyz(p.section)},{"incarnation",p.incarnationKnown?n(p.incarnation):"null"},{"incarnation_known",b(p.incarnationKnown)},{"incarnation_scope",q("OPEN_PUBLIC_LOCKED_SNAPSHOT_HAS_NO_INCARNATION")},{"live_revision",n(p.liveRevision)},{"upload_revision",n(p.uploadRevision)},{"gpu_resident",b(p.gpuResident)}});}
bool samePart(const FloraWindCapture::PartState&a,const FloraWindCapture::PartState&b) {return a.section==b.section&&a.incarnationKnown==b.incarnationKnown&&(!a.incarnationKnown||a.incarnation==b.incarnation)&&a.liveRevision==b.liveRevision&&a.uploadRevision==b.uploadRevision&&a.gpuResident==b.gpuResident;}
struct Range {std::size_t vb=0,ve=0,ib=0,ie=0;};
Range identify(const PackedTerrainRenderBatch& cpu,glm::ivec3 origin,glm::ivec3 source) {
    const Ogre::Vector3 offset(float(origin.x)*CHUNK_SIZE,float(origin.y)*CHUNK_SIZE,float(origin.z)*CHUNK_SIZE);
    std::vector<std::size_t> v,i;
    for(std::size_t k=0;k<cpu.vertices.size();++k){const auto& p=cpu.vertices[k];const Ogre::Vector3 a=Ogre::Vector3(p.x,p.y,p.z)+offset-Ogre::Vector3(source.x,source.y,source.z);if(a.x>0&&a.x<1&&a.y>0&&a.y<1&&a.z>0&&a.z<1)v.push_back(k);}
    require(v.size()==24&&v.back()-v.front()+1==24,"natural Fern does not have 24 contiguous original vertices");
    for(std::size_t k=0;k<cpu.indices.size();k+=3){require(k+2<cpu.indices.size(),"incomplete triangle");unsigned inside=0;for(unsigned j=0;j<3;++j){require(cpu.indices[k+j]<cpu.vertices.size(),"CPU index out of range");inside+=cpu.indices[k+j]>=v.front()&&cpu.indices[k+j]<=v.back();}require(inside==0||inside==3,"triangle crosses source range");if(inside==3)for(unsigned j=0;j<3;++j)i.push_back(k+j);}
    require(i.size()==36&&i.back()-i.front()+1==36,"natural Fern does not have 36 contiguous original indices");return {v.front(),v.back()+1,i.front(),i.back()+1};
}
std::string sourceQuery(World& world,const FloraWindCapture::Binding& binding,glm::ivec3 p) {
    const glm::ivec3 section(int(std::floor(p.x/double(CHUNK_SIZE))),int(std::floor(p.y/double(CHUNK_SIZE))),int(std::floor(p.z/double(CHUNK_SIZE))));
    const auto before=world.collectSectionMeshSnapshot(false);
    const auto bversion=std::find_if(before.liveSectionVersions.begin(),before.liveSectionVersions.end(),[&](const auto& x){return x.location==section;});
    const auto block=world.getBlock(p.x,p.y,p.z);
    const int sun=world.getSunlight(p.x,p.y,p.z),light=world.getBlockLight(p.x,p.y,p.z);
    const auto after=world.collectSectionMeshSnapshot(false);
    const auto aversion=std::find_if(after.liveSectionVersions.begin(),after.liveSectionVersions.end(),[&](const auto& x){return x.location==section;});
    const bool known=bversion!=before.liveSectionVersions.end()&&aversion!=after.liveSectionVersions.end()&&bversion->blockRevision==aversion->blockRevision;
    const auto chain=std::find_if(binding.parts.begin(),binding.parts.end(),[&](const auto& a){return a.section==section;});
    require(known&&block==ChunkBlock(BlockId::TallGrass,BlockMetadata::TallGrass::Fern),"natural source is not resident Fern at stable locked version endpoints");
    require(chain!=binding.parts.end()&&chain->gpuResident&&chain->liveRevision==aversion->blockRevision&&chain->uploadRevision==aversion->blockRevision,"source live/upload/GpuResident chain mismatch");
    auto& manager=world.getChunkManager();
    const auto range=identify(binding.cpu,binding.origin,p);
    return object({{"query_xyz",xyz(p)},{"known",b(known)},{"returned_id",n(block.id)},{"returned_metadata",n(block.metadata)},
        {"section",xyz(section)},{"renderer_incarnation",chain->incarnationKnown?n(chain->incarnation):"null"},{"renderer_incarnation_known",b(chain->incarnationKnown)},{"world_incarnation",q("OPEN_NO_INCARNATION_IN_PUBLIC_LIVE_VERSION_SNAPSHOT")},{"actual_block_revision",n(aversion->blockRevision)},{"version_before",n(bversion->blockRevision)},{"version_after",n(aversion->blockRevision)},{"atomic_copy",b(false)},
        {"live_revision",n(chain->liveRevision)},{"upload_revision",n(chain->uploadRevision)},{"gpu_resident",b(chain->gpuResident)},
        {"seed",n(manager.getTerrainSeed())},{"terrain_generation_version",n(manager.getTerrainGenerationVersion())},
        {"biome",n(int(manager.getTerrainGenerator().getBiomeAtWorld(p.x,p.z)))},{"sunlight",n(sun)},{"blocklight",n(light)},
        {"vertex_begin",n(range.vb)},{"vertex_end",n(range.ve)},{"index_begin",n(range.ib)},{"index_end",n(range.ie)},
        {"range_origin",q("actual retained CPU packed positions/indices; native full bytes compared separately")}});
}
void validateParts(World& world,const FloraWindCapture::Binding& binding) {
    require(binding.parts.size()>0&&binding.parts.size()<=4&&binding.sources.size()>0&&binding.sources.size()<=2,"part/source bound");
    const auto snapshot=world.collectSectionMeshSnapshot(false);
    for(const auto& state:binding.parts){const auto s=std::find_if(snapshot.liveSectionVersions.begin(),snapshot.liveSectionVersions.end(),[&](const auto& x){return x.location==state.section;});require(s!=snapshot.liveSectionVersions.end()&&state.liveRevision==s->blockRevision&&state.uploadRevision==state.liveRevision&&state.gpuResident,"batch part locked current/upload/resident mismatch");}
}
std::string floatUniform(GLuint program,const char* name,float& result) {
    GLuint index=GL_INVALID_INDEX;glGetUniformIndices(program,1,&name,&index);require(index!=GL_INVALID_INDEX,std::string("missing native uniform ")+name);
    GLint type=0,size=0;glGetActiveUniformsiv(program,1,&index,GL_UNIFORM_TYPE,&type);glGetActiveUniformsiv(program,1,&index,GL_UNIFORM_SIZE,&size);
    const GLint loc=glGetUniformLocation(program,name);require(loc>=0&&type==GL_FLOAT&&size==1,"native uniform is not scalar float");glGetUniformfv(program,loc,&result);require(std::isfinite(result),"native time is nonfinite");return object({{"location",n(loc)},{"type",n(type)},{"value",n(result)}});
}
std::string matrixUniform(GLuint program,const char* name,std::array<float,16>& result) {
    GLuint index=GL_INVALID_INDEX;glGetUniformIndices(program,1,&name,&index);require(index!=GL_INVALID_INDEX,std::string("missing native uniform ")+name);
    GLint type=0,size=0;glGetActiveUniformsiv(program,1,&index,GL_UNIFORM_TYPE,&type);glGetActiveUniformsiv(program,1,&index,GL_UNIFORM_SIZE,&size);
    const GLint loc=glGetUniformLocation(program,name);require(loc>=0&&type==GL_FLOAT_MAT4&&size==1,"native uniform is not mat4");glGetUniformfv(program,loc,result.data());for(float x:result)require(std::isfinite(x),"native matrix is nonfinite");return object({{"location",n(loc)},{"type",n(type)},{"layout",q("column_major")},{"values",numbers(result)}});
}
}

struct FloraWindCapture::Impl {
    fs::path directory; Ogre::SceneManager& scene; Ogre::Camera& camera; Ogre::RenderWindow& window;
    FloraWindCapture& owner; std::thread::id thread=std::this_thread::get_id(); World* world=nullptr;
    std::size_t bytes=0,frames=0;bool open=false;std::uint64_t frameId=0;
    std::string prefix,phase,mode;std::vector<Binding> bindings;std::vector<std::string> framesJson,operations,beginSources,glErrors,openReasons;
    struct Pending { const Ogre::Pass* pass=nullptr;Ogre::Matrix4 world=Ogre::Matrix4::IDENTITY,wvp=Ogre::Matrix4::IDENTITY;GLuint query=0;bool began=false;GLint conflict=0; };
    std::map<ChunkSectionRenderable*,Pending> pending;
    Impl(FloraWindCapture& o,const std::string& path,Ogre::SceneManager& s,Ogre::Camera& c,Ogre::RenderWindow& w):directory(path),scene(s),camera(c),window(w),owner(o) {
        require(!path.empty()&&!fs::exists(directory),"output directory must be new");fs::create_directories(directory);index();scene.addRenderObjectListener(&owner);
    }
    void check() const {require(thread==std::this_thread::get_id(),"render thread changed");}
    void write(const std::string& file,const void* data,std::size_t size) {require(size<=PacketLimit-bytes,"256MiB session output bound");std::ofstream s(directory/file,std::ios::binary);s.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));require(bool(s),"cannot write "+file);bytes+=size;}
    void textFile(const std::string& file,const std::string& text){write(file,text.data(),text.size());}
    void errorAt(const std::string& stage){for(const auto&e:errors())glErrors.push_back(object({{"stage",q(stage)},{"code",e}}));}
    void cleanupQuery(Pending& p) noexcept {
        if(!p.query)return;GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);
        if(p.began&&GLuint(active)==p.query)glEndQuery(GL_PRIMITIVES_GENERATED);
        glDeleteQueries(1,&p.query);p.query=0;p.began=false;
    }
    void detachAll() noexcept {for(auto& item:pending)cleanupQuery(item.second);pending.clear();for(auto& x:bindings)if(x.renderable)x.renderable->setNativeDrawObserver(nullptr);}
    Binding* binding(ChunkSectionRenderable* object) {auto i=std::find_if(bindings.begin(),bindings.end(),[&](auto& x){return x.renderable==object;});return i==bindings.end()?nullptr:&*i;}
    void index() {
        const auto& profile=runtimeTerrainMaterialProfile();const auto& p=profile.parameters();std::vector<std::string> resources;
        for(const auto& r:runtimeResourcePackResolver().effectiveResources())resources.push_back(object({{"logical_path",q(r.logicalPath)},{"resolved_file",q(r.sourcePath)},{"owner",q(r.packName)}}));
        const auto text=object({{"schema",q("hellomine3d-flora-wind-native-session-v1")},{"scope",q("actual client postRender program/time/matrices and original native storage; primitive query measures attempted draw interval; inner draw VAO/attribute binding OPEN (renderer may unbind before post on its updateVAO branch); diagnostic, ordinary input NOT_RUN; endpoint checks are not atomic ABA proof")},
            {"max_frames",n(12)},{"max_ops_per_frame",n(4)},{"max_raw_bytes_per_op",n(OpLimit)},{"max_session_bytes",n(PacketLimit)},
            {"resources",array(resources)},{"profile",object({{"format_version",n(p.formatVersion)},{"atlas_pixels",n(p.atlasPixels)},{"tile_pixels",n(p.tilePixels)},{"tiles_per_row",n(p.tilesPerRow)},{"uses_array",b(profile.usesTextureArray())},{"mode_reason",q(profile.renderingModeReason())}})},
            {"frame_count",n(frames)},{"complete",b(frames==4)},{"frames",array(framesJson)}});
        // Index is bounded independently; rewriting it doesn't multiply payload.
        require(text.size()<1024u*1024u&&text.size()<=PacketLimit-bytes,"bounded session index");std::ofstream s(directory/"index.json");s<<text<<'\n';require(bool(s),"cannot write session index");
    }
};

FloraWindCapture::FloraWindCapture(const std::string& output,Ogre::SceneManager& scene,Ogre::Camera& camera,Ogre::RenderWindow& window):m_impl(std::make_unique<Impl>(*this,output,scene,camera,window)){}
FloraWindCapture::~FloraWindCapture(){if(m_impl){m_impl->detachAll();m_impl->scene.removeRenderObjectListener(this);}}
bool FloraWindCapture::isFrameOpen()const noexcept{return m_impl->open;}
std::size_t FloraWindCapture::frameCount()const noexcept{return m_impl->frames;}
bool FloraWindCapture::isComplete()const noexcept{return frameCount()==4;}
std::string FloraWindCapture::framePngPath()const{return (m_impl->directory/(m_impl->prefix+".png")).string();}
void FloraWindCapture::detachRenderable(ChunkSectionRenderable& renderable)noexcept {
    renderable.setNativeDrawObserver(nullptr);auto p=m_impl->pending.find(&renderable);if(p!=m_impl->pending.end()){m_impl->cleanupQuery(p->second);m_impl->pending.erase(p);}for(auto& x:m_impl->bindings)if(x.renderable==&renderable)x.renderable=nullptr;
}
void FloraWindCapture::beginFrame(const std::string& phase,const std::string& mode,std::uint64_t id,World& world,std::vector<Binding> bindings) {
    auto& s=*m_impl;s.check();require(!s.open&&s.frames<12&&bindings.size()>0&&bindings.size()<=4,"frame/op bound");require(mode==(runtimeTerrainMaterialProfile().usesTextureArray()?"standard":"compatibility"),"mode differs from actual frozen profile");
    s.phase=phase;s.mode=mode;s.frameId=id;std::ostringstream name;name<<"frame-"<<std::setw(3)<<std::setfill('0')<<s.frames;s.prefix=name.str();s.bindings=std::move(bindings);s.world=&world;s.operations.clear();s.beginSources.clear();s.glErrors.clear();s.openReasons.clear();s.pending.clear();s.errorAt("begin-frame");
    std::size_t total=0;std::vector<ChunkSectionRenderable*> unique;
    for(auto& x:s.bindings){require(x.renderable&&std::find(unique.begin(),unique.end(),x.renderable)==unique.end(),"missing/duplicate actual instance");unique.push_back(x.renderable);total+=cpuBytes(x.cpu);validateParts(world,x);require(x.renderable->getMaterial()->getName()=="HelloMine3D/Flora","registered instance is not production Flora");for(const auto& source:x.sources)s.beginSources.push_back(sourceQuery(world,x,source.position));}
    require(total<PacketLimit-s.bytes,"session bound before attachment");s.open=true;for(auto& x:s.bindings)x.renderable->setNativeDrawObserver(this);
}
void FloraWindCapture::notifyRenderSingleObject(Ogre::Renderable* renderable,const Ogre::Pass* pass,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed) {
    auto& s=*m_impl;if(!s.open)return;s.check();auto* object=dynamic_cast<ChunkSectionRenderable*>(renderable);if(!object||!s.binding(object))return;
    if(suppressed||!source||source->getCurrentCamera()!=&s.camera||!pass){s.openReasons.push_back(q("selected listener is suppressed or not main camera"));return;}
    auto& p=s.pending[object];require(!p.began,"nested draw on selected instance");p.pass=pass;p.world=source->getWorldMatrix();p.wvp=source->getWorldViewProjMatrix();
}
void FloraWindCapture::beforeNativeDraw(ChunkSectionRenderable& renderable,Ogre::SceneManager* scene,Ogre::RenderSystem*) {
    auto& s=*m_impl;if(!s.open||!s.binding(&renderable))return;s.check();require(scene==&s.scene,"scene identity mismatch");require(s.operations.size()<4,"four attempted operation bound");
    auto& p=s.pending[&renderable];if(!p.pass){s.openReasons.push_back(q("missing matching prebind listener"));return;}
    glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&p.conflict);
    if(p.conflict!=0){s.openReasons.push_back(q("existing primitive query; not interfered with"));return;}
    glGenQueries(1,&p.query);require(p.query!=0,"query allocation failed");glBeginQuery(GL_PRIMITIVES_GENERATED,p.query);p.began=true;s.errorAt("primitive-query-begin");
}
void FloraWindCapture::afterNativeDraw(ChunkSectionRenderable& renderable,Ogre::SceneManager* scene,Ogre::RenderSystem*) {
    auto& s=*m_impl;if(!s.open)return;s.check();auto* binding=s.binding(&renderable);if(!binding)return;require(scene==&s.scene,"post scene identity mismatch");
    auto pending=s.pending.find(&renderable);require(pending!=s.pending.end(),"unpaired native callback");auto& p=pending->second;
    GLint query=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&query);GLuint generated=0,available=0;const GLuint ownQuery=p.query;
    const bool ended=p.began&&GLuint(query)==p.query;
    if(ended){glEndQuery(GL_PRIMITIVES_GENERATED);p.began=false;glGetQueryObjectuiv(p.query,GL_QUERY_RESULT_AVAILABLE,&available);glGetQueryObjectuiv(p.query,GL_QUERY_RESULT,&generated);}else s.openReasons.push_back(q("no owned primitive query proof"));
    s.cleanupQuery(p);ReadState state;s.errorAt("native-post-entry");
    Ogre::RenderOperation op;renderable.getRenderOperation(op);Ogre::Matrix4 node;renderable.getWorldTransforms(&node);
    require(op.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST&&op.useIndexes&&op.vertexData&&op.indexData,"not an original indexed triangle list");
    require(op.numberOfInstances==1&&op.vertexData->vertexStart==0&&op.indexData->indexStart==0,"unexpected original instance/start contract");
    require(p.pass&&p.pass->getPassIterationCount()==1&&!p.pass->hasGeometryProgram()&&!p.pass->hasTessellationHullProgram()&&!p.pass->hasTessellationDomainProgram(),"unsupported multiple iteration/geometry/tessellation pass");
    const auto& vb=op.vertexData->vertexBufferBinding->getBindings();require(vb.size()==1&&vb.begin()->first==0,"original Flora must retain its one source0 buffer");
    const auto& hardware=vb.begin()->second;auto* native=dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(hardware.get());auto* index=dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());
    require(native&&index&&hardware->getVertexSize()==sizeof(TerrainRenderVertex)&&op.indexData->indexBuffer->getIndexSize()==4,"actual storage is not production44B/u32 GL3Plus");
    const auto vbytes=hardware->getSizeInBytes(),ibytes=op.indexData->indexBuffer->getSizeInBytes();require(vbytes<=OpLimit&&ibytes<=OpLimit-vbytes,"combined raw16MiB bound");
    require(op.vertexData->vertexCount==binding->cpu.vertices.size()&&op.indexData->indexCount==binding->cpu.indices.size()&&vbytes==binding->cpu.vertices.size()*sizeof(TerrainRenderVertex)&&ibytes==binding->cpu.indices.size()*4,"original CPU/native size mismatch");
    Bytes vertices(vbytes),indices(ibytes);GLint64 actualSize=0;
    glBindBuffer(GL_COPY_READ_BUFFER,native->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&actualSize);require(actualSize==GLint64(vbytes),"native VBO allocation mismatch");glGetBufferSubData(GL_COPY_READ_BUFFER,0,vbytes,vertices.data());
    glBindBuffer(GL_COPY_READ_BUFFER,index->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&actualSize);require(actualSize==GLint64(ibytes),"native IBO allocation mismatch");glGetBufferSubData(GL_COPY_READ_BUFFER,0,ibytes,indices.data());s.errorAt("native-original-storage");
    const bool cpuEqual=std::memcmp(vertices.data(),binding->cpu.vertices.data(),vbytes)==0&&std::memcmp(indices.data(),binding->cpu.indices.data(),ibytes)==0;
    Ogre::Matrix4 expected=Ogre::Matrix4::IDENTITY;expected.setTrans(Ogre::Vector3(float(binding->origin.x)*CHUNK_SIZE,float(binding->origin.y)*CHUNK_SIZE,float(binding->origin.z)*CHUNK_SIZE));
    require(difference(expected,node)==0,"actual object transform is not its registered batch origin");
    const auto file=s.prefix+"-op-"+n(s.operations.size());s.write(file+".vbo0.bin",vertices.data(),vbytes);s.write(file+".ibo.bin",indices.data(),ibytes);s.write(file+".cpu-vbo0.bin",binding->cpu.vertices.data(),vbytes);s.write(file+".cpu-ibo.bin",binding->cpu.indices.data(),ibytes);
    std::vector<std::string> elements;for(const auto& e:op.vertexData->vertexDeclaration->getElements()){require(e.getSource()==0&&e.getOffset()+Ogre::VertexElement::getTypeSize(e.getType())<=hardware->getVertexSize(),"invalid actual declaration");elements.push_back(object({{"source",n(e.getSource())},{"offset",n(e.getOffset())},{"type",n(e.getType())},{"semantic",n(e.getSemantic())},{"semantic_index",n(e.getIndex())},{"components",n(Ogre::VertexElement::getTypeCount(e.getType()))},{"bytes",n(Ogre::VertexElement::getTypeSize(e.getType()))}}));}
    GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);require(program>0&&glIsProgram(program),"no actual bound monolithic program");GLint link=0,count=0;glGetProgramiv(program,GL_LINK_STATUS,&link);glGetProgramiv(program,GL_ATTACHED_SHADERS,&count);require(link==GL_TRUE&&count==2,"actual bound program must have two linked production shaders");
    std::array<GLuint,2> shaders{};GLsizei actualCount=0;glGetAttachedShaders(program,2,&actualCount,shaders.data());require(actualCount==2,"native attached shader count changed");
    const auto* vertex=dynamic_cast<const Ogre::GLSLShader*>(p.pass->getVertexProgram().get());const auto* fragment=dynamic_cast<const Ogre::GLSLShader*>(p.pass->getFragmentProgram().get());require(vertex&&fragment,"selected pass is not loaded production GLSL");
    const bool matched=std::find(shaders.begin(),shaders.end(),vertex->getGLShaderHandle())!=shaders.end()&&std::find(shaders.begin(),shaders.end(),fragment->getGLShaderHandle())!=shaders.end();
    std::vector<std::string> attached;for(auto shader:shaders){GLint type=0,status=0,length=0;glGetShaderiv(shader,GL_SHADER_TYPE,&type);glGetShaderiv(shader,GL_COMPILE_STATUS,&status);glGetShaderiv(shader,GL_SHADER_SOURCE_LENGTH,&length);require(length>0&&length<256*1024,"native shader source bound");std::vector<char> text(length);GLsizei written=0;glGetShaderSource(shader,length,&written,text.data());const auto sf=file+"-shader-"+n(shader)+".glsl";s.write(sf,text.data(),written);attached.push_back(object({{"id",n(shader)},{"type",n(type)},{"compiled",b(status==GL_TRUE)},{"source_file",q(sf)}}));}
    s.errorAt("native-linked-program");
    float time=0;std::array<float,16> nativeWorld{},nativeWvp{};const auto timeJson=floatUniform(program,"globalTime",time);const auto worldJson=matrixUniform(program,"world",nativeWorld);const auto wvpJson=matrixUniform(program,"worldViewProj",nativeWvp);s.errorAt("native-bound-uniforms");
    const double worldDifference=difference(node,fromColumn(nativeWorld)),wvpDifference=difference(p.wvp,fromColumn(nativeWvp));
    // OpenGL 4.1 queries sampler binding on the active unit. Indexed sampler
    // binding queries require newer direct-state-access support.
    glActiveTexture(GL_TEXTURE0);GLint texture2d=0,textureArray=0,sampler=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture2d);glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&textureArray);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);s.errorAt("native-texture-unit0");
    std::vector<std::string> parts,sources;validateParts(*s.world,*binding);for(const auto& part:binding->parts)parts.push_back(partJson(part));for(const auto& source:binding->sources)sources.push_back(sourceQuery(*s.world,*binding,source.position));
    if(!ended||generated==0||generated!=op.indexData->indexCount/3)s.openReasons.push_back(q("generated primitives do not prove one complete original operation"));
    if(!cpuEqual||!matched||worldDifference>1e-5||wvpDifference>1e-5)s.openReasons.push_back(q("native/source/pass/matrix identity mismatch"));
    const bool restored=state.verify();s.errorAt("native-post-restored");require(restored,"native read state restoration mismatch");
    s.operations.push_back(object({{"object_name",q(renderable.getName())},{"object_id",n(reinterpret_cast<std::uintptr_t>(&renderable))},{"material_name",q(renderable.getMaterial()->getName())},{"origin",xyz(binding->origin)},
        {"vertex_start",n(op.vertexData->vertexStart)},{"vertex_count",n(op.vertexData->vertexCount)},{"index_start",n(op.indexData->indexStart)},{"index_count",n(op.indexData->indexCount)},{"index_type",q("u32")},{"operation_type",n(op.operationType)},{"instances",n(op.numberOfInstances)},
        {"vertex_buffers",array({object({{"source",n(0)},{"gl_id",n(native->getGLBufferId())},{"stride",n(hardware->getVertexSize())},{"vertices",n(hardware->getNumVertices())},{"bytes",n(vbytes)},{"file",q(file+".vbo0.bin")},{"cpu_file",q(file+".cpu-vbo0.bin")}})})},
        {"index_buffer",object({{"gl_id",n(index->getGLBufferId())},{"bytes",n(ibytes)},{"file",q(file+".ibo.bin")},{"cpu_file",q(file+".cpu-ibo.bin")}})},{"elements",array(elements)},{"native_cpu_bytes_equal",b(cpuEqual)},{"post_vao",n(state.vao)},{"inner_draw_vertex_fetch",q("OPEN_POST_RENDER_VAO_MAY_BE_UNBOUND_ATTRIBUTES_NOT_OBSERVED")},{"parts",array(parts)},{"source_queries",array(sources)},
        {"node_world_row_major",matrix(node)},{"prebind_world_row_major",matrix(p.world)},{"prebind_wvp_row_major",matrix(p.wvp)},
        {"primitive_query",object({{"target",n(GL_PRIMITIVES_GENERATED)},{"id",n(ownQuery)},{"conflicting_query",n(p.conflict)},{"began",b(ownQuery!=0)},{"ended",b(ended)},{"available_before_blocking_read",b(available!=0)},{"result_read",b(ended)},{"result",n(generated)},{"expected_whole_operation",n(op.indexData->indexCount/3)}})},
        {"actual_program",n(program)},{"linked",b(link==GL_TRUE)},{"attached_shaders",array(attached)},{"attached_matches_selected_pass",b(matched)},
        {"selected_pass_id",n(reinterpret_cast<std::uintptr_t>(p.pass))},{"selected_vertex_program",q(p.pass->getVertexProgramName())},{"selected_fragment_program",q(p.pass->getFragmentProgramName())},{"selected_vertex_shader_id",n(vertex->getGLShaderHandle())},{"selected_fragment_shader_id",n(fragment->getGLShaderHandle())},
        {"native_uniforms",object({{"globalTime",timeJson},{"world",worldJson},{"worldViewProj",wvpJson}})},{"world_max_delta",n(worldDifference)},{"wvp_max_delta",n(wvpDifference)},
        {"native_texture_unit0",object({{"texture_2d",n(texture2d)},{"texture_array",n(textureArray)},{"sampler",n(sampler)}})},{"state_restored",b(restored)}}));
    s.pending.erase(pending);
}
void FloraWindCapture::finishFrame(std::vector<Binding> endBindings) {
    auto& s=*m_impl;s.check();require(s.open,"no open frame");require(endBindings.size()==s.bindings.size(),"endpoint bindings changed");
    std::vector<std::string> endSources;bool stable=true;
    for(const auto& begin:s.bindings){const auto end=std::find_if(endBindings.begin(),endBindings.end(),[&](const auto& x){return x.renderable==begin.renderable;});require(begin.renderable&&end!=endBindings.end(),"selected instance destroyed/replaced during frame");validateParts(*s.world,*end);cpuBytes(end->cpu);stable=stable&&end->origin==begin.origin&&end->parts.size()==begin.parts.size()&&end->sources.size()==begin.sources.size()&&end->cpu.vertices.size()==begin.cpu.vertices.size()&&end->cpu.indices==begin.cpu.indices;
        if(end->cpu.vertices.size()==begin.cpu.vertices.size())stable=stable&&std::memcmp(end->cpu.vertices.data(),begin.cpu.vertices.data(),begin.cpu.vertices.size()*sizeof(TerrainRenderVertex))==0;
        for(std::size_t i=0;i<std::min(begin.parts.size(),end->parts.size());++i)stable=stable&&samePart(begin.parts[i],end->parts[i]);
        for(std::size_t i=0;i<std::min(begin.sources.size(),end->sources.size());++i)stable=stable&&begin.sources[i].position==end->sources[i].position;
        for(const auto& source:end->sources)endSources.push_back(sourceQuery(*s.world,*end,source.position));}
    if(!stable)s.openReasons.push_back(q("endpoint CPU/parts changed; no atomic snapshot claim"));if(s.operations.size()!=s.bindings.size())s.openReasons.push_back(q("some registered original operations were not observed after actual draw"));
    s.detachAll();ReadState state;s.errorAt("frame-read-entry");state.tightPack();glBindFramebuffer(GL_READ_FRAMEBUFFER,state.drawFbo);glReadBuffer(state.drawBuffer);
    const auto width=s.window.getWidth(),height=s.window.getHeight();require(width>0&&height>0&&width<=8192&&height<=8192,"frame dimensions bound");const auto size=std::size_t(width)*height*4;require(size<=PacketLimit-s.bytes,"frame exceeds remaining session bytes");Bytes rgba(size);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());const bool srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    const bool restored=state.verify();s.errorAt("frame-read-restored");require(restored,"frame read state restoration mismatch");s.write(s.prefix+".rgba8",rgba.data(),rgba.size());
    require(fs::is_regular_file(s.directory/(s.prefix+".png")),"Root must save actual frame PNG before finishFrame");const auto pngBytes=fs::file_size(s.directory/(s.prefix+".png"));require(pngBytes>0&&pngBytes<=PacketLimit-s.bytes,"PNG session bound");s.bytes+=pngBytes;
    const auto frame=object({{"schema",q("hellomine3d-flora-wind-native-frame-v1")},{"mode",q(s.mode)},{"phase",q(s.phase)},{"frame_id",n(s.frameId)},
        {"source_queries_before",array(s.beginSources)},{"source_queries_after",array(endSources)},{"endpoint_identity_stable",b(stable)},{"atomic_snapshot",b(false)},{"inner_draw_vertex_fetch",q("OPEN_POST_RENDER_VAO_MAY_BE_UNBOUND_ATTRIBUTES_NOT_OBSERVED")},
        {"camera_view_row_major",matrix(s.camera.getViewMatrix())},{"camera_projection_rs_row_major",matrix(s.camera.getProjectionMatrixRS())},{"operations",array(s.operations)},
        {"framebuffer",object({{"width",n(width)},{"height",n(height)},{"origin",q("bottom_left")},{"format",q("RGBA8")},{"file",q(s.prefix+".rgba8")},{"png",q(s.prefix+".png")},{"read_fbo",n(state.drawFbo)},{"read_buffer",n(state.drawBuffer)},{"framebuffer_srgb",b(srgb)}})},
        {"state_restored",b(restored)},{"gl_errors",array(s.glErrors)},{"open_reasons",array(s.openReasons)},{"ordinary_input",q("NOT_RUN")}});
    s.textFile(s.prefix+".json",frame+'\n');s.framesJson.push_back(q(s.prefix+".json"));++s.frames;s.open=false;s.world=nullptr;s.bindings.clear();s.index();require(s.glErrors.empty(),"actual diagnostic GL errors recorded");
}
