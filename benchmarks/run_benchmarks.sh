#!/bin/bash
# Benchmark suite for HTTP Server
# Records: requests/sec, avg latency, p50, p95, p99

SERVER_PORT=${1:-8080}
DURATION=${2:-30}
RESULTS_DIR="$(dirname $0)/results"
mkdir -p "$RESULTS_DIR"

# Record environment
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
ENV_FILE="$RESULTS_DIR/env_${TIMESTAMP}.txt"
echo "Date: $(date)" > "$ENV_FILE"
echo "OS: $(uname -a)" >> "$ENV_FILE"
echo "CPU: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu 2>/dev/null | grep 'Model name' || echo 'unknown')" >> "$ENV_FILE"
echo "Compiler: $(g++ --version | head -1)" >> "$ENV_FILE"

# Check if wrk is available
if command -v wrk &> /dev/null; then
    TOOL="wrk"
else
    echo "wrk not found. Install with: brew install wrk (macOS) or build from source (Linux)"
    echo "Falling back to curl-based benchmarks"
    TOOL="curl"
fi

run_wrk_benchmark() {
    local name=$1
    local url=$2
    local threads=$3
    local connections=$4
    local output_file="$RESULTS_DIR/${name}_${TIMESTAMP}.txt"
    
    echo "Running: $name (threads=$threads, connections=$connections, duration=${DURATION}s)"
    wrk -t$threads -c$connections -d${DURATION}s --latency "$url" | tee "$output_file"
    echo ""
}

if [ "$TOOL" = "wrk" ]; then
    # Minimal endpoint
    run_wrk_benchmark "health_c100" "http://localhost:$SERVER_PORT/health" 4 100
    run_wrk_benchmark "health_c500" "http://localhost:$SERVER_PORT/health" 4 500
    
    # Static file
    run_wrk_benchmark "static_c100" "http://localhost:$SERVER_PORT/static/hello.txt" 4 100
    
    # API endpoint
    run_wrk_benchmark "api_c100" "http://localhost:$SERVER_PORT/api/info" 4 100
    
    # Root page
    run_wrk_benchmark "root_c100" "http://localhost:$SERVER_PORT/" 4 100
fi

echo "Results saved to $RESULTS_DIR/"
