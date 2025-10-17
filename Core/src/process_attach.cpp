#include "profiler.h"
#include <stdio.h>
#include <mach/mach.h>
#include <sys/types.h>
#include <mach/thread_info.h>

int profiler_attach(pid_t pid, ProfilerTarget *target)
{
    // Initialize structure
    target->pid = pid;
    target->task = 0;
    target->threads = NULL;
    target->thread_count = 0;

    // Get task port from PID
    kern_return_t kr = task_for_pid(
        mach_task_self(), // Our task
        pid,              // Target PID
        &target->task     // OUTPUT: task port
    );

    if (kr != KERN_SUCCESS)
    {
        printf("Error: task_for_pid failed with code: %d\n", kr);
        printf("Hint: Try running with sudo\n");
        return kr;
    }

    printf("Attached to process %d (task port: 0x%x)\n",
           pid, target->task);

    return 0;
}

int profiler_get_threads(ProfilerTarget *target)
{
    if (target->task == 0)
    {
        printf("Error: Not attached to any process\n");
        return -1;
    }

    // Get list of threads
    kern_return_t kr = task_threads(
        target->task,
        &target->threads,
        &target->thread_count);

    if (kr != KERN_SUCCESS)
    {
        printf("Error: task_threads failed with code: %d\n", kr);
        return kr;
    }

    printf("Found %d thread(s)\n", target->thread_count);

    return 0;
}

void profiler_print_thread_info(ProfilerTarget *target)
{
    printf("\n");
    printf("Process: %d\n", target->pid);
    printf("Threads: %d\n", target->thread_count);
    printf("\n");

    for (mach_msg_type_number_t i = 0; i < target->thread_count; i++)
    {
        thread_t thread = target->threads[i];

        // Get basic thread info
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
    // Deallocate thread ports
    if (target->threads != NULL)
    {
        for (mach_msg_type_number_t i = 0; i < target->thread_count; i++)
        {
            mach_port_deallocate(mach_task_self(), target->threads[i]);
        }

        // Free the thread array
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

    printf("Detached from process %d\n", target->pid);
}