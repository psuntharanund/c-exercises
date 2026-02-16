#!/bin/bash

# Local test script to mimic the three failing Gradescope tests
# Run this script from your project directory

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

TEST_DIR="test_workspace"
PORT=56727
SERVER_PID=""
CLIENT_PID=""

# Cleanup function
cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    if [ ! -z "$SERVER_PID" ]; then
        kill -9 $SERVER_PID 2>/dev/null || true
    fi
    if [ ! -z "$CLIENT_PID" ]; then
        kill -9 $CLIENT_PID 2>/dev/null || true
    fi
    rm -rf "$TEST_DIR"
}

trap cleanup EXIT

# Setup test environment
setup() {
    echo -e "${YELLOW}Setting up test environment...${NC}"
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR/content"
    mkdir -p "$TEST_DIR/downloads"
    cd "$TEST_DIR"
}

# Create test files
create_test_files() {
    echo -e "${YELLOW}Creating test files...${NC}"
    
    # Small file (1KB)
    dd if=/dev/urandom of=content/file000.txt bs=1K count=1 2>/dev/null
    
    # Medium files (2-4KB)
    dd if=/dev/urandom of=content/file001.txt bs=1K count=2 2>/dev/null
    dd if=/dev/urandom of=content/file003.txt bs=1K count=3 2>/dev/null
    dd if=/dev/urandom of=content/file005.txt bs=1K count=4 2>/dev/null
    dd if=/dev/urandom of=content/file006.txt bs=1K count=4 2>/dev/null
    dd if=/dev/urandom of=content/file007.txt bs=1K count=3 2>/dev/null
    
    # Huge file (100MB) - to test memory handling
    echo -e "${YELLOW}Creating 100MB test file (this may take a moment)...${NC}"
    dd if=/dev/urandom of=content/file_huge.txt bs=1M count=100 2>/dev/null
    
    # Create content.txt mapping
    cat > content.txt << EOF
/junk/file000.txt content/file000.txt
/junk/file001.txt content/file001.txt
/junk/file003.txt content/file003.txt
/junk/file005.txt content/file005.txt
/junk/file006.txt content/file006.txt
/junk/file007.txt content/file007.txt
/junk/file_huge.txt content/file_huge.txt
EOF
}

# Create workload files
create_workloads() {
    # Workload 1: Same file multiple times (test concurrent access to same file)
    cat > workload_same.txt << EOF
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
/junk/file000.txt
EOF

    # Workload 2: Mixed files (test multiple different files)
    cat > workload_mixed.txt << EOF
/junk/file001.txt
/junk/file003.txt
/junk/file005.txt
/junk/file006.txt
/junk/file007.txt
/junk/file001.txt
/junk/file003.txt
/junk/file005.txt
/junk/file006.txt
/junk/file007.txt
/junk/file001.txt
/junk/file003.txt
/junk/file005.txt
/junk/file006.txt
/junk/file007.txt
/junk/file001.txt
/junk/file003.txt
/junk/file005.txt
/junk/file006.txt
/junk/file007.txt
EOF

    # Workload 3: Huge file (test memory handling)
    cat > workload_huge.txt << EOF
/junk/file_huge.txt
/junk/file_huge.txt
/junk/file_huge.txt
/junk/file_huge.txt
EOF
}

