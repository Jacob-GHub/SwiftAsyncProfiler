import Foundation

// MARK: - C Function Imports

@_silgen_name("profiler_default_config")
func profiler_default_config() -> ProfilerConfig

@_silgen_name("profiler_attach")
func profiler_attach(
    _ pid: pid_t,
    _ config: UnsafePointer<ProfilerConfig>?,
    _ target: UnsafeMutablePointer<ProfilerTarget>
) -> Int32

@_silgen_name("profiler_refresh_threads")
func profiler_refresh_threads(_ target: UnsafeMutablePointer<ProfilerTarget>) -> Int32

@_silgen_name("profiler_capture_thread_stack")
func profiler_capture_thread_stack(
    _ target: UnsafeMutablePointer<ProfilerTarget>,
    _ threadIndex: UInt32,
    _ trace: UnsafeMutablePointer<StackTrace>
) -> Int32

@_silgen_name("profiler_capture_all_stacks")
func profiler_capture_all_stacks(
    _ target: UnsafeMutablePointer<ProfilerTarget>,
    _ traces: UnsafeMutablePointer<StackTrace>,
    _ traceCount: UnsafeMutablePointer<UInt32>
) -> Int32

@_silgen_name("profiler_get_stats")
func profiler_get_stats(
    _ target: UnsafePointer<ProfilerTarget>,
    _ stats: UnsafeMutablePointer<ProfilerStats>
)

@_silgen_name("profiler_print_thread_info")
func profiler_print_thread_info(_ target: UnsafeMutablePointer<ProfilerTarget>)

@_silgen_name("stack_walker_print")
func stack_walker_print(_ trace: UnsafePointer<StackTrace>)

@_silgen_name("profiler_detach")
func profiler_detach(_ target: UnsafeMutablePointer<ProfilerTarget>)

// MARK: - Swift Wrapper Class

/// High-level Swift interface to the profiler
public class Profiler {
    private var target = ProfilerTarget()
    private var isAttached = false
    
    public init() {}
    
    /// Attach to a process with custom configuration
    public func attach(pid: pid_t, config: Config? = nil) throws {
        var cConfig = config?.toCStruct() ?? profiler_default_config()
        
        let result = withUnsafePointer(to: &cConfig) { configPtr in
            profiler_attach(pid, configPtr, &target)
        }
        
        guard result == 0 else {
            throw ProfilerError.attachFailed(code: result)
        }
        
        isAttached = true
    }
    
    /// Refresh the list of threads in the target process
    public func refreshThreads() throws {
        guard isAttached else {
            throw ProfilerError.notAttached
        }
        
        let result = profiler_refresh_threads(&target)
        guard result == 0 else {
            throw ProfilerError.threadRefreshFailed(code: result)
        }
    }
    
    /// Get the number of threads
    public var threadCount: Int {
        return Int(target.thread_count)
    }
    
    /// Capture stack trace for a specific thread
    public func captureStack(forThreadAt index: Int) throws -> StackTrace {
        guard isAttached else {
            throw ProfilerError.notAttached
        }
        
        guard index >= 0 && index < threadCount else {
            throw ProfilerError.invalidThreadIndex(index: index, max: threadCount - 1)
        }
        
        var trace = StackTrace()
        let result = profiler_capture_thread_stack(&target, UInt32(index), &trace)
        
        guard result == 0 else {
            throw ProfilerError.stackCaptureFailed(code: result)
        }
        
        return trace
    }
    
    /// Capture stacks for all threads
    public func captureAllStacks() throws -> [StackTrace] {
        guard isAttached else {
            throw ProfilerError.notAttached
        }
        
        let maxThreads = Int(target.thread_count)
        guard maxThreads > 0 else {
            return []
        }
        
        var traces = [StackTrace](repeating: StackTrace(), count: maxThreads)
        var traceCount: UInt32 = 0
        
        let result = traces.withUnsafeMutableBufferPointer { buffer in
            profiler_capture_all_stacks(&target, buffer.baseAddress!, &traceCount)
        }
        
        guard result == 0 else {
            throw ProfilerError.stackCaptureFailed(code: result)
        }
        
        return Array(traces.prefix(Int(traceCount)))
    }
    
