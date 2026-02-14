#ifndef COMPRESSION_UTILS_H
#define COMPRESSION_UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdint>
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

    // Compress data using QPL. If out_latency_us is non-null and path is hardware,
    // writes wall time (us) around qpl_execute_job to *out_latency_us.
    // If out_str is non-null, writes compressed bytes into *out_str (avoids extra copy);
    // otherwise writes to internal buffer and returns result as vector.
    std::vector<uint8_t> compress(const std::string& data, uint64_t* out_latency_us = nullptr,
                                  std::string* out_str = nullptr) {
        if (!initialized_ok_) {
            std::cerr << "QPL job not initialized" << std::endl;
            if (out_str) out_str->clear();
            return std::vector<uint8_t>();
        }
        if (data.empty()) {
            if (out_str) out_str->clear();
            return std::vector<uint8_t>();
        }

        // Ensure buffer is large enough (deflate worst-case ~ n + n/16 + 64)
        size_t max_compressed_size = data.size() + (data.size() >> 4) + 2048;
        uint8_t* out_ptr;
        size_t out_cap;
        if (out_str) {
            out_str->resize(max_compressed_size);
            out_ptr = (uint8_t*)out_str->data();
            out_cap = max_compressed_size;
        } else {
            if (compressed_buffer_.size() < max_compressed_size) {
                compressed_buffer_.resize(max_compressed_size);
            }
            out_ptr = compressed_buffer_.data();
            out_cap = compressed_buffer_.size();
        }

        // Setup compression job
        job_ptr_->op = qpl_op_compress;
        job_ptr_->level = qpl_default_level;
        job_ptr_->next_in_ptr = (uint8_t*)data.data();
        job_ptr_->available_in = data.size();
        job_ptr_->next_out_ptr = out_ptr;
        job_ptr_->available_out = out_cap;
        // Align software path behavior with hardware: always use dynamic Huffman
        uint32_t flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;
        job_ptr_->flags = flags;

        // Execute compression (timed when out_latency_us requested on HW path)
        qpl_status status;
        if (out_latency_us != nullptr && execution_path_ == qpl_path_hardware) {
            auto t0 = std::chrono::steady_clock::now();
            status = qpl_execute_job(job_ptr_);
            auto t1 = std::chrono::steady_clock::now();
            *out_latency_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
        } else {
            status = qpl_execute_job(job_ptr_);
        }
        if (status != QPL_STS_OK) {
            std::cerr << "Compression failed with status: " << status << std::endl;
            if (out_str) out_str->clear();
            return std::vector<uint8_t>();
        }

        size_t compressed_size = job_ptr_->total_out;
        if (out_str) {
            out_str->resize(compressed_size);
            return std::vector<uint8_t>();
        }
        return std::vector<uint8_t>(compressed_buffer_.begin(),
                                    compressed_buffer_.begin() + compressed_size);
    }

    // Decompress from contiguous bytes. If out_str is non-null, writes into *out_str (no copy);
    // otherwise uses internal buffer and returns result (one copy).
    std::string decompress_impl(const uint8_t* data, size_t size, std::string* out_str = nullptr) {
        if (!initialized_ok_) {
            std::cerr << "QPL job not initialized" << std::endl;
            if (out_str) out_str->clear();
            return "";
        }
        if (size == 0) {
            if (out_str) out_str->clear();
            return "";
        }

        size_t estimated_size = size * 6;
        if (estimated_size < 8192) estimated_size = 8192;
        uint8_t* out_ptr;
        size_t out_cap;
        if (out_str) {
            out_str->resize(estimated_size);
            out_ptr = (uint8_t*)out_str->data();
            out_cap = estimated_size;
        } else {
            if (decompressed_buffer_.size() < estimated_size) {
                decompressed_buffer_.resize(estimated_size);
            }
            out_ptr = decompressed_buffer_.data();
            out_cap = decompressed_buffer_.size();
        }

        job_ptr_->op = qpl_op_decompress;
        job_ptr_->next_in_ptr = const_cast<uint8_t*>(data);
        job_ptr_->available_in = size;
        job_ptr_->next_out_ptr = out_ptr;
        job_ptr_->available_out = out_cap;
        job_ptr_->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;

        qpl_status status = qpl_execute_job(job_ptr_);
        if (status != QPL_STS_OK) {
            std::cerr << "Decompression failed with status: " << status << std::endl;
            if (out_str) out_str->clear();
            return "";
        }
        size_t decompressed_size = job_ptr_->total_out;
        if (out_str) {
            out_str->resize(decompressed_size);
            return std::move(*out_str);
        }
        return std::string((char*)decompressed_buffer_.data(), decompressed_size);
    }

    // Decompress data using QPL (vector overload). No copy of output when using out buffer.
    std::string decompress(const std::vector<uint8_t>& compressed_data) {
        std::string result;
        return decompress_impl(compressed_data.data(), compressed_data.size(), &result);
    }

    // Decompress from string without copying input or output.
    std::string decompress(const std::string& compressed_data) {
        std::string result;
        return decompress_impl((const uint8_t*)compressed_data.data(), compressed_data.size(), &result);
    }

    // Compress to binary string (for network transmission). Writes directly into result string (no extra copy).
    // If out_latency_us is non-null and path is hardware, writes qpl_execute_job wall time (us) there.
    std::string compress_to_string(const std::string& data, uint64_t* out_latency_us = nullptr) {
        std::string result;
        compress(data, out_latency_us, &result);
        return result;
    }

    // Decompress from binary string (uses string buffer directly, no copy of input).
    std::string decompress_from_string(const std::string& compressed_data) {
        return decompress(compressed_data);
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
    
        
        if (is_decompress_) {
            return std::string((char*)output_buffer_.data(), produced);
        }
        // Return compressed binary with prefix (no hex encoding needed)
        std::string result;
        result.reserve(11 + produced);
        result += "COMPRESSED:";
        result.append((char*)output_buffer_.data(), produced);
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

    // Extract compressed binary data (no hex decoding needed)
    const std::string compressed_binary = data.substr(11);
    if (compressed_binary.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }
    
    // Convert string to binary buffer
    handle.input_buffer_.assign(compressed_binary.begin(), compressed_binary.end());

    // Estimate output size (6x as heuristic for highly compressed data)
    size_t estimated_size = handle.input_buffer_.size() * 6;
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