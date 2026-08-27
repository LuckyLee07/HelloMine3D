#ifndef ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED
#define ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED

class ResourcePackResolver;

/// Validates the CPU-to-GLSL parameter seam before Ogre creates a window.
/// This keeps malformed resource-pack overrides on the normal startup-error
/// path instead of failing later while a GPU program is being bound.
void validateAtmosphereShaderContract(
    const ResourcePackResolver &resolver);

#endif // ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED
