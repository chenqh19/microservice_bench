#ifndef COMPRESSION_UTILS_H
#define COMPRESSION_UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "qpl/qpl.h"

namespace microservice {
namespace compression {

class CompressionManager {
private:
    qpl_path_t execution_path_;
    qpl_job* job_ptr_;
    std::vector<uint8_t> compressed_buffer_;
    std::vector<uint8_t> decompressed_buffer_;

public:
    CompressionManager(qpl_path_t path = qpl_path_software) : execution_path_(path) {
        // Initialize QPL job
        job_ptr_ = nullptr;
        uint32_t size = 0;
        qpl_get_job_size(execution_path_, &size);
        job_ptr_ = (qpl_job*)std::malloc(size);
        qpl_init_job(execution_path_, job_ptr_);
        
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
        if (data.empty()) {
            return std::vector<uint8_t>();
        }

        // Ensure buffer is large enough
        size_t max_compressed_size = data.size() + 1024;  // Add some overhead
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
        job_ptr_->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;

        // Execute compression
        qpl_status status = qpl_execute_job(job_ptr_);
        if (status != QPL_STS_OK) {
            std::cerr << "Compression failed with status: " << status << std::endl;
            return std::vector<uint8_t>();
        }

        // Return compressed data
        size_t compressed_size = job_ptr_->total_out;
        return std::vector<uint8_t>(compressed_buffer_.begin(), 
                                   compressed_buffer_.begin() + compressed_size);
    }

    // Decompress data using QPL
    std::string decompress(const std::vector<uint8_t>& compressed_data) {
        if (compressed_data.empty()) {
            return "";
        }

        // Ensure buffer is large enough (start with 2x size)
        size_t estimated_size = compressed_data.size() * 2;
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

        // Return decompressed data
        size_t decompressed_size = job_ptr_->total_out;
        return std::string((char*)decompressed_buffer_.data(), decompressed_size);
    }

    // Compress and encode as base64-like string (for network transmission)
    std::string compress_to_string(const std::string& data) {
        auto compressed = compress(data);
        if (compressed.empty()) {
            return data;  // Return original if compression failed
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

        // Convert from hex string
        std::vector<uint8_t> compressed;
        compressed.reserve(encoded_data.size() / 2);
        
        for (size_t i = 0; i < encoded_data.size(); i += 2) {
            if (i + 1 < encoded_data.size()) {
                std::string hex_byte = encoded_data.substr(i, 2);
                uint8_t byte = std::stoi(hex_byte, nullptr, 16);
                compressed.push_back(byte);
            }
        }

        return decompress(compressed);
    }

    // Check if compression is beneficial
    bool should_compress(const std::string& data) {
        return data.size() > 64;  // Only compress if data is larger than 64 bytes
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

// Global compression manager instance (inline to avoid multiple definition issues)
inline std::unique_ptr<CompressionManager> g_compression_manager = nullptr;

// Initialize compression manager
inline void init_compression(qpl_path_t path = qpl_path_software) {
    g_compression_manager = std::make_unique<CompressionManager>(path);
}

// Compress data with fallback
inline std::string compress_data(const std::string& data) {
    if (!g_compression_manager) {
        init_compression();
    }
    
    if (!g_compression_manager->should_compress(data)) {
        return data;  // Return original if too small
    }
    
    auto compressed = g_compression_manager->compress_to_string(data);
    auto stats = g_compression_manager->get_compression_stats(data);
    
    // Only use compressed data if it's actually smaller
    if (stats.success && stats.compression_ratio > 1.1) {
        return "COMPRESSED:" + compressed;
    }
    
    return data;  // Return original if compression didn't help
}

// Decompress data with fallback
inline std::string decompress_data(const std::string& data) {
    if (!g_compression_manager) {
        init_compression();
    }
    
    if (data.substr(0, 11) == "COMPRESSED:") {
        std::string compressed_part = data.substr(11);
        return g_compression_manager->decompress_from_string(compressed_part);
    }
    
    return data;  // Return as-is if not compressed
}

} // namespace compression
} // namespace microservice

#endif // COMPRESSION_UTILS_H 