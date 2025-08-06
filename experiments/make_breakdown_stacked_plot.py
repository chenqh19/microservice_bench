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
    
    # Convert to microseconds for better readability
    def ns_to_us(ns):
        return ns / 1000
    
    # Prepare data for plotting
    n_requests = len(request_types)
    x = np.arange(n_requests)
    width = 0.2  # Width of bars
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    # For each request type, create 4 bars
    for i, req_type in enumerate(request_types):
        protobuf_req_avg, protobuf_ser, protobuf_de = protobuf_data[req_type]
        ser1de_req_avg, ser1de_ser, ser1de_de = ser1de_data[req_type]
        
        # Convert to us
        protobuf_req_avg_us = ns_to_us(protobuf_req_avg)
        protobuf_ser_us = ns_to_us(protobuf_ser)
        protobuf_de_us = ns_to_us(protobuf_de)
        
        ser1de_req_avg_us = ns_to_us(ser1de_req_avg)
        ser1de_ser_us = ns_to_us(ser1de_ser)
        ser1de_de_us = ns_to_us(ser1de_de)
        
        # Position bars
        pos1 = i - 1.5*width  # Protobuf total
        pos2 = i - 0.5*width  # Protobuf ser/de stacked
        pos3 = i + 0.5*width  # Ser1de total
        pos4 = i + 1.5*width  # Ser1de ser/de stacked
        
        # Plot in specific order to achieve legend order: first row = protobuf items, second row = serenade items
        # 1. Protobuf Total
        ax.bar(pos1, protobuf_req_avg_us, width, 
               label='Protobuf Total' if i == 0 else "", 
               color='#1f77b4', alpha=0.8)

        # 4. SERenaDE Total  
        ax.bar(pos3, ser1de_req_avg_us, width, 
               label='SERenaDE Total' if i == 0 else "", 
               color='#2ca02c', alpha=0.8)
        
        # 2. Protobuf Serialization
        ax.bar(pos2, protobuf_ser_us, width, 
               label='Protobuf Serialization' if i == 0 else "", 
               color='#ff7f0e', alpha=0.8)
        
        # 5. SERenaDE Serialization
        ax.bar(pos4, ser1de_ser_us, width, 
               label='SERenaDE Serialization' if i == 0 else "", 
               color='#d62728', alpha=0.8)

        # 3. Protobuf Deserialization
        ax.bar(pos2, protobuf_de_us, width, bottom=protobuf_ser_us,
               label='Protobuf Deserialization' if i == 0 else "", 
               color='#ffbb78', alpha=0.8)
        
        # 6. SERenaDE Deserialization
        ax.bar(pos4, ser1de_de_us, width, bottom=ser1de_ser_us,
               label='SERenaDE Deserialization' if i == 0 else "", 
               color='#ff9999', alpha=0.8)
    
    # Customize the plot
    ax.set_xlabel('Request Types', fontsize=22)
    ax.set_ylabel('Time (μs)', fontsize=22)
    ax.set_title('Performance Comparison: Protobuf vs SERenaDE\n(Total Request and Serialization/Deserialization Time)', fontsize=26)
    ax.set_xticks(x)
    ax.set_xticklabels([req.upper() for req in request_types], fontsize=22)
    ax.legend(loc='upper right', fontsize=22, ncol=3)
    
    ax.tick_params(axis='both', which='major', labelsize=22)

    ax.set_ylim(0, 1000)
    
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
    
    # Add weighted average as a new "request type"
    weights = [0.6, 0.39, 0.005, 0.005]
    
    # Calculate weighted averages for protobuf
    protobuf_weighted_req_avg = sum(weights[i] * protobuf_data[req_type][0] for i, req_type in enumerate(request_types))
    protobuf_weighted_ser = sum(weights[i] * protobuf_data[req_type][1] for i, req_type in enumerate(request_types))
    protobuf_weighted_de = sum(weights[i] * protobuf_data[req_type][2] for i, req_type in enumerate(request_types))
    
    # Calculate weighted averages for ser1de
    ser1de_weighted_req_avg = sum(weights[i] * ser1de_data[req_type][0] for i, req_type in enumerate(request_types))
    ser1de_weighted_ser = sum(weights[i] * ser1de_data[req_type][1] for i, req_type in enumerate(request_types))
    ser1de_weighted_de = sum(weights[i] * ser1de_data[req_type][2] for i, req_type in enumerate(request_types))
    
    # Add weighted averages to data
    protobuf_data['mixture'] = (protobuf_weighted_req_avg, protobuf_weighted_ser, protobuf_weighted_de)
    ser1de_data['mixture'] = (ser1de_weighted_req_avg, ser1de_weighted_ser, ser1de_weighted_de)
    
    # Update request types to include mixture
    request_types.append('mixture')
    
    # Convert to microseconds for better readability
    def ns_to_us(ns):
        return ns / 1000
    
    # Prepare data for plotting
    n_requests = len(request_types)
    x = np.arange(n_requests)
    width = 0.35  # Width of bars
    
    fig, ax = plt.subplots(figsize=(18, 8))  # Slightly wider to accommodate the extra bar
    
    # For each request type, create stacked bars
    # Plot in specific order to achieve legend order: first row = protobuf items, second row = serenade items
    for i, req_type in enumerate(request_types):
        protobuf_req_avg, protobuf_ser, protobuf_de = protobuf_data[req_type]
        ser1de_req_avg, ser1de_ser, ser1de_de = ser1de_data[req_type]
        
        # Convert to us
        protobuf_req_avg_us = ns_to_us(protobuf_req_avg)
        protobuf_ser_us = ns_to_us(protobuf_ser)
        protobuf_de_us = ns_to_us(protobuf_de)
        
        ser1de_req_avg_us = ns_to_us(ser1de_req_avg)
        ser1de_ser_us = ns_to_us(ser1de_ser)
        ser1de_de_us = ns_to_us(ser1de_de)
        
        # Calculate "other" time (total - serialization - deserialization)
        protobuf_other_us = protobuf_req_avg_us - protobuf_ser_us - protobuf_de_us
        ser1de_other_us = ser1de_req_avg_us - ser1de_ser_us - ser1de_de_us
        
        # Position bars
        protobuf_pos = i - width/2
        ser1de_pos = i + width/2
        
        # 1. Logic + Network (Protobuf)
        ax.bar(protobuf_pos, protobuf_other_us, width, 
               label='Logic + Network (Protobuf)' if i == 0 else "", 
               color='#1f77b4', alpha=0.8)
        
        # 4. Logic + Network (SERenaDE)
        ax.bar(ser1de_pos, ser1de_other_us, width, 
               label='Logic + Network (SERenaDE)' if i == 0 else "", 
               color='#2ca02c', alpha=0.8)

        # 2. Protobuf Serialization
        ax.bar(protobuf_pos, protobuf_ser_us, width, bottom=protobuf_other_us,
               label='Protobuf Serialization' if i == 0 else "", 
               color='#ff7f0e', alpha=0.8)
        
        # 5. SERenaDE Serialization
        ax.bar(ser1de_pos, ser1de_ser_us, width, bottom=ser1de_other_us,
               label='SERenaDE Serialization' if i == 0 else "", 
               color='#d62728', alpha=0.8)

        # 3. Protobuf Deserialization
        ax.bar(protobuf_pos, protobuf_de_us, width, 
               bottom=protobuf_other_us + protobuf_ser_us,
               label='Protobuf Deserialization' if i == 0 else "", 
               color='#ffbb78', alpha=0.8)
        
        # 6. SERenaDE Deserialization
        ax.bar(ser1de_pos, ser1de_de_us, width, 
               bottom=ser1de_other_us + ser1de_ser_us,
               label='SERenaDE Deserialization' if i == 0 else "", 
               color='#ff9999', alpha=0.8)
    
    # Customize the plot
    ax.set_xlabel('Request Types', fontsize=24)
    ax.set_ylabel('Time (μs)', fontsize=24)
    ax.set_title('Performance Breakdown: Protobuf vs SERenaDE', fontsize=26)
    ax.set_xticks(x)
    ax.set_xticklabels([req.upper() if req != 'mixture' else 'MIXTURE' for req in request_types], fontsize=22)
    ax.legend(bbox_to_anchor=(0.47, 1.05), loc='lower center', ncol=3, fontsize=24)
    
    ax.tick_params(axis='both', which='major', labelsize=24)

    ax.set_ylim(0, 1000)
    
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
    protobuf_file = "breakdown_protobuf.txt"
    ser1de_file = "breakdown_ser1de.txt"
    
    print("Creating comparison plot...")
    create_comparison_plot(protobuf_file, ser1de_file, "performance_comparison.pdf")
    
    print("\nCreating stacked breakdown plot...")
    create_stacked_breakdown_plot(protobuf_file, ser1de_file, "performance_breakdown_stacked.pdf")
    
    # Also print the data for verification
    print("\nParsed data:")
    print("Protobuf:")
    protobuf_data = parse_breakdown_file(protobuf_file)
    for req_type, (req_avg, ser_sum, de_sum) in protobuf_data.items():
        other_time = (req_avg - ser_sum - de_sum) / 1000
        print(f"  {req_type.upper()}: {req_avg/1000:.0f}μs total, {ser_sum/1000:.0f}μs ser, {de_sum/1000:.0f}μs de, {other_time:.0f}μs other")
    
    print("\nSer1de:")
    ser1de_data = parse_breakdown_file(ser1de_file)
    for req_type, (req_avg, ser_sum, de_sum) in ser1de_data.items():
        other_time = (req_avg - ser_sum - de_sum) / 1000
        print(f"  {req_type.upper()}: {req_avg/1000:.0f}μs total, {ser_sum/1000:.0f}μs ser, {de_sum/1000:.0f}μs de, {other_time:.0f}μs other")