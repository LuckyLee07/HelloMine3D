#ifndef RUNTIMECONFIG_H_INCLUDED
#define RUNTIMECONFIG_H_INCLUDED

#include <string>

#include "Config.h"

/// Loads the per-user runtime configuration, writing documented defaults when
/// the file does not exist.
Config loadRuntimeConfig(const std::string &path);

#endif // RUNTIMECONFIG_H_INCLUDED
