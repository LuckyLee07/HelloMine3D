#ifndef CHUNKEVENTS_H_INCLUDED
#define CHUNKEVENTS_H_INCLUDED

#include "../../Maths/Vector2XZ.h"
#include "SandboxEventBus.h"

struct ChunkGeneratedEvent : public SandboxEvent {
    explicit ChunkGeneratedEvent(const VectorXZ &chunkPosition)
        : SandboxEvent(SandboxEventType::ChunkGenerated)
        , position(chunkPosition)
    {
    }

    VectorXZ position;
};

struct ChunkLoadedEvent : public SandboxEvent {
    ChunkLoadedEvent(const VectorXZ &chunkPosition, bool loadedFromStorage)
        : SandboxEvent(SandboxEventType::ChunkLoaded)
        , position(chunkPosition)
        , fromStorage(loadedFromStorage)
    {
    }

    VectorXZ position;
    bool fromStorage = false;
};

struct ChunkUnloadedEvent : public SandboxEvent {
    explicit ChunkUnloadedEvent(const VectorXZ &chunkPosition)
        : SandboxEvent(SandboxEventType::ChunkUnloaded)
        , position(chunkPosition)
    {
    }

    VectorXZ position;
};

struct ChunkSavedEvent : public SandboxEvent {
    explicit ChunkSavedEvent(const VectorXZ &chunkPosition)
        : SandboxEvent(SandboxEventType::ChunkSaved)
        , position(chunkPosition)
    {
    }

    VectorXZ position;
};

#endif // CHUNKEVENTS_H_INCLUDED
