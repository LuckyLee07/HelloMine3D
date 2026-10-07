#pragma once

#include "ChunkSectionRenderable.h"
#include "OgreRenderCapture.h"
#include <Ogre.h>
#include <OgreGL3PlusPrerequisites.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <OgreGL3PlusTexture.h>
#include <GLSL/OgreGLSLShader.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <thread>

// Two original draws of the one wetland wall. No normal-path construction,
// World query, rendering replay or shader/texture/state replacement.
class ShadowPatchCapture final : public ChunkSectionRenderable::NativeDrawObserver,
                                 public Ogre::RenderObjectListener {
    // Reserve two maximum original PNG files inside the same session budget.
    static constexpr std::size_t ObjectLimit = 12u*1024u*1024u, PngLimit = 15u*1024u*1024u,
        OutputLimit = 64u*1024u*1024u;
    using Matrix = std::array<float,16>;
    std::filesystem::path directory;
    Ogre::SceneManager& scene;
    Ogre::Camera& camera;
    const std::thread::id thread = std::this_thread::get_id();
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    ChunkSectionRenderable* selected = nullptr;
    const Ogre::Pass* pass = nullptr;
    GLuint query = 0;
    bool queryActive = false, open = false, posted = false, failed = false;
    std::uint64_t frame = 0, completed = 0;
    std::size_t written = 2*PngLimit;
    std::string packet, ownerKey;
    glm::ivec3 ownerOrigin{0}; bool ownerBatch=false;
    GLuint firstProgram = 0, firstVbo = 0, firstIbo = 0, firstMap = 0;
    ChunkSectionRenderable* firstObject = nullptr;
    Matrix firstWvp{}, firstWorld{};
    std::vector<unsigned char> firstVertices, firstIndices;

    static void require(bool yes, const char* why) {
        if (!yes) throw std::runtime_error(std::string("Shadow patch: ")+why);
    }
    static std::string quote(const std::string& s) {
        std::ostringstream out; out << '"';
        for(unsigned char c:s) { if(c=='"'||c=='\\')out<<'\\'<<c;
            else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
            else out<<c; }
        return out.str()+'"';
    }
    template<class T> static std::string numbers(const T& a) {
        std::ostringstream o; o<<'['<<std::setprecision(9); bool first=true;
        for(auto v:a){if(!first)o<<',';first=false;o<<v;}o<<']';return o.str();
    }
    void write(const std::string& name,const void* data,std::size_t bytes) {
        require(bytes<=OutputLimit-written,"64MiB output limit exceeded");
        const auto path=directory/name; require(!std::filesystem::exists(path),"original evidence already exists");
        written+=bytes; std::ofstream file(path,std::ios::binary); require(file.good(),"evidence open failed");
        file.write(static_cast<const char*>(data),std::streamsize(bytes));file.close();require(file.good(),"evidence write failed");
    }
    void text(const std::string& name,const std::string& s){write(name,s.data(),s.size());}
    void cleanup() noexcept {
        if(queryActive){glEndQuery(GL_PRIMITIVES_GENERATED);queryActive=false;}
        if(query){glDeleteQueries(1,&query);query=0;}
        if(selected)selected->setNativeDrawObserver(nullptr);
        selected=nullptr; pass=nullptr; open=false;
    }
    void fail(const char* why) noexcept {
        failed=true;
        cleanup();
        try { if(!std::filesystem::exists(directory/"failure.json"))
            text("failure.json","{\"status\":\"FAIL\",\"reason\":"+quote(why)+"}\n"); }catch(...){}
    }
    static Matrix matrix(GLuint p,const char* name) {
        GLuint index=GL_INVALID_INDEX;glGetUniformIndices(p,1,&name,&index);require(index!=GL_INVALID_INDEX,"missing native matrix");
        GLint type=0,size=0;glGetActiveUniformsiv(p,1,&index,GL_UNIFORM_TYPE,&type);glGetActiveUniformsiv(p,1,&index,GL_UNIFORM_SIZE,&size);
        const GLint loc=glGetUniformLocation(p,name);require(loc>=0&&type==GL_FLOAT_MAT4&&size==1,"native matrix type differs");
        Matrix value{};glGetUniformfv(p,loc,value.data());for(float x:value)require(std::isfinite(x),"nonfinite native matrix");return value;
    }
    static std::string scalar(GLuint p,const char* name) {
        GLuint index=GL_INVALID_INDEX;glGetUniformIndices(p,1,&name,&index);require(index!=GL_INVALID_INDEX,"missing native scalar");
        GLint type=0,size=0;glGetActiveUniformsiv(p,1,&index,GL_UNIFORM_TYPE,&type);glGetActiveUniformsiv(p,1,&index,GL_UNIFORM_SIZE,&size);
        const GLint loc=glGetUniformLocation(p,name);require(loc>=0&&type==GL_FLOAT&&size==1,"native scalar type differs");
        float value=0;glGetUniformfv(p,loc,&value);require(std::isfinite(value),"nonfinite native scalar");return numbers(std::array<float,1>{{value}});
    }
    // These reads change COPY_READ, active unit and PACK state only. Never bind
    // a VAO/ARRAY/ELEMENT_ARRAY, sampler, framebuffer or program.
    struct ReadState {
        GLint copy=0,active=0,packBuffer=0,readFbo=0,readBuffer=0;std::array<GLint,8> pack{};
        const std::array<GLenum,8> names{{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_IMAGE_HEIGHT,
            GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_IMAGES,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST}};
        ReadState(){glGetIntegerv(GL_COPY_READ_BUFFER_BINDING,&copy);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packBuffer);for(std::size_t i=0;i<8;++i)glGetIntegerv(names[i],&pack[i]);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);glGetIntegerv(GL_READ_BUFFER,&readBuffer);}
        void tight(){glBindBuffer(GL_PIXEL_PACK_BUFFER,0);for(auto n:names)glPixelStorei(n,n==GL_PACK_ALIGNMENT?1:0);}
        void restore()const{glBindBuffer(GL_COPY_READ_BUFFER,copy);glActiveTexture(active);glBindBuffer(GL_PIXEL_PACK_BUFFER,packBuffer);
            for(std::size_t i=0;i<8;++i)glPixelStorei(names[i],pack[i]);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);glReadBuffer(readBuffer);}
        bool verify()const{ReadState after;return copy==after.copy&&active==after.active&&packBuffer==after.packBuffer&&pack==after.pack&&readFbo==after.readFbo&&readBuffer==after.readBuffer;}
        ~ReadState(){restore();}
    };
    static std::array<float,4> transform(const Matrix& m,float x,float y,float z) {
        std::array<float,4> r{};for(int row=0;row<4;++row)r[row]=m[row]*x+m[4+row]*y+m[8+row]*z+m[12+row];return r;
    }
    std::string wall(const std::vector<unsigned char>& vb,const std::vector<unsigned char>& ib,const Matrix& w,const Matrix& wvp) {
        const std::size_t vc=vb.size()/44,ic=ib.size()/4;
        require(ic%6==0,"original operation is not complete six-index faces");
        double nearest=1e9;std::size_t best=ic;
        const double px=350,py=1080; // Inside the observed patch, top-left native pixels.
        for(std::size_t i=0;i<ic;i+=3) {
            std::array<std::array<double,3>,3> screen{};std::array<std::array<float,4>,3> world{};bool visible=true;
            for(int j=0;j<3;++j){std::uint32_t ix;std::memcpy(&ix,ib.data()+4*(i+j),4);require(ix<vc,"native index outside VBO");
                TerrainRenderVertex v;std::memcpy(&v,vb.data()+44*ix,44);world[j]=transform(w,v.x,v.y,v.z);auto clip=transform(wvp,v.x,v.y,v.z);
                if(clip[3]<=0){visible=false;break;}screen[j]={{(clip[0]/clip[3]*.5+.5)*2560,(.5-clip[1]/clip[3]*.5)*1440,clip[2]/clip[3]}};}
            if(!visible)continue;
            const Ogre::Vector3 a(world[1][0]-world[0][0],world[1][1]-world[0][1],world[1][2]-world[0][2]);
            const Ogre::Vector3 b(world[2][0]-world[0][0],world[2][1]-world[0][1],world[2][2]-world[0][2]);
            const auto normal=a.crossProduct(b);if(normal.squaredLength()<1e-10||std::abs(normal.y)>1e-6)continue;
            const double det=(screen[1][1]-screen[2][1])*(screen[0][0]-screen[2][0])+(screen[2][0]-screen[1][0])*(screen[0][1]-screen[2][1]);
            if(std::abs(det)<1e-10)continue;
            const double l0=((screen[1][1]-screen[2][1])*(px-screen[2][0])+(screen[2][0]-screen[1][0])*(py-screen[2][1]))/det;
            const double l1=((screen[2][1]-screen[0][1])*(px-screen[2][0])+(screen[0][0]-screen[2][0])*(py-screen[2][1]))/det,l2=1-l0-l1;
            if(l0<0||l1<0||l2<0)continue;const double z=l0*screen[0][2]+l1*screen[1][2]+l2*screen[2][2];
            if(z>=-1&&z<=1&&z<nearest){nearest=z;best=i/6*6;}
        }
        require(best<ic,"fixed native pixel is not covered by a selected vertical wall face");
        std::ostringstream o;o<<"{\"first_index\":"<<best<<",\"index_count\":6,\"native_pixel_top_left\":[350,1080],\"vertices\":[";
        std::set<std::uint32_t> used;
        for(std::size_t i=best;i<best+6;++i){std::uint32_t ix;std::memcpy(&ix,ib.data()+4*i,4);if(!used.insert(ix).second)continue;
            TerrainRenderVertex v;std::memcpy(&v,vb.data()+44*ix,44);auto wp=transform(w,v.x,v.y,v.z),cp=transform(wvp,v.x,v.y,v.z);
            if(used.size()>1)o<<',';o<<"{\"original_vertex_index\":"<<ix<<",\"local\":"<<numbers(std::array<float,3>{{v.x,v.y,v.z}})
                <<",\"world\":"<<numbers(wp)<<",\"clip\":"<<numbers(cp)<<"}";}
        o<<"],\"pixel_attribution\":\"GEOMETRIC_PROJECTION_ONLY_GPU_FRAGMENT_ID_NOT_OBSERVED\"}";return o.str();
    }
