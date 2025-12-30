import xml.etree.ElementTree as ET
from collections import defaultdict

def get_highway_attributes(file_path):
    try:
        tree = ET.parse(file_path)
        root = tree.getroot()
    except Exception as e:
        return f"Error parsing XML: {e}"

    # Dictionary to store unique combinations
    # Key: highway value, Value: set of (width, surface) tuples
    highway_data = defaultdict(set)
    highway_counts = defaultdict(int)

    # Iterate through all elements that have a 'highway' tag
    for element in root.findall(".//*[@k='highway']/.."):
        tags = {tag.get('k'): tag.get('v') for tag in element.findall('tag')}
        
        h_val = tags.get('highway')
        w_val = tags.get('width', 'N/A')
        s_val = tags.get('surface', 'N/A')
        
        highway_counts[h_val] += 1
        highway_data[h_val].add((w_val, s_val))

    # Display results
    print(f"{'Highway Value':<20} | {'Count':<8} | {'Width':<10} | {'Surface':<15}")
    print("-" * 60)
    for h_type in sorted(highway_data.keys()):
        count = highway_counts[h_type]
        for width, surface in sorted(highway_data[h_type]):
            print(f"{h_type:<20} | {count:<8} | {width:<10} | {surface:<15}")

if __name__ == "__main__":
    get_highway_attributes('/home/benjamin/Downloads/map.osm')
