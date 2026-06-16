import sys
import os
import struct

FNV_OFFSET = 0x811c9dc5
FNV_PRIME = 0x01000193

def fnv1a_32(symbol_str: str) -> int:
    h = FNV_OFFSET
    clean_str = symbol_str.encode('ascii', 'ignore')
    for char in clean_str:
        h ^= char
        h = (h * FNV_PRIME) & 0xFFFFFFFF
    return h

def generate_hash_map(input_path: str):
    dir_name = os.path.dirname(input_path)
    output_path = os.path.join(dir_name, "khash.map")
    
    symbols = []
    
    print(f"[*] Parsing {input_path}...")
    with open(input_path, 'r', encoding='utf-8-sig', errors='ignore') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
                
            parts = line.split()
            if len(parts) < 3:
                continue
                
            addr_str, sym_type, sym_name = parts[0], parts[1], parts[2]

            if sym_type.upper() == 'A':
                continue
            
            try:
                addr = int(addr_str, 16)
                sym_hash = fnv1a_32(sym_name)
                symbols.append((sym_hash, addr))
            except ValueError:
                continue

    symbols.sort(key=lambda x: x[0])
    num_symbols = len(symbols)
    
    print(f"[*] Writing {num_symbols} symbols to binary map...")

    with open(output_path, 'wb') as out:
        out.write(struct.pack("<Q", num_symbols))
        
        for sym_hash, addr in symbols:
            out.write(struct.pack("<IIQ", sym_hash, 0, addr))

    print("[+] Complete!")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(1)
    generate_hash_map(sys.argv[1])