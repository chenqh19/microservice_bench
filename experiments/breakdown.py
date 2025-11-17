import os
import glob
import numpy as np

# Mapping of request types to their corresponding messages
# Request types are keys, message types are values
REQUEST_TO_MESSAGE_MAPPING = {
    # Search request (from frontend /search endpoint)
    'search': [
        'SearchRequest',
        'SearchResponse', 
        'NearbyRequest',
        'NearbyResponse',
        'GetRatesRequest',
        'GetRatesResponse',
        'GetProfilesRequest',
        'GetProfilesResponse'
    ],
    
    # Recommend request (from frontend /recommend endpoint)
    'recommend': [
        'RecommendRequest',
        'RecommendResponse',
        'GetRatesRequest',
        'GetRatesResponse',
        'GetProfilesRequest',
        'GetProfilesResponse'
    ],
    
    # User request (from frontend /user endpoint)
    'user': [
        'UserRequest',
        'UserResponse'
    ],
    
    # Reservation request (from frontend /reservation endpoint)
    'reservation': [
        'ReservationRequest',
        'ReservationResponse',
        'CheckUserRequestESe',
        'CheckUserResponseEDe',
        'UserRequestEDe',
        'UserResponseESe'
    ]
}

SERVICE_TO_MESSAGE_MAPPING = {
    'search': [
        'SearchRequestEDe',
        'SearchResponseESe',
        'NearbyRequestESe',
        'NearbyResponseEDe',
        'GetRatesRequestESe',
        'GetRatesResponseEDe',
        'GetProfilesRequestESe',
        'GetProfilesResponseEDe',
    ],
    'recommendation': [
        'RecommendRequestEDe',
        'RecommendResponseESe',
        'GetRatesRequestESe',
        'GetRatesResponseEDe',
        'GetProfilesRequestESe',
        'GetProfilesResponseEDe',
    ],
    'user': [
        'UserRequestEDe',
        'UserResponseESe',
        'CheckUserRequestEDe',
        'CheckUserResponseESe',
    ],
    'reservation': [
        'ReservationRequestEDe',
        'ReservationResponseESe',
        'CheckUserRequestESe',
        'CheckUserResponseEDe',
        'UserRequestESe',
        'UserResponseEDe',
    ],
    'profile': [
        'GetProfilesRequestEDe',
        'GetProfilesResponseESe',
    ],
    'rate': [
        'GetRatesRequestEDe',
        'GetRatesResponseESe',
    ],
    'geo': [
        'NearbyRequestEDe',
        'NearbyResponseESe',
    ],
}

def analyze_timing_logs(logs_dir="../logs"):
    """
    Analyze timing data from log files in the specified directory.
    
    Args:
        logs_dir (str): Path to the logs directory (default: "../logs")
    
    Returns:
        dict: Dictionary with filename as key and average timing as value
    """
    results = {}
    
    # Check if logs directory exists
    if not os.path.exists(logs_dir):
        return results
    
    # Find all .txt files in the logs directory
    log_files = glob.glob(os.path.join(logs_dir, "*.txt"))
    
    for log_file in log_files:
        filename = os.path.basename(log_file)
        try:
            # Read all lines from the file
            with open(log_file, 'r') as f:
                lines = f.readlines()
            
            # Convert lines to numbers, filtering out empty lines
            numbers = []
            for line in lines:
                line = line.strip()
                if line:  # Skip empty lines
                    try:
                        numbers.append(float(line))
                    except ValueError:
                        continue
            
            if numbers:
                # Calculate average
                avg_time = np.mean(numbers)
                results[filename] = avg_time
                
        except Exception as e:
            continue
    
    return results

def get_average_only(logs_dir="../logs"):
    """
    Get only the average values from timing logs.
    
    Args:
        logs_dir (str): Path to the logs directory (default: "../logs")
    
    Returns:
        dict: Dictionary with filename as key and average timing as value
    """
    return analyze_timing_logs(logs_dir)

def get_request_message_mapping():
    """
    Get the mapping of request types to their corresponding messages.
    
    Returns:
        dict: Mapping of request types to message lists
    """
    return REQUEST_TO_MESSAGE_MAPPING

