import matplotlib.pyplot as plt
import numpy as np
import re

def parse_breakdown_file(filepath):
    """
    Parse a breakdown file and extract timing data.
    
    Returns:
        dict: Dictionary with request types as keys and (request_avg, ser_sum, de_sum) as values
    """
    data = {}
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Split by request sections
    request_sections = re.split(r'\n(?=[A-Z]+ request:)', content)
    
    for section in request_sections:
        if not section.strip():
            continue
            
        lines = section.strip().split('\n')
        if not lines[0].endswith('request:'):
            continue
            
        request_type = lines[0].replace(' request:', '').strip()
        
        # Extract timing values (in ns)
        request_avg = 0
        ser_sum = 0
        de_sum = 0
        
        for line in lines[1:]:
            if 'Request avg:' in line:
                # Extract number before 'ns'
                match = re.search(r'(\d+(?:\.\d+)?)\s*ns', line)
                if match:
                    request_avg = float(match.group(1))
            elif 'Serialization sum:' in line:
                match = re.search(r'(\d+(?:\.\d+)?)\s*ns', line)
                if match:
                    ser_sum = float(match.group(1))
            elif 'Deserialization sum:' in line:
                match = re.search(r'(\d+(?:\.\d+)?)\s*ns', line)
                if match:
                    de_sum = float(match.group(1))
        
        data[request_type.lower()] = (request_avg, ser_sum, de_sum)
    
    return data

