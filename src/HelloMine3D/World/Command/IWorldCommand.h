#ifndef IWORLDCOMMAND_H_INCLUDED
#define IWORLDCOMMAND_H_INCLUDED

class World;

/// A request to change authoritative World state. Commands are executed by
/// the World-owned FIFO and may be rejected by existing Gameplay rules.
struct IWorldCommand {
    virtual ~IWorldCommand() = default;
    virtual void execute(World &world) = 0;
};

#endif // IWORLDCOMMAND_H_INCLUDED
