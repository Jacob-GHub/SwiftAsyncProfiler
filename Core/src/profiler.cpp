#include "profiler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mach/thread_info.h>

// Internal data structure
typedef struct
{
    ProfilerConfig config;
    ProfilerStats stats;
} ProfilerInternalData;

ProfilerConfig profiler_default_config(void)
{
    ProfilerConfig config;
    config.sample_interval_ms = 10;
    config.max_stack_depth = 512;
    config.track_async = false;
    config.track_threads = true;
    config.stack_strategy = STACK_WALK_FRAME_POINTER; // STACK_WALK_FRAME_POINTER
    return config;
}

int profiler_attach(
    pid_t pid,
    const ProfilerConfig *config,
    ProfilerTarget *target)
{
    // Initialize structure
    memset(target, 0, sizeof(ProfilerTarget));
    target->pid = pid;
    target->state = PROFILER_STATE_DETACHED;

    // Allocate internal data
    ProfilerInternalData *internal = (ProfilerInternalData *)malloc(sizeof(ProfilerInternalData));
    if (!internal)
    {
        return -1;
    }

    // Store config
    internal->config = config ? *config : profiler_default_config();
    memset(&internal->stats, 0, sizeof(ProfilerStats));
    target->internal_data = internal;

    // Initialize stack walker with config
    StackWalkerConfig sw_config;
    sw_config.strategy = (StackWalkStrategy)internal->config.stack_strategy;
    sw_config.max_depth = internal->config.max_stack_depth;
    sw_config.capture_timestamps = true;
    sw_config.validate_addresses = false;
    stack_walker_init(&sw_config);

    // Get task port from PID
    kern_return_t kr = task_for_pid(
        mach_task_self(),
        pid,
        &target->task);

    if (kr != KERN_SUCCESS)
    {
        printf("Error: task_for_pid failed with code: %d\n", kr);
        printf("Hint: Try running with sudo or add task_for_pid entitlement\n");
        free(internal);
        target->internal_data = NULL;
        return kr;
    }

    target->state = PROFILER_STATE_ATTACHED;
    printf("Attached to process %d (task port: 0x%x)\n", pid, target->task);

    return 0;
}

int profiler_refresh_threads(ProfilerTarget *target)
{
    if (target->state == PROFILER_STATE_DETACHED)
    {
        printf("Error: Not attached to any process\n");
        return -1;
    }

    // Free old thread list if exists
    if (target->threads != NULL)
    {
        for (mach_msg_type_number_t i = 0; i < target->thread_count; i++)
        {
            mach_port_deallocate(mach_task_self(), target->threads[i]);
        }
        vm_deallocate(
            mach_task_self(),
            (vm_address_t)target->threads,
            target->thread_count * sizeof(thread_t));
        target->threads = NULL;
        target->thread_count = 0;
    }

    // Get fresh thread list
    kern_return_t kr = task_threads(
        target->task,
        &target->threads,
        &target->thread_count);

    if (kr != KERN_SUCCESS)
    {
        printf("Error: task_threads failed with code: %d\n", kr);
        target->state = PROFILER_STATE_ERROR;
        return kr;
    }

    printf("Found %d thread(s)\n", target->thread_count);
    return 0;
}

int profiler_capture_thread_stack(
    ProfilerTarget *target,
    uint32_t thread_index,
    StackTrace *trace)
{
    if (target->state == PROFILER_STATE_DETACHED)
    {
        return -1;
    }

    if (thread_index >= target->thread_count)
    {
        printf("Error: Invalid thread index %d (max: %d)\n",
               thread_index, target->thread_count - 1);
        return -1;
    }

    ProfilerInternalData *internal = (ProfilerInternalData *)target->internal_data;

    thread_t thread = target->threads[thread_index];
    int result = stack_walker_capture(target->task, thread, trace);

    // Update stats
    internal->stats.total_samples++;
    if (result == 0)
    {
        internal->stats.successful_samples++;
        internal->stats.total_frames += trace->frame_count;
    }
    else
    {
        internal->stats.failed_samples++;
    }

    return result;
}

