// Included by the world smoke runner after the production tree/generator
// headers. These cases query pure plans only; they create no World or saves.
void caseNaturalTreeOwnership()
{
    const auto mixValue = [](std::uint64_t &hash, std::uint32_t value) {
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<std::uint8_t>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
    const auto planHash = [&](int seed, AdventureTreeKind kind, int stature) {
        std::uint64_t hash = 14695981039346656037ull;
        visitPolishedAdventureTreeBlocks(seed, 352, 75, -159, kind, stature,
            [&](int x, int y, int z, ChunkBlock block) {
                for (int value : {x, y, z, int(block.id), int(block.metadata)})
                    mixValue(hash, static_cast<std::uint32_t>(value));
            });
        return hash;
    };
    std::uint64_t aggregate = 14695981039346656037ull;
    for (auto kind : {AdventureTreeKind::Oak, AdventureTreeKind::Birch,
                     AdventureTreeKind::Spruce, AdventureTreeKind::Willow,
                     AdventureTreeKind::Cactus, AdventureTreeKind::Palm})
        for (int stature = 0; stature < 3; ++stature) {
            const auto hash = planHash(12345 + stature, kind, stature);
            mixValue(aggregate, static_cast<std::uint32_t>(hash));
            mixValue(aggregate, static_cast<std::uint32_t>(hash >> 32));
        }
    // Frozen from the unmodified v24 production write sequence, including
    // duplicate foliage/wood writes. Refactoring must retain random draws and
    // write order as well as the final materials of all six existing shapes.
    check("TREE_OWNER/frozen-v24-production-write-order",
          aggregate == 10051525680494606488ull);

    bool bounded = true;
    for (auto kind : {AdventureTreeKind::Oak, AdventureTreeKind::Birch,
                     AdventureTreeKind::Spruce, AdventureTreeKind::Willow,
                     AdventureTreeKind::Cactus, AdventureTreeKind::Palm})
        for (int stature = 0; stature < 3; ++stature)
            for (int seed = 0; seed < 64; ++seed) {
                std::size_t count = 0;
                visitPolishedAdventureTreeBlocks(seed, -17, 75, 16, kind, stature,
                    [&](int x, int y, int z, ChunkBlock block) {
                        ++count;
                        bounded &= std::abs(x + 17) <= PolishedTreeMaximumHorizontalRadius &&
                            std::abs(z - 16) <= PolishedTreeMaximumHorizontalRadius &&
                            y >= 75 && y <= 90 &&
                            (block.id == static_cast<Block_t>(BlockId::OakBark) ||
                             block.id == static_cast<Block_t>(BlockId::OakLeaf) ||
                             block.id == static_cast<Block_t>(BlockId::Cactus));
                    });
                bounded &= count > 0 && count <= PolishedTreeMaximumPlannedBlocks;
            }
    check("TREE_OWNER/all-six-kinds-three-statures-plans-bounded", bounded);

    const auto relativePlan = [](int x, int y, int z) {
        std::vector<std::array<int, 5>> result;
        visitPolishedAdventureTreeBlocks(3421, x, y, z, AdventureTreeKind::Oak, 2,
            [&](int px, int py, int pz, ChunkBlock block) {
                result.push_back({px - x, py - y, pz - z, block.id, block.metadata});
            });
        return result;
    };
    check("TREE_OWNER/translation-retains-relative-geometry-and-metadata",
          relativePlan(352, 75, -159) == relativePlan(-64, 90, 48));
    bool emitted = false;
    visitPolishedAdventureTreeBlocks(42, 0, 75, 0, AdventureTreeKind::None, 0,
        [&](int, int, int, ChunkBlock) { emitted = true; });
    check("TREE_OWNER/no-tree-emits-no-plan", !emitted);

    bool historicalFallback = true;
    for (int version = LegacyTerrainGenerationVersion;
         version < VegetationPolishTerrainGenerationVersion; ++version) {
        ClassicOverWorldGenerator historical(42, version);
        historicalFallback &= !historical.visitNaturalTreeOwnership(21, 4, -10,
            [&](const NaturalTreeOwnershipBlock &) { historicalFallback = false; });
    }
    check("TREE_OWNER/historical-v1-v23-fallback-emits-no-ownership", historicalFallback);

    ClassicOverWorldGenerator generator(42, CurrentTerrainGenerationVersion);
    std::map<std::array<int, 3>, ChunkBlock> knownTree;
    for (int chunkX = 21; chunkX <= 22; ++chunkX)
        for (int chunkZ = -11; chunkZ <= -10; ++chunkZ)
            for (int sectionY : {4, 5})
                generator.visitNaturalTreeOwnership(chunkX, sectionY, chunkZ,
                    [&](const NaturalTreeOwnershipBlock &owned) {
                        if (owned.root == std::array<int, 2>{352, -159})
                            knownTree.emplace(owned.position, owned.block);
                    });
    std::vector<std::array<int, 3>> pending{{352, 75, -159}};
    std::set<std::array<int, 3>> connected;
    if (knownTree.count(pending.front()) != 0) connected.insert(pending.front());
    for (std::size_t index = 0; index < pending.size(); ++index)
        for (int axis = 0; axis < 3; ++axis)
            for (int direction : {-1, 1}) {
                auto adjacent = pending[index];
                adjacent[axis] += direction;
                if (knownTree.count(adjacent) != 0 && connected.insert(adjacent).second)
                    pending.push_back(adjacent);
            }
    // Matches the independently sampled resident generation diagnostic for
    // the actual river-boundary tree, across four horizontal/two vertical cells.
    check("TREE_OWNER/known-cross-chunk-tree-has-132-connected-blocks",
          knownTree.size() == 132 && connected.size() == knownTree.size());
    bool knownIdentity = !knownTree.empty();
    for (const auto &entry : knownTree)
        knownIdentity &= entry.second.metadata == BlockMetadata::Tree::Oak &&
            (entry.second.id == static_cast<Block_t>(BlockId::OakBark) ||
             entry.second.id == static_cast<Block_t>(BlockId::OakLeaf)) &&
            std::abs(entry.first[0] - 352) <= NaturalTreeOwnershipRadius &&
            std::abs(entry.first[2] + 159) <= NaturalTreeOwnershipRadius;
    check("TREE_OWNER/known-root-retains-exact-material-metadata-and-halo", knownIdentity);

    const auto sectionOwnership = [&] {
        std::map<std::array<int, 3>, std::array<int, 4>> result;
        bool valid = true;
        std::set<std::array<int, 2>> roots;
        const bool supported = generator.visitNaturalTreeOwnership(21, 5, -10,
            [&](const NaturalTreeOwnershipBlock &owned) {
                valid &= owned.position[0] >= 336 && owned.position[0] <= 351 &&
                    owned.position[1] >= 80 && owned.position[1] <= 95 &&
                    owned.position[2] >= -160 && owned.position[2] <= -145 &&
                    owned.root[0] >= 330 && owned.root[0] <= 357 &&
                    owned.root[1] >= -166 && owned.root[1] <= -139;
                valid &= result.emplace(owned.position, std::array<int, 4>{
                    owned.block.id, owned.block.metadata, owned.root[0], owned.root[1]}).second;
                roots.insert(owned.root);
            });
        valid &= supported && result.size() <= NaturalTreeMaximumSectionOwnershipBlocks &&
            roots.size() <= NaturalTreeMaximumSourceRoots;
        return std::make_pair(valid, result);
    };
    const auto first = sectionOwnership();
    const auto second = sectionOwnership();
    check("TREE_OWNER/section-query-is-stable-unique-and-bounded",
          first.first && second.first && first.second == second.second);
    emitted = false;
    check("TREE_OWNER/rejects-invalid-signed-coordinate-halo",
          !generator.visitNaturalTreeOwnership(std::numeric_limits<int>::max(), 4, 0,
              [&](const NaturalTreeOwnershipBlock &) { emitted = true; }) && !emitted);
    emitted = false;
    const bool outside = generator.visitNaturalTreeOwnership(21, -1, -10,
        [&](const NaturalTreeOwnershipBlock &) { emitted = true; }) &&
        generator.visitNaturalTreeOwnership(21, 16, -10,
            [&](const NaturalTreeOwnershipBlock &) { emitted = true; });
    check("TREE_OWNER/empty-vertical-range-emits-no-ownership", outside && !emitted);
}
