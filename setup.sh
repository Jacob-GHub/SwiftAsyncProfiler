#!/bin/bash

# SwiftAsyncProfiler Setup Script
# This script helps you get started quickly

set -e  # Exit on error

echo "╔══════════════════════════════════════════════════════╗"
echo "║      SwiftAsyncProfiler - Setup Script              ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This project only works on macOS"
    exit 1
fi

# Check Swift version
echo "Checking Swift version..."
if ! command -v swift &> /dev/null; then
    echo "Error: Swift not found. Please install Xcode."
    exit 1
fi

SWIFT_VERSION=$(swift --version | head -n 1)
echo "   Found: $SWIFT_VERSION"

# Check Xcode command line tools
echo "Checking Xcode command line tools..."
if ! xcode-select -p &> /dev/null; then
    echo "Error: Xcode command line tools not installed"
    echo "   Run: xcode-select --install"
    exit 1
fi
echo "   ✓ Installed"

# Create directory structure
echo ""
echo "Creating directory structure..."

mkdir -p Core/include
mkdir -p Core/src
mkdir -p SwiftBridge
mkdir -p CLI
mkdir -p Tests/Fixtures
mkdir -p Analysis
mkdir -p Output

echo "   ✓ Directories created"

# Build the project
echo ""
echo "🔨 Building project..."
swift build

if [ $? -eq 0 ]; then
    echo "   Build successful"
else
    echo "   Build failed"
    exit 1
fi

# Check if we need sudo
echo ""
echo "   Checking permissions..."
echo "   Note: Profiling requires 'task_for_pid' permission"
echo "   You'll need to run with 'sudo' or add an entitlement"

# Create a quick test
echo ""
echo "   Running quick test..."
echo "   Starting test target in background..."

.build/debug/test-target &
TEST_PID=$!
echo "   Test PID: $TEST_PID"

# Give it a moment to start
sleep 2

# Try to profile it (may fail without sudo)
echo "   Attempting to profile (may ask for sudo)..."
if sudo .build/debug/profiler $TEST_PID info &> /dev/null; then
    echo "   ✓ Basic profiling works"
    
    echo ""
    echo "   Capturing stacks..."
    sudo .build/debug/profiler $TEST_PID stacks | head -n 30
    
    echo ""
    echo "   ✓ Stack walking works"
else
    echo "     Profiling requires sudo"
fi

# Cleanup
kill $TEST_PID 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║                  Setup Complete!                     ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Quick Start:"
echo ""
echo "1. Build the project:"
echo "   swift build"
echo ""
echo "2. Start the test target:"
echo "   .build/debug/test-target"
echo ""
echo "3. In another terminal, profile it:"
echo "   sudo .build/debug/profiler <PID> stacks"
echo ""
echo "4. Or profile any running process:"
echo "   sudo .build/debug/profiler <PID> info"
echo ""
echo "Examples:"
echo "   sudo .build/debug/profiler 1234 info      # Show threads"
echo "   sudo .build/debug/profiler 1234 stacks    # Capture all stacks"
echo "   sudo .build/debug/profiler 1234 stack 0   # Capture thread 0"
echo "   sudo .build/debug/profiler 1234 sample 10 # Sample 10 times"
echo ""
echo "Documentation:"
echo "   README.md               - User guide"
echo "   IMPLEMENTATION_GUIDE.md - Developer guide"
echo ""
echo "Need help? Check the README.md file!"
echo ""