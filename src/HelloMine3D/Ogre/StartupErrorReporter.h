#pragma once

#include <string>

namespace StartupErrorReporter
{
    void present(const std::string& diagnostic, bool requestDialog);
}
