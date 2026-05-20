#include "model_downloader.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <filesystem>
#include <array>
#include <cstdio>
#include <thread>

namespace fs = std::filesystem;

ModelDownloader::ModelDownloader() = default;

ModelDownloader::~ModelDownloader() {
    cancel();
    if (download_thread_.joinable()) {
        download_thread_.join();
    }
}

void ModelDownloader::download_async(const std::string& model_name,
                                     const std::string& save_path,
                                     ProgressCallback on_progress,
                                     CompleteCallback on_complete) {
    if (downloading_.load()) {
        on_complete(false, "Already downloading");
        return;
    }

    cancelled_.store(false);
    downloading_.store(true);

    download_thread_ = std::thread([this, model_name, save_path,
                                    on_progress, on_complete]() {
        std::string url = get_model_url(model_name);
        std::string path = save_path.empty() ? get_default_save_path(model_name) : save_path;

        // Ensure directory exists
        fs::path dir = fs::path(path).parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (ec) {
                spdlog::error("Failed to create directory: {}", dir.string());
                downloading_.store(false);
                on_complete(false, "");
                return;
            }
        }

        bool success = download_file(url, path, on_progress);
        downloading_.store(false);

        if (success) {
            spdlog::info("Model downloaded: {}", path);
            on_complete(true, path);
        } else {
            spdlog::error("Failed to download model: {}", model_name);
            on_complete(false, "");
        }
    });
}

void ModelDownloader::cancel() {
    cancelled_.store(true);
}

bool ModelDownloader::is_downloading() const {
    return downloading_.load();
}

std::string ModelDownloader::get_model_url(const std::string& model_name) {
    // HuggingFace model URLs
    static const std::unordered_map<std::string, std::string> model_urls = {
        {"qwen-0.5b-q4", "https://huggingface.co/Qwen/Qwen2-0.5B-Instruct-GGUF/resolve/main/qwen2-0_5b-instruct-q4_0.gguf"},
        {"qwen-0.5b-q8", "https://huggingface.co/Qwen/Qwen2-0.5B-Instruct-GGUF/resolve/main/qwen2-0_5b-instruct-q8_0.gguf"},
        {"qwen-1.5b-q4", "https://huggingface.co/Qwen/Qwen2-1.5B-Instruct-GGUF/resolve/main/qwen2-1_5b-instruct-q4_0.gguf"},
        {"tinyllama-1.1b-q4", "https://huggingface.co/TinyLlama/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_0.gguf"},
        {"phi-2-q4", "https://huggingface.co/microsoft/Phi-2-GGUF/resolve/main/phi-2.Q4_0.gguf"},
    };

    auto it = model_urls.find(model_name);
    if (it != model_urls.end()) {
        return it->second;
    }

    // Default to Qwen 0.5B Q4
    return model_urls.at("qwen-0.5b-q4");
}

std::string ModelDownloader::get_default_save_path(const std::string& model_name) {
    // Use XDG data home or fallback
    std::string data_home;
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    if (xdg_data && *xdg_data) {
        data_home = xdg_data;
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            data_home = std::string(home) + "/.local/share";
        } else {
            data_home = "/tmp";
        }
    }

    return data_home + "/term-ime/models/" + model_name + ".gguf";
}

bool ModelDownloader::model_exists(const std::string& model_name) {
    std::string path = get_default_save_path(model_name);
    return fs::exists(path);
}

std::vector<std::pair<std::string, std::string>> ModelDownloader::available_models() {
    return {
        {"qwen-0.5b-q4", "Qwen 0.5B Q4 (~300MB) - Recommended"},
        {"qwen-0.5b-q8", "Qwen 0.5B Q8 (~600MB)"},
        {"qwen-1.5b-q4", "Qwen 1.5B Q4 (~900MB)"},
        {"tinyllama-1.1b-q4", "TinyLlama 1.1B Q4 (~600MB)"},
        {"phi-2-q4", "Phi-2 Q4 (~1.5GB)"},
    };
}

bool ModelDownloader::download_file(const std::string& url,
                                    const std::string& path,
                                    ProgressCallback on_progress) {
    // Use curl for downloading
    std::string temp_path = path + ".tmp";

    // Build curl command
    std::string cmd = "curl -L -o \"" + temp_path + "\" \"" + url + "\" 2>&1";

    spdlog::info("Downloading: {} -> {}", url, path);

    // Execute curl and capture output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to execute curl");
        return false;
    }

    // Read progress from curl output
    char buffer[256];
    std::string output;
    int last_progress = -1;

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (cancelled_.load()) {
            pclose(pipe);
            fs::remove(temp_path);
            spdlog::info("Download cancelled");
            return false;
        }

        output = buffer;

        // Parse curl progress: "  0  100M    0     0  1234k      0  0:00:15  0:00:15 --:--:--  0"
        // or: "100  100M  100  100M    0     0  1234k      0  0:00:00  0:00:00 --:--:-- 1234k"
        if (output.find("%") != std::string::npos) {
            // Try to extract percentage
            size_t pos = output.find("%");
            if (pos > 0 && pos < output.size()) {
                // Find the number before %
                size_t start = pos;
                while (start > 0 && std::isdigit(output[start - 1])) {
                    start--;
                }
                if (start < pos) {
                    try {
                        int progress = std::stoi(output.substr(start, pos - start));
                        if (progress != last_progress && progress <= 100) {
                            last_progress = progress;
                            if (on_progress) {
                                on_progress(progress, "Downloading...");
                            }
                        }
                    } catch (...) {}
                }
            }
        }
    }

    int result = pclose(pipe);

    if (result != 0) {
        spdlog::error("curl failed with code: {}", result);
        fs::remove(temp_path);
        return false;
    }

    // Move temp file to final location
    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
        spdlog::error("Failed to move file: {}", ec.message());
        fs::remove(temp_path);
        return false;
    }

    if (on_progress) {
        on_progress(100, "Complete");
    }

    return true;
}
