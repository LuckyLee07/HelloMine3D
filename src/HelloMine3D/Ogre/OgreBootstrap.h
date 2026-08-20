#pragma once

#include <vector>

#include "../Diagnostics/CrashReportInbox.h"

int runOgreBootstrap(bool validateOnly,
                     std::vector<PendingCrashReport> crashReports = {});
