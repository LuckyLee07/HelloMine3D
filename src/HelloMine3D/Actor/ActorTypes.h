#ifndef ACTORTYPES_H_INCLUDED
#define ACTORTYPES_H_INCLUDED

#include <cstdint>

using ActorId = std::uint64_t;

static constexpr ActorId InvalidActorId = 0;
static constexpr ActorId DefaultPlayerActorId = 1;

#endif // ACTORTYPES_H_INCLUDED
