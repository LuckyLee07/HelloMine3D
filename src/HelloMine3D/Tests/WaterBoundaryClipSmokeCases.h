#pragma once
#include "../World/Chunk/WaterBoundaryClip.h"

namespace {
using WaterBoundaryFields=std::array<float,7>;
WaterBoundaryFields waterBoundaryFields(const ChunkMesh &source,std::size_t index) {
    const auto &mesh=source.getClientMesh();const auto light=source.getLightSources()[index];
    return {mesh.textureCoords[index*2],mesh.textureCoords[index*2+1],
        mesh.textureRepeatCoords[index*2],mesh.textureRepeatCoords[index*2+1],
        source.getLight()[index],light.x,light.y};
}
bool sampleWaterBoundary(const ChunkMesh &source,const glm::vec3 &point,int axis,float sign,WaterBoundaryFields &value) {
    const auto &mesh=source.getClientMesh();const int u=(axis+1)%3,v=(axis+2)%3;
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        std::array<glm::vec3,3> p{};
        for(int k=0;k<3;++k) {const auto n=mesh.indices[i+k]*3;p[k]={mesh.vertexPositions[n],mesh.vertexPositions[n+1],mesh.vertexPositions[n+2]};}
        const auto normal=glm::cross(p[1]-p[0],p[2]-p[0]);
        if(normal[axis]*sign<=0 || p[0][axis]!=point[axis] || p[1][axis]!=point[axis] || p[2][axis]!=point[axis])continue;
        const auto a=p[1]-p[0],b=p[2]-p[0],q=point-p[0];const float determinant=a[u]*b[v]-a[v]*b[u];
        const float one=(q[u]*b[v]-q[v]*b[u])/determinant,two=(a[u]*q[v]-a[v]*q[u])/determinant;
        if(one<-.00001f || two<-.00001f || one+two>1.00001f)continue;
        value.fill(0);
        for(int k=0;k<3;++k) {const auto fields=waterBoundaryFields(source,mesh.indices[i+k]);const float weight=k==0?1-one-two:k==1?one:two;
            for(int field=0;field<7;++field)value[field]+=fields[field]*weight;}
        return true;
    }
    return false;
}
std::vector<std::array<float,33>> waterBoundaryTop(const ChunkMesh &source,float y) {
    const auto &mesh=source.getClientMesh();std::vector<std::array<float,33>> result;
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        std::array<glm::vec3,3> p{};
        for(int k=0;k<3;++k) {const auto n=mesh.indices[i+k]*3;p[k]={mesh.vertexPositions[n],mesh.vertexPositions[n+1],mesh.vertexPositions[n+2]};}
        if(p[0].y!=y || p[1].y!=y || p[2].y!=y || glm::cross(p[1]-p[0],p[2]-p[0]).y<=0)continue;
        std::array<float,33> triangle{};
        for(int k=0;k<3;++k) {for(int axis=0;axis<3;++axis)triangle[k*11+axis]=p[k][axis];
            const auto fields=waterBoundaryFields(source,mesh.indices[i+k]);for(int f=0;f<7;++f)triangle[k*11+3+f]=fields[f];
            triangle[k*11+10]=source.getRootTags()[mesh.indices[i+k]];}
        result.push_back(triangle);
    }
    return result;
}
void caseWaterCompoundBoundary() {
    struct RestorePolicy {bool old=BlockDatabase::get().waterBoundaryPinsAvailable();~RestorePolicy(){BlockDatabase::get().setWaterBoundaryPinsAvailable(old);}} restore;
    Config config=makeConfig();Camera camera(config);Player player;
    World world(camera,config,player,freshSaveDirectory("water_compound_boundary"),false,0);
    auto &manager=world.getChunkManager();auto &chunk=manager.getOrCreateChunk(-8,-8);
    chunk.transitionDataResidency(ChunkDataResidencyState::Requested);chunk.transitionDataResidency(ChunkDataResidencyState::Loading);
    std::vector<Block_t> ids(CHUNK_VOLUME*5,0);std::vector<BlockMetadata_t> metadata(ids.size(),0);chunk.loadBlockData(5,ids,metadata);
    chunk.setBlock(8,66,8,BlockId::Water);
    for(int z=5;z<=11;++z)for(int y=64;y<=69;++y)for(int x=5;x<=11;++x)chunk.setBlockLight(x,y,z,static_cast<LightLevel>((x*3+y+z*2)%14+1));
    auto count=manager.getChunks().size();auto *section=chunk.findSection(4);
    const auto build=[](const SectionMeshInput &input){ChunkMeshCollection meshes;ChunkMeshBuilder(input,meshes).buildMesh();return meshes;};
    // Exercise all world axes/signs independently; yaw does not replace the
    // X-facing coverage layout (u=Y,v=Z) or face-specific lighting order.
    const std::array<glm::ivec3,4> offsets{{{-1,0,0},{1,0,0},{0,0,1},{0,0,-1}}};
    const std::array<unsigned,4> neighbourBoundary{{3,2,1,0}};
    const auto run=[&](int id,int yaw,int face,glm::ivec3 water,Chunk &bankChunk,glm::ivec3 bank,const std::string &prefix) {
        bankChunk.setBlock(bank.x,bank.y,bank.z,ChunkBlock(static_cast<Block_t>(id),static_cast<BlockMetadata_t>(yaw)));
        const auto &definition=BlockDatabase::get().getDefinition(static_cast<BlockId>(id));
        const auto mask=definition.render.shape.variants[yaw].boundaryCoverage[neighbourBoundary[face]];
        BlockDatabase::get().setWaterBoundaryPinsAvailable(false);SectionMeshInput oldInput;section->captureMeshInput(oldInput);const auto oldMesh=build(oldInput);
        BlockDatabase::get().setWaterBoundaryPinsAvailable(true);SectionMeshInput input;section->captureMeshInput(input);
        // The builder must keep its captured capability when the global test
        // policy changes, without reading a live renderer/World flag.
        BlockDatabase::get().setWaterBoundaryPinsAvailable(false);const auto meshes=build(input);
        const auto &mesh=meshes.waterMesh.getClientMesh();float area=0;int vertices=0,triangles=0;bool exposed=true,attributes=true,pins=true,shore=true;
        const int axis=face<2?0:2,u=(axis+1)%3,v=(axis+2)%3;
        const float sign=face==0 || face==3?-1.f:1.f;
        const glm::vec3 origin{water.x-128.f,static_cast<float>(water.y),water.z-128.f};
        const float plane=origin[axis]+(sign>0?1.f:0.f);
        for(std::size_t i=0;i<mesh.indices.size();i+=3) {
            std::array<glm::vec3,3> p{};
            for(int k=0;k<3;++k) {const auto n=mesh.indices[i+k]*3;p[k]={mesh.vertexPositions[n],mesh.vertexPositions[n+1],mesh.vertexPositions[n+2]};}
            const auto normal=glm::cross(p[1]-p[0],p[2]-p[0]);
            if(p[0][axis]!=plane || p[1][axis]!=plane || p[2][axis]!=plane || normal[axis]*sign<=0)continue;
            ++triangles;area+=normal[axis]*sign*.5f;const auto middle=(p[0]+p[1]+p[2])/3.f-origin;
            exposed &= !WaterBoundaryClip::covered(mask,int(std::floor(middle[u]*8)),int(std::floor(middle[v]*8)));
            for(int k=0;k<3;++k) {
                ++vertices;WaterBoundaryFields expected{};attributes &= sampleWaterBoundary(oldMesh.waterMesh,p[k],axis,sign,expected);
                const auto actual=waterBoundaryFields(meshes.waterMesh,mesh.indices[i+k]);
                for(int field=0;field<7;++field)attributes &= std::abs(actual[field]-expected[field])<.00002f;
                const float tag=meshes.waterMesh.getRootTags()[mesh.indices[i+k]];
                const auto local=p[k]-origin;
                const bool expectedPin=mask!=0 && WaterBoundaryClip::pin(mask,{local[u],local[v]},u==1?0:1);
                pins &= tag==(expectedPin?1.f:0.f);
                // The active-capability VS removes shore waves, while keeping
                // the old -0.10 offset at unpinned corners. The shared bank top
                // therefore stays at .90, above every eighth-grid cut (.875).
                if(local.y>0.f)shore &= actual[3]>0.f;
                if(local.y==1.f)shore &= actual[3]>=.25f && tag==0.f;
                if(tag==1.f)shore &= local.y<=.875f && .9f-local.y>=.02499f;
            }
        }
        // Sample the independently emitted top face as well as the side:
        // both endpoints and the new midpoint must share positive raw shore.
        for(float along:{0.f,.5f,1.f}) {
            auto point=origin;point.y+=1.f;point[axis]=plane;point[axis==0?2:0]+=along;
            WaterBoundaryFields top{};
            shore &= sampleWaterBoundary(meshes.waterMesh,point,1,1.f,top) && top[3]>=.25f;
        }
        const auto label=prefix+"compound-"+std::to_string(id)+"-yaw"+std::to_string(yaw)+"-face"+std::to_string(face)+"/";
        check(label+"exact-exposed-area-and-bounded-valid-triangles",exposed && std::abs(area-(64-__builtin_popcountll(mask))/64.f)<.00001f && triangles<=WaterBoundaryClip::MaximumTriangles && vertices<=WaterBoundaryClip::MaximumVertices);
        check(label+"original-triangle-depth-shore-drift-light-sources",attributes && (area==0 || vertices>0));
        const auto originalTop=waterBoundaryTop(oldMesh.waterMesh,67);
        check(label+"top-geometry-and-all-upload-attributes-byte-identical",!originalTop.empty() && originalTop==waterBoundaryTop(meshes.waterMesh,67));
        check(label+"only-real-opaque-cut-contour-pinned",pins);
        check(label+"shore-corners-and-midpoints-bound-high-cut-below-shared-top",shore);
        const auto neighbour=water+offsets[face];
        check(label+"snapshot-capability-independent-and-resident-only",!oldInput.waterBoundaryPinsAvailable() && input.waterBoundaryPinsAvailable() && manager.getChunks().size()==count && input.getBlock(neighbour.x,neighbour.y-64,neighbour.z).metadata==yaw);
        bankChunk.setBlock(bank.x,bank.y,bank.z,BlockId::Air);
    };
    for(int id=33;id<=44;++id)for(int yaw=0;yaw<4;++yaw)for(int face=0;face<4;++face)
        run(id,yaw,face,{8,66,8},chunk,glm::ivec3(8,66,8)+offsets[face],"WATER_DEPTH/");
    chunk.setBlock(8,66,8,BlockId::Air);
    for(int direction=0;direction<2;++direction) {
        auto &bankChunk=manager.getOrCreateChunk(-8+(direction==0),-8+(direction==1));
        bankChunk.transitionDataResidency(ChunkDataResidencyState::Requested);bankChunk.transitionDataResidency(ChunkDataResidencyState::Loading);
        bankChunk.loadBlockData(5,ids,metadata);count=manager.getChunks().size();
        const glm::ivec3 water=direction==0?glm::ivec3(15,66,8):glm::ivec3(8,66,15);
        const glm::ivec3 bank=direction==0?glm::ivec3(0,66,8):glm::ivec3(8,66,0);
        chunk.setBlock(water.x,water.y,water.z,BlockId::Water);
        for(int yaw=0;yaw<4;++yaw)run(33,yaw,direction==0?1:2,water,bankChunk,bank,"WATER_DEPTH/section-halo-");
        chunk.setBlock(water.x,water.y,water.z,BlockId::Air);
    }
    chunk.setBlock(8,66,8,BlockId::Water);
    // A legacy transparent neighbour keeps the identical old mesh path even
    // with the new renderer capability available.
    chunk.setBlock(8,66,7,BlockId::Glass);
    BlockDatabase::get().setWaterBoundaryPinsAvailable(false);SectionMeshInput glassOld;section->captureMeshInput(glassOld);const auto before=build(glassOld);
    BlockDatabase::get().setWaterBoundaryPinsAvailable(true);SectionMeshInput glassNew;section->captureMeshInput(glassNew);const auto after=build(glassNew);
    const auto &a=before.waterMesh.getClientMesh(),&b=after.waterMesh.getClientMesh();
    check("WATER_DEPTH/legacy-transparent-neighbour-keeps-old-mesh-bytes",a.vertexPositions==b.vertexPositions && a.textureCoords==b.textureCoords && a.textureRepeatCoords==b.textureRepeatCoords && a.indices==b.indices && before.waterMesh.getLight()==after.waterMesh.getLight() && before.waterMesh.getLightSources()==after.waterMesh.getLightSources() && before.waterMesh.getRootTags()==after.waterMesh.getRootTags());
}
} // namespace