    /// Get profiler statistics
    public func getStats() -> Stats {
        var cStats = ProfilerStats()
        withUnsafePointer(to: target) { targetPtr in
            profiler_get_stats(targetPtr, &cStats)
        }
        return Stats(from: cStats)
    }
    
    /// Print thread information (for debugging)
    public func printThreadInfo() {
        guard isAttached else {
            print("Not attached to any process")
            return
        }
        profiler_print_thread_info(&target)
    }
    
    /// Print a stack trace (for debugging)
    public func printStackTrace(_ trace: StackTrace) {
        var mutableTrace = trace
        stack_walker_print(&mutableTrace)
    }
    
    /// Detach from the process
    public func detach() {
        guard isAttached else { return }
        profiler_detach(&target)
        isAttached = false
    }
    
    deinit {
        if isAttached {
            detach()
        }
    }
}

// MARK: - Swift Configuration

extension Profiler {
    public struct Config {
        public var sampleIntervalMs: UInt32
        public var maxStackDepth: UInt32
        public var trackAsync: Bool
        public var trackThreads: Bool
        public var stackStrategy: StackWalkStrategy
        
        public init(
            sampleIntervalMs: UInt32 = 10,
            maxStackDepth: UInt32 = 512,
            trackAsync: Bool = false,
            trackThreads: Bool = true,
            stackStrategy: StackWalkStrategy = .framePointer
        ) {
            self.sampleIntervalMs = sampleIntervalMs
            self.maxStackDepth = maxStackDepth
            self.trackAsync = trackAsync
            self.trackThreads = trackThreads
            self.stackStrategy = stackStrategy
        }
        
        func toCStruct() -> ProfilerConfig {
            return ProfilerConfig(
                sample_interval_ms: sampleIntervalMs,
                max_stack_depth: maxStackDepth,
                track_async: trackAsync,
                track_threads: trackThreads,
                stack_strategy: stackStrategy.rawValue
            )
        }
    }
    
    public enum StackWalkStrategy: UInt32 {
        case framePointer = 0
        case libunwind = 1
        case hybrid = 2
    }
}

// MARK: - Swift Statistics

extension Profiler {
    public struct Stats {
        public let totalSamples: UInt64
        public let successfulSamples: UInt64
        public let failedSamples: UInt64
        public let totalFrames: UInt64
        public let uniqueAddresses: UInt64
        
        public var successRate: Double {
            guard totalSamples > 0 else { return 0.0 }
            return Double(successfulSamples) / Double(totalSamples)
        }
        
        public var averageFramesPerSample: Double {
            guard successfulSamples > 0 else { return 0.0 }
            return Double(totalFrames) / Double(successfulSamples)
        }
        
        init(from cStats: ProfilerStats) {
            self.totalSamples = cStats.total_samples
            self.successfulSamples = cStats.successful_samples
            self.failedSamples = cStats.failed_samples
            self.totalFrames = cStats.total_frames
            self.uniqueAddresses = cStats.unique_addresses
        }
    }
}

// MARK: - Errors

public enum ProfilerError: Error, CustomStringConvertible {
    case attachFailed(code: Int32)
    case notAttached
    case threadRefreshFailed(code: Int32)
    case stackCaptureFailed(code: Int32)
    case invalidThreadIndex(index: Int, max: Int)
    
    public var description: String {
        switch self {
        case .attachFailed(let code):
            return "Failed to attach to process (error code: \(code))"
        case .notAttached:
            return "Not attached to any process"
        case .threadRefreshFailed(let code):
            return "Failed to refresh threads (error code: \(code))"
        case .stackCaptureFailed(let code):
            return "Failed to capture stack trace (error code: \(code))"
        case .invalidThreadIndex(let index, let max):
            return "Invalid thread index \(index) (valid range: 0-\(max))"
        }
    }
}

// MARK: - Helper Extensions

extension StackTrace {
    /// Get all frame addresses as an array
    public var frameAddresses: [UInt64] {
        let mirror = Mirror(reflecting: frames)
        let allFrames = mirror.children.map { $0.value as! StackFrame }
        return allFrames.prefix(Int(frame_count)).map { $0.address }
    }
    
    /// Get a readable description
    public var description: String {
        var result = "Thread \(thread_id) (\(frame_count) frames)"
        if timestamp_ns > 0 {
            result += " @ \(timestamp_ns)ns"
        }
        return result
    }
}