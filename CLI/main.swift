import Foundation
import SwiftBridge

@main
struct ProfilerCLI {
    static func main() {
        // Parse command line arguments
        guard CommandLine.arguments.count > 1 else {
            printUsage()
            exit(1)
        }
        
        guard let pid = Int32(CommandLine.arguments[1]) else {
            print("Error: Invalid PID")
            exit(1)
        }
        
        let command = CommandLine.arguments.count > 2 ? CommandLine.arguments[2] : "info"
        
        print("╔══════════════════════════════════════════════════════╗")
        print("║      SwiftAsyncProfiler v0.2 - MVP1 Block 2         ║")
        print("║              Stack Walking Enabled                   ║")
        print("╚══════════════════════════════════════════════════════╝")
        print("")
        print("Target PID: \(pid)")
        print("Command: \(command)")
        print("")
        
        let profiler = Profiler()
        
        do {
            // Configure profiler
            let config = Profiler.Config(
                sampleIntervalMs: 10,
                maxStackDepth: 64,
                trackAsync: false,
                stackStrategy: .framePointer
            )
            
            // Attach
            print("Attaching to process...")
            try profiler.attach(pid: pid, config: config)
            
            // Get threads
            print("Discovering threads...")
            try profiler.refreshThreads()
            
            // Execute command
            try executeCommand(command, profiler: profiler)
            
            // Show stats
            print("\nStatistics:")
            let stats = profiler.getStats()
            printStats(stats)
            
            // Detach
            profiler.detach()
            
            print("\nSuccess!")
            
        } catch {
            print("\nError: \(error)")
            profiler.detach()
            exit(1)
        }
    }
    
    static func executeCommand(_ command: String, profiler: Profiler) throws {
        switch command {
        case "info":
            print("\n=== Thread Information ===")
            profiler.printThreadInfo()
            
        case "stacks":
            print("\n=== Capturing All Stacks ===\n")
            let traces = try profiler.captureAllStacks()
            
            for (index, trace) in traces.enumerated() {
                print("[\(index)] \(trace.description)")
                profiler.printStackTrace(trace)
            }
            
            print("Summary:")
            print("  Threads captured: \(traces.count)")
            print("  Total frames: \(traces.reduce(0) { $0 + Int($1.frame_count) })")
            
        case "stack":
            guard CommandLine.arguments.count > 3,
                  let threadIndex = Int(CommandLine.arguments[3]) else {
                print("Error: Please specify thread index")
                print("Usage: profiler <pid> stack <thread_index>")
                exit(1)
            }
            
            print("\n=== Capturing Thread \(threadIndex) ===\n")
            let trace = try profiler.captureStack(forThreadAt: threadIndex)
            profiler.printStackTrace(trace)
            
        case "sample":
            // Quick test: capture stacks multiple times
            let iterations = CommandLine.arguments.count > 3 ? Int(CommandLine.arguments[3]) ?? 5 : 5
            
            print("\n=== Sampling (x\(iterations)) ===\n")
            
            for i in 1...iterations {
                print("Sample \(i)/\(iterations)...")
                let traces = try profiler.captureAllStacks()
                
                // Just show summary for each sample
                let totalFrames = traces.reduce(0) { $0 + Int($1.frame_count) }
                print("  Captured \(traces.count) threads, \(totalFrames) frames")
                
                if i < iterations {
                    Thread.sleep(forTimeInterval: 0.01) // 10ms between samples
                }
            }
            
        default:
            print("Unknown command: \(command)")
            printUsage()
            exit(1)
        }
    }
    
    static func printStats(_ stats: Profiler.Stats) {
        print("  Total samples: \(stats.totalSamples)")
        print("  Successful: \(stats.successfulSamples)")
        print("  Failed: \(stats.failedSamples)")
        print("  Success rate: \(String(format: "%.1f%%", stats.successRate * 100))")
        print("  Total frames: \(stats.totalFrames)")
        if stats.successfulSamples > 0 {
            print("  Avg frames/sample: \(String(format: "%.1f", stats.averageFramesPerSample))")
        }
    }
    
    static func printUsage() {
        print("""
        Usage: profiler <pid> [command] [options]
        
        Commands:
          info              Show thread info (default)
          stacks            Capture and show all stack traces
          stack <N>         Capture stack for thread N
          sample [N]        Capture N samples (default: 5)
        
        Examples:
          sudo profiler 1234
          sudo profiler 1234 stacks
          sudo profiler 1234 stack 0
          sudo profiler 1234 sample 10
        
        Note: Requires sudo or task_for_pid entitlement
        """)
    }
}