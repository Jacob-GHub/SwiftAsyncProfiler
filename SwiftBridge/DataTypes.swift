import Foundation

// MARK: - C Structure Mirrors
// These must match the C structures exactly

// Profiler State
public enum ProfilerState: UInt32 {
    case detached = 0
    case attached = 1
    case sampling = 2
    case error = 3
}

// Stack Frame
public struct StackFrame {
    public var address: UInt64
    public var frame_pointer: UInt64
    
    public init() {
        self.address = 0
        self.frame_pointer = 0
    }
    
    public init(address: UInt64, framePointer: UInt64 = 0) {
        self.address = address
        self.frame_pointer = framePointer
    }
}

// Stack Trace (64 frames to keep struct size reasonable)
public struct StackTrace {
    public var frames: (
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 0-4
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 5-9
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 10-14
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 15-19
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 20-24
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 25-29
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 30-34
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 35-39
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 40-44
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 45-49
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 50-54
        StackFrame, StackFrame, StackFrame, StackFrame, StackFrame, // 55-59
        StackFrame, StackFrame, StackFrame, StackFrame              // 60-63
    )
    public var frame_count: UInt32
    public var thread: thread_t
    public var thread_id: UInt64
    public var timestamp_ns: UInt64
    
    public init() {
        self.frames = (
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame(), StackFrame(),
            StackFrame(), StackFrame(), StackFrame(), StackFrame()
        )
        self.frame_count = 0
        self.thread = 0
        self.thread_id = 0
        self.timestamp_ns = 0
    }
}

// Profiler Target
public struct ProfilerTarget {
    public var pid: pid_t
    public var task: mach_port_t
    public var threads: thread_act_array_t?
    public var thread_count: mach_msg_type_number_t
    public var state: ProfilerState
    public var internal_data: UnsafeMutableRawPointer?
    
    public init() {
        self.pid = 0
        self.task = 0
        self.threads = nil
        self.thread_count = 0
        self.state = .detached
        self.internal_data = nil
    }
}

// Profiler Config
public struct ProfilerConfig {
    public var sample_interval_ms: UInt32
    public var max_stack_depth: UInt32
    public var track_async: Bool
    public var track_threads: Bool
    public var stack_strategy: UInt32
    
    public init() {
        self.sample_interval_ms = 10
        self.max_stack_depth = 512
        self.track_async = false
        self.track_threads = true
        self.stack_strategy = 0
    }
    
    public init(
        sample_interval_ms: UInt32,
        max_stack_depth: UInt32,
        track_async: Bool,
        track_threads: Bool,
        stack_strategy: UInt32
    ) {
        self.sample_interval_ms = sample_interval_ms
        self.max_stack_depth = max_stack_depth
        self.track_async = track_async
        self.track_threads = track_threads
        self.stack_strategy = stack_strategy
    }
}

// Profiler Statistics
public struct ProfilerStats {
    public var total_samples: UInt64
    public var successful_samples: UInt64
    public var failed_samples: UInt64
    public var total_frames: UInt64
    public var unique_addresses: UInt64
    
    public init() {
        self.total_samples = 0
        self.successful_samples = 0
        self.failed_samples = 0
        self.total_frames = 0
        self.unique_addresses = 0
    }
}

// Stack Walker Config
public struct StackWalkerConfig {
    public var strategy: UInt32
    public var max_depth: UInt32
    public var capture_timestamps: Bool
    public var validate_addresses: Bool
    
    public init() {
        self.strategy = 0
        self.max_depth = 512
        self.capture_timestamps = true
        self.validate_addresses = false
    }
}