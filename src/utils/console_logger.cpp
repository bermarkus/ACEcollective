#include "utils/console_logger.h"
#include <iostream>

namespace ace {
namespace utils {

// Static member initialization
std::ofstream ConsoleLogger::log_file_;
std::streambuf* ConsoleLogger::cout_backup_ = nullptr;
std::streambuf* ConsoleLogger::cerr_backup_ = nullptr;
TeeStreamBuf* ConsoleLogger::cout_tee_ = nullptr;
TeeStreamBuf* ConsoleLogger::cerr_tee_ = nullptr;
bool ConsoleLogger::is_logging_ = false;

bool ConsoleLogger::start_logging(const std::string& filename) {
    if (is_logging_) {
        return true; // Already logging
    }
    
    // Open log file (overwrite mode)
    log_file_.open(filename, std::ios::out | std::ios::trunc);
    if (!log_file_.is_open()) {
        // Don't write to stderr here - it can't be captured since tee buffers aren't set up yet
        // The caller will handle the error
        return false;
    }
    
    // Write header first, before setting up tee buffers
    log_file_ << "=== ACEcollective Console Log ===" << std::endl;
    log_file_.flush();
    
    // Backup original stream buffers
    cout_backup_ = std::cout.rdbuf();
    cerr_backup_ = std::cerr.rdbuf();
    
    // Create tee stream buffers that write to both console and file
    cout_tee_ = new TeeStreamBuf(cout_backup_, log_file_.rdbuf());
    cerr_tee_ = new TeeStreamBuf(cerr_backup_, log_file_.rdbuf());
    
    // Redirect cout and cerr to use the tee buffers
    std::cout.rdbuf(cout_tee_);
    std::cerr.rdbuf(cerr_tee_);
    
    is_logging_ = true;
    
    // Write rest of header (will appear in both console and file)
    std::cout << "Log started" << std::endl;
    std::cout << "===========================" << std::endl;
    
    return true;
}

void ConsoleLogger::stop_logging() {
    if (!is_logging_) {
        return;
    }
    
    // Write footer (will appear in both console and file)
    std::cout << "===========================" << std::endl;
    std::cout << "Log ended" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Flush everything before cleanup
    std::cout.flush();
    std::cerr.flush();
    if (log_file_.is_open()) {
        log_file_.flush();
    }
    
    // Delete tee buffers FIRST (while streams still redirected)
    if (cout_tee_) {
        delete cout_tee_;
        cout_tee_ = nullptr;
    }
    
    if (cerr_tee_) {
        delete cerr_tee_;
        cerr_tee_ = nullptr;
    }
    
    // Restore original stream buffers
    if (cout_backup_) {
        std::cout.rdbuf(cout_backup_);
        cout_backup_ = nullptr;
    }
    
    if (cerr_backup_) {
        std::cerr.rdbuf(cerr_backup_);
        cerr_backup_ = nullptr;
    }
    
    // Close log file LAST
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    is_logging_ = false;
}

bool ConsoleLogger::is_logging() {
    return is_logging_;
}

} // namespace utils
} // namespace ace
