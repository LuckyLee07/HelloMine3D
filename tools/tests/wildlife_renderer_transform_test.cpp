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
#include <utility>
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
struct CadenceCase {
    std::string species, activity;
    std::vector<Box> blocks;
    std::vector<ActorSnapshot> snapshots;
};

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

std::vector<CadenceCase> readCadenceOracle(const char* filename)
{
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Cannot read real ActorManager cadence oracle");
    expect(in,"HMWILDLIFE_CADENCE");
    int version = 0;
    in >> version;
    if (version != 1) throw std::runtime_error("ActorManager cadence oracle version must be 1");
    std::vector<CadenceCase> result;
    for (int i=0; i<6; ++i) {
        expect(in,"CASE");
        CadenceCase value;
        std::size_t blockCount = 0, snapshotCount = 0;
        in >> value.species >> value.activity >> blockCount >> snapshotCount;
        if (!in || !WildlifeSpecies::isWildlife(value.species) ||
            (value.activity != "FLEE" && value.activity != "WANDER") ||
            blockCount == 0 || blockCount > 256 || snapshotCount < 3 || snapshotCount > 161)
            throw std::runtime_error("Invalid bounded ActorManager cadence CASE");
        for (const auto& prior : result)
            if (prior.species == value.species && prior.activity == value.activity)
                throw std::runtime_error("Duplicate species/activity cadence CASE");
        for (std::size_t block=0; block<blockCount; ++block) {
            expect(in,"BLOCK");
            Vector minimum;
            double size = 0;
            in >> minimum.x >> minimum.y >> minimum.z >> size;
            if (!in || size != 1 || !std::isfinite(minimum.x) ||
                !std::isfinite(minimum.y) || !std::isfinite(minimum.z))
                throw std::runtime_error("Invalid actual cadence voxel block");
            value.blocks.push_back(cube(minimum,size));
        }
        for (std::size_t index=0; index<snapshotCount; ++index)
            value.snapshots.push_back(snapshot(in));
        expect(in,"ENDCASE");
        result.push_back(std::move(value));
    }
    expect(in,"END");
    std::string extra;
    if (in >> extra) throw std::runtime_error("Trailing ActorManager cadence oracle data");
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

// Cadence checks use a geometric union of the World-approved straight legs.
// They do not predict MotionBlend's queue fractions, compression or timing.
struct RouteLeg {
    Vector from, to;
    double begin = 0, distance = 0;
    std::size_t segment = 0;
};
struct CadencePath {
    std::vector<WildlifeMotionSegment> segments;
    std::vector<RouteLeg> legs;
    std::vector<double> published;
    std::vector<double> segmentEnds;
    double totalLength = 0, maximumNativeSpeed = 0;
    bool valid = true, historyEvicted = false;
    std::size_t rises = 0, descents = 0;
};
bool sameSegment(const WildlifeMotionSegment& a, const WildlifeMotionSegment& b)
{
    return a.sequence == b.sequence && a.from == b.from && a.to == b.to &&
        a.seconds == b.seconds && a.kind == b.kind;
}
CadencePath cadencePathFor(const CadenceCase& value)
{
    CadencePath result;
    const auto& initial = value.snapshots.front();
    Vector previous = vector(initial.position);
    result.valid &= initial.type == value.species && initial.wildlifeMotionHistory.count == 0 &&
        initial.wildlifeMotionHistory.newestSequence == 0 &&
        initial.wildlifeMotionSeconds == 0 && initial.position.x == 5.5f;
    const auto addLeg = [&result](Vector from, Vector to, std::size_t segment) {
        const double distance = length(to-from);
        if (distance > 0) {
            result.legs.push_back({from,to,result.totalLength,distance,segment});
            result.totalLength += distance;
        }
    };
    const int expectedActivity = value.activity == "FLEE" ? int(WildlifeActivity::Flee) :
        int(WildlifeActivity::Wander);
    for (std::size_t index=0; index<value.snapshots.size(); ++index) {
        const auto& actor = value.snapshots[index];
        const auto& history = actor.wildlifeMotionHistory;
        result.valid &= actor.id == initial.id && actor.type == initial.type &&
            actor.dimensions == initial.dimensions && actor.dimensions.x > 0 &&
            actor.dimensions.y > 0 && actor.dimensions.z > 0 &&
            std::isfinite(actor.rotation.y) &&
            std::abs(double(actor.wildlifeMotionSeconds)-index*.05) < .0001 &&
            history.newestSequence >= result.segments.size();
        for (std::size_t item=0; item<history.count; ++item) {
            const auto& segment = history.segments[item];
            if (segment.sequence == 0 || segment.sequence > result.segments.size()+1) {
                result.valid = false;
                continue;
            }
            if (segment.sequence <= result.segments.size()) {
                result.valid &= sameSegment(segment,result.segments[std::size_t(segment.sequence-1)]);
                continue;
            }
            const Vector from = vector(segment.from), to = vector(segment.to), delta = to-from;
            const std::size_t number = result.segments.size();
            result.valid &= near(from,previous,1e-7) && to.x > from.x &&
                std::isfinite(delta.z) && std::abs(delta.z) <= .61 &&
                std::isfinite(segment.seconds) &&
                segment.seconds > 0 && segment.seconds <= .2001f &&
                actor.wildlifeActivity == expectedActivity;
            const double begin = result.totalLength;
            switch (segment.kind) {
                case WildlifeMotionPath::GroundedLevel:
                    result.valid &= delta.y == 0;
                    addLeg(from,to,number);
                    break;
                case WildlifeMotionPath::SupportRise:
                    result.valid &= std::abs(delta.y-1) < 1e-7;
                    ++result.rises;
                    addLeg(from,{from.x,to.y,from.z},number);
                    addLeg({from.x,to.y,from.z},to,number);
                    break;
                case WildlifeMotionPath::SupportDescent:
                    result.valid &= std::abs(delta.y+1) < 1e-7;
                    ++result.descents;
                    addLeg(from,{to.x,from.y,to.z},number);
                    addLeg({to.x,from.y,to.z},to,number);
                    break;
                default:
                    result.valid = false;
                    addLeg(from,to,number);
                    break;
            }
            result.maximumNativeSpeed = std::max(result.maximumNativeSpeed,
                (result.totalLength-begin)/segment.seconds);
            result.segmentEnds.push_back(result.totalLength);
            result.segments.push_back(segment);
            previous = to;
        }
        result.historyEvicted |= history.count == WildlifeMotionHistory::MaximumSegments &&
            history.segments[0].sequence > 1;
        result.valid &= history.newestSequence == result.segments.size() &&
            near(vector(actor.position),previous,1e-7) &&
            (history.count == 0 || history.segments[history.count-1].to == actor.position);
        result.published.push_back(result.totalLength);
    }
    result.valid &= result.rises == 1 && result.descents == 1 && result.historyEvicted &&
        value.snapshots.back().position.x >= 10 && !result.legs.empty() &&
        result.segments.size() > WildlifeMotionHistory::MaximumSegments;
    return result;
}
struct Projection {
    double distance = std::numeric_limits<double>::infinity(), progress = 0;
    std::size_t segment = 0;
};
Projection projectPublished(const CadencePath& path, Vector point,
                            double published, double previousProgress)
{
    Projection best;
    for (const auto& leg : path.legs) {
        const Vector edge = leg.to-leg.from;
        const double fraction = std::clamp(dot(point-leg.from,edge)/(leg.distance*leg.distance),0.0,1.0);
        const double progress = leg.begin+leg.distance*fraction;
        if (progress > published+PositionTolerance || progress+PositionTolerance < previousProgress)
            continue;
        const double distance = length(point-(leg.from+edge*fraction));
        if (distance < best.distance-1e-10 ||
            (std::abs(distance-best.distance) <= 1e-10 && progress > best.progress))
            best = {distance,progress,leg.segment};
    }
    return best;
}
double voxelPenetration(const Box& part, const Box& voxel)
{
    // AABB separation is a valid early rejection; overlapping candidates
    // still receive the full independent double-precision 15-axis SAT.
    Vector lo = part[0], hi = part[0];
    for (const Vector corner : part) {
        lo.x = std::min(lo.x,corner.x); hi.x = std::max(hi.x,corner.x);
        lo.y = std::min(lo.y,corner.y); hi.y = std::max(hi.y,corner.y);
        lo.z = std::min(lo.z,corner.z); hi.z = std::max(hi.z,corner.z);
    }
    if (hi.x <= voxel[0].x || lo.x >= voxel[6].x ||
        hi.y <= voxel[0].y || lo.y >= voxel[6].y ||
        hi.z <= voxel[0].z || lo.z >= voxel[6].z) return 0;
    return penetration(part,voxel);
}
double heading(Vector direction)
{ return std::atan2(direction.x,-direction.z)*180/3.14159265358979323846; }

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

void cadencePaths(Ogre::SceneManager& scene, Ogre::SceneManager& neutralScene,
                  const std::vector<CadenceCase>& cases)
{
    for (const auto& actual : cases) {
        const CadencePath path = cadencePathFor(actual);
        const std::string name = actual.species+"/"+actual.activity;
        check("CADENCE/"+name+"/actual-20Hz-manager-history-and-one-block-routes",path.valid);
        if (!path.valid) continue;
        for (float strength : {0.f,.35f,1.f}) for (int fps : {30,120}) {
            const std::string prefix = "CADENCE/"+name+"/strength="+
                (strength == 0 ? "0" : strength == 1 ? "1" : "0.35")+
                "/fps="+std::to_string(fps);
            OgreActorRenderer renderer(scene), neutralRenderer(neutralScene);
            renderer.sync({actual.snapshots.front()},{1000,1000,1000},strength,0);
            neutralRenderer.sync({actual.snapshots.front()},{1000,1000,1000},0,0);
            Frame previous = readFrame(scene,actual.snapshots.front());
            Frame previousNeutral = readFrame(neutralScene,actual.snapshots.front());
            bool paused = true, onPath = true, continuous = true, geometry = true;
            bool noNewPenetration = true, headingMatches = true, complete = false;
            bool releasePositions = true;
            double previousProgress = 0, previousDesired = actual.snapshots.front().rotation.y;
            double previousHeadingError = std::abs(std::remainder(heading(previous.forward)-previousDesired,360.0));
            double maximumDepth = 0, maximumAllowed = 0, maximumExcess = 0;
            double maximumAdvance = 0, maximumFrameDisplacement = 0, maximumLag = 0;
            std::size_t latest = 0, pendingPublications = 0, maximumCopiedHistory = 0;
            std::vector<bool> interiors(path.segments.size(),false);
            const double dt = 1.0/fps;
            const double lastTime = (actual.snapshots.size()-1)*.05;
            const int lastFrame = int(std::ceil((lastTime+.5)*fps));
            for (int index=1; index<=lastFrame; ++index) {
                const double time = index*dt;
                const std::size_t priorLatest = latest;
                while (latest+1 < actual.snapshots.size() && (latest+1)*.05 <= time+1e-8)
                    ++latest;
                const auto& actor = actual.snapshots[latest];
                maximumCopiedHistory = std::max(maximumCopiedHistory,actor.wildlifeMotionHistory.count);
                if (latest != priorLatest) {
                    if (actor.wildlifeMotionHistory.newestSequence >
                            actual.snapshots[priorLatest].wildlifeMotionHistory.newestSequence) {
                        if (previousProgress+PositionTolerance < path.published[priorLatest])
                            ++pendingPublications;
                        else {
                            const auto& priorActor = actual.snapshots[priorLatest];
                            const Vector priorBase{previousNeutral.root.x,
                                previousNeutral.root.y-priorActor.dimensions.y,previousNeutral.root.z};
                            releasePositions &= near(priorBase,vector(priorActor.position));
                        }
                    }
                    // A freshly arriving actual snapshot cannot consume its
                    // sequence or replace a running motion target at dt zero.
                    renderer.sync({actor},{1000,1000,1000},strength,0);
                    neutralRenderer.sync({actor},{1000,1000,1000},0,0);
                    paused &= sameFrame(previous,readFrame(scene,actor));
                    paused &= sameFrame(previousNeutral,readFrame(neutralScene,actor));
                }
                renderer.sync({actor},{1000,1000,1000},strength,float(dt));
                neutralRenderer.sync({actor},{1000,1000,1000},0,float(dt));
                const Frame frame = readFrame(scene,actor), neutral = readFrame(neutralScene,actor);
                const Vector motionBase{neutral.root.x,neutral.root.y-actor.dimensions.y,neutral.root.z};
                const Vector rootBase{frame.root.x,frame.root.y-actor.dimensions.y,frame.root.z};
                const Projection projected = projectPublished(path,motionBase,path.published[latest],previousProgress);
                const double lift = rootBase.y-motionBase.y;
                const bool frameOnPath = projected.distance <= PositionTolerance &&
                    projected.progress <= path.published[latest]+PositionTolerance &&
                    projected.progress+PositionTolerance >= previousProgress &&
                    std::abs(rootBase.x-motionBase.x) <= PositionTolerance &&
                    std::abs(rootBase.z-motionBase.z) <= PositionTolerance &&
                    lift >= -PositionTolerance && lift <= MaximumPoseLift;
                onPath &= frameOnPath;
                const double advance = projected.progress-previousProgress;
                maximumAdvance = std::max(maximumAdvance,advance);
                maximumFrameDisplacement = std::max(maximumFrameDisplacement,length(frame.root-previous.root));
                maximumLag = std::max(maximumLag,path.published[latest]-projected.progress);
                // At 20Hz no native segment arrives faster than one per
                // render interval. The accepted maximum arclength / seconds
                // bounds frame travel without reproducing a blend formula.
                continuous &= frameOnPath && advance >= -PositionTolerance &&
                    advance <= path.maximumNativeSpeed*dt+PositionTolerance &&
                    length(frame.root-previous.root) <=
                        path.maximumNativeSpeed*dt+MaximumPoseLift+PositionTolerance;
                geometry &= frame.geometry && neutral.geometry &&
                    scene.getRootSceneNode()->numChildren() == 1 &&
                    neutralScene.getRootSceneNode()->numChildren() == 1 &&
                    maximumCopiedHistory <= WildlifeMotionHistory::MaximumSegments;
                const double desired = std::remainder(double(actor.rotation.y),360.0);
                const double angle = heading(frame.forward);
                const double error = std::abs(std::remainder(angle-desired,360.0));
                const double radians = desired*3.14159265358979323846/180;
                const Vector expectedForward{std::sin(radians),0,-std::cos(radians)};
                headingMatches &= near(neutral.forward,expectedForward,1e-5) &&
                    std::abs(frame.forward.y) <= 1e-6 && std::abs(length(frame.forward)-1) <= 1e-5;
                if (std::abs(std::remainder(desired-previousDesired,360.0)) < 1e-5)
                    headingMatches &= error <= previousHeadingError+.0001;
                if (index == lastFrame) headingMatches &= error < .2;
                previousDesired = desired;
                previousHeadingError = error;
                const auto& segment = path.segments[projected.segment];
                const double segmentBegin = projected.segment == 0 ? 0 : path.segmentEnds[projected.segment-1];
                if (frameOnPath && projected.progress > segmentBegin+PositionTolerance &&
                    projected.progress < path.segmentEnds[projected.segment]-PositionTolerance) {
                    const bool first = !interiors[projected.segment];
                    interiors[projected.segment] = true;
                    if (first && (segment.kind == WildlifeMotionPath::SupportRise ||
                                  segment.kind == WildlifeMotionPath::SupportDescent))
                        std::cout << "[WILDLIFE_RENDERER] TRACE " << prefix << " time=" << time
                                  << " snapshot_tick=" << latest << " actual_sequence=" << segment.sequence
                                  << " kind=" << int(segment.kind) << " actual_base="
                                  << motionBase.x << ',' << motionBase.y << ',' << motionBase.z
                                  << " published_sequence=" << actor.wildlifeMotionHistory.newestSequence << '\n';
                }
                // Every candidate is a copied voxel in this actual World.
                // Endpoint probes preserve this frame's actual node yaw and
                // actual articulated VBO corners; neutral supplies its base.
                for (const Box& part : frame.parts) {
                    const Box from = translated(part,vector(segment.from)-motionBase);
                    const Box to = translated(part,vector(segment.to)-motionBase);
                    for (const Box& block : actual.blocks) {
                        const double depth = voxelPenetration(part,block);
                        const double allowed = std::max(voxelPenetration(from,block),voxelPenetration(to,block));
                        maximumDepth = std::max(maximumDepth,depth);
                        maximumAllowed = std::max(maximumAllowed,allowed);
                        maximumExcess = std::max(maximumExcess,depth-allowed);
                        noNewPenetration &= depth <= allowed+PenetrationTolerance;
                    }
                }
                if (index%25 == 0) {
                    renderer.sync({actor},{1000,1000,1000},strength,0);
                    neutralRenderer.sync({actor},{1000,1000,1000},0,0);
                    paused &= sameFrame(frame,readFrame(scene,actor));
                    paused &= sameFrame(neutral,readFrame(neutralScene,actor));
                }
                if (time >= lastTime+.25) complete = near(motionBase,vector(actual.snapshots.back().position));
                previous = frame;
                previousNeutral = neutral;
                previousProgress = projected.progress;
            }
            bool allInteriors = true, riseInterior = false, descentInterior = false;
            std::size_t observed = 0;
            for (std::size_t segment=0; segment<path.segments.size(); ++segment) {
                allInteriors &= interiors[segment];
                observed += interiors[segment] ? 1 : 0;
                if (interiors[segment] && path.segments[segment].kind == WildlifeMotionPath::SupportRise)
                    riseInterior = true;
                if (interiors[segment] && path.segments[segment].kind == WildlifeMotionPath::SupportDescent)
                    descentInterior = true;
            }
            check(prefix+"/newly-arriving-and-running-dt-zero-transforms-freeze",paused);
            check(prefix+"/motion-base-stays-on-published-L-route-and-never-reverses",onPath);
            check(prefix+"/accepted-native-travel-speed-rejects-full-step-snap",continuous);
            // At 120Hz each .05s segment may finish six render intervals
            // before the next publication. That drained cadence is valid;
            // the non-integral 30Hz cadence exercises the in-flight append.
            check(prefix+(fps == 30 ? "/actual-cadence-appends-during-in-flight-replay" :
                  "/actual-cadence-arrives-in-flight-or-at-real-drained-endpoints"),
                  releasePositions && (fps != 30 || pendingPublications > 0));
            check(prefix+"/every-real-sequence-including-rise-and-descent-has-interior-progress",
                  allInteriors && riseInterior && descentInterior);
            check(prefix+"/drains-at-actual-final-authoritative-position",complete &&
                  std::abs(previousProgress-path.totalLength) <= PositionTolerance);
            check(prefix+"/actual-VBO-scene-and-copied-history-remain-bounded",geometry &&
                  maximumCopiedHistory == WildlifeMotionHistory::MaximumSegments);
            check(prefix+"/actual-root-forward-converges-to-authoritative-heading",headingMatches);
            check(prefix+"/all-actual-voxels-have-no-new-same-pose-endpoint-penetration",noNewPenetration);
            std::cout << "[WILDLIFE_RENDERER] METRIC " << prefix
                      << " ticks=" << actual.snapshots.size()-1 << " frames=" << lastFrame
                      << " observed_sequence_interiors=" << observed << '/' << path.segments.size()
                      << " pending_publications=" << pendingPublications
                      << " max_copied_history=" << maximumCopiedHistory
                      << " actual_voxels=" << actual.blocks.size()
                      << " max_arclength_advance=" << maximumAdvance
                      << " native_route_speed_bound=" << path.maximumNativeSpeed
                      << " max_root_frame_displacement=" << maximumFrameDisplacement
                      << " max_published_route_lag=" << maximumLag
                      << " max_part_penetration=" << maximumDepth
                      << " permitted_endpoint_overhang=" << maximumAllowed
                      << " max_new_penetration=" << maximumExcess << '\n';
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: wildlife-renderer-test actual-world-oracle-file ogre-log [actual-cadence-oracle-file]\n";
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
        if (argc == 4) cadencePaths(*scene,*neutralScene,readCadenceOracle(argv[3]));
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
