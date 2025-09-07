#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Build Configuration Options
// ============================================================================

// Compression Configuration
// Set to 1 to use Intel IAX hardware acceleration for compression
// Set to 0 to use software compression (default)
#define USE_HARDWARE_COMPRESSION 1

// Set to 1 to use non-blocking (asynchronous) compression APIs
// Set to 0 to use blocking (synchronous) compression APIs (default)
#define USE_NONBLOCKING_COMPRESSION 1

// Serialization Configuration
// Set to 1 to use ser1de library for serialization
// Set to 0 to use standard protobuf serialization
#define USE_SER1DE 0

// Timing Configuration
// Set to 1 to enable timing measurements and statistics for serialization & deserialization
// Set to 0 to disable timing (improves performance)
#define ENABLE_TIMING 0

// ============================================================================
// Derived Configuration
// ============================================================================

// Compression path selection based on hardware setting
#if USE_HARDWARE_COMPRESSION
    #define COMPRESSION_PATH qpl_path_hardware
#else
    #define COMPRESSION_PATH qpl_path_software
#endif

// Compression mode selection
#if USE_NONBLOCKING_COMPRESSION
    #define COMPRESSION_NONBLOCKING 1
#else
    #define COMPRESSION_NONBLOCKING 0
#endif

// ============================================================================
// Configuration Validation
// ============================================================================

// Validate configuration combinations
#if USE_HARDWARE_COMPRESSION && !defined(QPL_HARDWARE_SUPPORT)
    #warning "Hardware compression enabled but QPL hardware support may not be available"
#endif

#endif // CONFIG_H 