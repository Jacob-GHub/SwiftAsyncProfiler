# SwiftAsyncProfiler - MVP1: Basic Profiler

## What's Working (Block 1 & 2)

**Block 1: Process Attachment**
- Attach to running processes via PID
- Enumerate threads
- Get thread information

**Block 2: Stack Walking**
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

### Prerequisites
- macOS 13.0 or later
- Xcode 15.0 or later
- Swift 5.9 or later

### Build Commands

```bash
# Build everything
swift build

# Build in release mode (faster profiling)
swift build -c release

# Build just the profiler CLI
swift build --product profiler

# Build the test target
swift build --product test-target
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

# Capture specific thread
sudo profiler <pid> stack 0

# Run sampling test (capture multiple times)
sudo profiler <pid> sample 10
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

## Testing Your Changes

### 1. Test with a Simple Program

```bash
# Start a Swift program
swift run test-target &
PID=$!

# Profile it
sudo swift run profiler $PID stacks

# Clean up
kill $PID
```

### 2. Test with a Real App

```bash
# Find Safari's PID
ps aux | grep Safari

# Profile Safari
sudo swift run profiler <SAFARI_PID> stacks
```

### 3. Test Sampling

```bash
# Capture 20 samples from the test target
sudo swift run profiler <PID> sample 20
```

## What's Next (Block 3)

The next block is **Symbol Resolution** - turning memory addresses into function names:

```
0x0000000100003a40  →  TestTarget.Worker.level5()
0x0000000100003b20  →  TestTarget.Worker.level4()
```

This requires:
- Symbol table parsing
- DWARF debug info
- Swift name demangling
- Module/library tracking

## Architecture Notes

### How Stack Walking Works

1. **Suspend Thread**: Briefly freeze the target thread
2. **Read Registers**: Get PC (program counter) and FP (frame pointer)
3. **Walk Frames**: Follow the frame pointer chain
4. **Read Memory**: Use `vm_read_overwrite` to read from target process
5. **Resume Thread**: Unfreeze the thread (typically <1ms suspension)

### Frame Pointer Chain

```
[FP] → Previous FP
[FP+8] → Return Address
```

On both x86_64 and ARM64, we follow this chain until we hit:
- FP == 0 (end of stack)
- Invalid address
- FP doesn't increase (loop detection)

### Performance

Current implementation:
- Single thread capture: ~0.5-1ms
- Batch capture (5 threads): ~2-3ms
- Overhead on target: <0.1ms per thread

## Common Issues

### "task_for_pid failed with code: 5"
**Solution**: Run with `sudo` or add debugger entitlement

### "No threads found"
**Solution**: Process might have exited, check if PID is valid

### Empty stacks or few frames
**Possible causes**:
- Thread was in system call
- Optimized code removed frame pointers
- Try different threads

### Build errors
**Solution**: Make sure you're on macOS and have Xcode command line tools:
```bash
xcode-select --install
```

## Contributing

This is MVP1 - basic profiling works but there's much to do:


## License

MIT (or your preferred license)