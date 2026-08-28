#ifndef ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED
#define ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED

class ResourcePackResolver;

/// Validates the CPU-to-GLSL parameter seam before Ogre creates a window.
/// This keeps malformed resource-pack overrides on the normal startup-error
/// path instead of failing later while a GPU program is being bound.
void validateAtmosphereShaderContract(
    const ResourcePackResolver &resolver);

/// Validates the V10D single directional shadow map seam, including the
/// receiver and depth-caster programs used by Ogre's integrated path.
void validateDirectionalShadowShaderContract(
    const ResourcePackResolver &resolver);

/// Validates the V10E compositor, material, program and GLSL seam before the
/// render window is created. Resource-pack drift therefore remains a normal
/// startup error instead of becoming a late GPU binding failure.
void validatePostProcessingShaderContract(
    const ResourcePackResolver &resolver);

#endif // ATMOSPHERE_SHADER_CONTRACT_H_INCLUDED
