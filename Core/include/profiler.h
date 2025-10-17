#ifndef PROFILER_H
#define PROFILER_H

#include <mach/mach.h>
#include <sys/types.h>
#include <stdbool.h>
#include "stack_walker.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declarations
    typedef struct ProfilerTarget ProfilerTarget;
    typedef struct ProfilerConfig ProfilerConfig;
    typedef struct ProfilerStats ProfilerStats;

    // Profiler state
    typedef enum
    {
        PROFILER_STATE_DETACHED,
        PROFILER_STATE_ATTACHED,
        PROFILER_STATE_SAMPLING,
        PROFILER_STATE_ERROR
    } ProfilerState;

    // Main profiler target structure
    struct ProfilerTarget
    {
        pid_t pid;
        mach_port_t task;
        thread_act_array_t threads;
        mach_msg_type_number_t thread_count;
        ProfilerState state;
        void *internal_data; // For future extension
    };

    // Configuration
    struct ProfilerConfig
    {
        uint32_t sample_interval_ms; // Sampling interval (default: 10ms)
        uint32_t max_stack_depth;    // Max frames per stack (default: 512)
        bool track_async;            // Track async/await (default: false)
        bool track_threads;          // Track thread lifecycle (default: true)
        StackWalkStrategy stack_strategy;
    };

    // Statistics
    struct ProfilerStats
    {
        uint64_t total_samples;
        uint64_t successful_samples;
        uint64_t failed_samples;
        uint64_t total_frames;
        uint64_t unique_addresses;
    };

    /**
     * Get default configuration
     */
    ProfilerConfig profiler_default_config(void);

    /**
     * Attach to a process by PID
     *
     * @param pid Process ID to attach to
     * @param config Configuration (NULL for defaults)
     * @param target Output: profiler target handle
     * @return 0 on success, error code otherwise
     */
    int profiler_attach(
        pid_t pid,
        const ProfilerConfig *config,
        ProfilerTarget *target);

    /**
     * Get list of threads in the target process
     * This refreshes the thread list
     *
     * @param target The profiler target
     * @return 0 on success, error code otherwise
     */
    int profiler_refresh_threads(ProfilerTarget *target);

    /**
     * Capture stack trace for a specific thread
     *
     * @param target The profiler target
     * @param thread_index Index into the threads array
     * @param trace Output: the captured stack trace
     * @return 0 on success, error code otherwise
     */
    int profiler_capture_thread_stack(
        ProfilerTarget *target,
        uint32_t thread_index,
        StackTrace *trace);

    /**
     * Capture stacks for ALL threads
     * This is more efficient than calling profiler_capture_thread_stack repeatedly
     *
     * @param target The profiler target
     * @param traces Output array (must be pre-allocated with thread_count size)
     * @param trace_count Output: number of traces captured
     * @return 0 on success, error code otherwise
     */
    int profiler_capture_all_stacks(
        ProfilerTarget *target,
        StackTrace *traces,
        uint32_t *trace_count);

    /**
     * Get profiler statistics
     *
     * @param target The profiler target
     * @param stats Output: statistics
     */
    void profiler_get_stats(
        const ProfilerTarget *target,
        ProfilerStats *stats);

    /**
     * Print basic thread information (for debugging)
     *
     * @param target The profiler target
     */
    void profiler_print_thread_info(ProfilerTarget *target);

    /**
     * Detach from the process and cleanup
     *
     * @param target The profiler target
     */
    void profiler_detach(ProfilerTarget *target);

#ifdef __cplusplus
}
#endif

#endif // PROFILER_H