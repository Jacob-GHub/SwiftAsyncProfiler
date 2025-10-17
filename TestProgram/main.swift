import Foundation

// Test program with interesting stack traces

class Worker {
    let id: Int
    
    init(id: Int) {
        self.id = id
    }
    
    func level5() {
        // Busy wait so profiler can catch us
        let start = Date()
        while Date().timeIntervalSince(start) < 0.1 {
            _ = (0..<1000).reduce(0, +)
        }
    }
    
    func level4() {
        level5()
    }
    
    func level3() {
        level4()
    }
    
    func level2() {
        level3()
    }
    
    func level1() {
        level2()
    }
    
    func run() {
        print("Worker \(id) started on thread \(pthread_mach_thread_np(pthread_self()))")
        while true {
            level1()
        }
    }
}

func computeTask() {
    print("Compute task started on thread \(pthread_mach_thread_np(pthread_self()))")
    while true {
        // Different stack depth
        var result = 0.0
        for i in 0..<10000 {
            result += sin(Double(i)) * cos(Double(i))
        }
        Thread.sleep(forTimeInterval: 0.01)
    }
}

@main
struct TestTarget {
    static func main() {
        print("╔══════════════════════════════════════════════════════╗")
        print("║              Test Target Program                     ║")
        print("╚══════════════════════════════════════════════════════╝")
        print("")
        print("PID: \(getpid())")
        print("Main thread: \(pthread_mach_thread_np(pthread_self()))")
        print("")
        print("This program will:")
        print("  - Run 3 worker threads with deep call stacks")
        print("  - Run 1 compute thread with shallow stacks")
        print("  - Keep running until killed")
        print("")
        print("To profile this process, run:")
        print("  sudo profiler \(getpid()) stacks")
        print("")
        print("Press Ctrl+C to stop")
        print("")
        
        // Start worker threads
        for i in 1...3 {
            let worker = Worker(id: i)
            Thread.detachNewThread {
                worker.run()
            }
        }
        
        // Start compute thread
        Thread.detachNewThread {
            computeTask()
        }
        
        // Main thread just sleeps
        print("All threads started. Main thread sleeping...")
        while true {
            Thread.sleep(forTimeInterval: 1.0)
        }
    }
}