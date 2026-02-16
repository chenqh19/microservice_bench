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
    // Uses a large output estimate (size * 256, min 256KB) to avoid QPL_STS_MORE_OUTPUT_NEEDED for
    // highly compressible data; retries with 4x buffer if that status is returned.
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

        const size_t min_estimated = 256 * 1024;  // 256KB minimum for highly compressible data
        size_t estimated_size = size * 256;
        if (estimated_size < min_estimated) estimated_size = min_estimated;

        for (int retry = 0; retry < 3; ++retry) {
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
            if (status == QPL_STS_OK) {
                size_t decompressed_size = job_ptr_->total_out;
                if (out_str) {
                    out_str->resize(decompressed_size);
                    return std::move(*out_str);
                }
                return std::string((char*)decompressed_buffer_.data(), decompressed_size);
            }
            if (status == QPL_STS_MORE_OUTPUT_NEEDED) {
                estimated_size *= 4;
                continue;
            }
            std::cerr << "Decompression failed with status: " << status << std::endl;
            if (out_str) out_str->clear();
            return "";
        }
        std::cerr << "Decompression failed: output buffer still too small after retries" << std::endl;
        if (out_str) out_str->clear();
        return "";
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
// Non-blocking (asynchronous) helpers using QPL submit/check APIs
// Submit with qpl_submit_job; poll for completion with qpl_check_job (no blocking qpl_wait_job).
// Jobs are reused per thread per path (like sync CompressionManager): one job per (path, op).
// ============================================================================

struct ReusableQplJob {
    qpl_job* job_ptr_ = nullptr;
    qpl_path_t path_ = COMPRESSION_PATH;
    bool is_decompress_ = false;
    std::vector<uint8_t> input_buffer_;
    std::vector<uint8_t> output_buffer_;
    bool initialized_ = false;

    bool ensure_init() {
        if (initialized_) return true;
        uint32_t size = 0;
        qpl_get_job_size(path_, &size);
        job_ptr_ = (qpl_job*)std::malloc(size);
        if (!job_ptr_) return false;
        qpl_status st = qpl_init_job(path_, job_ptr_);
        if (st != QPL_STS_OK) {
            std::free(job_ptr_);
            job_ptr_ = nullptr;
            return false;
        }
        initialized_ = true;
        return true;
    }

    void wait_idle() {
        if (!job_ptr_) return;
        qpl_status st;
        do {
            st = qpl_check_job(job_ptr_);
            if (st == QPL_STS_BEING_PROCESSED) {
                std::this_thread::yield();
            }
        } while (st == QPL_STS_BEING_PROCESSED);
    }
};

namespace {
inline int reusable_index(qpl_path_t path, bool is_decompress) {
    return (path == qpl_path_hardware ? 0 : 1) + (is_decompress ? 2 : 0);
}
thread_local ReusableQplJob g_reusable_jobs[4];

inline ReusableQplJob* get_reusable_job(qpl_path_t path, bool is_decompress) {
    int idx = reusable_index(path, is_decompress);
    ReusableQplJob* r = &g_reusable_jobs[idx];
    r->path_ = path;
    r->is_decompress_ = is_decompress;
    if (!r->ensure_init()) return nullptr;
    return r;
}
}  // namespace

class AsyncQplJob {
private:
    bool owns_job_ = true;
    ReusableQplJob* borrowed_ = nullptr;

    void cleanup() {
        if (owns_job_ && job_ptr_) {
            qpl_fini_job(job_ptr_);
            std::free(job_ptr_);
            job_ptr_ = nullptr;
        }
        borrowed_ = nullptr;
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
        owns_job_ = other.owns_job_;
        borrowed_ = other.borrowed_;
        job_ptr_ = other.job_ptr_;
        input_buffer_ = std::move(other.input_buffer_);
        output_buffer_ = std::move(other.output_buffer_);
        is_decompress_ = other.is_decompress_;
        is_passthrough_ = other.is_passthrough_;
        passthrough_value_ = std::move(other.passthrough_value_);
        submitted_ = other.submitted_;
        submit_status_ = other.submit_status_;
        other.owns_job_ = true;
        other.borrowed_ = nullptr;
        other.job_ptr_ = nullptr;
    }

