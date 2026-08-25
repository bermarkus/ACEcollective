#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include "imgui/imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "application.h"
#include "utils/console_logger.h"
#include "utils/app_paths.h"
#include "version.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#else
#include <GLFW/glfw3.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error " << error << ": " << description << std::endl;
}

// Ported from ACEapp's main.cpp: log to Documents/ACEcollective/logs/, with
// the same fallback chain and 5-file rotation.
static void start_logging()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif
    char timestamp[32];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm);

    std::string log_dir;
    std::string log_path;
    try {
        log_dir = ace::utils::get_logs_directory();
        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
        log_path = log_dir + "/acecollective_" + timestamp + ".txt";
    } catch (...) {
        log_dir = std::filesystem::temp_directory_path().string();
        log_path = log_dir + "/acecollective_" + timestamp + ".txt";
    }

    try {
        if (!log_dir.empty() && std::filesystem::exists(log_dir)) {
            std::vector<std::filesystem::directory_entry> log_files;
            for (const auto& entry : std::filesystem::directory_iterator(log_dir)) {
                if (entry.is_regular_file() && entry.path().filename().string().rfind("acecollective_", 0) == 0) {
                    log_files.push_back(entry);
                }
            }
            std::sort(log_files.begin(), log_files.end(), [](const auto& a, const auto& b) {
                return a.last_write_time() > b.last_write_time();
            });
            for (size_t i = 4; i < log_files.size(); ++i) {
                std::error_code ec;
                std::filesystem::remove(log_files[i].path(), ec);
            }
        }
    } catch (...) {}

    if (ace::utils::ConsoleLogger::start_logging(log_path)) {
        std::cout << "Log file: " << log_path << std::endl;
    } else {
        std::cout << "WARNING: Failed to create log file at: " << log_path << std::endl;
    }
}

static int run_application()
{
    start_logging();
    std::cout << "ACEcollective v" << ace::kAppVersion << std::endl;

    try {
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit()) return 1;

#if defined(IMGUI_IMPL_OPENGL_ES2)
        const char* glsl_version = "#version 100";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

        GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        std::string window_title = std::string("ACEcollective v") + ace::kAppVersion + " - Inter-Brain Coherence";
        GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, window_title.c_str(), nullptr, nullptr);
        if (window == nullptr) return 1;
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        Application app;
        app.initialize();

        // Paced by vsync alone (glfwSwapInterval(1) above) -- an earlier
        // version also manually slept to cap at a hardcoded 60fps on top of
        // that, which only ever added latency on displays refreshing faster
        // than 60Hz and was redundant on a 60Hz display where vsync already
        // paces every swap. Dropped rather than matching ACEapp's detected-
        // display-refresh-rate approach, since vsync alone is sufficient
        // here and adds no extra complexity.
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            app.update();
            app.render();

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.07f, 0.07f, 0.07f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        ace::utils::ConsoleLogger::stop_logging();
        glfwTerminate();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        ace::utils::ConsoleLogger::stop_logging();
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        ace::utils::ConsoleLogger::stop_logging();
        return 2;
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    return run_application();
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return run_application();
}
#endif
