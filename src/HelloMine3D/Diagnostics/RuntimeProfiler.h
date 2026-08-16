#ifndef HELLOMINE3D_DIAGNOSTICS_RUNTIMEPROFILER_H
#define HELLOMINE3D_DIAGNOSTICS_RUNTIMEPROFILER_H

#if defined(HELLOMINE3D_ENABLE_TRACY)

#include <tracy/Tracy.hpp>

#define HELLOMINE3D_PROFILE_FRAME() FrameMark
#define HELLOMINE3D_PROFILE_SCOPE(name) ZoneScopedN(name)
#define HELLOMINE3D_PROFILE_PLOT(name, value) TracyPlot(name, value)
#define HELLOMINE3D_PROFILE_THREAD(name) ::tracy::SetThreadName(name)

namespace RuntimeProfiler
{
    constexpr bool isEnabled() noexcept
    {
        return true;
    }
}

#else

#define HELLOMINE3D_PROFILE_FRAME() ((void)0)
#define HELLOMINE3D_PROFILE_SCOPE(name) ((void)0)
#define HELLOMINE3D_PROFILE_PLOT(name, value) ((void)0)
#define HELLOMINE3D_PROFILE_THREAD(name) ((void)0)

namespace RuntimeProfiler
{
    constexpr bool isEnabled() noexcept
    {
        return false;
    }
}

#endif

#endif
