#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

// Model downloader with progress callback
class ModelDownloader {
public:
    using ProgressCallback = std::function<void(int progress, const std::string& status)>;
    using CompleteCallback = std::function<void(bool success, const std::string& path)>;

    ModelDownloader();
    ~ModelDownloader();

    // Download model asynchronously
    void download_async(const std::string& model_name,
                       const std::string& save_path,
                       ProgressCallback on_progress,
                       CompleteCallback on_complete);

    // Cancel download
    void cancel();

    // Check if downloading
    bool is_downloading() const;

    // Get default model URL
    static std::string get_model_url(const std::string& model_name);

    // Get default save path
    static std::string get_default_save_path(const std::string& model_name);

    // Check if model exists
    static bool model_exists(const std::string& model_name);

    // Get available models
    static std::vector<std::pair<std::string, std::string>> available_models();

private:
    std::atomic<bool> downloading_{false};
    std::atomic<bool> cancelled_{false};
    std::thread download_thread_;
    std::mutex mutex_;

    bool download_file(const std::string& url,
                      const std::string& path,
                      ProgressCallback on_progress);
};
