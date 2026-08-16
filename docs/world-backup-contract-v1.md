# World Backup And Restore Contract v1

This contract freezes K3's renderer-independent backup boundary. It builds on
K1 world identity and K2 per-file publication. K4 owns the future player-facing
world-management UI; Q2 owns approved backup/restore timing budgets.

## Layout and limits

Each world stores backups below `backups/`, so snapshots are not sibling world
catalogue entries. Published snapshots use monotonically increasing canonical
names such as `backup-00000000000000000001`. The default policy keeps at most
three snapshots, 512 MiB of aggregate payload, 4,097 files per snapshot and
128 MiB per file. Tests and future callers may supply smaller positive limits.
Rotation removes the oldest published snapshot until both count and payload
limits hold.

Every snapshot contains only:

- `world.meta`;
- canonical immediate `chunks/chunk_<x>_<z>.hmcchunk` files; and
- `manifest.hmb`.

The version-1 manifest records the canonical backup id, sequence, UTC creation
time, world format version, exact file count, aggregate payload bytes and each
relative path's exact size plus a deterministic 64-bit content fingerprint.
The fingerprint detects accidental corruption; it is not an authenticity or
security signature. Unknown files, directories, symlinks, duplicate paths,
unsafe names, mismatched sizes/hashes and trailing manifest data are rejected.

## Backup publication

`WorldBackup::createBackup` first validates the live metadata and every chunk
through `WorldSave` and `ChunkStorageData`. It then stages exact bytes under
`backups/.pending`. Each staged file is durably published through K2's
`StorageTransaction`; the manifest is written last. The complete candidate is
re-enumerated, fingerprinted and parsed before its directory is renamed to the
final backup id. A policy/configuration rejection is reported without moving a
valid published backup into the corruption slot. A stale or failed candidate moves to the one bounded
`backups/.failed` slot.

`World::save()` creates a backup only after all dirty chunks and world metadata
have published successfully. Failure to create the required snapshot makes the
explicit save report failure, while the already validated primary generation
remains loadable. Destructor persistence does not create an additional backup.

## Verified restore

Restore is intended for a world that is not concurrently running. The selected
published backup is read-only throughout the operation.

1. Validate its directory inventory, manifest, sizes, fingerprints and real
   world/chunk parsers.
2. Copy every selected file into `backups/.restore.pending` through K2, then
   validate the complete staged generation again.
3. Preserve the current primary bytes in the single bounded
   `recovery.failed` directory. Corrupt and even empty primary files are kept
   for diagnosis.
4. Publish staged chunks and metadata to the live paths through K2. Remove
   canonical newer chunks that are not part of the selected generation.
5. If publication stops after any file changed, republish the preserved bytes
   and remove newly introduced files before returning failure.

An interrupted staging candidate moves to `backups/.restore.failed`. A selected
backup that fails inventory, fingerprint or parser validation moves intact to
the bounded `backups/.corrupt.failed` slot and cannot modify the primary.
Successful restore keeps the selected backup untouched and retains the replaced
primary in `recovery.failed`.

## Compatibility and evidence

The K2 readers remain the authority, so version-1 world metadata and version-1
chunks can be backed up and restored without an in-place migration. Normal game
save upgrades them afterward.

`HelloMine3DWorldBackupSmoke` owns 19 focused assertions covering complete
creation/listing, publication ordering, policy rejection, count/byte rotation,
oversize rejection, corrupt backup and primary handling, interruption before
validation, rollback after the first published restore file, version-1 restore
and an exact gameplay-state recovery. Three Q2 assertions additionally require
complete successful backup/restore records and a complete failure record.
The complete fixture restores player inventory, actors, chunks, crop metadata
and block-entity payloads. `HelloMine3DWorldRuntimeSmoke` separately proves the
real `World::save()` path publishes one validated snapshot and emits the Q2
save/backup summary without changing a subsequent instrumentation-off save.
