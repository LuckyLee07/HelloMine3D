# HelloMine3D World Catalogue Contract v1

This contract defines the renderer-independent discovery boundary used by K1,
the delivered K2 save transaction and later K3-K4 recovery/world-screen work.
Catalogue enumeration remains read-only; K2 publication semantics are frozen
separately in `docs/storage-transaction-contract-v1.md`.

## Catalogue boundary

- A catalogue root contains zero or more immediate world directories.
- A missing catalogue root is an empty catalogue. Enumeration does not create
  it.
- Every immediate directory is a world candidate and must contain one real,
  regular `world.meta` file. Other regular files at the catalogue root are not
  world candidates.
- Catalogue roots, world directories and `world.meta` files must not be
  symlinks. Every canonical world directory must remain below the canonical
  catalogue root.
- A malformed candidate invalidates the complete scan. Callers never receive a
  partial list that could be mistaken for the authoritative catalogue.
- Enumeration only reads directory entries and metadata. It never creates,
  repairs, upgrades, renames or touches a world.

The result is ordered by `last_played_utc` descending, then `created_utc`
descending, then immutable `world_id` ascending. This makes repeated scans
stable even when the host returns directory entries in a different order.

## `world.meta` version 3 identity fields

New worlds use save format 3 and write these singleton fields alongside the
existing runtime state:

```text
version 3
world_id world-0123456789abcdef0123456789abcdef
world_name "My World"
seed 20260816
created_utc 1786838400
last_played_utc 1786838460
last_build development
```

| Field | Contract |
| ----- | -------- |
| `version` | Canonical integer `3`. Catalogue v1 also reads legacy values `1` and `2`. Other versions fail explicitly. |
| `world_id` | Immutable, 1-64 byte lowercase ASCII identity. The first byte is alphanumeric; remaining bytes are `[a-z0-9_-]`. It is independent of the directory name. Duplicate ids invalidate the scan. |
| `world_name` | Quoted UTF-8 display text for version 3, 1-80 Unicode scalar values. Leading/trailing spaces, controls, invalid UTF-8, quotes, path separators, `.` and `..` are rejected. Rename work in K4 changes only this field. |
| `seed` | Canonical signed 32-bit integer. |
| `created_utc` | Unix UTC seconds in `[946684800, 253402300799]`; once published it remains unchanged. |
| `last_played_utc` | Unix UTC seconds in the same range and not earlier than `created_utc`. A normal save advances it monotonically. |
| `last_build` | 1-80 byte ASCII identity using `[A-Za-z0-9._+-]`. Unmanaged local builds use `development`; H2 later requires exact release build identities. |

Metadata is bounded to 64 KiB. Unknown keys, duplicate identity fields, empty
values, non-canonical integers and partial version-3 identity fields are
errors. Repeated runtime records such as `inventory_slot` and `actor` remain
valid and are not catalogue identities.

`WorldSave::save` validates every version-3 identity field before opening the
published file, so an invalid id/name/time/build request cannot truncate the
current metadata. K2 now adds candidate validation and atomic publication
after this input guard.

## Legacy discovery and upgrade

Version-1 and version-2 metadata must provide valid `world_id`, `world_name`
and `seed` fields. Their original directory is reported without renaming. They
receive the deterministic read-only catalogue values below:

- `created_utc = last_played_utc = 946684800` (unknown legacy time sentinel);
- `last_build = legacy-v1` or `legacy-v2`;
- `legacyMetadata = true`.

Enumeration never writes those values back. Opening a legacy world through the
normal runtime and completing a successful save upgrades `world.meta` to
version 3, preserves its existing id/name/seed, assigns the actual upgrade time
as creation/last-played time and records the current build identity.

## Validation evidence

`HelloMine3DWorldCatalogueSmoke` owns the focused K1 fixtures. It covers a
missing and empty root, multiple worlds and stable order, version 1/2 discovery,
version-3 fields, duplicate ids, malformed names/timestamps/versions, traversal,
missing/duplicate/unknown metadata and real directory/file symlinks. Before and
after snapshots prove enumeration is non-mutating. The full world runtime smoke
also proves version-3 creation, quoted display-name round-trip, immutable id
across relaunch and version-1 load/upgrade through the real `World` path.