def create_comparison_plot(protobuf_file, ser1de_file, output_file=None):
    """
    Create a stacked bar plot comparing protobuf and ser1de performance.
    
    Args:
        protobuf_file (str): Path to protobuf breakdown file
        ser1de_file (str): Path to ser1de breakdown file
        output_file (str): Optional path to save the plot
    """
    # Parse both files
    protobuf_data = parse_breakdown_file(protobuf_file)
    ser1de_data = parse_breakdown_file(ser1de_file)
    
    # Get all request types (should be the same in both files)
    request_types = list(protobuf_data.keys())
    
    # Convert to milliseconds for better readability
    def ns_to_ms(ns):
        return ns / 1000000
    
    # Prepare data for plotting
    n_requests = len(request_types)
    x = np.arange(n_requests)
    width = 0.2  # Width of bars
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    # For each request type, create 4 bars
    for i, req_type in enumerate(request_types):
        protobuf_req_avg, protobuf_ser, protobuf_de = protobuf_data[req_type]
        ser1de_req_avg, ser1de_ser, ser1de_de = ser1de_data[req_type]
        
        # Convert to ms
        protobuf_req_avg_ms = ns_to_ms(protobuf_req_avg)
        protobuf_ser_ms = ns_to_ms(protobuf_ser)
        protobuf_de_ms = ns_to_ms(protobuf_de)
        
        ser1de_req_avg_ms = ns_to_ms(ser1de_req_avg)
        ser1de_ser_ms = ns_to_ms(ser1de_ser)
        ser1de_de_ms = ns_to_ms(ser1de_de)
        
        # Position bars
        pos1 = i - 1.5*width  # Protobuf total
        pos2 = i - 0.5*width  # Protobuf ser/de stacked
        pos3 = i + 0.5*width  # Ser1de total
        pos4 = i + 1.5*width  # Ser1de ser/de stacked
        
        # Protobuf total request time
        ax.bar(pos1, protobuf_req_avg_ms, width, 
               label='Protobuf Total' if i == 0 else "", 
               color='#1f77b4', alpha=0.8)
        
        # Protobuf serialization + deserialization (stacked)
        ax.bar(pos2, protobuf_ser_ms, width, 
               label='Protobuf Serialization' if i == 0 else "", 
               color='#ff7f0e', alpha=0.8)
        ax.bar(pos2, protobuf_de_ms, width, bottom=protobuf_ser_ms,
               label='Protobuf Deserialization' if i == 0 else "", 
               color='#ffbb78', alpha=0.8)
        
        # Ser1de total request time
        ax.bar(pos3, ser1de_req_avg_ms, width, 
               label='Ser1de Total' if i == 0 else "", 
               color='#2ca02c', alpha=0.8)
        
        # Ser1de serialization + deserialization (stacked)
        ax.bar(pos4, ser1de_ser_ms, width, 
               label='Ser1de Serialization' if i == 0 else "", 
               color='#d62728', alpha=0.8)
        ax.bar(pos4, ser1de_de_ms, width, bottom=ser1de_ser_ms,
               label='Ser1de Deserialization' if i == 0 else "", 
               color='#ff9999', alpha=0.8)
        
        # Add value labels on bars
        # Protobuf total
        ax.text(pos1, protobuf_req_avg_ms + 0.02, f'{protobuf_req_avg_ms:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
        
        # Protobuf ser/de total
        total_protobuf_ser_de = protobuf_ser_ms + protobuf_de_ms
        ax.text(pos2, total_protobuf_ser_de + 0.02, f'{total_protobuf_ser_de:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
        
        # Ser1de total
        ax.text(pos3, ser1de_req_avg_ms + 0.02, f'{ser1de_req_avg_ms:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
        
        # Ser1de ser/de total
        total_ser1de_ser_de = ser1de_ser_ms + ser1de_de_ms
        ax.text(pos4, total_ser1de_ser_de + 0.02, f'{total_ser1de_ser_de:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
    
    # Customize the plot
    ax.set_xlabel('Request Types', fontsize=22)
    ax.set_ylabel('Time (ms)', fontsize=22)
    ax.set_title('Performance Comparison: Protobuf vs Ser1de\n(Total Request and Serialization/Deserialization Time)', fontsize=26)
    ax.set_xticks(x)
    ax.set_xticklabels([req.upper() for req in request_types], fontsize=22)
    ax.legend(loc='upper right', fontsize=22)
    
    ax.tick_params(axis='both', which='major', labelsize=22)

    ax.set_ylim(0, 3)
    
    # Add grid for better readability
    ax.grid(True, alpha=0.3, axis='y')
    
    # Adjust layout
    plt.tight_layout()
    
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Plot saved as {output_file}")
    
    plt.show()

def create_stacked_breakdown_plot(protobuf_file, ser1de_file, output_file=None):
    """
    Create a stacked bar plot showing breakdown of time into serialization, 
    deserialization, and other processing time.
    
    Args:
        protobuf_file (str): Path to protobuf breakdown file
        ser1de_file (str): Path to ser1de breakdown file
        output_file (str): Optional path to save the plot
    """
    # Parse both files
    protobuf_data = parse_breakdown_file(protobuf_file)
    ser1de_data = parse_breakdown_file(ser1de_file)
    
    # Get all request types (should be the same in both files)
    request_types = list(protobuf_data.keys())
    
    # Convert to milliseconds for better readability
    def ns_to_ms(ns):
        return ns / 1000000
    
    # Prepare data for plotting
    n_requests = len(request_types)
    x = np.arange(n_requests)
    width = 0.35  # Width of bars
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    # For each request type, create stacked bars
    for i, req_type in enumerate(request_types):
        protobuf_req_avg, protobuf_ser, protobuf_de = protobuf_data[req_type]
        ser1de_req_avg, ser1de_ser, ser1de_de = ser1de_data[req_type]
        
        # Convert to ms
        protobuf_req_avg_ms = ns_to_ms(protobuf_req_avg)
        protobuf_ser_ms = ns_to_ms(protobuf_ser)
        protobuf_de_ms = ns_to_ms(protobuf_de)
        
        ser1de_req_avg_ms = ns_to_ms(ser1de_req_avg)
        ser1de_ser_ms = ns_to_ms(ser1de_ser)
        ser1de_de_ms = ns_to_ms(ser1de_de)
        
        # Calculate "other" time (total - serialization - deserialization)
        protobuf_other_ms = protobuf_req_avg_ms - protobuf_ser_ms - protobuf_de_ms
        ser1de_other_ms = ser1de_req_avg_ms - ser1de_ser_ms - ser1de_de_ms
        
        # Position bars
        protobuf_pos = i - width/2
        ser1de_pos = i + width/2
        
        # Create stacked bars
        # Protobuf stacked bar
        ax.bar(protobuf_pos, protobuf_other_ms, width, 
               label='Protobuf Other' if i == 0 else "", 
               color='#1f77b4', alpha=0.8)
        ax.bar(protobuf_pos, protobuf_ser_ms, width, bottom=protobuf_other_ms,
               label='Protobuf Serialization' if i == 0 else "", 
               color='#ff7f0e', alpha=0.8)
        ax.bar(protobuf_pos, protobuf_de_ms, width, 
               bottom=protobuf_other_ms + protobuf_ser_ms,
               label='Protobuf Deserialization' if i == 0 else "", 
               color='#ffbb78', alpha=0.8)
        
        # Ser1de stacked bar
        ax.bar(ser1de_pos, ser1de_other_ms, width, 
               label='Ser1de Other' if i == 0 else "", 
               color='#2ca02c', alpha=0.8)
        ax.bar(ser1de_pos, ser1de_ser_ms, width, bottom=ser1de_other_ms,
               label='Ser1de Serialization' if i == 0 else "", 
               color='#d62728', alpha=0.8)
        ax.bar(ser1de_pos, ser1de_de_ms, width, 
               bottom=ser1de_other_ms + ser1de_ser_ms,
               label='Ser1de Deserialization' if i == 0 else "", 
               color='#ff9999', alpha=0.8)
        
        # Add value labels on bars (total time)
        # Protobuf total
        ax.text(protobuf_pos, protobuf_req_avg_ms + 0.02, f'{protobuf_req_avg_ms:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
        
        # Ser1de total
        ax.text(ser1de_pos, ser1de_req_avg_ms + 0.02, f'{ser1de_req_avg_ms:.2f}', 
                ha='center', va='bottom', fontsize=22, rotation=90)
    
    # Customize the plot
    ax.set_xlabel('Request Types', fontsize=22)
    ax.set_ylabel('Time (ms)', fontsize=22)
    ax.set_title('Performance Breakdown: Protobuf vs Ser1de', fontsize=26)
    ax.set_xticks(x)
    ax.set_xticklabels([req.upper() for req in request_types], fontsize=22)
    ax.legend(loc='upper right', fontsize=22)
    
    ax.tick_params(axis='both', which='major', labelsize=22)

    ax.set_ylim(0, 3)
    
    # Add grid for better readability
    ax.grid(True, alpha=0.3, axis='y')
    
    # Adjust layout
    plt.tight_layout()
    
    if output_file:
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        print(f"Plot saved as {output_file}")
    
    plt.show()

if __name__ == "__main__":
    # Example usage
    protobuf_file = "breakdown_protobuf_300B.txt"
    ser1de_file = "breakdown_ser1de_300B.txt"
    
    print("Creating comparison plot...")
    create_comparison_plot(protobuf_file, ser1de_file, "performance_comparison.pdf")
    
    print("\nCreating stacked breakdown plot...")
    create_stacked_breakdown_plot(protobuf_file, ser1de_file, "performance_breakdown_stacked.pdf")
    
    # Also print the data for verification
    print("\nParsed data:")
    print("Protobuf:")
    protobuf_data = parse_breakdown_file(protobuf_file)
    for req_type, (req_avg, ser_sum, de_sum) in protobuf_data.items():
        other_time = (req_avg - ser_sum - de_sum) / 1000000
        print(f"  {req_type.upper()}: {req_avg/1000000:.2f}ms total, {ser_sum/1000000:.2f}ms ser, {de_sum/1000000:.2f}ms de, {other_time:.2f}ms other")
    
    print("\nSer1de:")
    ser1de_data = parse_breakdown_file(ser1de_file)
    for req_type, (req_avg, ser_sum, de_sum) in ser1de_data.items():
        other_time = (req_avg - ser_sum - de_sum) / 1000000
        print(f"  {req_type.upper()}: {req_avg/1000000:.2f}ms total, {ser_sum/1000000:.2f}ms ser, {de_sum/1000000:.2f}ms de, {other_time:.2f}ms other")