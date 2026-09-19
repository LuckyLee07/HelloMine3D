#include "Presentation/ProjectilePresentation.h"
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

int main()
{
    using namespace ProjectilePresentation;
    int checks = 0;
    auto check = [&](const char* name, bool ok) {
        if (!ok) throw std::runtime_error(name);
        ++checks; std::cout << "[PROJECTILE] PASS " << name << '\n';
    };
    const auto& geometry = mesh();
    bool compact = true, finite = true, outward = true;
    std::map<std::pair<unsigned,unsigned>, int> edges;
    double volume = 0;
    for (const auto& point : geometry.positions) {
        finite &= std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
        compact &= std::abs(point.x) <= .40f && std::abs(point.y) <= .40f &&
            point.z >= -.51f && point.z <= .66f;
    }
    for (std::size_t i = 0; i < geometry.indices.size(); i += 3) {
        const auto a = geometry.indices[i], b = geometry.indices[i+1], c = geometry.indices[i+2];
        check("triangle-indices-valid", a < geometry.positions.size() && b < geometry.positions.size() && c < geometry.positions.size());
        const auto p = geometry.positions[a], q = geometry.positions[b], r = geometry.positions[c];
        const auto normal = glm::cross(q-p,r-p);
        outward &= glm::dot(normal, (p+q+r)/3.f) > .0001f;
        volume += glm::dot(p, glm::cross(q,r))/6.;
        for (const auto edge : {std::make_pair(a,b),std::make_pair(b,c),std::make_pair(c,a)}) ++edges[edge];
    }
    bool closed = true;
    for (const auto& edge : edges)
        closed &= edge.second == 1 && edges[{edge.first.second,edge.first.first}] == 1;
    check("opaque-closed-mesh-no-missing-faces", closed && outward);
    check("bounded-volume-smaller-than-old-long-box", finite && compact && volume > .08 && volume < .30);
    for (const auto velocity : {glm::vec3(1,0,0), {-1,0,0}, {0,0,-1}, {0,0,1},
         {0,1,0}, {0,-1,0}, {1,.45f,-.35f}, {-1,-.4f,.25f}}) {
        const auto basis = frame(velocity);
        check("leading-tip-faces-real-velocity", glm::dot(-basis.back,glm::normalize(velocity)) > .99999f);
        check("orientation-is-orthonormal-right-handed",
            std::abs(glm::dot(basis.right,basis.up)) < .00001f &&
            glm::dot(glm::cross(basis.right,basis.up),basis.back) > .99999f);
    }
    check("reject-nonfinite-and-out-of-range-radius",
        !validRadius(std::numeric_limits<float>::quiet_NaN()) &&
        !validRadius(std::numeric_limits<float>::infinity()) &&
        !validRadius(0.f) && !validRadius(-.1f) && !validRadius(.51f) &&
        validRadius(.05f) && validRadius(.15f) && validRadius(.5f));
    std::cout << "checks=" << checks << " failures=0 volume=" << volume << '\n';
}
