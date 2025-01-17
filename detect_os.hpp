#ifndef PAZ_AUDIO_DETECT_OS_HPP
#define PAZ_AUDIO_DETECT_OS_HPP

#if defined(__APPLE__) && defined(__MACH__)
#if !defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE
#define PAZ_MACOS
#else
static_assert(false, "Failed to detect an acceptable OS (iOS is not supported)."
    );
#endif
#elif defined(_WIN32) || defined(_WIN64)
#define PAZ_WINDOWS
#elif defined(__linux__)
#define PAZ_LINUX
#else
static_assert(false, "Failed to detect an acceptable OS (no valid macros define"
    "d).");
#endif
#if defined(PAZ_MACOS) || defined(PAZ_LINUX)
#define PAZ_UNIX
#endif

#endif
