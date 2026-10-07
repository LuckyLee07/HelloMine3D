#pragma once

namespace {

// Real production mesh output, in an isolated World with no loader thread.
// Derived states are prepared here only to exercise the read-only replay API;
// renderer absence is deliberately not represented in authoritative World.
ChunkSection& retainedMeshFixtureSection(Chunk& chunk, int sectionY)
{
    // captureMeshInput reads the upper layer; allocate it before taking the
    // section reference so vector growth cannot invalidate the fixture.
    chunk.setBlock(0,(sectionY+1)*CHUNK_SIZE,0,BlockId::Air);
    for(int y=0;y<CHUNK_SIZE;++y)
        for(int z=0;z<CHUNK_SIZE;++z)
            for(int x=0;x<CHUNK_SIZE;++x)
                chunk.setBlock(x,sectionY*CHUNK_SIZE+y,z,BlockId::Air);
    chunk.setBlock(8,sectionY*CHUNK_SIZE+8,8,BlockId::Stone);
    auto* section=chunk.findSection(sectionY);
    if(!section || !section->makeMesh() || section->getMeshes().solidMesh.getClientMesh().indices.empty())
        throw std::runtime_error("Retained mesh fixture did not build a visible production mesh");
    return *section;
}

WorldSectionMeshVersion retainedMeshVersion(const Chunk& chunk,const ChunkSection& section)
{
    return {section.getLocation(),section.getBlockRevision(),chunk.getIncarnation(),section.getMeshState()};
}

bool retainedMeshSamePart(const ChunkMesh& a,const ChunkMesh& b)
{
    const auto& x=a.getClientMesh();const auto& y=b.getClientMesh();
    return x.vertexPositions==y.vertexPositions && x.textureCoords==y.textureCoords &&
        x.textureRepeatCoords==y.textureRepeatCoords && x.indices==y.indices &&
        a.getLight()==b.getLight() && a.getLightSources()==b.getLightSources() &&
        a.getRootTags()==b.getRootTags() && a.faces==b.faces;
}

bool retainedMeshSame(const ChunkMeshCollection& a,const ChunkMeshCollection& b)
{
    return retainedMeshSamePart(a.solidMesh,b.solidMesh) &&
        retainedMeshSamePart(a.transparentMesh,b.transparentMesh) &&
        retainedMeshSamePart(a.waterMesh,b.waterMesh) &&
        retainedMeshSamePart(a.floraMesh,b.floraMesh);
}

void caseRetainedSectionMeshReplay()
{
    CaveBoundaryFixture fixture("retained_clean_mesh_replay");
    World& world=*fixture.world;auto& manager=world.getChunkManager();
    manager.loadChunk(CaveBoundaryCenterX,CaveBoundaryCenterZ);
    Chunk& chunk=*manager.findChunk(CaveBoundaryCenterX,CaveBoundaryCenterZ);
    auto& section=retainedMeshFixtureSection(chunk,12);
    check("RETAINED_MESH/cpu-ready-is-not-retained-clean",world.observeRetainedSectionMeshes({retainedMeshVersion(chunk,section)},0).sections.empty());
    auto wrongAck=retainedMeshVersion(chunk,section);++wrongAck.incarnation;
    world.acknowledgeSectionMeshUploads({wrongAck});
    check("RETAINED_MESH/nonzero-incarnation-ack-rejects-aba",section.getMeshState()==ChunkMeshState::CpuReady);
    world.acknowledgeSectionMeshUploads({retainedMeshVersion(chunk,section)});
    check("RETAINED_MESH/current-incarnation-ack-confirms-normal-upload",section.getMeshState()==ChunkMeshState::Clean);
    const auto request=retainedMeshVersion(chunk,section);
    const auto beforeMesh=section.getMeshes();
    check("RETAINED_MESH/fixture-save-before-observation",world.save());
    const auto beforeFiles=caveBoundaryFiles(fixture.directory);
    const auto beforeBlocks=caveBoundaryBlocks(world);
    const auto ready=world.collectSectionMeshSnapshot(false);
    check("RETAINED_MESH/clean-output-is-not-a-normal-cpu-ready-offer",
        std::none_of(ready.cpuReadySections.begin(),ready.cpuReadySections.end(),[&](const auto& s){return s.location==request.location;}));
    auto replay=world.observeRetainedSectionMeshes({request},ready.cpuReadySections.size());
    check("RETAINED_MESH/current-near-resident-clean-copy",
        replay.sections.size()==1 && replay.sections[0].retainedCleanReplay &&
        replay.sections[0].location==request.location && replay.sections[0].blockRevision==request.blockRevision &&
        replay.sections[0].incarnation==request.incarnation && replay.sections[0].meshState==ChunkMeshState::Clean &&
        retainedMeshSame(replay.sections[0].meshes,beforeMesh));
    check("RETAINED_MESH/query-preserves-state-revision-incarnation",
        section.getMeshState()==ChunkMeshState::Clean && section.getBlockRevision()==request.blockRevision &&
        chunk.getIncarnation()==request.incarnation && chunk.getDataResidencyState()==ChunkDataResidencyState::Resident);
    check("RETAINED_MESH/query-preserves-blocks-save-bytes-and-dirty-save-flag",
        caveBoundaryBlocks(world)==beforeBlocks && caveBoundaryFiles(fixture.directory)==beforeFiles && !chunk.needsSave());
    if(replay.sections.empty())throw std::runtime_error("Retained mesh copy missing");
    replay.sections[0].meshes.solidMesh.clearClientData();
    const auto repeated=world.observeRetainedSectionMeshes({request},0);
    check("RETAINED_MESH/copied-payload-does-not-alias-world",
        repeated.sections.size()==1 && retainedMeshSame(section.getMeshes(),beforeMesh) && retainedMeshSame(repeated.sections[0].meshes,beforeMesh));
    auto wrong=request;++wrong.blockRevision;
    check("RETAINED_MESH/stale-revision-rejected",world.observeRetainedSectionMeshes({wrong},0).sections.empty());
    wrong=request;++wrong.incarnation;
    check("RETAINED_MESH/stale-incarnation-rejected",world.observeRetainedSectionMeshes({wrong},0).sections.empty());
    wrong=request;wrong.meshState=ChunkMeshState::CpuReady;
    check("RETAINED_MESH/non-clean-request-rejected",world.observeRetainedSectionMeshes({wrong},0).sections.empty());
    check("RETAINED_MESH/duplicate-request-copied-once",world.observeRetainedSectionMeshes({request,request},0).sections.size()==1);
    const auto countBefore=manager.getChunks().size();
    wrong=request;wrong.location={1000,12,1000};
    check("RETAINED_MESH/absent-find-only-does-not-load",
        world.observeRetainedSectionMeshes({wrong},0).sections.empty() && manager.getChunks().size()==countBefore);
    const int farX=CaveBoundaryCenterX+4;
    manager.loadChunk(farX,CaveBoundaryCenterZ);
    auto& farChunk=*manager.findChunk(farX,CaveBoundaryCenterZ);
    auto& farSection=retainedMeshFixtureSection(farChunk,12);farSection.markMeshClean();
    const auto farRequest=retainedMeshVersion(farChunk,farSection);
    check("RETAINED_MESH/resident-outside-near-rejected",
        world.observeRetainedSectionMeshes({farRequest},0).sections.empty() && farSection.getMeshState()==ChunkMeshState::Clean);
    world.preloadAround({float(farX*CHUNK_SIZE+8),200.f,float(CaveBoundaryCenterZ*CHUNK_SIZE+8)});
    const auto interest=world.collectDebugStats().spatialInterest;
    auto& residentOnlySection=*farChunk.findSection(12);
    if(residentOnlySection.getMeshState()!=ChunkMeshState::Clean) {
        residentOnlySection.makeMesh();residentOnlySection.markMeshClean();
    }
    const auto residentOnlyRequest=retainedMeshVersion(farChunk,residentOnlySection);
    check("RETAINED_MESH/resident-only-preload-remains-ineligible",
        interest.residentDataCells>interest.nearRepresentationCells &&
        world.observeRetainedSectionMeshes({residentOnlyRequest},0).sections.empty() &&
        residentOnlySection.getMeshState()==ChunkMeshState::Clean && residentOnlySection.getBlockRevision()==residentOnlyRequest.blockRevision);

    // A production-built Air section is a legitimate retained empty mesh,
    // distinct from a missing or stale renderer representation.
    for(int y=0;y<CHUNK_SIZE;++y)
        for(int z=0;z<CHUNK_SIZE;++z)
            for(int x=0;x<CHUNK_SIZE;++x)
                chunk.setBlock(x,11*CHUNK_SIZE+y,z,BlockId::Air);
    auto* emptySection=chunk.findSection(11);emptySection->makeMesh();
    world.acknowledgeSectionMeshUploads({retainedMeshVersion(chunk,*emptySection)});
    const auto emptyVersion=retainedMeshVersion(chunk,*emptySection);
    const auto emptyReplay=world.observeRetainedSectionMeshes({emptyVersion},0);
    check("RETAINED_MESH/legal-current-clean-empty-copy-keeps-state-and-revision",
        emptyReplay.sections.size()==1 && emptyReplay.sections[0].meshState==ChunkMeshState::Clean &&
        emptyReplay.sections[0].incarnation==emptyVersion.incarnation &&
        emptyReplay.sections[0].meshes.solidMesh.getClientMesh().indices.empty() &&
        emptyReplay.sections[0].meshes.transparentMesh.getClientMesh().indices.empty() &&
        emptyReplay.sections[0].meshes.waterMesh.getClientMesh().indices.empty() &&
        emptyReplay.sections[0].meshes.floraMesh.getClientMesh().indices.empty() &&
        emptySection->getMeshState()==ChunkMeshState::Clean && emptySection->getBlockRevision()==emptyVersion.blockRevision);

    // Eight actual CpuReady offers exhaust the same production budget first.
    for(int y=4;y<=10;++y)retainedMeshFixtureSection(chunk,y);
    retainedMeshFixtureSection(chunk,13);
    const auto normal=world.collectSectionMeshSnapshot(false);
    const auto noBudget=world.observeRetainedSectionMeshes({request},normal.cpuReadySections.size());
    check("RETAINED_MESH/normal-cpu-ready-offers-exhaust-shared-eight-budget",
        normal.cpuReadySections.size()==ChunkRuntime::MaxSectionUploadsPerFrame && noBudget.sections.empty() &&
        noBudget.cpuReadyUploadsReserved==normal.cpuReadySections.size());
    if(normal.cpuReadySections.empty())throw std::runtime_error("Normal upload offers missing");
    const auto& compatibilityOffer=normal.cpuReadySections.front();
    // The original two-field acknowledgement explicitly remains the zero-inc
    // compatibility domain; production snapshots now carry the real inc.
    world.acknowledgeSectionMeshUploads({{compatibilityOffer.location,compatibilityOffer.blockRevision}});
    check("RETAINED_MESH/original-zero-incarnation-ack-compatibility",
        chunk.findSection(compatibilityOffer.location.y)->getMeshState()==ChunkMeshState::Clean);
    std::vector<WorldSectionMeshVersion> acknowledgements;
    for(const auto& offered:normal.cpuReadySections)acknowledgements.push_back(offered);
    world.acknowledgeSectionMeshUploads(acknowledgements);
    std::vector<WorldSectionMeshVersion> requests;
    for(int y=10;y>=4;--y)requests.push_back(retainedMeshVersion(chunk,*chunk.findSection(y)));
    requests.push_back(request);
    const auto all=world.observeRetainedSectionMeshes(requests,0);
    check("RETAINED_MESH/eight-current-clean-copies-bounded-and-prioritized",
        all.sections.size()==8 && all.sections.front().location.y==4 && all.sections.back().location.y==12 &&
        std::all_of(all.sections.begin(),all.sections.end(),[](const auto& s){return s.meshState==ChunkMeshState::Clean && s.retainedCleanReplay;}));
    const auto one=world.observeRetainedSectionMeshes(requests,7);
    check("RETAINED_MESH/seven-normal-reservations-leave-one-replay",
        one.sections.size()==1 && one.sections[0].location.y==4 && one.cpuReadyUploadsReserved+one.sections.size()==8);
    bool oversize=false;try{auto tooMany=requests;tooMany.push_back(request);world.observeRetainedSectionMeshes(tooMany,0);}catch(const std::invalid_argument&){oversize=true;}
    check("RETAINED_MESH/request-over-eight-hard-rejected",oversize);
    oversize=false;try{world.observeRetainedSectionMeshes({request},9);}catch(const std::invalid_argument&){oversize=true;}
    check("RETAINED_MESH/reservation-over-eight-hard-rejected",oversize);
    // Higher fixture sections may have grown the vector; reacquire the lower
    // section instead of retaining a pointer across that growth.
    auto& dirtySection=*chunk.findSection(12);
    dirtySection.invalidateMeshInput();
    const auto dirty=retainedMeshVersion(chunk,dirtySection);
    check("RETAINED_MESH/current-dirty-output-rejected-without-rebuild",
        world.observeRetainedSectionMeshes({dirty},0).sections.empty() && dirtySection.getMeshState()==ChunkMeshState::Dirty &&
        dirtySection.getBlockRevision()==dirty.blockRevision);
    dirtySection.markMeshQueued();
    check("RETAINED_MESH/queued-output-rejected",world.observeRetainedSectionMeshes({retainedMeshVersion(chunk,dirtySection)},0).sections.empty());
    dirtySection.beginMeshBuild();
    check("RETAINED_MESH/building-output-rejected",world.observeRetainedSectionMeshes({retainedMeshVersion(chunk,dirtySection)},0).sections.empty());
    clearDeterministicEnv();
}

} // namespace
