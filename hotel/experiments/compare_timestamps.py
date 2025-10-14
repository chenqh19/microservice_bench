import re

def parse_latency_file(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    section = None
    results = {'serialization': {}, 'deserialization': {}}
    current = None
    for line in lines:
        if 'Serialization Latencies' in line:
            section = 'serialization'
        elif 'Deserialization Latencies' in line:
            section = 'deserialization'
        m = re.match(r'^(N\d+hotelreservation\w+E):', line)
        if m:
            current = m.group(1)
        m = re.match(r'\s*Average: ([\d.]+) ns', line)
        if m and current and section:
            results[section][current] = float(m.group(1))
    return results

def compare_latencies(proto, ser1de):
    print(f"{'Type':<40} {'Proto(ns)':>12} {'Ser1de(ns)':>12} {'Speedup':>10}")
    print('-'*76)
    all_types = set(proto.keys()) | set(ser1de.keys())
    for t in sorted(all_types):
        p = proto.get(t)
        s = ser1de.get(t)
        if p is not None and s is not None:
            speedup = s/p
            print(f"{t:<40} {p:12.2f} {s:12.2f} {speedup:10.2f}")
        else:
            print(f"{t:<40} {str(p):>12} {str(s):>12} {'N/A':>10}")

def main():
    proto = parse_latency_file('latency_summary_protobuf.txt')
    ser1de = parse_latency_file('latency_summary_ser1de.txt')
    print('Serialization:')
    compare_latencies(proto['serialization'], ser1de['serialization'])
    print('\nDeserialization:')
    compare_latencies(proto['deserialization'], ser1de['deserialization'])

if __name__ == '__main__':
    main()
