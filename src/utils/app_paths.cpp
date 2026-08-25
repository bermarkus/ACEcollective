#include "utils/app_paths.h"

#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <pwd.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

namespace ace {
namespace utils {

std::string get_app_directory() {
    try {
#ifdef __APPLE__
        char buf[4096];
        uint32_t size = sizeof(buf);
        if (_NSGetExecutablePath(buf, &size) == 0) {
            std::filesystem::path exe = std::filesystem::canonical(buf);
            for (auto p = exe.parent_path(); !p.empty() && p != p.root_path(); p = p.parent_path()) {
                if (p.extension() == ".app") {
                    return p.parent_path().string();
                }
            }
            return exe.parent_path().string();
        }
#elif defined(_WIN32)
        char buf[MAX_PATH];
        if (GetModuleFileNameA(nullptr, buf, MAX_PATH) > 0) {
            std::filesystem::path exe = buf;
            return exe.parent_path().string();
        }
#else
        std::filesystem::path exe = std::filesystem::canonical("/proc/self/exe");
        return exe.parent_path().string();
#endif
    } catch (...) {
        // fall through
    }
    return std::filesystem::current_path().string();
}

std::string get_app_data_directory() {
    std::filesystem::path dir;

    try {
#ifdef __APPLE__
        const char* home = getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) {
            dir = std::filesystem::path(home) / "Documents" / "ACEcollective";
        }
#elif defined(_WIN32)
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path))) {
            dir = std::filesystem::path(path) / "ACEcollective";
        }
#else
        const char* home = getenv("HOME");
        if (home) {
            dir = std::filesystem::path(home) / "Documents" / "ACEcollective";
        }
#endif
        if (dir.empty()) {
            dir = std::filesystem::path(get_app_directory()) / "ACEcollective";
        }
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception&) {
        try {
            dir = std::filesystem::path(get_app_directory()) / "ACEcollective";
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }
        } catch (...) {
            try {
                dir = std::filesystem::temp_directory_path() / "ACEcollective";
                if (!std::filesystem::exists(dir)) {
                    std::filesystem::create_directories(dir);
                }
            } catch (...) {
                dir = std::filesystem::current_path() / "ACEcollective";
            }
        }
    }

    return dir.string();
}

std::string get_recordings_directory() {
    std::filesystem::path dir = std::filesystem::path(get_app_data_directory()) / "CoherenceRecordings";
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception&) {
        // Silently fail -- logging may not be initialized yet
    }
    return dir.string();
}

std::string get_logs_directory() {
    std::filesystem::path dir = std::filesystem::path(get_app_data_directory()) / "logs";
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception&) {
        // Silently fail -- logging may not be initialized yet
    }
    return dir.string();
}

} // namespace utils
} // namespace ace