int profiler_capture_all_stacks(
    ProfilerTarget *target,
    StackTrace *traces,
    uint32_t *trace_count)
{
    if (target->state == PROFILER_STATE_DETACHED)
    {
        return -1;
    }

    *trace_count = 0;

    // Use batch capture for efficiency
    int captured = stack_walker_capture_batch(
        target->task,
        target->threads,
        target->thread_count,
        traces);

    *trace_count = captured;

    // Update stats
    ProfilerInternalData *internal = (ProfilerInternalData *)target->internal_data;
    internal->stats.total_samples += target->thread_count;
    internal->stats.successful_samples += captured;
    internal->stats.failed_samples += (target->thread_count - captured);

    for (uint32_t i = 0; i < captured; i++)
    {
        internal->stats.total_frames += traces[i].frame_count;
    }

    return 0;
}

void profiler_get_stats(
    const ProfilerTarget *target,
    ProfilerStats *stats)
{
    if (!target->internal_data)
    {
        memset(stats, 0, sizeof(ProfilerStats));
        return;
    }

    ProfilerInternalData *internal = (ProfilerInternalData *)target->internal_data;
    *stats = internal->stats;
}

void profiler_print_thread_info(ProfilerTarget *target)
{
    printf("\n");
    printf("Process: %d\n", target->pid);
    printf("Threads: %d\n", target->thread_count);
    printf("State: ");

    switch (target->state)
    {
    case PROFILER_STATE_DETACHED:
        printf("DETACHED\n");
        break;
    case PROFILER_STATE_ATTACHED:
        printf("ATTACHED\n");
        break;
    case PROFILER_STATE_SAMPLING:
        printf("SAMPLING\n");
        break;
    case PROFILER_STATE_ERROR:
        printf("ERROR\n");
        break;
    }

    printf("\n");

    for (mach_msg_type_number_t i = 0; i < target->thread_count; i++)
    {
        thread_t thread = target->threads[i];

        thread_basic_info_data_t basic_info;
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

        kern_return_t kr = thread_info(
            thread,
            THREAD_BASIC_INFO,
            (thread_info_t)&basic_info,
            &count);

        if (kr == KERN_SUCCESS)
        {
            printf("  Thread %d (port: 0x%x)\n", i, thread);
            printf("    State: ");
            switch (basic_info.run_state)
            {
            case TH_STATE_RUNNING:
                printf("RUNNING\n");
                break;
            case TH_STATE_STOPPED:
                printf("STOPPED\n");
                break;
            case TH_STATE_WAITING:
                printf("WAITING\n");
                break;
            case TH_STATE_UNINTERRUPTIBLE:
                printf("UNINTERRUPTIBLE\n");
                break;
            case TH_STATE_HALTED:
                printf("HALTED\n");
                break;
            default:
                printf("UNKNOWN\n");
            }
            printf("    CPU time: %d.%06d seconds\n",
                   basic_info.user_time.seconds,
                   basic_info.user_time.microseconds);
        }
        else
        {
            printf("  Thread %d: Could not get info\n", i);
        }
        printf("\n");
    }
}

void profiler_detach(ProfilerTarget *target)
{
    if (target->state == PROFILER_STATE_DETACHED)
    {
        return;
    }

    // Free threads
    if (target->threads != NULL)
    {
        for (mach_msg_type_number_t i = 0; i < target->thread_count; i++)
        {
            mach_port_deallocate(mach_task_self(), target->threads[i]);
        }
        vm_deallocate(
            mach_task_self(),
            (vm_address_t)target->threads,
            target->thread_count * sizeof(thread_t));
        target->threads = NULL;
        target->thread_count = 0;
    }

    // Deallocate task port
    if (target->task != 0)
    {
        mach_port_deallocate(mach_task_self(), target->task);
        target->task = 0;
    }

    // Free internal data
    if (target->internal_data)
    {
        free(target->internal_data);
        target->internal_data = NULL;
    }

    // Cleanup stack walker
    stack_walker_cleanup();

    printf("Detached from process %d\n", target->pid);
    target->state = PROFILER_STATE_DETACHED;
}