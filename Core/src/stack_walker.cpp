#include "stack_walker.h"
#include <stdio.h>
#include <string.h>
#include <mach/mach.h>
#include <pthread.h>
#include <time.h>

// Architecture detection
#if defined(__x86_64__)
#include <mach/i386/thread_status.h>
#define THREAD_STATE_FLAVOR x86_THREAD_STATE64
#define THREAD_STATE_COUNT x86_THREAD_STATE64_COUNT
typedef x86_thread_state64_t cpu_state_t;
#define GET_PC(state) ((state).__rip)
#define GET_FP(state) ((state).__rbp)
#define GET_SP(state) ((state).__rsp)
#elif defined(__arm64__) || defined(__aarch64__)
#include <mach/arm/thread_status.h>
#define THREAD_STATE_FLAVOR ARM_THREAD_STATE64
#define THREAD_STATE_COUNT ARM_THREAD_STATE64_COUNT
typedef arm_thread_state64_t cpu_state_t;
#define GET_PC(state) ((state).__pc)
#define GET_FP(state) ((state).__fp)
#define GET_SP(state) ((state).__sp)
#else
#error "Unsupported architecture"
#endif

// Global configuration
static StackWalkerConfig g_config;
static bool g_initialized = false;

// Helper: Get current time in nanoseconds
static uint64_t get_timestamp_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Helper: Check if an address looks valid
static bool is_valid_address(uint64_t addr)
{
    // Basic sanity checks for macOS user space addresses
    if (addr == 0)
        return false;

    // User space on macOS:
    // - x86_64: typically 0x100000000 - 0x7FFFFFFFF000
    // - ARM64: typically 0x100000000 - 0x200000000
    // Allow a wider range to be safe
    if (addr < 0x100000) // Below typical executable base
        return false;

#if defined(__x86_64__)
    if (addr >= 0x800000000000ULL) // Kernel space
        return false;
#elif defined(__arm64__) || defined(__aarch64__)
    if (addr >= 0x1000000000ULL) // Above typical user space on ARM64
        return false;
#endif

    // Should be at least 2-byte aligned for instructions
    if (addr & 0x1)
        return false;

    return true;
}

// Helper: Read memory from target process
static kern_return_t read_memory(
    task_t task,
    uint64_t address,
    void *data,
    size_t size)
{
    vm_size_t read_size = size;
    return vm_read_overwrite(
        task,
        address,
        size,
        (vm_address_t)data,
        &read_size);
}

// Frame pointer based stack walking
static int walk_stack_frame_pointer(
    task_t task,
    cpu_state_t *state,
    StackTrace *trace)
{
    uint64_t pc = GET_PC(*state);
    uint64_t fp = GET_FP(*state);

    trace->frame_count = 0;

    // Debug: Print initial values
    // fprintf(stderr, "DEBUG: PC=0x%llx FP=0x%llx\n", pc, fp);

    // First frame is current PC
    if (is_valid_address(pc))
    {
        trace->frames[trace->frame_count].address = pc;
        trace->frames[trace->frame_count].frame_pointer = fp;
        trace->frame_count++;
    }
    else
    {
        // PC is not valid - thread might be in syscall or optimized code
        // fprintf(stderr, "DEBUG: Invalid PC=0x%llx\n", pc);

        // Try to continue with frame pointer if it's valid
        if (!is_valid_address(fp))
        {
            return 0; // No frames captured, but not an error
        }
    }

    // Walk the frame pointer chain
    uint64_t prev_fp = 0;
    while (trace->frame_count < g_config.max_depth)
    {
        // Safety check: ensure FP is valid and increasing
        if (!is_valid_address(fp))
            break;

        if (fp <= prev_fp)
            break; // Stack should grow toward higher addresses

        if (fp - prev_fp > 0x100000)
            break; // Unreasonably large frame

        // Read the frame:
        // [fp]     = previous frame pointer
        // [fp + 8] = return address
        uint64_t frame_data[2];
        kern_return_t kr = read_memory(task, fp, frame_data, sizeof(frame_data));

        if (kr != KERN_SUCCESS)
            break;

        uint64_t next_fp = frame_data[0];
        uint64_t return_addr = frame_data[1];

        // Validate return address
        if (!is_valid_address(return_addr))
            break;

        // Add frame
        trace->frames[trace->frame_count].address = return_addr;
        trace->frames[trace->frame_count].frame_pointer = fp;
        trace->frame_count++;

        // Move to next frame
        prev_fp = fp;
        fp = next_fp;

        // Stop if we hit the bottom
        if (fp == 0)
            break;
    }

    return 0;
}

