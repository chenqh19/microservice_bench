#!/usr/bin/env python3
import os
import glob
import numpy as np
from pathlib import Path

def collect_averages():
    """Collect average latency from entries 5001-15000 of each log file."""
    
    # Create logs directory if it doesn't exist
    logs_dir = Path("../logs")
    if not logs_dir.exists():
        print("Logs directory not found. Creating it...")
        logs_dir.mkdir(exist_ok=True)
        return
    
    # Find all log files
    log_files = list(logs_dir.glob("*.txt"))
    
    if not log_files:
        print("No log files found in ./logs directory.")
        return
    
    results = []
    
    for log_file in sorted(log_files):
        try:
            # Read all lines from the file
            with open(log_file, 'r') as f:
                lines = f.readlines()
            
            # Convert to integers (nanoseconds)
            values = []
            for line in lines:
                line = line.strip()
                if line and line.isdigit():
                    values.append(int(line))
            
            if not values:
                print(f"Warning: No valid data found in {log_file.name}")
                continue
            
            # # Take entries from position 5001 to 15000 (entries 5001-15000)
            # if len(values) >= 15000:
            #     selected_values = values[5000:15000]  # 0-indexed, so 5000-14999
            # elif len(values) > 5000:
            #     selected_values = values[5000:]  # From 5001 to end
            # else:
            #     print(f"Warning: Not enough data in {log_file.name} (need at least 5001 entries, got {len(values)})")
            #     continue
            selected_values = values
            
            # Calculate average of selected values
            avg_latency = np.mean(selected_values)
            count = len(selected_values)
            total_count = len(values)
            
            # Determine if it's serialization or deserialization
            if log_file.name.endswith("Se.txt"):
                operation = "Serialization"
            elif log_file.name.endswith("De.txt"):
                operation = "Deserialization"
            else:
                operation = "Unknown"
            
            # Extract message type name (remove Se.txt or De.txt suffix)
            message_type = log_file.name.replace("Se.txt", "").replace("De.txt", "")
            
            results.append({
                'file': log_file.name,
                'message_type': message_type,
                'operation': operation,
                'avg_latency_ns': avg_latency,
                'avg_latency_us': avg_latency / 1000.0,
                'avg_latency_ms': avg_latency / 1000000.0,
                'count': count,
                'total_count': total_count
            })
            
            print(f"Processed {log_file.name}: {operation} for {message_type}")
            print(f"  Average: {avg_latency:.2f} ns ({avg_latency/1000:.2f} μs, {avg_latency/1000000:.3f} ms)")
            print(f"  Entries: {count} (entries 5001-{5000+count}) out of {total_count} total")
            print()
            
        except Exception as e:
            print(f"Error processing {log_file.name}: {e}")
    
    # Write results to summary file
    if results:
        summary_file = "latency_summary.txt"
        with open(summary_file, 'w') as f:
            f.write("Serialization/Deserialization Latency Summary\n")
            f.write("=" * 50 + "\n\n")
            f.write("Average of entries 5001-15000 (or 5001-end if less than 15000 entries)\n")
            f.write("All values included in calculation (no outliers removed)\n\n")
            
            # Group by operation type
            for operation in ["Serialization", "Deserialization"]:
                op_results = [r for r in results if r['operation'] == operation]
                if op_results:
                    f.write(f"\n{operation} Latencies:\n")
                    f.write("-" * 30 + "\n")
                    
                    for result in sorted(op_results, key=lambda x: x['avg_latency_ns']):
                        f.write(f"{result['message_type']}:\n")
                        f.write(f"  Average: {result['avg_latency_ns']:.2f} ns")
                        f.write(f" ({result['avg_latency_us']:.2f} μs")
                        f.write(f", {result['avg_latency_ms']:.3f} ms)\n")
                        f.write(f"  Entries: {result['count']} (entries 5001-{5000+result['count']}) out of {result['total_count']} total\n\n")
        
        print(f"\nSummary written to {summary_file}")
        
        # Also print a quick summary to console
        print("\nQuick Summary:")
        print("=" * 30)
        for result in sorted(results, key=lambda x: x['avg_latency_ns']):
            print(f"{result['operation']} - {result['message_type']}: {result['avg_latency_ns']:.0f} ns")
    else:
        print("No valid data found in any log files.")

if __name__ == "__main__":
    collect_averages()