def analyze_request_breakdown(logs_dir="../logs"):
    """
    Analyze timing data according to REQUEST_TO_MESSAGE_MAPPING.
    For each request type, return 3 numbers:
    - request avg: average of request log file (frontend_X_request.txt)
    - serialization sum: sum of all serialization averages (Se.txt files)
    - deserialization sum: sum of all deserialization averages (De.txt files)
    
    Args:
        logs_dir (str): Path to the logs directory (default: "../logs")
    
    Returns:
        dict: Dictionary with request type as key and (request_avg, ser_sum, de_sum) as value
    """
    results = analyze_timing_logs(logs_dir)
    breakdown = {}
    
    for request_type, messages in REQUEST_TO_MESSAGE_MAPPING.items():
        # Get request timing (frontend_X_request.txt)
        request_file = f"frontend_{request_type}_request.txt"
        request_avg = results.get(request_file, 0)
        
        # Get serialization and deserialization sums
        ser_sum = 0
        de_sum = 0
        
        for message_type in messages:
            # Find files that contain the message type string
            for filename, avg_time in results.items():
                if message_type in filename:
                    if filename.endswith('Se.txt'):
                        ser_sum += avg_time
                    elif filename.endswith('De.txt'):
                        de_sum += avg_time
        
        breakdown[request_type] = (request_avg, ser_sum, de_sum)
    
    return breakdown

def analyze_service_breakdown(logs_dir="../logs"):
    """
    Analyze timing data according to SERVICE_TO_MESSAGE_MAPPING.
    For each service, return 3 numbers:
    - request avg: average of request log file (frontend_X_request.txt)
    - serialization sum: sum of all serialization averages (service_X_Se.txt files)
    - deserialization sum: sum of all deserialization averages (service_X_De.txt files)
    """
    results = analyze_timing_logs(logs_dir)
    breakdown = {}
    
    for service_type, messages in SERVICE_TO_MESSAGE_MAPPING.items():
        # Get request timing (frontend_X_request.txt)
        service_file = f"service_{service_type}_request.txt"
        service_avg = results.get(service_file, 0)
        
        # Get serialization and deserialization sums
        ser_sum = 0
        de_sum = 0
        
        for message_type in messages:
            # Find files that contain the message type string
            for filename, avg_time in results.items():
                if message_type in filename:
                    if filename.endswith('Se.txt'):
                        ser_sum += avg_time
                    elif filename.endswith('De.txt'):
                        de_sum += avg_time
        
        breakdown[service_type] = (service_avg, ser_sum, de_sum)
    
    return breakdown

if __name__ == "__main__":
    # Example usage
    results = analyze_timing_logs()
    
    if results:
        print("Individual message averages:")
        for filename, avg in results.items():
            print(f"{filename}: {avg:.2f} ns ({avg/1000000:.2f} ms)")
        
        # print("\n" + "="*50)
        # print("Messages by request type:")
        # for request_type, messages in REQUEST_TO_MESSAGE_MAPPING.items():
        #     print(f"{request_type}: {messages}")
        
        # print("\n" + "="*50)
        # print("Request breakdown analysis:")
        # breakdown = analyze_request_breakdown()
        # for request_type, data in breakdown.items():
        #     print(f"\n{request_type.upper()} request:")
        #     print(f"  Request avg: {data[0]:.2f} ns ({data[0]/1000000:.2f} ms)")
        #     print(f"  Serialization sum: {data[1]:.2f} ns ({data[1]/1000000:.2f} ms)")
        #     print(f"  Deserialization sum: {data[2]:.2f} ns ({data[2]/1000000:.2f} ms)")

        print("\n" + "="*50)
        print("Service breakdown analysis:")
        breakdown = analyze_service_breakdown()
        for service_type, data in breakdown.items():
            print(f"\n{service_type.upper()} service:")
            print(f"  Service avg: {data[0]:.2f} ns ({data[0]/1000000:.2f} ms)")
            print(f"  Serialization sum: {data[1]:.2f} ns ({data[1]/1000000:.2f} ms)")
            print(f"  Deserialization sum: {data[2]:.2f} ns ({data[2]/1000000:.2f} ms)")
    else:
        print("No timing data found.")

