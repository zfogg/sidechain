#==============================================================================
# CPU Core Detection Utility
# Detects the number of available CPU cores for parallel builds/checks
# Works on macOS, Linux, and Windows
#==============================================================================

if(NOT DEFINED CMAKE_CPU_CORES)
    if(APPLE)
        # macOS: use sysctl
        execute_process(
            COMMAND sysctl -n hw.ncpu
            OUTPUT_VARIABLE CMAKE_CPU_CORES
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    elseif(UNIX)
        # Linux: use nproc or /proc/cpuinfo
        if(EXISTS /proc/cpuinfo)
            execute_process(
                COMMAND grep -c "^processor" /proc/cpuinfo
                OUTPUT_VARIABLE CMAKE_CPU_CORES
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        else()
            execute_process(
                COMMAND nproc
                OUTPUT_VARIABLE CMAKE_CPU_CORES
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()
    elseif(WIN32)
        # Windows: use environment variable or CMake's ProcessorCount
        if(DEFINED ENV{NUMBER_OF_PROCESSORS})
            set(CMAKE_CPU_CORES $ENV{NUMBER_OF_PROCESSORS})
        else()
            include(ProcessorCount)
            ProcessorCount(CMAKE_CPU_CORES)
        endif()
    else()
        # Fallback: use CMake's ProcessorCount
        include(ProcessorCount)
        ProcessorCount(CMAKE_CPU_CORES)
    endif()

    # Ensure we have a valid number
    if(NOT CMAKE_CPU_CORES OR CMAKE_CPU_CORES EQUAL 0)
        set(CMAKE_CPU_CORES 1)
    endif()

    # Cache the result
    set(CMAKE_CPU_CORES ${CMAKE_CPU_CORES} CACHE STRING "Number of available CPU cores" FORCE)
endif()

message(STATUS "Detected ${CMAKE_CPU_CORES} CPU cores available for parallel builds")
