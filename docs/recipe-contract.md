# HelloMine3D Recipe Contract v1

Recipe v1 is a startup-only data format. It names registered materials and
describes crafting inputs and outputs; it cannot call code, include another
file, alter inventory behavior or reload while the process is running.

## Source format

Every `.recipe` file begins with this exact UTF-8 header:

```text
# HelloMine3D recipe registry v1
```

Blank lines and later `#` comment lines are ignored. A shaped recipe uses one
to three equal-width rows of one to three cells. `_` is an empty cell and the
outer empty rows/columns do not change pattern identity:

```text
recipe hellomine:chest shaped
row hellomine:oak_bark hellomine:oak_bark hellomine:oak_bark
row hellomine:oak_bark _ hellomine:oak_bark
row hellomine:oak_bark hellomine:oak_bark hellomine:oak_bark
output hellomine:chest 1
end
```

A shapeless recipe has at most nine unique material entries. Input order does
not change pattern identity and the combined count may not exceed 99:

```text
recipe hellomine:borderless_glass shapeless
input hellomine:glass 1
output hellomine:glass_borderless 1
end
```

Recipe ids are lowercase ASCII `namespace:path` values of at most 80 characters.
The namespace accepts `a-z`, `0-9`, `_`, `-` and `.`, while the path also
accepts `/`; leading/trailing slashes, `//`, `..`, extra colons, uppercase and
non-ASCII bytes are rejected identically on every host locale.
Material ids must round-trip through the built-in `Material` registry;
`hellomine:nothing` is never a valid ingredient or output. Counts are integers
in `[1, 99]`, and an output may not exceed its material stack limit.

## Bounds and duplicate policy

One process accepts at most 32 source files, 256 KiB per file and 256 recipes.
The parser rejects unknown directives, unfinished recipes, mixed shaped and
shapeless directives, uneven or oversized rows, duplicate shapeless
materials, missing/duplicate outputs and invalid counts.

Sources are sorted by logical path before parsing and the final registry is
sorted by recipe id. Duplicate ids name the first sorted source. Shaped
patterns are compared after trimming their empty border; shapeless patterns
are compared after sorting material/count pairs. Equivalent patterns are
rejected instead of relying on file-system enumeration order.

Parsing is atomic: a failed freeze publishes no entries. A successful freeze
cannot be repeated in the same process.

## Resource ownership

The generated startup manifest lists base recipe files with the `recipe`
category. The registry reads them only from an already frozen effective
resource view and requires `base` ownership. Resource-pack contract v1 still
allows only block, font, resource-script, shader, shape and texture overrides;
a pack containing a recipe path fails as a stale or unsupported override.
A future recipe override format therefore requires an explicit resource-pack
contract version rather than silently becoming a seventh v1 class.

## Verification

`HelloMine3DRecipeSmoke` freezes the expected coverage count at 40 assertions.
It covers registered ids, locale-independent ASCII ids, UTF-8 BOM/CRLF input,
shaped/shapeless parsing, source/grid/entry/recipe bounds, directive
completeness, deterministic ordering and duplicates, atomic failure, frozen
resource loading and missing resources. `HelloMine3DResourcePackSmoke` separately
proves that recipe overrides remain forbidden. The normal client prints
`[RECIPE_REGISTRY] frozen=1 recipes=<count>` before constructing Ogre.