// Main capture function
void stack_walker_init(const StackWalkerConfig *config)
{
    if (config)
    {
        g_config = *config;
    }
    else
    {
        // Default configuration
        g_config.strategy = STACK_WALK_FRAME_POINTER;
        g_config.max_depth = MAX_STACK_DEPTH;
        g_config.capture_timestamps = true;
        g_config.validate_addresses = false;
    }

    // Cap max depth
    if (g_config.max_depth > MAX_STACK_DEPTH)
        g_config.max_depth = MAX_STACK_DEPTH;

    g_initialized = true;
}

int stack_walker_capture(
    task_t task,
    thread_t thread,
    StackTrace *trace)
{
    if (!g_initialized)
    {
        StackWalkerConfig default_config;
        default_config.strategy = STACK_WALK_FRAME_POINTER;
        default_config.max_depth = MAX_STACK_DEPTH;
        default_config.capture_timestamps = true;
        default_config.validate_addresses = false;
        stack_walker_init(&default_config);
    }

    // Initialize trace
    memset(trace, 0, sizeof(StackTrace));
    trace->thread = thread;

    // Get thread ID
    stack_walker_get_thread_id(thread, &trace->thread_id);

    // Capture timestamp if enabled
    if (g_config.capture_timestamps)
    {
        trace->timestamp_ns = get_timestamp_ns();
    }

    // Suspend the thread
    kern_return_t kr = thread_suspend(thread);
    if (kr != KERN_SUCCESS)
    {
        fprintf(stderr, "Warning: thread_suspend failed: %d\n", kr);
        return kr;
    }

    // Get thread state (registers)
    cpu_state_t state;
    mach_msg_type_number_t state_count = THREAD_STATE_COUNT;

    kr = thread_get_state(
        thread,
        THREAD_STATE_FLAVOR,
        (thread_state_t)&state,
        &state_count);

    if (kr != KERN_SUCCESS)
    {
        fprintf(stderr, "Warning: thread_get_state failed: %d\n", kr);
        thread_resume(thread);
        return kr;
    }

    // Debug: Uncomment to see register values
    // fprintf(stderr, "Thread %u: PC=0x%llx FP=0x%llx SP=0x%llx\n",
    //         thread, GET_PC(state), GET_FP(state), GET_SP(state));

    // Walk the stack based on strategy
    int result = 0;
    switch (g_config.strategy)
    {
    case STACK_WALK_FRAME_POINTER:
        result = walk_stack_frame_pointer(task, &state, trace);
        break;

    case STACK_WALK_LIBUNWIND:
        // TODO: Implement libunwind fallback
        printf("Warning: libunwind not yet implemented, using frame pointer\n");
        result = walk_stack_frame_pointer(task, &state, trace);
        break;

    case STACK_WALK_HYBRID:
        // Try frame pointer first
        result = walk_stack_frame_pointer(task, &state, trace);
        // TODO: If failed or too few frames, try libunwind
        break;
    }

    // Resume the thread
    thread_resume(thread);

    return result;
}

int stack_walker_capture_batch(
    task_t task,
    thread_t *threads,
    uint32_t thread_count,
    StackTrace *traces)
{
    int successful = 0;

    for (uint32_t i = 0; i < thread_count; i++)
    {
        int result = stack_walker_capture(task, threads[i], &traces[i]);
        if (result == 0 && traces[i].frame_count > 0)
        {
            successful++;
        }
    }

    return successful;
}

void stack_walker_print(const StackTrace *trace)
{
    printf("[%llu] Thread %u (%d frames)\n",
           trace->thread_id,
           trace->thread,
           trace->frame_count);

    for (uint32_t i = 0; i < trace->frame_count; i++)
    {
        printf("  #%-3d 0x%016llx", i, trace->frames[i].address);

        // Optionally show frame pointer for debugging
        if (trace->frames[i].frame_pointer != 0)
        {
            printf("  (fp: 0x%llx)", trace->frames[i].frame_pointer);
        }

        printf("\n");
    }

    if (trace->timestamp_ns > 0)
    {
        printf("  Captured at: %llu ns\n", trace->timestamp_ns);
    }
}

int stack_walker_get_thread_id(thread_t thread, uint64_t *thread_id)
{
    thread_identifier_info_data_t identifier_info;
    mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;

    kern_return_t kr = thread_info(
        thread,
        THREAD_IDENTIFIER_INFO,
        (thread_info_t)&identifier_info,
        &count);

    if (kr == KERN_SUCCESS)
    {
        *thread_id = identifier_info.thread_id;
        return 0;
    }

    // Fallback: use thread port as ID
    *thread_id = thread;
    return kr;
}

void stack_walker_cleanup(void)
{
    // Currently nothing to clean up
    // This is here for future use (e.g., libunwind cleanup)
    g_initialized = false;
}