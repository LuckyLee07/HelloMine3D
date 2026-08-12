#include "StartupErrorReporter.h"

#include <cstdlib>
#include <fstream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{
    bool isTrueValue(const char* value)
    {
        if (value == nullptr || value[0] == '\0')
        {
            return false;
        }
        const std::string text(value);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "False" && text != "off" && text != "OFF";
    }

    std::string buildMessage(const std::string& diagnostic)
    {
        return "HelloMine3D could not start.\n\n" + diagnostic +
               "\n\nThe complete diagnostic was also written to stderr. "
               "See MineOgre.log when Ogre logging was initialized.";
    }

#if defined(_WIN32)
    std::wstring toWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        int length = MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0);
        UINT codePage = CP_UTF8;
        if (length <= 0)
        {
            codePage = CP_ACP;
            length = MultiByteToWideChar(
                codePage, 0, value.data(),
                static_cast<int>(value.size()), nullptr, 0);
        }
        if (length <= 0)
        {
            return L"HelloMine3D startup failed. See stderr.";
        }

        std::wstring wide(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            codePage, 0, value.data(), static_cast<int>(value.size()),
            wide.data(), length);
        return wide;
    }
#endif
}

void StartupErrorReporter::present(const std::string& diagnostic,
                                   bool requestDialog)
{
    const std::string message = buildMessage(diagnostic);
    const bool suppressDialog = isTrueValue(
        std::getenv("HELLOMINE3D_STARTUP_ERROR_NO_DIALOG"));
    const char* reportPath =
        std::getenv("HELLOMINE3D_STARTUP_ERROR_REPORT");
    if (reportPath != nullptr && reportPath[0] != '\0')
    {
        std::ofstream report(reportPath, std::ios::out | std::ios::trunc);
        if (report)
        {
#if defined(_WIN32)
            report << "ui=MessageBoxW\n";
#else
            report << "ui=stderr-only\n";
#endif
            report << "dialog_requested="
                   << (requestDialog ? "true" : "false") << "\n";
            report << "dialog_suppressed="
                   << (suppressDialog ? "true" : "false") << "\n";
            report << message << '\n';
        }
    }

#if defined(_WIN32)
    if (requestDialog && !suppressDialog)
    {
        const std::wstring wideMessage = toWide(message);
        MessageBoxW(nullptr, wideMessage.c_str(),
                    L"HelloMine3D startup failed",
                    MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
    }
#else
    (void)requestDialog;
    (void)suppressDialog;
#endif
}
