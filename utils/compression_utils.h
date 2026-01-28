#ifndef COMPRESSION_UTILS_H
#define COMPRESSION_UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include "qpl/qpl.h"
#include "../config.h"

// Compression path configuration is now defined in config.h
// USE_HARDWARE_COMPRESSION and COMPRESSION_PATH are defined there

namespace microservice {
namespace compression {

class CompressionManager {
private:
    qpl_path_t execution_path_;
    qpl_job* job_ptr_;
    std::vector<uint8_t> compressed_buffer_;
    std::vector<uint8_t> decompressed_buffer_;
    bool initialized_ok_;

public:
    CompressionManager(qpl_path_t path = COMPRESSION_PATH)
        : execution_path_(path), job_ptr_(nullptr), initialized_ok_(false) {
        // Initialize QPL job
        uint32_t size = 0;
        qpl_get_job_size(execution_path_, &size);
        job_ptr_ = (qpl_job*)std::malloc(size);
        qpl_status st = qpl_init_job(execution_path_, job_ptr_);
        if (st != QPL_STS_OK) {
            std::cerr << "QPL init failed with status: " << st << std::endl;
        }
        initialized_ok_ = (st == QPL_STS_OK);
        
        // Initialize buffers
        compressed_buffer_.resize(8192);  // 8KB initial size
        decompressed_buffer_.resize(8192);
    }

    ~CompressionManager() {
        if (job_ptr_) {
            qpl_fini_job(job_ptr_);
            std::free(job_ptr_);
        }
    }

    // Compress data using QPL
    std::vector<uint8_t> compress(const std::string& data) {
        if (!initialized_ok_) {
            std::cerr << "QPL job not initialized" << std::endl;
            return std::vector<uint8_t>();
        }
        if (data.empty()) {
            return std::vector<uint8_t>();
        }

        // Ensure buffer is large enough (deflate worst-case ~ n + n/16 + 64)
        size_t max_compressed_size = data.size() + (data.size() >> 4) + 2048;
        if (compressed_buffer_.size() < max_compressed_size) {
            compressed_buffer_.resize(max_compressed_size);
        }

        // Setup compression job
        job_ptr_->op = qpl_op_compress;
        job_ptr_->level = qpl_default_level;
        job_ptr_->next_in_ptr = (uint8_t*)data.data();
        job_ptr_->available_in = data.size();
        job_ptr_->next_out_ptr = compressed_buffer_.data();
        job_ptr_->available_out = compressed_buffer_.size();
        // Align software path behavior with hardware: always use dynamic Huffman
        uint32_t flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;
        job_ptr_->flags = flags;

        // Execute compression
        qpl_status status = qpl_execute_job(job_ptr_);
        if (status != QPL_STS_OK) {
            std::cerr << "Compression failed with status: " << status << std::endl;
            return std::vector<uint8_t>();
        }

        // Print compression path used
        if (execution_path_ == qpl_path_hardware) {
            std::cout << "compress hardware" << std::endl;
        } else {
            std::cout << "compress software" << std::endl;
        }

        // Return compressed data
        size_t compressed_size = job_ptr_->total_out;
        return std::vector<uint8_t>(compressed_buffer_.begin(), 
                                   compressed_buffer_.begin() + compressed_size);
    }

