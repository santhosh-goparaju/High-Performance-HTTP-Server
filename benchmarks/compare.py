#!/usr/bin/env python3
import sys, re, os

def parse_wrk_output(file_path):
    data = {'file': os.path.basename(file_path)}
    with open(file_path, 'r') as f:
        content = f.read()
        
    req_sec_match = re.search(r'Requests/sec:\s+([\d.]+)', content)
    if req_sec_match:
        data['req_sec'] = float(req_sec_match.group(1))
        
    latency_match = re.search(r'Latency\s+([\d.]+[a-zA-Z]+)', content)
    if latency_match:
        data['avg_latency'] = latency_match.group(1)
        
    p50_match = re.search(r'50%\s+([\d.]+[a-zA-Z]+)', content)
    if p50_match:
        data['p50'] = p50_match.group(1)
        
    p99_match = re.search(r'99%\s+([\d.]+[a-zA-Z]+)', content)
    if p99_match:
        data['p99'] = p99_match.group(1)
        
    return data

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 compare.py results/file1.txt results/file2.txt ...")
        sys.exit(1)
        
    files = sys.argv[1:]
    results = []
    
    for file in files:
        if os.path.exists(file):
            results.append(parse_wrk_output(file))
            
    print(f"{'File':<30} | {'Req/Sec':<15} | {'Avg Latency':<15} | {'p50':<15} | {'p99':<15}")
    print("-" * 95)
    
    for res in results:
        print(f"{res.get('file', 'N/A')[:28]:<30} | "
              f"{res.get('req_sec', 'N/A'):<15.2f} | "
              f"{res.get('avg_latency', 'N/A'):<15} | "
              f"{res.get('p50', 'N/A'):<15} | "
              f"{res.get('p99', 'N/A'):<15}")

if __name__ == '__main__':
    main()
