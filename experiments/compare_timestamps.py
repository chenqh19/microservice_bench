#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import re
from pathlib import Path

def parse_summary_file(filename):
    """Parse a latency summary file and extract message types and their latencies."""
    data = {'serialization': {}, 'deserialization': {}}
    
    with open(filename, 'r') as f:
        content = f.read()
    
    # Split into sections
    sections = content.split('\n\n')
    
    for section in sections:
        if 'Serialization Latencies:' in section:
            current_op = 'serialization'
        elif 'Deserialization Latencies:' in section:
            current_op = 'deserialization'
        else:
            continue
        
        # Extract message types and their latencies
        lines = section.split('\n')
        for line in lines:
            if 'Average:' in line and 'ns' in line:
                # Extract message type from previous line
                prev_line = lines[lines.index(line) - 1] if lines.index(line) > 0 else ""
                if prev_line.strip() and not prev_line.startswith('-'):
                    message_type = prev_line.strip().rstrip(':')
                    
                    # Extract latency value
                    match = re.search(r'Average: ([\d.]+) ns', line)
                    if match:
                        latency = float(match.group(1))
                        data[current_op][message_type] = latency
    
    return data

def create_comparison_visualizations():
    """Create visualizations comparing protobuf vs ser1de performance."""
    
    # Parse both summary files
    protobuf_data = parse_summary_file('latency_summary_protobuf.txt')
    ser1de_data = parse_summary_file('latency_summary_ser1de.txt')
    
    # Create figure with subplots
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Protobuf vs SER1DE Performance Comparison', fontsize=16, fontweight='bold')
    
    # Colors for the bars
    colors = ['#2E86AB', '#A23B72']
    
    # 1. Serialization Comparison
    ax1.set_title('Serialization Latency Comparison', fontweight='bold')
    
    # Get common message types
    common_ser_types = set(protobuf_data['serialization'].keys()) & set(ser1de_data['serialization'].keys())
    common_ser_types = sorted(common_ser_types, key=lambda x: protobuf_data['serialization'][x])
    
    if common_ser_types:
        x = np.arange(len(common_ser_types))
        width = 0.35
        
        protobuf_ser_values = [protobuf_data['serialization'][msg] for msg in common_ser_types]
        ser1de_ser_values = [ser1de_data['serialization'][msg] for msg in common_ser_types]
        
        bars1 = ax1.bar(x - width/2, protobuf_ser_values, width, label='Protobuf', color=colors[0], alpha=0.8)
        bars2 = ax1.bar(x + width/2, ser1de_ser_values, width, label='SER1DE', color=colors[1], alpha=0.8)
        
        ax1.set_xlabel('Message Types')
        ax1.set_ylabel('Latency (nanoseconds)')
        ax1.set_title('Serialization Latency Comparison')
        ax1.set_xticks(x)
        ax1.set_xticklabels([msg.replace('N16hotelreservation', '').replace('E', '') for msg in common_ser_types], rotation=45, ha='right')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Add value labels on bars
        for bar in bars1:
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height + height*0.01,
                    f'{height:.0f}', ha='center', va='bottom', fontsize=8)
        
        for bar in bars2:
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height + height*0.01,
                    f'{height:.0f}', ha='center', va='bottom', fontsize=8)
    
    # 2. Deserialization Comparison
    ax2.set_title('Deserialization Latency Comparison', fontweight='bold')
    
    common_deser_types = set(protobuf_data['deserialization'].keys()) & set(ser1de_data['deserialization'].keys())
    common_deser_types = sorted(common_deser_types, key=lambda x: protobuf_data['deserialization'][x])
    
    if common_deser_types:
        x = np.arange(len(common_deser_types))
        width = 0.35
        
        protobuf_deser_values = [protobuf_data['deserialization'][msg] for msg in common_deser_types]
        ser1de_deser_values = [ser1de_data['deserialization'][msg] for msg in common_deser_types]
        
        bars1 = ax2.bar(x - width/2, protobuf_deser_values, width, label='Protobuf', color=colors[0], alpha=0.8)
        bars2 = ax2.bar(x + width/2, ser1de_deser_values, width, label='SER1DE', color=colors[1], alpha=0.8)
        
        ax2.set_xlabel('Message Types')
        ax2.set_ylabel('Latency (nanoseconds)')
        ax2.set_title('Deserialization Latency Comparison')
        ax2.set_xticks(x)
        ax2.set_xticklabels([msg.replace('N16hotelreservation', '').replace('E', '') for msg in common_deser_types], rotation=45, ha='right')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # Add value labels on bars
        for bar in bars1:
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height + height*0.01,
                    f'{height:.0f}', ha='center', va='bottom', fontsize=8)
        
        for bar in bars2:
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height + height*0.01,
                    f'{height:.0f}', ha='center', va='bottom', fontsize=8)
    
    # 3. Speedup Ratio (Protobuf / SER1DE)
    ax3.set_title('Speedup Ratio (Protobuf / SER1DE)', fontweight='bold')
    
    # Calculate speedup ratios
    ser_speedups = []
    deser_speedups = []
    ser_labels = []
    deser_labels = []
    
    for msg in common_ser_types:
        if ser1de_data['serialization'][msg] > 0:
            speedup = protobuf_data['serialization'][msg] / ser1de_data['serialization'][msg]
            ser_speedups.append(speedup)
            ser_labels.append(msg.replace('N16hotelreservation', '').replace('E', ''))
    
    for msg in common_deser_types:
        if ser1de_data['deserialization'][msg] > 0:
            speedup = protobuf_data['deserialization'][msg] / ser1de_data['deserialization'][msg]
            deser_speedups.append(speedup)
            deser_labels.append(msg.replace('N16hotelreservation', '').replace('E', ''))
    
    if ser_speedups:
        x = np.arange(len(ser_speedups))
        bars = ax3.bar(x, ser_speedups, color='green', alpha=0.7, label='Serialization')
        ax3.set_xlabel('Message Types')
        ax3.set_ylabel('Speedup Ratio')
        ax3.set_xticks(x)
        ax3.set_xticklabels(ser_labels, rotation=45, ha='right')
        ax3.axhline(y=1, color='red', linestyle='--', alpha=0.7, label='Equal Performance')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
        
        # Add value labels
        for bar in bars:
            height = bar.get_height()
            ax3.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                    f'{height:.2f}x', ha='center', va='bottom', fontsize=8)
    
    # 4. Summary Statistics
    ax4.set_title('Summary Statistics', fontweight='bold')
    ax4.axis('off')
    
    # Calculate summary statistics
    summary_text = "Performance Summary:\n\n"
    
    # Average latencies
    if protobuf_data['serialization'] and ser1de_data['serialization']:
        avg_protobuf_ser = np.mean(list(protobuf_data['serialization'].values()))
        avg_ser1de_ser = np.mean(list(ser1de_data['serialization'].values()))
        summary_text += f"Serialization:\n"
        summary_text += f"  Protobuf avg: {avg_protobuf_ser:.0f} ns\n"
        summary_text += f"  SER1DE avg: {avg_ser1de_ser:.0f} ns\n"
        summary_text += f"  Speedup: {avg_protobuf_ser/avg_ser1de_ser:.2f}x\n\n"
    
    if protobuf_data['deserialization'] and ser1de_data['deserialization']:
        avg_protobuf_deser = np.mean(list(protobuf_data['deserialization'].values()))
        avg_ser1de_deser = np.mean(list(ser1de_data['deserialization'].values()))
        summary_text += f"Deserialization:\n"
        summary_text += f"  Protobuf avg: {avg_protobuf_deser:.0f} ns\n"
        summary_text += f"  SER1DE avg: {avg_ser1de_deser:.0f} ns\n"
        summary_text += f"  Speedup: {avg_protobuf_deser/avg_ser1de_deser:.2f}x\n\n"
    
    # Best and worst cases
    if common_ser_types:
        best_ser_speedup = min(ser_speedups)
        worst_ser_speedup = max(ser_speedups)
        summary_text += f"Serialization Range:\n"
        summary_text += f"  Best speedup: {best_ser_speedup:.2f}x\n"
        summary_text += f"  Worst speedup: {worst_ser_speedup:.2f}x\n\n"
    
    if common_deser_types:
        best_deser_speedup = min(deser_speedups)
        worst_deser_speedup = max(deser_speedups)
        summary_text += f"Deserialization Range:\n"
        summary_text += f"  Best speedup: {best_deser_speedup:.2f}x\n"
        summary_text += f"  Worst speedup: {worst_deser_speedup:.2f}x"
    
    ax4.text(0.05, 0.95, summary_text, transform=ax4.transAxes, fontsize=10,
             verticalalignment='top', fontfamily='monospace',
             bbox=dict(boxstyle="round,pad=0.3", facecolor="lightgray", alpha=0.8))
    
    plt.tight_layout()
    plt.savefig('protobuf_vs_ser1de_comparison.png', dpi=300, bbox_inches='tight')
    plt.show()
    
    # Print summary to console
    print("Protobuf vs SER1DE Performance Comparison")
    print("=" * 50)
    print(summary_text)

if __name__ == "__main__":
    create_comparison_visualizations()
