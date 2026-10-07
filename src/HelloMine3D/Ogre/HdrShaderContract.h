#pragma once
class ResourcePackResolver;
void validateHdrShaderContract(const ResourcePackResolver& resolver);

// Optional scene interface: legacy resource packs need not implement HDR.
void validateHdrSceneShaderContract(const ResourcePackResolver& resolver);
