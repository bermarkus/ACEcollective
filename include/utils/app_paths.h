#pragma once

#include <string>

namespace ace {
namespace utils {

// Returns the directory containing the running executable / .app bundle.
// Ported from ACEapp's utils/app_paths.h with ACEcollective's own data dir.
std::string get_app_directory();

// Returns the main ACEcollective data directory in the user's Documents folder.
// Location: ~/Documents/ACEcollective (macOS/Linux) or
// %USERPROFILE%\Documents\ACEcollective (Windows). Created if missing.
std::string get_app_data_directory();

// Returns the recordings directory: Documents/ACEcollective/CoherenceRecordings/
std::string get_recordings_directory();

// Returns the logs directory: Documents/ACEcollective/logs/
std::string get_logs_directory();

} // namespace utils
} // namespace ace
