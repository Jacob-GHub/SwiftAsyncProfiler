// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "SwiftAsyncProfiler",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        // Main profiler CLI tool
        .executable(
            name: "profiler",
            targets: ["ProfilerCLI"]
        ),
        // Test target program
        .executable(
            name: "test-target",
            targets: ["TestTarget"]
        ),
        // Library for integration
        .library(
            name: "SwiftAsyncProfiler",
            targets: ["SwiftBridge"]
        ),
    ],
    targets: [
        // C++ Core
        .target(
            name: "Core",
            path: "Core",
            exclude: [],
            sources: [
                "src/profiler.cpp",
                "src/stack_walker.cpp"
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("include"),
                .define("_DARWIN_C_SOURCE"),
            ],
            linkerSettings: [
                .linkedFramework("Foundation"),
            ]
        ),
        
        // Swift Bridge Layer
        .target(
            name: "SwiftBridge",
            dependencies: ["Core"],
            path: "SwiftBridge",
            sources: [
                "ProfilerBridge.swift",
                "DataTypes.swift"
            ]
        ),
        
        // CLI Tool
        .executableTarget(
            name: "ProfilerCLI",
            dependencies: ["SwiftBridge"],
            path: "CLI",
            sources: ["main.swift"]
        ),
        
        // Test Target
        .executableTarget(
            name: "TestTarget",
            path: "Tests/Fixtures",
            sources: ["test_target.swift"]
        ),
    ],
    cxxLanguageStandard: .cxx17
)