    // Decompress data using QPL
    std::string decompress(const std::vector<uint8_t>& compressed_data) {
        if (!initialized_ok_) {
            std::cerr << "QPL job not initialized" << std::endl;
            return "";
        }
        if (compressed_data.empty()) {
            return "";
        }

        // Ensure buffer is large enough - start with larger estimate for better compression ratios
        // For deflate, worst case is that data didn't compress, so estimate larger
        size_t estimated_size = compressed_data.size() * 6;
        if (estimated_size < 8192) {
            estimated_size = 8192;  // Minimum buffer size
        }
        if (decompressed_buffer_.size() < estimated_size) {
            decompressed_buffer_.resize(estimated_size);
        }

        // Setup decompression job
        job_ptr_->op = qpl_op_decompress;
        job_ptr_->next_in_ptr = (uint8_t*)compressed_data.data();
        job_ptr_->available_in = compressed_data.size();
        job_ptr_->next_out_ptr = decompressed_buffer_.data();
        job_ptr_->available_out = decompressed_buffer_.size();
        job_ptr_->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;

        // Execute decompression
        qpl_status status = qpl_execute_job(job_ptr_);
        if (status != QPL_STS_OK) {
            std::cerr << "Decompression failed with status: " << status << std::endl;
            return "";
        }

        // Print decompression path used
        if (execution_path_ == qpl_path_hardware) {
            std::cout << "decompress hardware" << std::endl;
        } else {
            std::cout << "decompress software" << std::endl;
        }

        // Return decompressed data
        size_t decompressed_size = job_ptr_->total_out;
        return std::string((char*)decompressed_buffer_.data(), decompressed_size);
    }

    // Compress and encode as base64-like string (for network transmission)
    std::string compress_to_string(const std::string& data) {
        auto compressed = compress(data);
        if (compressed.empty()) {
            return "";  // Return empty string if compression failed
        }
        
        // Simple encoding: convert to hex string
        std::string result;
        result.reserve(compressed.size() * 2);
        for (uint8_t byte : compressed) {
            char hex[3];
            snprintf(hex, sizeof(hex), "%02x", byte);
            result += hex;
        }
        return result;
    }

    // Decompress from encoded string
    std::string decompress_from_string(const std::string& encoded_data) {
        if (encoded_data.empty()) {
            return "";
        }

        // Validate hex string contains only valid hex characters
        for (size_t i = 0; i < encoded_data.size(); i++) {
            char c = encoded_data[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return "";  // Return empty on invalid hex
            }
        }
        
        // Convert from hex string
        std::vector<uint8_t> compressed;
        compressed.reserve(encoded_data.size() / 2);
        
        for (size_t i = 0; i < encoded_data.size(); i += 2) {
            if (i + 1 < encoded_data.size()) {
                std::string hex_byte = encoded_data.substr(i, 2);
                try {
                    uint8_t byte = std::stoi(hex_byte, nullptr, 16);
                    compressed.push_back(byte);
                } catch (const std::exception& e) {
                    return "";  // Return empty on hex decode failure
                }
            } else {
                return "";  // Return empty on odd length
            }
        }

        return decompress(compressed);
    }

    // Check if compression is beneficial
    bool should_compress(const std::string& data) {
        return true;  // Always compress
    }

    // Get compression statistics
    struct CompressionStats {
        size_t original_size;
        size_t compressed_size;
        double compression_ratio;
        bool success;
    };

