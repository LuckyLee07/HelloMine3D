#include "Ogre/OgreActorRenderer.h"

#include <Ogre.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <OgreVertexIndexData.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
constexpr double PositionTolerance = 0.0002;
constexpr double PenetrationTolerance = 0.0002;
constexpr double MaximumPoseLift = 0.0801;

void check(const std::string& name, bool passed)
{
    ++checks;
    failures += !passed;
    std::cout << "[WILDLIFE_RENDERER] " << (passed ? "PASS " : "FAIL ")
              << name << '\n';
}

struct Vector {
    double x = 0, y = 0, z = 0;
    Vector operator+(Vector b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vector operator-(Vector b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vector operator*(double s) const { return {x*s, y*s, z*s}; }
};
Vector vector(glm::vec3 p) { return {p.x, p.y, p.z}; }
Vector vector(Ogre::Vector3 p) { return {p.x, p.y, p.z}; }
double dot(Vector a, Vector b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vector cross(Vector a, Vector b)
{ return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
double length(Vector p) { return std::sqrt(dot(p,p)); }
bool near(Vector a, Vector b, double tolerance = PositionTolerance)
{ return length(a-b) <= tolerance; }
using Box = std::array<Vector,8>;

Box cube(Vector minimum, double size)
{
    return {{{minimum.x,minimum.y,minimum.z},
             {minimum.x+size,minimum.y,minimum.z},
             {minimum.x+size,minimum.y+size,minimum.z},
             {minimum.x,minimum.y+size,minimum.z},
             {minimum.x,minimum.y,minimum.z+size},
             {minimum.x+size,minimum.y,minimum.z+size},
             {minimum.x+size,minimum.y+size,minimum.z+size},
             {minimum.x,minimum.y+size,minimum.z+size}}};
}
Box translated(Box box, Vector displacement)
{
    for (Vector& p : box) p = p + displacement;
    return box;
}

// The 3 face normals of each box and 9 pairwise edge cross products are
// tested using double precision projections of all eight actual vertices.
// The directional interval distances handle containment correctly; the
// length of an interval intersection would underestimate its MTV.
double penetration(const Box& a, const Box& b, bool includeCrossProducts = true)
{
    const std::array<Vector,3> ae{{a[1]-a[0],a[3]-a[0],a[4]-a[0]}};
    const std::array<Vector,3> be{{b[1]-b[0],b[3]-b[0],b[4]-b[0]}};
    std::array<Vector,15> axes;
    for (std::size_t i=0; i<3; ++i) {
        axes[i] = cross(ae[(i+1)%3],ae[(i+2)%3]);
        axes[i+3] = cross(be[(i+1)%3],be[(i+2)%3]);
    }
    for (std::size_t i=0; i<3; ++i)
        for (std::size_t j=0; j<3; ++j)
            axes[6+3*i+j] = cross(ae[i],be[j]);
    double minimum = std::numeric_limits<double>::infinity();
    const std::size_t axisCount = includeCrossProducts ? axes.size() : 6;
    for (std::size_t candidate=0; candidate<axisCount; ++candidate) {
        Vector axis = axes[candidate];
        const double magnitude = length(axis);
        if (magnitude <= 1e-12) continue;
        axis = axis * (1.0/magnitude);
        double alo = dot(a[0],axis), ahi = alo;
        double blo = dot(b[0],axis), bhi = blo;
        for (std::size_t i=1; i<8; ++i) {
            const double ap = dot(a[i],axis), bp = dot(b[i],axis);
            alo = std::min(alo,ap); ahi = std::max(ahi,ap);
            blo = std::min(blo,bp); bhi = std::max(bhi,bp);
        }
        const double depth = std::min(ahi-blo,bhi-alo);
        if (depth <= 0) return 0;
        minimum = std::min(minimum,depth);
    }
    return minimum;
}

struct Species { std::string type; glm::vec3 dimensions{}; };
struct WorldCase { std::string name; ActorSnapshot before, after; };
struct Oracle { Box block; std::vector<Species> species; std::vector<WorldCase> cases; };

void expect(std::istream& in, const std::string& expected)
{
    std::string token;
    if (!(in >> token) || token != expected)
        throw std::runtime_error("World oracle expected token " + expected);
}
ActorSnapshot snapshot(std::istream& in)
{
    expect(in,"SNAPSHOT");
    ActorSnapshot result;
    auto& history = result.wildlifeMotionHistory;
    in >> result.id >> result.type
       >> result.position.x >> result.position.y >> result.position.z
       >> result.rotation.y
       >> result.dimensions.x >> result.dimensions.y >> result.dimensions.z
       >> result.wildlifeActivity >> result.wildlifeMotionSeconds
       >> history.count >> history.newestSequence;
    if (!in || history.count > history.segments.size())
        throw std::runtime_error("Invalid bounded ActorSnapshot in World oracle");
    for (std::size_t i=0; i<history.count; ++i) {
        expect(in,"SEG");
        auto& segment = history.segments[i];
        int kind = 0;
        in >> segment.sequence
           >> segment.from.x >> segment.from.y >> segment.from.z
           >> segment.to.x >> segment.to.y >> segment.to.z
           >> segment.seconds >> kind;
        if (!in || kind < int(WildlifeMotionPath::GroundedLevel) ||
            kind > int(WildlifeMotionPath::AirborneFall))
            throw std::runtime_error("Invalid actual motion segment in World oracle");
        segment.kind = static_cast<WildlifeMotionPath>(kind);
    }
    return result;
}
Oracle readOracle(const char* filename)
{
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Cannot read real World oracle");
    expect(in,"HMWILDLIFE_ORACLE");
    int version = 0;
    in >> version;
    if (version != 2) throw std::runtime_error("World oracle version must be 2");
    expect(in,"BLOCK");
    Vector minimum;
    double size = 0;
    in >> minimum.x >> minimum.y >> minimum.z >> size;
    if (!in || size != 1) throw std::runtime_error("Expected one actual voxel block");
    Oracle result;
    result.block = cube(minimum,size);
    for (int i=0; i<3; ++i) {
        expect(in,"SPECIES");
        Species value;
        in >> value.type >> value.dimensions.x >> value.dimensions.y >> value.dimensions.z;
        if (!in || !WildlifeSpecies::isWildlife(value.type))
            throw std::runtime_error("Invalid real species dimensions");
        result.species.push_back(value);
    }
    for (const char* name : {"ASCENT","DESCENT","MULTI_FALL"}) {
        expect(in,"CASE");
        WorldCase value;
        in >> value.name;
        if (value.name != name) throw std::runtime_error("Unexpected World motion case");
        value.before = snapshot(in);
        value.after = snapshot(in);
        result.cases.push_back(value);
    }
    expect(in,"END");
    std::string extra;
    if (in >> extra) throw std::runtime_error("Trailing World oracle data");
    return result;
}

struct Frame {
    Vector root;
    Vector forward;
    std::vector<Box> parts;
    bool geometry = true;
};
Frame readFrame(Ogre::SceneManager& scene, const ActorSnapshot& actor)
{
    scene.getRootSceneNode()->_update(true,false);
    const std::string base = "Actor_" + std::to_string(actor.id);
    Ogre::SceneNode* root = scene.getSceneNode(base+"_Node");
    Frame result;
    result.root = vector(root->_getDerivedPosition());
    result.forward = vector(root->_getDerivedOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Z);
    const auto profile = WildlifePresentation::profileFor(actor.type);
    result.geometry = root->numChildren() == profile.partCount && profile.partCount <= 8;
    for (std::size_t i=0; i<profile.partCount; ++i) {
        auto* part = scene.getSceneNode(base+"_PartNode_"+std::to_string(i));
        if (part->numAttachedObjects() != 1)
            throw std::runtime_error("Actual part must own exactly one ManualObject");
        auto* object = dynamic_cast<Ogre::ManualObject*>(part->getAttachedObject(0));
        if (!object || object->getNumSections() != 1)
            throw std::runtime_error("Actual production part is not a single ManualObject section");
        const auto& op = *object->getSection(0)->getRenderOperation();
        const auto* position = op.vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
        if (!position || position->getType() != Ogre::VET_FLOAT3 || op.vertexData->vertexCount != 8)
            throw std::runtime_error("Expected eight real unit-cube position vertices");
        auto buffer = op.vertexData->vertexBufferBinding->getBuffer(position->getSource());
        result.geometry &= dynamic_cast<Ogre::DefaultHardwareVertexBuffer*>(buffer.get()) != nullptr &&
            op.operationType == Ogre::RenderOperation::OT_TRIANGLE_LIST &&
            op.indexData != nullptr && op.indexData->indexCount == 36;
        Box corners;
        const Box expectedLocal = cube({-.5,-.5,-.5},1);
        const auto& transform = part->_getFullTransform();
        for (std::size_t v=0; v<8; ++v) {
            float xyz[3] = {};
            buffer->readData((op.vertexData->vertexStart+v)*buffer->getVertexSize()+position->getOffset(),
                             sizeof(xyz),xyz);
            result.geometry &= near({xyz[0],xyz[1],xyz[2]},expectedLocal[v],1e-7);
            // This is the Ogre node's actual complete transform. No renderer
            // rotation order, scale or joint formula is reproduced here.
            corners[v] = vector(transform * Ogre::Vector3(xyz[0],xyz[1],xyz[2]));
        }
        result.parts.push_back(corners);
    }
    return result;
}
bool sameFrame(const Frame& a, const Frame& b)
{
    if (!near(a.root,b.root,1e-7) || !near(a.forward,b.forward,1e-7) || a.parts.size() != b.parts.size())
        return false;
    for (std::size_t i=0; i<a.parts.size(); ++i)
        for (std::size_t v=0; v<8; ++v)
            if (!near(a.parts[i][v],b.parts[i][v],1e-7)) return false;
    return true;
}

// A reference path uses the accepted semantic route, independently of
// MotionBlend: support rise goes up then across, descent across then down,
// and an airborne fall follows each copied authoritative straight segment.
struct Path {
    std::vector<WildlifeMotionSegment> segments;
    double seconds = 0, totalLength = 0;
    bool valid = true;
};
double routeLength(const WildlifeMotionSegment& segment)
{
    const auto delta = vector(segment.to)-vector(segment.from);
    if (segment.kind == WildlifeMotionPath::SupportRise || segment.kind == WildlifeMotionPath::SupportDescent)
        return std::hypot(delta.x,delta.z)+std::abs(delta.y);
    return length(delta);
}
Path pathFor(const WorldCase& value)
{
    Path path;
    Vector previous = vector(value.before.position);
    auto sequence = value.before.wildlifeMotionHistory.newestSequence;
    for (std::size_t i=0; i<value.after.wildlifeMotionHistory.count; ++i) {
        const auto& segment = value.after.wildlifeMotionHistory.segments[i];
        if (segment.sequence <= sequence) continue;
        path.valid &= segment.sequence == sequence+1 && near(previous,vector(segment.from)) &&
            std::isfinite(segment.seconds) && segment.seconds > 0;
        if (value.name == "ASCENT") path.valid &= segment.kind == WildlifeMotionPath::SupportRise;
        if (value.name == "DESCENT") path.valid &= segment.kind == WildlifeMotionPath::SupportDescent;
        if (value.name == "MULTI_FALL") path.valid &= segment.kind == WildlifeMotionPath::AirborneFall &&
            segment.from.x == segment.to.x && segment.from.z == segment.to.z &&
            segment.from.y > segment.to.y && segment.from.y-segment.to.y <= .8001f;
        path.segments.push_back(segment);
        path.seconds += segment.seconds;
        path.totalLength += routeLength(segment);
        sequence = segment.sequence;
        previous = vector(segment.to);
    }
    path.valid &= !path.segments.empty() && near(previous,vector(value.after.position)) &&
        sequence == value.after.wildlifeMotionHistory.newestSequence &&
        (value.name == "MULTI_FALL" ? path.segments.size() == 5 : path.segments.size() == 1);
    return path;
}
Vector routePoint(const WildlifeMotionSegment& segment, double fraction)
{
    const Vector from = vector(segment.from), to = vector(segment.to), delta = to-from;
    fraction = std::clamp(fraction,0.0,1.0);
    if (segment.kind != WildlifeMotionPath::SupportRise && segment.kind != WildlifeMotionPath::SupportDescent)
        return from+delta*fraction;
    const double horizontal = std::hypot(delta.x,delta.z), vertical = std::abs(delta.y);
    const double distance = fraction*(horizontal+vertical);
    if (segment.kind == WildlifeMotionPath::SupportRise) {
        if (distance <= vertical) return from+Vector{0,delta.y,0}*(distance/vertical);
        return Vector{from.x,to.y,from.z}+Vector{delta.x,0,delta.z}*((distance-vertical)/horizontal);
    }
    if (distance <= horizontal) return from+Vector{delta.x,0,delta.z}*(distance/horizontal);
    return Vector{to.x,from.y,to.z}+Vector{0,delta.y,0}*((distance-horizontal)/vertical);
}
struct Sample { Vector position; std::size_t segment = 0; double progress = 0; };
Sample sample(const Path& path, double time)
{
    const double scale = std::min(1.0,.20/path.seconds);
    double beforeTime = 0, beforeLength = 0;
    for (std::size_t i=0; i<path.segments.size(); ++i) {
        const auto& segment = path.segments[i];
        const double duration = segment.seconds*scale;
        if (time < beforeTime+duration || i+1 == path.segments.size()) {
            const double fraction = std::clamp((time-beforeTime)/duration,0.0,1.0);
            return {routePoint(segment,fraction),i,beforeLength+routeLength(segment)*fraction};
        }
        beforeTime += duration;
        beforeLength += routeLength(segment);
    }
    throw std::runtime_error("Empty accepted path");
}

void headings(Ogre::SceneManager& scene, const Oracle& oracle)
{
    for (const auto& species : oracle.species) {
        ActorSnapshot actor;
        actor.id = 970001;
        actor.type = species.type;
        actor.dimensions = species.dimensions;
        actor.position = {4,64,-8};
        OgreActorRenderer renderer(scene);
        for (int heading=0; heading<360; heading+=45) {
            // First sync initializes at exactly this copied heading.
            renderer.clear();
            actor.rotation.y = float(heading);
            renderer.sync({actor},{1000,1000,1000},0,0);
            const auto frame = readFrame(scene,actor);
            const double radians = heading*3.14159265358979323846/180;
            const Vector expected{std::sin(radians),0,-std::cos(radians)};
            const std::string prefix = "HEADING/"+species.type+"/"+std::to_string(heading);
            check(prefix+"/model-forward-matches-authoritative-yaw",near(frame.forward,expected,1e-5));
            check(prefix+"/actual-eight-corner-vbo-and-part-budget",frame.geometry);
        }
        renderer.clear();
        actor.rotation.y = 179;
        renderer.sync({actor},{1000,1000,1000},1,0);
        const Frame before = readFrame(scene,actor);
        actor.rotation.y = -179;
        renderer.sync({actor},{1000,1000,1000},1,1.f/60);
        const Frame after = readFrame(scene,actor);
        const double angle = std::atan2(after.forward.x,-after.forward.z)*180/3.14159265358979323846;
        const double advance = std::remainder(angle-179,360.0);
        check("HEADING/"+species.type+"/shortest-turn-crosses-180-smoothly",advance > 0 && advance < 1);
        renderer.sync({actor},{1000,1000,1000},1,0);
        check("HEADING/"+species.type+"/paused-real-node-and-vbo-transforms",sameFrame(after,readFrame(scene,actor)));
        check("HEADING/"+species.type+"/turn-is-visible-in-actual-node",!near(before.forward,after.forward,1e-5));
    }
}

void worldPaths(Ogre::SceneManager& scene, Ogre::SceneManager& neutralScene, const Oracle& oracle)
{
    for (const auto& actual : oracle.cases) {
        const Path path = pathFor(actual);
        check("WORLD/"+actual.name+"/actual-snapshot-history-provenance-and-kind",path.valid);
        if (!path.valid) continue;
        for (float strength : {0.f,.35f,1.f}) for (int activity=0; activity<4; ++activity)
            for (int fps : {30,120}) {
                const std::string prefix = "WORLD/"+actual.name+"/strength="+
                    (strength == 0 ? "0" : strength == 1 ? "1" : "0.35")+
                    "/activity="+std::to_string(activity)+"/fps="+std::to_string(fps);
                auto before = actual.before, after = actual.after;
                before.wildlifeActivity = after.wildlifeActivity = activity;
                OgreActorRenderer renderer(scene);
                // A second actual renderer supplies the actual motion base.
                // Strength zero makes its copied dimensions the sole root
                // height offset; no pose or transform formula is duplicated.
                OgreActorRenderer neutralRenderer(neutralScene);
                renderer.sync({before},{1000,1000,1000},strength,0);
                neutralRenderer.sync({before},{1000,1000,1000},0,0);
                Frame previous = readFrame(scene,before);
                renderer.sync({after},{1000,1000,1000},strength,0);
                neutralRenderer.sync({after},{1000,1000,1000},0,0);
                bool paused = sameFrame(previous,readFrame(scene,after));
                bool onPath = true, continuous = true, geometry = true, noNewPenetration = true;
                bool firstProgress = false, complete = false;
                double maximumDepth = 0, maximumAllowed = 0, maximumExcess = 0, maximumDelta = 0;
                double previousProgress = 0;
                std::array<bool,WildlifeMotionHistory::MaximumSegments> observed{};
                const double dt = 1.0/fps, duration = std::min(.20,path.seconds);
                // Extra frames ensure all copied segments drain and stay at
                // the authoritative endpoint without repeating old history.
                for (int frameIndex=1; frameIndex<=fps/2; ++frameIndex) {
                    renderer.sync({after},{1000,1000,1000},strength,float(dt));
                    neutralRenderer.sync({after},{1000,1000,1000},0,float(dt));
                    const Frame frame = readFrame(scene,after);
                    const Frame neutral = readFrame(neutralScene,after);
                    const Vector actualMotionBase{neutral.root.x,
                        neutral.root.y-after.dimensions.y,neutral.root.z};
                    const Sample expected = sample(path,frameIndex*dt);
                    const Vector rootBase{frame.root.x,frame.root.y-after.dimensions.y,frame.root.z};
                    const double lift = rootBase.y-actualMotionBase.y;
                    const bool frameOnPath = near(actualMotionBase,expected.position) &&
                        std::abs(rootBase.x-actualMotionBase.x) <= PositionTolerance &&
                        std::abs(rootBase.z-actualMotionBase.z) <= PositionTolerance &&
                        lift >= -PositionTolerance && lift <= MaximumPoseLift;
                    onPath &= frameOnPath;
                    const double displacement = length(frame.root-previous.root);
                    maximumDelta = std::max(maximumDelta,displacement);
                    // Twice average route speed permits unequal segment
                    // lengths, while rejecting any one-frame full-step snap.
                    continuous &= displacement <= 2*path.totalLength*dt/duration + MaximumPoseLift + PositionTolerance &&
                        expected.progress+PositionTolerance >= previousProgress;
                    if (frameIndex == 1)
                        firstProgress = expected.progress > 0 && expected.progress < path.totalLength &&
                            length(frame.root-vector(before.position)-Vector{0,before.dimensions.y,0}) > PositionTolerance &&
                            !near(rootBase,vector(after.position));
                    if (frameIndex*dt >= duration+dt)
                        complete = std::abs(rootBase.x-after.position.x) <= PositionTolerance &&
                            std::abs(rootBase.z-after.position.z) <= PositionTolerance &&
                            rootBase.y >= after.position.y-PositionTolerance &&
                            rootBase.y <= after.position.y+MaximumPoseLift;
                    const bool firstObservation = !observed[expected.segment] && frameOnPath;
                    observed[expected.segment] = observed[expected.segment] || frameOnPath;
                    geometry &= frame.geometry && neutral.geometry;
                    const auto& segment = path.segments[expected.segment];
                    // Translate the actual frame's eight world corners to
                    // BOTH endpoints of this real segment, preserving the
                    // very same actual orientation, pose and dimensions.
                    for (const Box& part : frame.parts) {
                        const double depth = penetration(part,oracle.block);
                        const Box from = translated(part,vector(segment.from)-actualMotionBase);
                        const Box to = translated(part,vector(segment.to)-actualMotionBase);
                        const double allowed = std::max(penetration(from,oracle.block),penetration(to,oracle.block));
                        maximumDepth = std::max(maximumDepth,depth);
                        maximumAllowed = std::max(maximumAllowed,allowed);
                        maximumExcess = std::max(maximumExcess,depth-allowed);
                        noNewPenetration &= depth <= allowed+PenetrationTolerance;
                    }
                    if (firstObservation && strength == 0 && activity == 0)
                        std::cout << "[WILDLIFE_RENDERER] TRACE " << prefix
                                  << " frame=" << frameIndex << " real_sequence=" << segment.sequence
                                  << " kind=" << int(segment.kind)
                                  << " actual_base=" << actualMotionBase.x << ',' << actualMotionBase.y << ',' << actualMotionBase.z
                                  << " max_new_penetration_so_far=" << maximumExcess << '\n';
                    if (frameIndex == 2 || frameIndex == fps/4) {
                        renderer.sync({after},{1000,1000,1000},strength,0);
                        neutralRenderer.sync({after},{1000,1000,1000},0,0);
                        paused &= sameFrame(frame,readFrame(scene,after));
                        paused &= sameFrame(neutral,readFrame(neutralScene,after));
                    }
                    previous = frame;
                    previousProgress = expected.progress;
                }
                bool allSegments = true;
                std::size_t observedCount = 0;
                for (std::size_t i=0; i<path.segments.size(); ++i) {
                    allSegments &= observed[i];
                    observedCount += observed[i] ? 1 : 0;
                }
                check(prefix+"/paused-transforms-do-not-advance",paused);
                check(prefix+"/accepted-semantic-path-is-followed",onPath);
                check(prefix+"/continuity-rejects-snap",continuous && firstProgress);
                check(prefix+"/all-real-sequences-progress-and-finish",allSegments && complete);
                check(prefix+"/actual-vbo-corners-and-fixed-part-budget",geometry);
                check(prefix+"/no-new-penetration-beyond-same-pose-endpoints",noNewPenetration);
                std::cout << "[WILDLIFE_RENDERER] METRIC " << prefix
                          << " observed_sequences=" << observedCount << '/' << path.segments.size()
                          << " max_root_frame_displacement=" << maximumDelta
                          << " max_part_penetration=" << maximumDepth
                          << " permitted_endpoint_overhang=" << maximumAllowed
                          << " max_new_penetration=" << maximumExcess << '\n';
            }
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: wildlife-renderer-test actual-world-oracle-file ogre-log\n";
        return EXIT_FAILURE;
    }
    std::cout << std::setprecision(9);
    try {
        const Oracle oracle = readOracle(argv[1]);
        const Box unit = cube({0,0,0},1);
        check("SAT/double-15-candidate-axis-identical-depth",std::abs(penetration(unit,unit)-1) < 1e-12);
        check("SAT/contact-is-not-penetration",penetration(unit,cube({1,0,0},1)) == 0);
        check("SAT/contained-box-uses-separating-mtv",std::abs(penetration(unit,cube({.4,.4,.4},.2))-.6) < 1e-12);
        check("SAT/separated-box-is-rejected",penetration(unit,cube({1.01,0,0},1)) == 0);
        // A fixed skewed thin box overlaps every face-normal projection of
        // the centred unit voxel. An edge cross product separates it.
        const Box crossEdgeFixture{{
            {1.12513209694627,-.184079028238846,1.01031305320058},
            {.0591868117274572,-.91831704561277,.252252908645694},
            {-.00411443778195364,-.930870535342265,.353422873092454},
            {1.06183084743686,-.19663251796834,1.11148301764735},
            {1.05529948498612,-.0542225274376882,.982732344244848},
            {-.0106458002327003,-.788460544811613,.224672199689957},
            {-.0739470497421111,-.801014034541108,.325842164136717},
            {.991998235476706,-.0667760171671828,1.08390230869161}}};
        const Box centred = cube({-.5,-.5,-.5},1);
        check("SAT/cross-edge-axis-rejects-face-normal-false-positive",
            penetration(centred,crossEdgeFixture,false) > .01 && penetration(centred,crossEdgeFixture) == 0);
        // There is deliberately no RenderSystem, window, GL context or GPU
        // draw. Production sync, actual scene graph and real Ogre software
        // vertex buffers are the precise scope of this oracle.
        Ogre::Root root("","",argv[2]);
        Ogre::DefaultHardwareBufferManager buffers;
        for (const char* name : {"HelloMine3D/ActorSheep","HelloMine3D/ActorRabbit","HelloMine3D/ActorMarshBird"})
            Ogre::MaterialManager::getSingleton().create(name,Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        Ogre::SceneManager* scene = root.createSceneManager(Ogre::ST_GENERIC,"WildlifeRendererOracle");
        Ogre::SceneManager* neutralScene = root.createSceneManager(Ogre::ST_GENERIC,"WildlifeNeutralMotionProbe");
        headings(*scene,oracle);
        worldPaths(*scene,*neutralScene,oracle);
        check("SCOPE/no-render-system-window-or-GPU-claim",root.getRenderSystem() == nullptr);
        check("LIFECYCLE/renderer-clears-all-actor-nodes",scene->getRootSceneNode()->numChildren() == 0);
        check("LIFECYCLE/neutral-motion-probe-clears-all-actor-nodes",neutralScene->getRootSceneNode()->numChildren() == 0);
        root.destroySceneManager(neutralScene);
        root.destroySceneManager(scene);
    }
    catch (const std::exception& error) {
        check("unexpected-exception",false);
        std::cerr << error.what() << '\n';
    }
    std::cout << "[WILDLIFE_RENDERER] checks=" << checks << " failures=" << failures << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
