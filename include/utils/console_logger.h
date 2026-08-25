#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <streambuf>

namespace ace {
namespace utils {

/**
 * @brief Custom stream buffer that writes to both console and file
 */
class TeeStreamBuf : public std::streambuf {
public:
    TeeStreamBuf(std::streambuf* sb1, std::streambuf* sb2)
        : sb1_(sb1), sb2_(sb2) {}

protected:
    virtual int overflow(int c) override {
        if (c == EOF) {
            return !EOF;
        } else {
            // Try to write to console (may fail in GUI mode - that's OK)
            if (sb1_) {
                try { sb1_->sputc(c); } catch (...) {}
            }
            // Always write to file - this is what matters
            if (sb2_) {
                int const r2 = sb2_->sputc(c);
                if (r2 == EOF) return EOF;
            }
            return c;
        }
    }

    virtual int sync() override {
        // Try to sync console (may fail in GUI mode - that's OK)
        if (sb1_) {
            try { sb1_->pubsync(); } catch (...) {}
        }
        // Always sync file - this is what matters
        if (sb2_) {
            return sb2_->pubsync();
        }
        return 0;
    }

private:
    std::streambuf* sb1_;
    std::streambuf* sb2_;
};

/**
 * @brief Console logger that writes to both console and file
 * 
 * This class captures all console output and writes it to both the console
 * and a log file. The log file is overwritten on each program startup.
 */
class ConsoleLogger {
public:
    /**
     * @brief Start logging console output to both console and file
     * 
     * @param filename Path to the log file (will be overwritten)
     * @return true if logging started successfully, false otherwise
     */
    static bool start_logging(const std::string& filename);
    
    /**
     * @brief Stop logging and close the log file
     */
    static void stop_logging();
    
    /**
     * @brief Check if logging is active
     * 
     * @return true if logging is active, false otherwise
     */
    static bool is_logging();

private:
    static std::ofstream log_file_;
    static std::streambuf* cout_backup_;
    static std::streambuf* cerr_backup_;
    static TeeStreamBuf* cout_tee_;
    static TeeStreamBuf* cerr_tee_;
    static bool is_logging_;
};

} // namespace utils
} // namespace ace