    AsyncQplJob& operator=(AsyncQplJob&& other) noexcept {
        if (this != &other) {
            cleanup();
            owns_job_ = other.owns_job_;
            borrowed_ = other.borrowed_;
            job_ptr_ = other.job_ptr_;
            input_buffer_ = std::move(other.input_buffer_);
            output_buffer_ = std::move(other.output_buffer_);
            is_decompress_ = other.is_decompress_;
            is_passthrough_ = other.is_passthrough_;
            passthrough_value_ = std::move(other.passthrough_value_);
            submitted_ = other.submitted_;
            submit_status_ = other.submit_status_;
            other.owns_job_ = true;
            other.borrowed_ = nullptr;
            other.job_ptr_ = nullptr;
        }
        return *this;
    }

    // Set this handle to reference a reusable (pool) job; used by submit_*_job.
    void set_borrowed(ReusableQplJob* r, bool is_decompress) {
        owns_job_ = false;
        borrowed_ = r;
        job_ptr_ = r ? r->job_ptr_ : nullptr;
        is_decompress_ = is_decompress;
    }

    // Poll for completion using qpl_check_job (non-blocking check). Loops until job is done
    // so the calling thread yields instead of blocking in qpl_wait_job.
    qpl_status wait() {
        if (is_passthrough_) {
            return QPL_STS_OK;
        }
        if (!submitted_ || !job_ptr_) {
            return submit_status_;
        }
        qpl_status st;
        do {
            st = qpl_check_job(job_ptr_);
            if (st == QPL_STS_BEING_PROCESSED) {
                std::this_thread::yield();
            }
        } while (st == QPL_STS_BEING_PROCESSED);
        submit_status_ = st;
        return st;
    }

    // Get result string. If out_latency_us is non-null, times the wait() call and
    // writes wall-clock microseconds to *out_latency_us (for HW latency sampling).
    std::string get_result(uint64_t* out_latency_us = nullptr) {
        if (is_passthrough_) {
            return passthrough_value_;
        }
        qpl_status st;
        if (out_latency_us) {
            auto t0 = std::chrono::steady_clock::now();
            st = wait();
            auto t1 = std::chrono::steady_clock::now();
            *out_latency_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
        } else {
            st = wait();
        }
        if (st != QPL_STS_OK) {
            std::cerr << "QPL async job failed with status: " << st << std::endl;
            return std::string();
        }
        size_t produced = job_ptr_ ? job_ptr_->total_out : 0u;
        const uint8_t* out_data = borrowed_ ? borrowed_->output_buffer_.data() : output_buffer_.data();

        if (is_decompress_) {
            return std::string((char*)out_data, produced);
        }
        // Return compressed binary with prefix (no hex encoding needed)
        std::string result;
        result.reserve(11 + produced);
        result += "COMPRESSED:";
        result.append((char*)out_data, produced);
        return result;
    }
};

inline AsyncQplJob submit_compress_job(const std::string& data, qpl_path_t path = COMPRESSION_PATH) {
    AsyncQplJob handle;
    if (data.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_.clear();
        return handle;
    }

    ReusableQplJob* reusable = get_reusable_job(path, false);
    if (!reusable) {
        std::cerr << "Failed to get reusable compress job for path" << std::endl;
        handle.submit_status_ = static_cast<qpl_status>(1);
        handle.submitted_ = false;
        return handle;
    }
    reusable->wait_idle();

    reusable->input_buffer_.assign(data.begin(), data.end());
    size_t max_compressed_size = data.size() + (data.size() >> 4) + 2048;
    reusable->output_buffer_.resize(max_compressed_size);

    reusable->job_ptr_->op = qpl_op_compress;
    reusable->job_ptr_->level = qpl_default_level;
    reusable->job_ptr_->next_in_ptr = reusable->input_buffer_.data();
    reusable->job_ptr_->available_in = (uint32_t)reusable->input_buffer_.size();
    reusable->job_ptr_->next_out_ptr = reusable->output_buffer_.data();
    reusable->job_ptr_->available_out = (uint32_t)reusable->output_buffer_.size();
    uint32_t flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;
    reusable->job_ptr_->flags = flags;

    handle.set_borrowed(reusable, false);
    handle.submit_status_ = qpl_submit_job(reusable->job_ptr_);
    handle.submitted_ = (handle.submit_status_ == QPL_STS_OK);
    if (!handle.submitted_) {
        std::cerr << "qpl_submit_job (compress) failed with status: " << handle.submit_status_ << std::endl;
    }
    return handle;
}

