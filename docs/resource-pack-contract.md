# HelloMine3D Resource Pack Contract v1

This contract defines the deliberately bounded resource override layer shipped
by X1-X3. A pack is a read-only directory that may replace an existing logical
resource. It cannot register a new block id, execute code, reload at runtime or
fetch content from an archive or network source.

## Pack layout

Every pack root contains `pack.meta` and zero or more files at the same logical
paths used by the base game:

```text
packs/example-stone/
  pack.meta
  media/
    blocks/
      Stone.block
```

`pack.meta` is UTF-8 text with this exact version header and two unique fields:

```text
# HelloMine3D resource pack v1
name=Example Stone
format=1
```

`name` is a diagnostic identity of at most 80 letters, digits, spaces, `_`,
`-` or `.`. Names are unique case-insensitively among enabled packs. `format`
must equal `1`; incompatible versions fail before Ogre construction.

## Enabling and precedence

`bin/resource-packs.txt` lists directory names below `packs/`, one per line.
Blank lines and lines beginning with `#` are ignored. Earlier entries have
higher priority. The automation-only `HELLOMINE3D_RESOURCE_PACKS` variable may
replace that list with semicolon-separated absolute directories or pack names.

For each logical resource, the first enabled pack containing an override wins.
If no pack owns it, the base `media/` or `bin/` file is used. This choice is
computed once and frozen for the process lifetime. Runtime reload is not part
of v1.

Example:

```text
high-contrast
example-stone
```

If both packs contain `media/textures/DefaultPack.png`, `high-contrast` wins.
An unoverridden shader still comes from the base root.

## Allowed resources

The startup resource manifest is the allowlist. A pack may override only an
existing entry in these categories:

- `block`
- `font`
- `resource-script`
- `shader`
- `shape`
- `texture`

`runtime-template` entries cannot be overridden. A pack cannot add a logical
path that is absent from `media/resource-manifest.txt`; such a file is reported
as a stale or unsupported override. This also means that v1 cannot introduce a
new block id, shape name, behavior script or executable extension.

The base manifest may also contain `recipe`, `tool`, `audio` and `objective`
entries, but none is an allowed v1 override class. These versioned gameplay
registries load only from base-owned sources; a pack containing the same
logical path is rejected as stale or unsupported. Adding ownership for any of
these categories requires a new resource-pack format and an explicit migration
policy for saved gameplay state.

## Path and trust policy

Logical paths use forward slashes, are repository-relative and canonical, and
may not contain an absolute root, drive prefix, `.` or `..` segment. Configured
pack names may not contain separators or traversal. Absolute pack directories
are accepted only through the automation environment variable.

Every scanned source is canonicalized and must remain inside its pack root.
Directory and file symlinks are rejected. Empty overrides, case-insensitive
duplicate logical paths, duplicate pack names, unknown metadata keys, missing
metadata and unreadable sources all fail startup. Diagnostics include the pack
name, logical path and physical source whenever an override is involved.

Valid override:

```text
media/blocks/Stone.block
```

Invalid examples:

```text
../media/blocks/Stone.block       # traversal
media/textures/NewAtlas.png       # absent from the base manifest
bin/resource-packs.txt            # runtime template is not overridable
media/blocks/stone.block          # case-collides with Stone.block on Windows
```

## Effective manifests and diagnostics

The frozen view is emitted in sorted, root-independent form:

```text
# HelloMine3D effective resource manifest v1
block|media/blocks/Stone.block|Example Stone
font|media/fonts/rs.ttf|base
```

Set `HELLOMINE3D_EFFECTIVE_MANIFEST_OUT` to an output path to record this view.
Startup also prints the enabled pack, override and effective-entry counts.
Generated manifests are evidence and remain untracked.

## Verification

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\validate_resource_packs.ps1
```

`HelloMine3DResourcePackSmoke` provides 24 isolated parser/resolver assertions:
no-pack compatibility, deterministic precedence, fallback, all six resource
classes, Ogre directory order, version/traversal/stale/empty/duplicate/missing
rejection, explicit recipe/tool/audio/objective-override rejection, sorted
ownership and one-time freeze. The current base view has 41 entries including
the four base-owned gameplay registries.
The wrapper then launches the real Release/Debug client with no pack and with
`packs/example-stone`, requiring manifests that differ only in ownership of
Stone.
