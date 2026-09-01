# AL-A4 Event / Command / Query Boundary Contract v1

> Status: Frozen after AL-A4 verification
>
> Runtime behavior: preserve current player interaction and domain outcomes
>
> Authoritative task status: `docs/current/todolist.md`

## 1. Purpose and approved scope

AL-A4 gives three existing runtime concepts distinct names and enforceable
boundaries:

```text
Command = a request to change authoritative world state
Event   = an immutable fact published after an outcome occurred
Query   = an observation that does not commit a new Gameplay result
```

The batch may rename the legacy queued `IWorldEvent` interaction path to a
command, harden `SandboxEventBus`, classify current subscribers, add static and
behavioral gates, and update the AL-A1 responsibility map for the reviewed
public rename. It must preserve interaction outcomes, save v12, resources,
recipes, rendering, fixed-tick order and ChunkRuntime behavior.

## 2. Command boundary

`World::addCommand<T>` accepts an `IWorldCommand` and appends it to the existing
frame-owned FIFO. `World::update` executes each accepted command once, in
submission order, before unload and bounded mesh processing, then clears the
queue.

The first concrete command is `PlayerBlockInteractionCommand` with Break, Use
and Place actions. A command may be rejected by existing Gameplay rules. Only
facts produced by completed mutations may be published to `SandboxEventBus`.
The old `addEvent` / `IWorldEvent` / `PlayerDigEvent` vocabulary is not retained
as a second live path.

## 3. Event boundary

Every event delivered by `SandboxEventBus` is viewed as a const fact and carries
one category:

- `Domain`: an outcome in the game domain, including all 22 event types that
  exist at A4 entry;
- `Diagnostic`: observation-only telemetry or diagnostics that must not drive
  authoritative Gameplay state.

The event type and category are immutable after construction. The bus remains
synchronous and World-local. It does not own Gameplay state, queue work across
frames, persist history, call Ogre, or become a service locator.

## 4. Subscriber effect declaration

Every subscription records an owner label and two independent policies:

| Policy | Meaning |
| ------ | ------- |
| `ObserveOnly` | May update presentation, audio, logging or test observations; must not change authoritative Gameplay state. |
| `DomainMutation` | May perform the named, bounded domain reaction documented below. |
| `Forbidden` republish | Handler may not synchronously publish another event. |
| `Bounded` republish | Handler may publish a nested fact within the bus depth limit. |

The current production inventory is frozen as:

| Owner | Effect | Republish | Bounded state/result |
| ----- | ------ | --------- | -------------------- |
| `ObjectiveSystem` | `DomainMutation` | `Forbidden` | objective progress, completion feedback and discovered-recipe state only |
| `World.WaystoneGuardianDeath` | `DomainMutation` | `Bounded` | Waystone encounter state; actor spawning may publish actor facts |
| `ActionFeedbackTimeline` | `ObserveOnly` | `Forbidden` | transient presentation feedback only |
| `AudioRuntime` | `ObserveOnly` | `Forbidden` | transient audio queue/statistics only |

Test recorders default to `ObserveOnly / Forbidden`. New handlers must choose
an owner and policy deliberately; `DomainMutation` is not a default.

## 5. Recursion and subscription mutation

Synchronous publication is bounded to eight active dispatch frames. A nested
publication is rejected when the current handler has `Forbidden` republish or
the depth cap would be exceeded. Rejection is observable in the dispatch result
and bus debug snapshot; it is never converted into an implicit queued event.

Each publication uses the subscription membership captured at its entry.
Subscribe/unsubscribe during a handler affects later (including later nested)
publications, not the current membership snapshot. Handler exceptions continue
to propagate, while bus depth bookkeeping must still be restored.

A `Diagnostic` event is never delivered to a `DomainMutation` handler. Such a
handler is counted as rejected, so diagnostics cannot become an indirect
Gameplay command.

## 6. Query boundary and compatibility

The AL-A1 map remains the authoritative inventory of World queries. A new query
must not commit Gameplay, enqueue commands or publish domain facts. Existing
mutable manager/reference getters remain documented compatibility escape
hatches rather than templates for new API.

Renaming `addEvent` to `addCommand` keeps the World public method count and
Command classification stable, but requires a reviewed map row and normalized
public-surface hash update in the same A4 change.

## 7. Dependency and non-goal rules

- a future Network domain may consume domain facts or commands but must not
  include or call Ogre;
- a future Machine domain may consume domain facts or commands but must not
  depend on UI or Presentation;
- neither future domain receives an empty registry slot in A4;
- AL-A4 does not add Machine, Network, scheduler, metrics/budget vocabulary,
  jobs, persistence formats or new Gameplay content;
- AL-A4 does not begin A5, B1, C1 or any Extended capability.

## 8. Acceptance

The contract may be marked verified only when:

1. a static A4 gate proves the command vocabulary is singular, production
   subscribers declare their effects, recursion/snapshot/diagnostic guards are
   present, and future Network/Machine dependency rules are checked when those
   directories exist;
2. focused runtime tests cover domain category, observer delivery, forbidden
   nested publication, explicitly allowed nesting, depth rejection, diagnostic
   mutation rejection and subscription snapshot semantics;
3. the AL-A1 validator passes with 78 method names and the reviewed A4 hash;
4. existing interaction, objective, Waystone, audio, feedback, Actor, save,
   simulation and streaming regressions remain green;
5. VS2017/v141 Debug and Release complete gates pass, including the isolated
   clean package.

AI gameplay remains `NOT_RUN` unless executed separately through the active
package-only protocol. Human fun, aesthetics and physical input feel remain
`NOT_CLAIMED`.