public:
    ShadowPatchCapture(const std::string& output,Ogre::SceneManager& sm,Ogre::Camera& c):directory(output),scene(sm),camera(c) {
        require(!std::filesystem::exists(directory),"output must be fresh");std::filesystem::create_directory(directory);
        text("begin.json","{\"schema\":\"hellomine3d-wetland-shadow-patch-v1\",\"status\":\"OPEN\",\"output_limit_bytes\":67108864}\n");scene.addRenderObjectListener(this);
    }
    ~ShadowPatchCapture() override {cleanup();scene.removeRenderObjectListener(this);}
    void detachRenderable(ChunkSectionRenderable& r)noexcept{if(selected==&r||(firstObject==&r&&completed<2)){fail("selected object replaced before both checkpoints");}}
    void begin(ChunkSectionRenderable& r,std::uint64_t id,const glm::ivec3& origin,const std::string& key,bool batch) {
        require(thread==std::this_thread::get_id()&&!open&&!failed&&completed<2,"invalid native frame preparation");
        require(std::chrono::steady_clock::now()-started<std::chrono::seconds(30),"30-second diagnostic deadline");
        require(origin.x>=100&&origin.x<=102&&origin.z>=-61&&origin.z<=-59&&origin.y>=0&&origin.y<8&&
            (!batch||origin.y==0||origin.y==4),"owner outside fixed diagnostic range");
        ownerOrigin=origin;ownerKey=key;ownerBatch=batch;
        selected=&r;frame=id;open=true;posted=false;pass=nullptr;packet.clear();selected->setNativeDrawObserver(this);
    }
    void notifyRenderSingleObject(Ogre::Renderable* r,const Ogre::Pass* p,const Ogre::AutoParamDataSource* source,const Ogre::LightList*,bool suppressed)override {
        if(!open||r!=selected)return;pass=nullptr;
        if(!suppressed&&source&&source->getCurrentCamera()==&camera)pass=p;
    }
    void beforeNativeDraw(ChunkSectionRenderable& r,Ogre::SceneManager* sm,Ogre::RenderSystem*)override {
        if(!open||&r!=selected||!pass)return;
        try{require(thread==std::this_thread::get_id()&&sm==&scene&&!posted&&!query,"unexpected or repeated main draw");
            require(pass->getPassIterationCount()==1&&!pass->hasGeometryProgram()&&!pass->hasTessellationHullProgram()&&!pass->hasTessellationDomainProgram(),"unsupported pass stages");
            GLint active=0;glGetQueryiv(GL_PRIMITIVES_GENERATED,GL_CURRENT_QUERY,&active);require(!active,"conflicting native primitive query");
            glGenQueries(1,&query);glBeginQuery(GL_PRIMITIVES_GENERATED,query);queryActive=true;require(glGetError()==GL_NO_ERROR,"native pre GL error");
        }catch(const std::exception&e){fail(e.what());throw;}
    }
    void afterNativeDraw(ChunkSectionRenderable& r,Ogre::SceneManager*,Ogre::RenderSystem*)override {
        if(!open||&r!=selected||!pass)return;
        try{
            require(query&&queryActive&&!posted,"unpaired native post");glEndQuery(GL_PRIMITIVES_GENERATED);queryActive=false;GLuint primitives=0;glGetQueryObjectuiv(query,GL_QUERY_RESULT,&primitives);glDeleteQueries(1,&query);query=0;
            Ogre::RenderOperation op;r.getRenderOperation(op);require(op.useIndexes&&op.operationType==Ogre::RenderOperation::OT_TRIANGLE_LIST&&op.numberOfInstances==1&&op.vertexData&&op.indexData&&op.vertexData->vertexStart==0&&op.indexData->indexStart==0&&primitives==op.indexData->indexCount/3&&primitives>0,"actual indexed draw differs");
            const auto& buffers=op.vertexData->vertexBufferBinding->getBindings();require(buffers.size()==1&&buffers.begin()->first==0,"actual source0 required");
            const auto& hw=buffers.begin()->second;auto* vb=dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(hw.get());auto* ib=dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());
            require(vb&&ib&&hw->getVertexSize()==44&&op.indexData->indexBuffer->getIndexSize()==4,"actual44B/u32 storage required");
            const auto& elements=op.vertexData->vertexDeclaration->getElements();require(elements.size()==5,"production vertex declaration count differs");
            for(const auto& e:elements){require(e.getSource()==0,"vertex declaration source differs");
                if(e.getSemantic()==Ogre::VES_POSITION)require(e.getOffset()==0&&e.getType()==Ogre::VET_FLOAT3,"position declaration differs");
                else{require(e.getSemantic()==Ogre::VES_TEXTURE_COORDINATES&&e.getIndex()<4,"vertex declaration semantic differs");
                    const std::array<std::size_t,4> offsets{{12,20,28,40}};const std::array<Ogre::VertexElementType,4> types{{Ogre::VET_FLOAT2,Ogre::VET_FLOAT2,Ogre::VET_FLOAT3,Ogre::VET_FLOAT1}};
                    require(e.getOffset()==offsets[e.getIndex()]&&e.getType()==types[e.getIndex()],"vertex declaration transport differs");}}
            const auto vbytes=hw->getSizeInBytes(),ibytes=op.indexData->indexBuffer->getSizeInBytes();require(vbytes<=ObjectLimit&&ibytes<=ObjectLimit-vbytes&&vbytes==op.vertexData->vertexCount*44&&ibytes==op.indexData->indexCount*4,"original storage extent exceeds bound");
            GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);require(program>0&&glIsProgram(program),"missing actual GL program");GLint linked=0,count=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);glGetProgramiv(program,GL_ATTACHED_SHADERS,&count);require(linked==GL_TRUE&&count==2,"unsupported actual linked stages");
            const auto* vs=dynamic_cast<const Ogre::GLSLShader*>(pass->getVertexProgram().get());const auto* fs=dynamic_cast<const Ogre::GLSLShader*>(pass->getFragmentProgram().get());require(vs&&fs&&pass->getVertexProgramName()=="HelloMine3D/TerrainShadowVertex"&&pass->getFragmentProgramName()=="HelloMine3D/TerrainShadowArrayFragment","fixed production receiver stages differ");
            std::array<GLuint,2> attached{};glGetAttachedShaders(program,2,nullptr,attached.data());require(std::find(attached.begin(),attached.end(),vs->getGLShaderHandle())!=attached.end()&&std::find(attached.begin(),attached.end(),fs->getGLShaderHandle())!=attached.end(),"native shaders do not match selected pass");
            Matrix world=matrix(program,"world"),wvp=matrix(program,"worldViewProj");Matrix expected{{1,0,0,0,0,1,0,0,0,0,1,0,
                float(ownerOrigin.x*CHUNK_SIZE),float(ownerOrigin.y*CHUNK_SIZE),float(ownerOrigin.z*CHUNK_SIZE),1}};
            require(world==expected,"selected original owner world transform differs");
            ReadState state;std::vector<unsigned char> vertices(vbytes),indices(ibytes);GLint64 size=0;
            glBindBuffer(GL_COPY_READ_BUFFER,vb->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);require(size==GLint64(vbytes),"native VBO size differs");glGetBufferSubData(GL_COPY_READ_BUFFER,0,vbytes,vertices.data());
            glBindBuffer(GL_COPY_READ_BUFFER,ib->getGLBufferId());glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE,&size);require(size==GLint64(ibytes),"native IBO size differs");glGetBufferSubData(GL_COPY_READ_BUFFER,0,ibytes,indices.data());
            const char* samplerName="directionalShadowMap";GLuint samplerIndex=GL_INVALID_INDEX;glGetUniformIndices(program,1,&samplerName,&samplerIndex);require(samplerIndex!=GL_INVALID_INDEX,"missing actual shadow sampler");
            GLint samplerType=0,samplerSize=0;glGetActiveUniformsiv(program,1,&samplerIndex,GL_UNIFORM_TYPE,&samplerType);glGetActiveUniformsiv(program,1,&samplerIndex,GL_UNIFORM_SIZE,&samplerSize);require(samplerType==GL_SAMPLER_2D&&samplerSize==1,"native shadow sampler type differs");
            const GLint samplerLoc=glGetUniformLocation(program,samplerName);require(samplerLoc>=0,"missing actual sampler location");GLint unit=-1;glGetUniformiv(program,samplerLoc,&unit);require(unit==1,"fixed shadow sampler unit differs");glActiveTexture(GL_TEXTURE0+unit);GLint texture=0,sampler=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
            const auto& ogreMap=scene.getShadowTexture(0);auto* native=dynamic_cast<Ogre::GL3PlusTexture*>(ogreMap.get());require(native&&texture==GLint(native->getGLID())&&glIsTexture(texture),"actual bound texture differs from original shadow map");
            GLint width=0,height=0,format=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);require(width==1024&&height==1024&&format==GL_R32F,"fixed R32F1024 map required");
            if(completed){require(firstObject==&r&&firstProgram==GLuint(program)&&firstVbo==vb->getGLBufferId()&&firstIbo==ib->getGLBufferId()&&firstMap==GLuint(texture)&&firstWorld==world&&firstWvp==wvp&&firstVertices==vertices&&firstIndices==indices,"original object/program/camera/storage identity changed");}
            else{firstObject=&r;firstProgram=program;firstVbo=vb->getGLBufferId();firstIbo=ib->getGLBufferId();firstMap=texture;firstWorld=world;firstWvp=wvp;firstVertices=vertices;firstIndices=indices;}
            const auto prefix="checkpoint-"+std::to_string(completed+1);std::ostringstream o;o<<"{\"native_frame\":"<<frame<<",\"owner_key\":"<<quote(ownerKey)<<",\"owner_kind\":"<<quote(ownerBatch?"terrain_batch":"section")<<",\"owner_section\":"<<numbers(std::array<int,3>{{ownerOrigin.x,ownerOrigin.y,ownerOrigin.z}})<<",\"program\":"<<program<<",\"shaders\":"<<numbers(attached)<<",\"vbo\":"<<vb->getGLBufferId()<<",\"ibo\":"<<ib->getGLBufferId()<<",\"vertex_count\":"<<op.vertexData->vertexCount<<",\"index_count\":"<<op.indexData->indexCount<<",\"primitive_count\":"<<primitives<<",\"stride\":44,\"index_type\":\"u32\",\"native_uniform_layout\":\"column_major\",\"native_uniforms\":{\"world\":"<<numbers(world)<<",\"worldViewProj\":"<<numbers(wvp);
            for(const char* name:{"worldView","shadowWorldViewProj","directionalShadowViewProj"})o<<','<<quote(name)<<':'<<numbers(matrix(program,name));
            for(const char* name:{"directionalShadowEnabled","directionalShadowBias","directionalShadowStrength","directionalShadowFadeStart","directionalShadowFadeEnd"})o<<','<<quote(name)<<':'<<scalar(program,name);
            const GLint sun=glGetUniformLocation(program,"sunDirection");require(sun>=0,"missing native sunDirection");std::array<float,3> direction{};glGetUniformfv(program,sun,direction.data());for(float x:direction)require(std::isfinite(x),"nonfinite sun");o<<",\"sunDirection\":"<<numbers(direction)<<"},\"wall_face\":"<<wall(vertices,indices,world,wvp)<<",\"shadow_sampler_unit\":"<<unit<<",\"shadow_texture\":"<<texture<<",\"sampler_object\":"<<sampler<<",\"effective_sampler\":{";
            bool first=true;for(auto pname:{GL_TEXTURE_MIN_FILTER,GL_TEXTURE_MAG_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T,GL_TEXTURE_COMPARE_MODE,GL_TEXTURE_COMPARE_FUNC}){GLint value=0;if(sampler)glGetSamplerParameteriv(sampler,pname,&value);else glGetTexParameteriv(GL_TEXTURE_2D,pname,&value);if(!first)o<<',';first=false;o<<quote(std::to_string(pname))<<':'<<value;}std::array<float,4> border{};if(sampler)glGetSamplerParameterfv(sampler,GL_TEXTURE_BORDER_COLOR,border.data());else glGetTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border.data());o<<",\"border_colour\":"<<numbers(border)<<"},\"shadow_map\":{\"width\":1024,\"height\":1024,\"internal_format\":\"GL_R32F\",\"read_format\":\"GL_RED/GL_FLOAT\",\"origin\":\"bottom_left\",\"byte_order\":\"host_native\"},\"inner_vertex_fetch\":\"OPEN_NOT_OBSERVED\"}";
            state.tight();std::vector<float> map(1024u*1024u);glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,map.data());for(float x:map)require(std::isfinite(x)&&x>=0&&x<=1,"invalid native shadow depth");state.restore();require(state.verify()&&glGetError()==GL_NO_ERROR,"native readback state/GL failure");
            require(o.str().size()<=64u*1024u,"metadata limit");write(prefix+".vbo0.bin",vertices.data(),vertices.size());write(prefix+".ibo.bin",indices.data(),indices.size());write(prefix+".shadow-r32f.bin",map.data(),map.size()*sizeof(float));packet=o.str();posted=true;pass=nullptr;
        }catch(const std::exception&e){fail(e.what());throw;}
    }
    void commit(const OgreRenderCapture::ShadowPatchCheckpoint& cp,float worldTime) {
        try{require(open&&posted&&cp.serial==completed+1&&cp.nativeFrame==frame&&cp.targetMs==(completed?9000:5000),"original PNG/native draw checkpoint mismatch");
            require(std::filesystem::is_regular_file(cp.path)&&std::filesystem::file_size(cp.path)>0&&std::filesystem::file_size(cp.path)<=PngLimit&&std::isfinite(worldTime),"missing/oversize original capture or time");
            std::ostringstream o;o<<std::setprecision(12)<<"{\"schema\":\"hellomine3d-wetland-shadow-patch-v1\",\"status\":\"CAPTURED\",\"normal_input\":false,\"capture_serial\":"<<cp.serial<<",\"target_ms\":"<<cp.targetMs<<",\"accepted_elapsed_ms\":"<<cp.elapsedMs<<",\"clock\":\"EXPLICIT_DIAGNOSTIC_FRAME_STARTED\",\"png_phase\":\"PRE_SWAP_ORIGINAL_BACK_BUFFER\",\"original_png\":"<<quote(cp.path)<<",\"world_time\":"<<worldTime<<",\"native_draw\":"<<packet<<"}\n";text("checkpoint-"+std::to_string(cp.serial)+".json",o.str());++completed;cleanup();
        }catch(const std::exception&e){fail(e.what());throw;}
    }
    void updateOriginalCapture(OgreRenderCapture& capture) {
        try{ReadState state;capture.update(0.f);state.restore();require(state.verify()&&glGetError()==GL_NO_ERROR,"original PNG readback state/GL failure");}
        catch(const std::exception&e){fail(e.what());throw;}
    }
    void checkDeadline(){try{require(!failed&&std::chrono::steady_clock::now()-started<std::chrono::seconds(30),"failed session or30-second diagnostic deadline");}catch(const std::exception&e){fail(e.what());throw;}}
};