# Verify downloaded files
verify_downloads() {
    local workload=$1
    local expected_downloads=$2
    local success=0
    local failed=0
    
    echo -e "${YELLOW}Verifying downloads...${NC}"
    
    # Count downloaded files
    local download_count=$(ls downloads/*-* 2>/dev/null | wc -l)
    
    if [ "$download_count" -ne "$expected_downloads" ]; then
        echo -e "${RED}FAILED: Expected $expected_downloads downloads, got $download_count${NC}"
        return 1
    fi
    
    # Verify each download against original
    for download in downloads/*-*; do
        if [ ! -f "$download" ]; then
            continue
        fi
        
        local size=$(stat -c%s "$download" 2>/dev/null || stat -f%z "$download" 2>/dev/null)
        
        if [ "$size" -eq 0 ]; then
            echo -e "${RED}FAILED: Downloaded file $download has size 0${NC}"
            failed=$((failed + 1))
        else
            success=$((success + 1))
        fi
    done
    
    if [ $failed -gt 0 ]; then
        echo -e "${RED}FAILED: $failed files have incorrect size${NC}"
        return 1
    fi
    
    echo -e "${GREEN}SUCCESS: All $success files downloaded correctly${NC}"
    return 0
}

# Test 1: Same file download (16 threads, same file)
test_same_file() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}TEST 1: Same file download (16 threads)${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # Start server
    ../gfserver_main -p $PORT -t 16 -m content.txt > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Run client
    timeout 20 ../gfclient_download -s localhost -p $PORT -t 16 -n 16 -w workload_same.txt > client.log 2>&1
    local client_exit=$?
    
    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
    
    # Check for errors
    if grep -q "Bad file descriptor" server.log; then
        echo -e "${RED}FAILED: Bad file descriptor error detected${NC}"
        echo "Server log:"
        cat server.log
        return 1
    fi
    
    if [ $client_exit -eq 124 ]; then
        echo -e "${RED}FAILED: Client timed out (probably hung)${NC}"
        return 1
    fi
    
    if [ $client_exit -ne 0 ]; then
        echo -e "${RED}FAILED: Client exited with error code $client_exit${NC}"
        return 1
    fi
    
    # Verify downloads
    verify_downloads "workload_same.txt" 16
    return $?
}

# Test 2: Multiple clients, mixed files
test_multiple_clients() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}TEST 2: Multiple clients, mixed files${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # Start server
    ../gfserver_main -p $PORT -t 8 -m content.txt > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Run client
    timeout 20 ../gfclient_download -s localhost -p $PORT -t 8 -n 20 -w workload_mixed.txt > client.log 2>&1
    local client_exit=$?
    
    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
    
    # Check for errors
    if grep -q "Bad file descriptor" server.log; then
        echo -e "${RED}FAILED: Bad file descriptor error detected${NC}"
        echo "Server log:"
        cat server.log
        return 1
    fi
    
    if grep -q "LeakSanitizer" server.log; then
        echo -e "${RED}FAILED: Memory leak detected${NC}"
        echo "Server log:"
        cat server.log
        return 1
    fi
    
    if [ $client_exit -eq 124 ]; then
        echo -e "${RED}FAILED: Client timed out${NC}"
        return 1
    fi
    
    if [ $client_exit -ne 0 ]; then
        echo -e "${RED}FAILED: Client exited with error code $client_exit${NC}"
        return 1
    fi
    
    # Verify downloads
    verify_downloads "workload_mixed.txt" 20
    return $?
}

# Test 3: Huge files (tests memory handling)
test_huge_files() {
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}TEST 3: Huge files (100MB files)${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # Start server
    ../gfserver_main -p $PORT -t 4 -m content.txt > server.log 2>&1 &
    SERVER_PID=$!
    sleep 1
    
    # Run client with timeout
    timeout 30 ../gfclient_download -s localhost -p $PORT -t 4 -n 4 -w workload_huge.txt > client.log 2>&1
    local client_exit=$?
    
    # Stop server
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
    
    # Check for errors
    if grep -q "Bad file descriptor" server.log; then
        echo -e "${RED}FAILED: Bad file descriptor error detected${NC}"
        echo "Server log:"
        cat server.log
        return 1
    fi
    
    if [ $client_exit -eq 124 ]; then
        echo -e "${RED}FAILED: Client timed out (probably hung)${NC}"
        echo "Client may be waiting on hung server connection"
        return 1
    fi
    
    if [ $client_exit -ne 0 ]; then
        echo -e "${RED}FAILED: Client exited with error code $client_exit${NC}"
        return 1
    fi
    
    # Verify downloads
    echo -e "${YELLOW}Verifying huge file downloads...${NC}"
    local expected_size=$(stat -c%s "content/file_huge.txt" 2>/dev/null || stat -f%z "content/file_huge.txt" 2>/dev/null)
    
    for download in downloads/*-*; do
        if [ ! -f "$download" ]; then
            continue
        fi
        
        local size=$(stat -c%s "$download" 2>/dev/null || stat -f%z "$download" 2>/dev/null)
        
        if [ "$size" -ne "$expected_size" ]; then
            echo -e "${RED}FAILED: Downloaded file $download has size $size, expected $expected_size${NC}"
            return 1
        fi
    done
    
    echo -e "${GREEN}SUCCESS: All huge files downloaded correctly${NC}"
    return 0
}

# Main execution
main() {
    echo -e "${GREEN}Starting local tests for multithreaded getfile server${NC}"
    echo ""
    
    setup
    create_test_files
    create_workloads
    
    local tests_passed=0
    local tests_failed=0
    
    # Run Test 1
    cd downloads
    if test_same_file; then
        tests_passed=$((tests_passed + 1))
    else
        tests_failed=$((tests_failed + 1))
    fi
    cd ..
    rm -rf downloads/*
    
    # Run Test 2
    cd downloads
    if test_multiple_clients; then
        tests_passed=$((tests_passed + 1))
    else
        tests_failed=$((tests_failed + 1))
    fi
    cd ..
    rm -rf downloads/*
    
    # Run Test 3
    cd downloads
    if test_huge_files; then
        tests_passed=$((tests_passed + 1))
    else
        tests_failed=$((tests_failed + 1))
    fi
    cd ..
    
    # Summary
    echo ""
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}TEST SUMMARY${NC}"
    echo -e "${YELLOW}========================================${NC}"
    echo -e "Tests passed: ${GREEN}$tests_passed${NC}"
    echo -e "Tests failed: ${RED}$tests_failed${NC}"
    
    if [ $tests_failed -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        return 0
    else
        echo -e "${RED}Some tests failed. Review the logs above.${NC}"
        return 1
    fi
}

# Check if we're in the right directory
if [ ! -f "gfserver_main" ] || [ ! -f "gfclient_download" ]; then
    echo -e "${RED}Error: Could not find gfserver_main or gfclient_download${NC}"
    echo "Please run this script from your build directory"
    exit 1
fi

main
