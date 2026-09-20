#pragma once

namespace {
void caseTerrainBankMeshTopology()
{
    setEnv("HELLOMINE3D_SEED","20260807");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig(); Camera camera(config); Player owner;
    World world(camera,config,owner,freshSaveDirectory("terrain_bank_topology"),false,0);
    // A diagonal stair shore at a real survey origin exposes triangular holes,
    // shared-index collisions and missing half faces that a flat slab cannot.
    for(const glm::ivec2 location: {glm::ivec2(77,-22),glm::ivec2(-78,21)}) {
        world.getChunkManager().loadChunk(location.x,location.y);
        const int ox=location.x*CHUNK_SIZE,oz=location.y*CHUNK_SIZE,oy=192;
        for(int x=0;x<CHUNK_SIZE;++x)for(int z=0;z<CHUNK_SIZE;++z)for(int y=0;y<CHUNK_SIZE;++y) {
            const bool inside=x>=2 && x<=13 && z>=2 && z<=13;
            const int height=2+(x+z)/4;
            world.setBlock(ox+x,oy+y,oz+z,inside && y<height?BlockId::Sand:BlockId::Air);
        }
        auto *section=world.getChunkManager().getChunk(location.x,location.y).findSection(oy/CHUNK_SIZE);
        check("BANK_MESH/section-"+std::to_string(location.x),section!=nullptr);
        if(!section)continue;
        SectionMeshInput input; section->captureMeshInput(input);
        double expectedArea=0;
        const glm::ivec3 offsets[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        for(int x=0;x<CHUNK_SIZE;++x)for(int y=0;y<CHUNK_SIZE;++y)for(int z=0;z<CHUNK_SIZE;++z) {
            if(input.getBlock(x,y,z)!=BlockId::Sand)continue;
            for(const auto d:offsets)expectedArea+=input.getBlock(x+d.x,y+d.y,z+d.z)!=BlockId::Sand;
        }
        for(bool ao: {false,true}) {
            ChunkMeshCollection meshes; ChunkMeshBuilder(input,meshes,ao).buildMesh();
            const auto &mesh=meshes.solidMesh.getClientMesh();
            bool planar=true,supported=true; double area=0;
            for(std::size_t i=0;i<mesh.indices.size();i+=3) {
                glm::vec3 p[3];
                for(int j=0;j<3;++j) {
                    const auto v=mesh.indices[i+j]*3;
                    p[j]={mesh.vertexPositions[v]-ox,mesh.vertexPositions[v+1]-oy,mesh.vertexPositions[v+2]-oz};
                }
                const auto cross=glm::cross(p[1]-p[0],p[2]-p[0]);
                const float length=glm::length(cross);
                const int axes=(std::abs(cross.x)>.0001f)+(std::abs(cross.y)>.0001f)+(std::abs(cross.z)>.0001f);
                planar &= axes==1 && length>.0001f; area+=length*.5;
                if(length<.0001f)continue;
                const auto normal=cross/length;
                // Irrational-looking barycentric weights avoid integer voxel
                // boundaries and the diagonal shared by the two triangles.
                for(const glm::vec3 weights: {glm::vec3(.19f,.34f,.47f),glm::vec3(.61f,.23f,.16f),glm::vec3(.12f,.67f,.21f)}) {
                    const auto point=p[0]*weights.x+p[1]*weights.y+p[2]*weights.z;
                    const auto inner=glm::ivec3(glm::floor(point-normal*.002f));
                    const auto outer=glm::ivec3(glm::floor(point+normal*.002f));
                    supported &= input.getBlock(inner.x,inner.y,inner.z)==BlockId::Sand &&
                        input.getBlock(outer.x,outer.y,outer.z)!=BlockId::Sand;
                }
            }
            const std::string suffix=std::to_string(location.x)+(ao?"-ao":"-plain");
            check("BANK_MESH/axis-aligned-outward-triangles-"+suffix,planar && supported);
            check("BANK_MESH/all-exposed-face-area-"+suffix,std::abs(area-expectedArea)<.001,
                  "area="+std::to_string(area)+" expected="+std::to_string(expectedArea));
        }
    }
    bool realPlanar=true,realSupported=true; double realArea=0,realExpected=0; int triangles=0;
    for(int cx=77;cx<=78;++cx)for(int cz=-23;cz<=-22;++cz) {
        world.getChunkManager().loadChunk(cx,cz);
        for(int sy=3;sy<=5;++sy) {
            auto *section=world.getChunkManager().getChunk(cx,cz).findSection(sy);
            if(!section)continue;
            SectionMeshInput input;section->captureMeshInput(input);
            const glm::ivec3 offsets[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            for(int x=0;x<CHUNK_SIZE;++x)for(int y=0;y<CHUNK_SIZE;++y)for(int z=0;z<CHUNK_SIZE;++z) {
                if(input.getBlock(x,y,z)!=BlockId::Sand)continue;
                for(const auto d:offsets) {
                    const auto adjacent=input.getBlock(x+d.x,y+d.y,z+d.z);
                    realExpected+=BlockDatabase::get().getDefinition(static_cast<BlockId>(adjacent.id)).transparent;
                }
            }
            ChunkMeshCollection meshes;ChunkMeshBuilder(input,meshes).buildMesh();
            const auto &mesh=meshes.solidMesh.getClientMesh();
            for(std::size_t i=0;i<mesh.indices.size();i+=3) {
                const auto uv=mesh.indices[i]*2;
                if(std::floor(mesh.textureCoords[uv]*16)!=7 || std::floor(mesh.textureCoords[uv+1]*16)!=0)continue;
                ++triangles;glm::vec3 p[3];
                for(int j=0;j<3;++j) {
                    const auto v=mesh.indices[i+j]*3;
                    p[j]={mesh.vertexPositions[v]-cx*CHUNK_SIZE,mesh.vertexPositions[v+1]-sy*CHUNK_SIZE,mesh.vertexPositions[v+2]-cz*CHUNK_SIZE};
                }
                const auto cross=glm::cross(p[1]-p[0],p[2]-p[0]);const float length=glm::length(cross);
                const int axes=(std::abs(cross.x)>.0001f)+(std::abs(cross.y)>.0001f)+(std::abs(cross.z)>.0001f);
                realPlanar &= axes==1 && length>.0001f;realArea+=length*.5;
                if(length<.0001f)continue;
                const auto point=p[0]*.19f+p[1]*.34f+p[2]*.47f,normal=cross/length;
                const auto inner=glm::ivec3(glm::floor(point-normal*.002f));
                realSupported &= input.getBlock(inner.x,inner.y,inner.z)==BlockId::Sand;
            }
        }
    }
    check("BANK_MESH/actual-survey-shore-triangles",realPlanar && realSupported && triangles>100,
          "triangles="+std::to_string(triangles));
    check("BANK_MESH/actual-survey-shore-exposed-area",std::abs(realArea-realExpected)<.001,
          "area="+std::to_string(realArea)+" expected="+std::to_string(realExpected));
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
