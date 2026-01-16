#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Build Configuration Options
// ============================================================================

// Compression Configuration
#define ENABLE_DUMMY_SERVICE_COMPRESSION 0

#define USE_HARDWARE_COMPRESSION 0
#define USE_NONBLOCKING_COMPRESSION 0

// Serialization Configuration
#define USE_SER1DE 1

// Timing Configuration
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