inline AsyncQplJob submit_decompress_job(const std::string& data, qpl_path_t path = COMPRESSION_PATH) {
    AsyncQplJob handle;
    if (data.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_.clear();
        return handle;
    }

    if (!(data.size() >= 11 && data.substr(0, 11) == "COMPRESSED:")) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }

    const std::string compressed_binary = data.substr(11);
    if (compressed_binary.empty()) {
        handle.is_passthrough_ = true;
        handle.passthrough_value_ = data;
        return handle;
    }

    ReusableQplJob* reusable = get_reusable_job(path, true);
    if (!reusable) {
        std::cerr << "Failed to get reusable decompress job for path" << std::endl;
        handle.submit_status_ = static_cast<qpl_status>(1);
        handle.submitted_ = false;
        return handle;
    }
    reusable->wait_idle();

    reusable->input_buffer_.assign(compressed_binary.begin(), compressed_binary.end());
    size_t estimated_size = reusable->input_buffer_.size() * 256;
    if (estimated_size < 256 * 1024) {
        estimated_size = 256 * 1024;
    }
    reusable->output_buffer_.resize(estimated_size);

    reusable->job_ptr_->op = qpl_op_decompress;
    reusable->job_ptr_->next_in_ptr = reusable->input_buffer_.data();
    reusable->job_ptr_->available_in = (uint32_t)reusable->input_buffer_.size();
    reusable->job_ptr_->next_out_ptr = reusable->output_buffer_.data();
    reusable->job_ptr_->available_out = (uint32_t)reusable->output_buffer_.size();
    reusable->job_ptr_->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST;

    handle.set_borrowed(reusable, true);
    handle.submit_status_ = qpl_submit_job(reusable->job_ptr_);
    handle.submitted_ = (handle.submit_status_ == QPL_STS_OK);
    if (!handle.submitted_) {
        std::cerr << "qpl_submit_job (decompress) failed with status: " << handle.submit_status_ << std::endl;
    }
    return handle;
}

// ----------------------------------------------------------------------------
// Sync-compatible API using async path (submit -> wait -> get result)
// Returns raw compressed bytes, same as CompressionManager::compress_to_string.
// Use this from decision layer so the service uses the async QPL path with
// per-request path selection and optional HW latency reporting.
// ----------------------------------------------------------------------------
inline std::string compress_to_string_via_async(const std::string& data, qpl_path_t path,
                                                uint64_t* out_latency_us = nullptr) {
    if (data.empty()) {
        return std::string();
    }
    AsyncQplJob handle = submit_compress_job(data, path);
    std::string full = handle.get_result(out_latency_us);
    if (full.empty()) {
        return std::string();
    }
    // get_result() for compress returns "COMPRESSED:" + binary; return raw bytes only
    if (full.size() >= 11 && full.substr(0, 11) == "COMPRESSED:") {
        return full.substr(11);
    }
    return full;
}

// ----------------------------------------------------------------------------
// Decompress using same async path and path (HW/SW) as compression.
// Input: string with optional "COMPRESSED:" prefix; output: decompressed string.
// When path is HW and out_latency_us is non-null, writes wait time (us) there.
// ----------------------------------------------------------------------------
inline std::string decompress_from_string_via_async(const std::string& data, qpl_path_t path,
                                                    uint64_t* out_latency_us = nullptr) {
    if (data.empty()) {
        return data;
    }
    AsyncQplJob handle = submit_decompress_job(data, path);
    std::string result = (path == qpl_path_hardware && out_latency_us)
        ? handle.get_result(out_latency_us) : handle.get_result(nullptr);
    return result.empty() ? data : result;
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