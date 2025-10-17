#ifndef STACK_WALKER_H
#define STACK_WALKER_H

#include <mach/mach.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Maximum stack depth we'll capture
#define MAX_STACK_DEPTH 512

    // Structure to hold a single stack frame
    typedef struct
    {
        uint64_t address;       // Program counter / instruction pointer
        uint64_t frame_pointer; // Frame pointer (for debugging)
    } StackFrame;

    // Structure to hold a complete stack trace
    typedef struct
    {
        StackFrame frames[MAX_STACK_DEPTH];
        uint32_t frame_count;
        thread_t thread;
        uint64_t thread_id;
        uint64_t timestamp_ns; // When this was captured (nanoseconds)
    } StackTrace;

    // Stack walking strategies
    typedef enum
    {
        STACK_WALK_FRAME_POINTER, // Use frame pointer chain (fastest)
        STACK_WALK_LIBUNWIND,     // Use libunwind (more reliable)
        STACK_WALK_HYBRID         // Try FP first, fallback to libunwind
    } StackWalkStrategy;

    // Configuration for stack walker
    typedef struct
    {
        StackWalkStrategy strategy;
        uint32_t max_depth;      // Max frames to capture
        bool capture_timestamps; // Include timestamps
        bool validate_addresses; // Extra validation (slower)
    } StackWalkerConfig;

    /**
     * Initialize stack walker with configuration
     * @param config Configuration (NULL for defaults)
     */
    void stack_walker_init(const StackWalkerConfig *config);

    /**
     * Capture the stack trace for a given thread
     *
     * @param task The task port of the target process
     * @param thread The thread to capture
     * @param trace Output structure to fill with stack data
     * @return 0 on success, error code otherwise
     */
    int stack_walker_capture(
        task_t task,
        thread_t thread,
        StackTrace *trace);

    /**
     * Capture stacks for multiple threads (batch operation)
     * More efficient than calling stack_walker_capture multiple times
     *
     * @param task The task port
     * @param threads Array of threads
     * @param thread_count Number of threads
     * @param traces Output array (must be pre-allocated)
     * @return Number of successful captures
     */
    int stack_walker_capture_batch(
        task_t task,
        thread_t *threads,
        uint32_t thread_count,
        StackTrace *traces);

    /**
     * Print a stack trace to stdout (for debugging)
     *
     * @param trace The stack trace to print
     */
    void stack_walker_print(const StackTrace *trace);

    /**
     * Get thread ID for display purposes
     *
     * @param thread The thread port
     * @param thread_id Output: the thread ID
     * @return 0 on success, error code otherwise
     */
    int stack_walker_get_thread_id(thread_t thread, uint64_t *thread_id);

    /**
     * Cleanup and release resources
     */
    void stack_walker_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // STACK_WALKER_H