    CompressionStats get_compression_stats(const std::string& data) {
        CompressionStats stats;
        stats.original_size = data.size();
        
        auto compressed = compress(data);
        stats.compressed_size = compressed.size();
        stats.success = !compressed.empty();
        
        if (stats.success && stats.original_size > 0) {
            stats.compression_ratio = (double)stats.original_size / stats.compressed_size;
        } else {
            stats.compression_ratio = 1.0;
        }
        
        return stats;
    }
};

// Thread-local compression manager instance for thread safety
thread_local std::unique_ptr<CompressionManager> g_compression_manager = nullptr;

// Initialize compression manager (thread-safe)
inline void init_compression(qpl_path_t path = COMPRESSION_PATH) {
    if (!g_compression_manager) {
        g_compression_manager = std::make_unique<CompressionManager>(path);
    }
}


// Compress data with fallback (thread-safe)
inline std::string compress_data(const std::string& data) {
    if (!g_compression_manager) {
        init_compression();
    }
    
    // Don't compress empty data
    if (data.empty()) {
        return data;
    }
    
    // Try to compress
    auto compressed = g_compression_manager->compress_to_string(data);
    
    // If compression succeeded and returned non-empty result, use it
    if (!compressed.empty()) {
        return "COMPRESSED:" + compressed;
    }
    
    // Compression failed or returned empty, return original data
    return data;
}

// Compress with an explicitly selected path (hardware/software) decided at runtime

// Decompress data with fallback (thread-safe)
inline std::string decompress_data(const std::string& data) {
    if (!g_compression_manager) {
        init_compression();
    }
    
    // Check if data is empty
    if (data.empty()) {
        return data;
    }
    
    // Check if data starts with COMPRESSED: prefix
    if (data.size() >= 11 && data.substr(0, 11) == "COMPRESSED:") {
        std::string compressed_part = data.substr(11);
        if (compressed_part.empty()) {
            return data;  // Return original if compressed part is empty
        }
        std::string decompressed = g_compression_manager->decompress_from_string(compressed_part);
        // If decompression failed (returned empty), return original data
        if (decompressed.empty() && !compressed_part.empty()) {
            return data;
        }
        return decompressed;
    }
    
    return data;  // Return as-is if not compressed
}

// ============================================================================
// Non-blocking (asynchronous) helpers using QPL submit/wait APIs
// Submit a job with qpl_submit_job and allow waiting later with qpl_wait_job.
// ============================================================================

class AsyncQplJob {
private:
    void cleanup() {
        if (job_ptr_) {
            qpl_fini_job(job_ptr_);
            std::free(job_ptr_);
            job_ptr_ = nullptr;
        }
    }

public:
    qpl_job* job_ptr_;
    std::vector<uint8_t> input_buffer_;
    std::vector<uint8_t> output_buffer_;
    bool is_decompress_;
    bool is_passthrough_;
    std::string passthrough_value_;
    bool submitted_;
    qpl_status submit_status_;

    AsyncQplJob()
        : job_ptr_(nullptr), is_decompress_(false), is_passthrough_(false),
          submitted_(false), submit_status_(QPL_STS_OK) {}

    ~AsyncQplJob() { cleanup(); }

    AsyncQplJob(const AsyncQplJob&) = delete;
    AsyncQplJob& operator=(const AsyncQplJob&) = delete;

    AsyncQplJob(AsyncQplJob&& other) noexcept {
        job_ptr_ = other.job_ptr_;
        input_buffer_ = std::move(other.input_buffer_);
        output_buffer_ = std::move(other.output_buffer_);
        is_decompress_ = other.is_decompress_;
        is_passthrough_ = other.is_passthrough_;
        passthrough_value_ = std::move(other.passthrough_value_);
        submitted_ = other.submitted_;
        submit_status_ = other.submit_status_;
        other.job_ptr_ = nullptr;
    }

    AsyncQplJob& operator=(AsyncQplJob&& other) noexcept {
        if (this != &other) {
            cleanup();
            job_ptr_ = other.job_ptr_;
            input_buffer_ = std::move(other.input_buffer_);
            output_buffer_ = std::move(other.output_buffer_);
            is_decompress_ = other.is_decompress_;
            is_passthrough_ = other.is_passthrough_;
            passthrough_value_ = std::move(other.passthrough_value_);
            submitted_ = other.submitted_;
            submit_status_ = other.submit_status_;
            other.job_ptr_ = nullptr;
        }
        return *this;
    }

    qpl_status wait() {
        if (is_passthrough_) {
            return QPL_STS_OK;
        }
        if (!submitted_ || !job_ptr_) {
            return submit_status_;
        }
        // Block until job completes
        qpl_status st = qpl_wait_job(job_ptr_);
        submit_status_ = st;
        return st;
    }

