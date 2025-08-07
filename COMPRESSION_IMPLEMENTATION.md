# QPL Compression Implementation in Microservice Bench

This document describes the implementation of Intel QPL (Query Processing Library) compression across all microservices in the microservice_bench project.

## Directory Structure

The project has been reorganized for better organization:

```
microservice_bench/
├── services/                    # All microservices
│   ├── user_service/
│   ├── profile_service/
│   ├── search_service/
│   ├── recommendation_service/
│   ├── reservation_service/
│   ├── geo_service/
│   ├── frontend_service/
│   └── rate_service/
├── utils/                       # Shared utilities
│   ├── compression_utils.h      # QPL compression utilities
│   ├── serialization_utils.h    # Protobuf serialization
│   ├── padding_utils.h          # Data padding utilities
│   └── prefork_utils.h          # Process management
├── protos/                      # Protocol buffer definitions
└── ... (other files)
```

## Compression Features

- **Universal compression**: All data is compressed regardless of size
- **Unconditional compression**: No compression ratio thresholds
- **Hardware acceleration**: Intel IAX support when available
- **Software fallback**: Automatic fallback to software compression
- **Network transmission**: Hex-encoded compressed data for network transfer
- **Complete cycle**: Full compression/decompression across all inter-service communication

## Compression Path Configuration

The compression path (hardware vs software) is explicitly defined in `config.h`:

```cpp
// Set USE_HARDWARE_COMPRESSION to 1 to use Intel IAX hardware acceleration
// Set USE_HARDWARE_COMPRESSION to 0 to use software compression (default)
#define USE_HARDWARE_COMPRESSION 1
#define COMPRESSION_PATH (USE_HARDWARE_COMPRESSION ? qpl_path_hardware : qpl_path_software)
```

## Centralized Configuration

All build-time options are now centralized in `config.h`:

```cpp
// Serialization Configuration
#define USE_SER1DE 1              // Use ser1de library for serialization

// Timing Configuration  
#define ENABLE_TIMING 1            // Enable timing measurements

// Compression Configuration
#define USE_HARDWARE_COMPRESSION 1 // Use Intel IAX hardware acceleration
```

This centralized approach makes it easy to:
- **Configure all options in one place**
- **Maintain consistent settings across all services**
- **Quickly switch between different configurations**
- **Document all available options**

## Services with Compression

All 8 microservices now have full compression implementation:

1. **user_service** - Compresses user registration responses and user verification responses
2. **profile_service** - Compresses hotel profile data (name, description, phone, address)
3. **search_service** - Compresses search results and decompresses incoming data
4. **recommendation_service** - Compresses recommendation results and decompresses incoming data
5. **reservation_service** - Compresses reservation data and decompresses user verification responses
6. **geo_service** - Compresses hotel ID lists
7. **frontend_service** - Compresses JSON responses and decompresses incoming data
8. **rate_service** - Compresses rate plan and room type data

## Complete Compression/Decompression Cycle

The implementation ensures data flows correctly through the compression pipeline:

1. **Service A** compresses data before sending to **Service B**
2. **Service B** decompresses incoming data from **Service A**
3. **Service B** processes the decompressed data
4. **Service B** re-compresses data before sending to **Service C**
5. **Service C** decompresses and processes the data

This cycle prevents data corruption and ensures all services can handle compressed data properly.

## Implementation Details

### Compression Manager

The `CompressionManager` class in `utils/compression_utils.h` provides:

- **QPL Integration**: Direct integration with Intel QPL library
- **Path Selection**: Hardware or software compression paths
- **Buffer Management**: Automatic buffer sizing for compression/decompression
- **Error Handling**: Graceful fallback to original data on failure
- **String Encoding**: Hex encoding for network transmission

### Global Instance

A global compression manager instance is available:

```cpp
// Global compression manager instance (inline to avoid multiple definition issues)
inline std::unique_ptr<CompressionManager> g_compression_manager = nullptr;
```

### Utility Functions

Simple interface functions for compression:

```cpp
// Initialize compression manager
inline void init_compression(qpl_path_t path = COMPRESSION_PATH);

// Compress data with fallback
inline std::string compress_data(const std::string& data);

// Decompress data with fallback
inline std::string decompress_data(const std::string& data);
```

## Monitoring and Statistics

The compression system provides detailed statistics:

- **Original size**: Size of uncompressed data
- **Compressed size**: Size of compressed data
- **Compression ratio**: Ratio of original to compressed size
- **Success status**: Whether compression/decompression succeeded

## Future Enhancements

- **Compression metrics**: Real-time monitoring of compression ratios
- **Adaptive compression**: Dynamic path selection based on data characteristics
- **Compression profiles**: Different compression levels for different data types
- **Performance optimization**: Further tuning of buffer sizes and compression parameters

## Troubleshooting

### Common Issues

1. **Hardware path failures**: Check Intel IAX device availability
2. **Compression failures**: Verify QPL library installation
3. **Data corruption**: Ensure proper compression/decompression cycle
4. **Performance issues**: Monitor compression ratios and adjust thresholds

### Debug Information

The system includes debug output for troubleshooting compression issues. Debug statements can be enabled in `utils/compression_utils.h` to trace data flow through the compression pipeline.

## Dependencies

- **Intel QPL**: Query Processing Library for compression
- **Intel IAX**: Hardware acceleration (optional)
- **Protocol Buffers**: For serialization
- **C++ Standard Library**: For data structures and utilities

The compression implementation is now complete across all microservices with proper error handling, hardware acceleration support, and comprehensive data flow management.