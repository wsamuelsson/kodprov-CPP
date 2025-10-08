import sys
import struct
import matplotlib.pyplot as plt


OBJECT_FORMAT = '<qiiBBBB4x'
OBJECT_SIZE = struct.calcsize(OBJECT_FORMAT)

PREAMBLE_SIZE = 4
COUNT_SIZE = 4
EXPECTED_PREAMBLE = 0xFE00

def read_binary_data(stream):
    """Reads the preamble, count, and object data from the binary stream."""
    
    # Read Preamble (4 bytes)
    preamble_bytes = stream.read(PREAMBLE_SIZE)
    if not preamble_bytes or len(preamble_bytes) < PREAMBLE_SIZE:
        return 0, [] # End of stream or incomplete read

    # Unpack the uint32_t preamble (assuming little-endian '<')
    preamble = struct.unpack('<I', preamble_bytes)[0]
    
    # Basic check (minimal)
    if preamble != EXPECTED_PREAMBLE:
        print(f"Error: Invalid preamble {hex(preamble)}. Expected {hex(EXPECTED_PREAMBLE)}", file=sys.stderr)
        return 0, []

    # Read Count (4 bytes)
    count_bytes = stream.read(COUNT_SIZE)
    if not count_bytes or len(count_bytes) < COUNT_SIZE:
        return 0, []

    # Unpack the uint32_t count
    count = struct.unpack('<I', count_bytes)[0]
    
    objects = []
    
    # Read N Objects
    for _ in range(count):
        object_bytes = stream.read(OBJECT_SIZE)
        if len(object_bytes) < OBJECT_SIZE:
             print("Warning: Incomplete object read.", file=sys.stderr)
             break
        
        # Unpack the object data
        data = struct.unpack(OBJECT_FORMAT, object_bytes)
        
        
        raw_r = data[4] # 0x5B (91)
        raw_g = data[5] # Will be 49, 51, or 52
        raw_b = data[6] # 0x6D (109)

        COLOR_MAP = {
            49: (1.0, 0.0, 0.0),        #Pure red  
            
            51: (1.0, 1.0, 0.0),        # Pure Yellow 
            
            52: (0.0, 0.0, 1.0),        # Pure Blue
            
            # Fallback to the raw color if an unexpected 'g' value is found
            'default': (raw_r / 255.0, raw_g / 255.0, raw_b / 255.0)
        }

        # Determine the final plot color based on the G value
        final_color = COLOR_MAP.get(raw_g, COLOR_MAP['default'])

        objects.append({
            'id': data[0],
            'x': data[1],
            'y': data[2],
            'type': '^' if data[3] in {1, 2} else '1',
            'color': final_color # Use the mapped color
        })
    
    return count, objects

# --- Main Plotting Loop ---
def main():
    # Use sys.stdin.buffer to read raw binary bytes from the pipe
    binary_stream = sys.stdin.buffer
    
    plt.ion() # Turn on interactive mode for live plotting
    plt.style.use('dark_background')
    fig, ax = plt.subplots()

    while True:
        try:
            count, objects = read_binary_data(binary_stream)
            
            if count > 0:
                ax.clear()
                ax.scatter(150, 150, c='green', s=50, marker='D')
                for obj in objects:    
                    # Extract coordinates and colors
                    x_coord = obj['x'] 
                    y_coord = obj['y']
                    color = obj['color']
                    symbol = obj['type']
                    # Plot the new locations
                    ax.scatter(x_coord, y_coord, c=color, s=50, marker=symbol) 
                    
                    ax.set_title(f"Objects Received: {count}")
                    ax.set_xlim(0, 450) # Assuming some bounds for coordinates
                    ax.set_ylim(0, 450)
                    
                fig.canvas.draw()
                fig.canvas.flush_events()
                
        except EOFError:
            print("Middleware connection closed.")
            break
        except Exception as e:
            print(f"An error occurred: {e}", file=sys.stderr)
            break

if __name__ == '__main__':
    main()