    std::string get_result() {
        if (is_passthrough_) {
            return passthrough_value_;
        }
        qpl_status st = wait();
        if (st != QPL_STS_OK) {
            std::cerr << "QPL async job failed with status: " << st << std::endl;
            return std::string();
        }
        size_t produced = job_ptr_ ? job_ptr_->total_out : 0u;
        
        // Print compression/decompression path used
#if USE_HARDWARE_COMPRESSION
        if (is_decompress_) {
            std::cout << "decompress hardware" << std::endl;
        } else {
            std::cout << "compress hardware" << std::endl;
        }
#else
        if (is_decompress_) {
            std::cout << "decompress software" << std::endl;
        } else {
            std::cout << "compress software" << std::endl;
        }
#endif
        
        if (is_decompress_) {
            return std::string((char*)output_buffer_.data(), produced);
        }
        // Encode compressed bytes to hex and add prefix
        std::string result;
        result.reserve(11 + produced * 2);
        result += "COMPRESSED:";
        for (size_t i = 0; i < produced; ++i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02x", output_buffer_[i]);
            result += hex;
        }
        return result;
    }
};

inline AsyncQplJob submit_compress_job(const std::string& data) {
    AsyncQplJob handle;
    if (data.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_.clear();
        return handle;
    }

    // Prepare buffers
    handle.input_buffer_.assign(data.begin(), data.end());
    size_t max_compressed_size = data.size() + (data.size() >> 4) + 2048;  // safer heuristic
    handle.output_buffer_.resize(max_compressed_size);
    handle.is_decompress_ = false;

    // Allocate and initialize job
    uint32_t size = 0;
    qpl_get_job_size(COMPRESSION_PATH, &size);
    handle.job_ptr_ = (qpl_job*)std::malloc(size);
    if (!handle.job_ptr_) {
        std::cerr << "Failed to allocate memory for QPL job (compress)" << std::endl;
        handle.submit_status_ = static_cast<qpl_status>(1);  // Use non-zero value to indicate error
        handle.submitted_ = false;
        return handle;
    }
    
    qpl_status init_status = qpl_init_job(COMPRESSION_PATH, handle.job_ptr_);
    if (init_status != QPL_STS_OK) {
        std::cerr << "qpl_init_job (compress) failed with status: " << init_status << std::endl;
        std::free(handle.job_ptr_);
        handle.job_ptr_ = nullptr;
        handle.submit_status_ = init_status;
        handle.submitted_ = false;
        return handle;
    }

    // Setup job
    handle.job_ptr_->op = qpl_op_compress;
    handle.job_ptr_->level = qpl_default_level;
    handle.job_ptr_->next_in_ptr = handle.input_buffer_.data();
    handle.job_ptr_->available_in = (uint32_t)handle.input_buffer_.size();
    handle.job_ptr_->next_out_ptr = handle.output_buffer_.data();
    handle.job_ptr_->available_out = (uint32_t)handle.output_buffer_.size();
    // Align software path behavior with hardware: always use dynamic Huffman
    uint32_t flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;
    handle.job_ptr_->flags = flags;

    // Submit non-blocking
    handle.submit_status_ = qpl_submit_job(handle.job_ptr_);
    handle.submitted_ = (handle.submit_status_ == QPL_STS_OK);
    if (!handle.submitted_) {
        std::cerr << "qpl_submit_job (compress) failed with status: " << handle.submit_status_ << std::endl;
    }
    return handle;
}

inline AsyncQplJob submit_decompress_job(const std::string& data) {
    AsyncQplJob handle;
    if (data.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_.clear();
        return handle;
    }

    if (!(data.size() >= 11 && data.substr(0, 11) == "COMPRESSED:")) {
        // Not a compressed payload, passthrough
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }

    // Decode hex to input buffer
    const std::string encoded = data.substr(11);
    if (encoded.size() % 2 != 0) {
        // Invalid hex string (odd number of characters)
        std::cerr << "Invalid hex-encoded compressed data: odd number of hex characters" << std::endl;
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }
    handle.input_buffer_.reserve(encoded.size() / 2);
    try {
        for (size_t i = 0; i + 1 < encoded.size(); i += 2) {
            std::string hex_byte = encoded.substr(i, 2);
            uint8_t byte = (uint8_t)std::stoi(hex_byte, nullptr, 16);
            handle.input_buffer_.push_back(byte);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to decode hex-encoded compressed data: " << e.what() << std::endl;
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }

    // Estimate output size (2x as heuristic)
    size_t estimated_size = handle.input_buffer_.size() * 2;
    if (estimated_size == 0) {
        estimated_size = 8192;
    }
    handle.output_buffer_.resize(estimated_size);
    handle.is_decompress_ = true;

    // Allocate and initialize job
    uint32_t size = 0;
    qpl_get_job_size(COMPRESSION_PATH, &size);
    handle.job_ptr_ = (qpl_job*)std::malloc(size);
    if (!handle.job_ptr_) {
        std::cerr << "Failed to allocate memory for QPL job (decompress)" << std::endl;
        handle.submit_status_ = static_cast<qpl_status>(1);  // Use non-zero value to indicate error
        handle.submitted_ = false;
        return handle;
    }
    
    qpl_status init_status = qpl_init_job(COMPRESSION_PATH, handle.job_ptr_);
    if (init_status != QPL_STS_OK) {
        std::cerr << "qpl_init_job (decompress) failed with status: " << init_status << std::endl;
        std::free(handle.job_ptr_);
        handle.job_ptr_ = nullptr;
        handle.submit_status_ = init_status;
        handle.submitted_ = false;
        return handle;
    }

    // Setup job
    handle.job_ptr_->op = qpl_op_decompress;
    handle.job_ptr_->next_in_ptr = handle.input_buffer_.data();
    handle.job_ptr_->available_in = (uint32_t)handle.input_buffer_.size();
    handle.job_ptr_->next_out_ptr = handle.output_buffer_.data();
    handle.job_ptr_->available_out = (uint32_t)handle.output_buffer_.size();
    handle.job_ptr_->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;

    // Submit non-blocking
    handle.submit_status_ = qpl_submit_job(handle.job_ptr_);
    handle.submitted_ = (handle.submit_status_ == QPL_STS_OK);
    if (!handle.submitted_) {
        std::cerr << "qpl_submit_job (decompress) failed with status: " << handle.submit_status_ << std::endl;
    }
    return handle;
}

// ----------------------------------------------------------------------------
// Config-selected API
// If COMPRESSION_NONBLOCKING==1, return AsyncQplJob handles (submit now, wait later)
// Otherwise, return blocking string results immediately.
// ----------------------------------------------------------------------------
#if COMPRESSION_NONBLOCKING
using compression_result_t = AsyncQplJob;

inline compression_result_t compress_data_configured(const std::string& data) {
    return submit_compress_job(data);
}

inline compression_result_t decompress_data_configured(const std::string& data) {
    return submit_decompress_job(data);
}
#else
using compression_result_t = std::string;

inline compression_result_t compress_data_configured(const std::string& data) {
    return compress_data(data);
}

inline compression_result_t decompress_data_configured(const std::string& data) {
    return decompress_data(data);
}
#endif

// ----------------------------------------------------------------------------
// Unified submission API (mode-agnostic)
// Always returns an AsyncQplJob. In blocking mode, we precompute and store the
// result in passthrough_value_ and mark is_passthrough_.
// ----------------------------------------------------------------------------
inline AsyncQplJob submit_compress(const std::string& data) {
#if COMPRESSION_NONBLOCKING
    return submit_compress_job(data);
#else
    AsyncQplJob handle;
    handle.is_passthrough_ = true;
    handle.passthrough_value_ = compress_data(data);
    return handle;
#endif
}

inline AsyncQplJob submit_decompress(const std::string& data) {
#if COMPRESSION_NONBLOCKING
    return submit_decompress_job(data);
#else
    AsyncQplJob handle;
    handle.is_passthrough_ = true;
    handle.passthrough_value_ = decompress_data(data);
    return handle;
#endif
}

} // namespace compression
} // namespace microservice

#endif // COMPRESSION_UTILS_H 