#ifndef VECTOR2XZ_H_INCLUDED
#define VECTOR2XZ_H_INCLUDED

#include <SFML/System/Vector3.hpp>
#include <cstddef>
#include <functional>

struct VectorXZ {
    int x, z;
};

bool operator==(const VectorXZ &left, const VectorXZ &right) noexcept;

namespace std {
template <> struct hash<VectorXZ> {
    std::size_t operator()(const VectorXZ &vect) const noexcept
    {
        std::size_t seed = 0;
        seed ^= std::hash<int>{}(vect.x) + 0x9e3779b9u + (seed << 6) +
                (seed >> 2);
        seed ^= std::hash<int>{}(vect.z) + 0x9e3779b9u + (seed << 6) +
                (seed >> 2);

        return seed;
    }
};
} // namespace std

namespace std {
template <> struct hash<sf::Vector3i> {
    std::size_t operator()(const sf::Vector3i &vect) const noexcept
    {
        std::size_t seed = 0;
        seed ^= std::hash<int>{}(vect.x) + 0x9e3779b9u + (seed << 6) +
                (seed >> 2);
        seed ^= std::hash<int>{}(vect.y) + 0x9e3779b9u + (seed << 6) +
                (seed >> 2);
        seed ^= std::hash<int>{}(vect.z) + 0x9e3779b9u + (seed << 6) +
                (seed >> 2);

        return seed;
    }
};
} // namespace std

#endif // VECTOR2XZ_H_INCLUDED
