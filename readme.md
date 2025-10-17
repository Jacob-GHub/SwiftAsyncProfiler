# SwiftAsyncProfiler

## What's Working 

**Block 1: Process Attachment**
- Attach to running processes via PID
- Enumerate threads
- Get thread information

**Block 2: Stack Walking - Currently Working On**
- Capture call stacks from any thread
- Frame pointer-based unwinding
- Batch capture for efficiency
- Statistics tracking

## Project Structure

```
SwiftAsyncProfiler/
├── Core/
│   ├── include/
│   │   ├── profiler.h          # Main profiler interface
│   │   └── stack_walker.h      # Stack unwinding
│   └── src/
│       ├── profiler.cpp        # Profiler implementation
│       └── stack_walker.cpp    # Stack walking logic
│
├── SwiftBridge/
│   ├── ProfilerBridge.swift    # Swift wrapper
│   └── DataTypes.swift         # Shared types
│
├── CLI/
│   └── main.swift              # Command-line interface
│
├── Tests/Fixtures/
│   └── test_target.swift       # Test program
│
└── Package.swift
```

## Building

### Build Commands

```bash
# Build everything
swift build
```

### Run the Test Target

```bash
# In Terminal 1: Run the test program
.build/debug/test-target

# Note the PID it prints, then in Terminal 2:
sudo .build/debug/profiler <PID> stacks
```

## Usage

### Basic Commands

```bash
# Show thread information
sudo profiler <pid>
sudo profiler <pid> info

# Capture all stack traces
sudo profiler <pid> stacks
```

### Why sudo?

The profiler needs `task_for_pid` permission to attach to other processes. Options:

1. **Run with sudo** (easiest for development)
2. **Add entitlement** (for distribution):
   ```xml
   <key>com.apple.security.cs.debugger</key>
   <true/>
   ```

## Example Output

```
╔══════════════════════════════════════════════════════╗
║      SwiftAsyncProfiler v0.2 - MVP1 Block 2         ║
║              Stack Walking Enabled                   ║
╚══════════════════════════════════════════════════════╝

Target PID: 12345
Command: stacks

Attaching to process...
Attached to process 12345 (task port: 0x507)
Discovering threads...
Found 5 thread(s)

=== Capturing All Stacks ===

[0] Thread 4567 (15 frames)
  #0   0x0000000100003a40
  #1   0x0000000100003b20
  #2   0x0000000100003c10
  ...

Summary:
  Threads captured: 5
  Total frames: 73

Statistics:
  Total samples: 5
  Successful: 5
  Failed: 0
  Success rate: 100.0%
  Total frames: 73
  Avg frames/sample: 14.6

Success!
```
