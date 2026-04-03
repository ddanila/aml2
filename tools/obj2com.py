#!/usr/bin/env python3
"""Convert a single-segment OMF .OBJ file to a .COM file."""
import sys


def obj2com(objpath, compath):
    with open(objpath, "rb") as f:
        data = f.read()

    segments = {}
    pos = 0
    while pos < len(data):
        rectype = data[pos]
        reclen = data[pos + 1] | (data[pos + 2] << 8)
        recdata = data[pos + 3 : pos + 3 + reclen - 1]

        if rectype == 0xA2:
            seg_idx = recdata[0]
            offset = recdata[1] | (recdata[2] << 8)
            if seg_idx not in segments:
                segments[seg_idx] = {}

            def parse_lidata(d, p):
                repeat = d[p] | (d[p + 1] << 8)
                block_count = d[p + 2] | (d[p + 3] << 8)
                p += 4
                if block_count == 0:
                    data_len = d[p]
                    p += 1
                    chunk = bytes(d[p : p + data_len])
                    p += data_len
                    return chunk * repeat, p
                one_iter = bytearray()
                for _ in range(block_count):
                    sub, p = parse_lidata(d, p)
                    one_iter.extend(sub)
                return bytes(one_iter) * repeat, p

            li_pos = 3
            while li_pos < len(recdata):
                expanded, li_pos = parse_lidata(recdata, li_pos)
                for i, b in enumerate(expanded):
                    segments[seg_idx][offset + i] = b
                offset += len(expanded)

        elif rectype == 0xA0:
            seg_idx = recdata[0]
            offset = recdata[1] | (recdata[2] << 8)
            payload = recdata[3:]
            if seg_idx not in segments:
                segments[seg_idx] = {}
            for i, b in enumerate(payload):
                segments[seg_idx][offset + i] = b
        elif rectype == 0xA1:
            seg_idx = recdata[0]
            offset = (
                recdata[1]
                | (recdata[2] << 8)
                | (recdata[3] << 16)
                | (recdata[4] << 24)
            )
            payload = recdata[5:]
            if seg_idx not in segments:
                segments[seg_idx] = {}
            for i, b in enumerate(payload):
                segments[seg_idx][offset + i] = b

        pos += 3 + reclen

    if not segments:
        print("No LEDATA records found", file=sys.stderr)
        sys.exit(1)

    seg = segments[min(segments.keys())]
    min_off = min(seg.keys())
    max_off = max(seg.keys())

    size = max_off - min_off + 1
    binary = bytearray(size)
    for off, b in seg.items():
        binary[off - min_off] = b

    with open(compath, "wb") as f:
        f.write(binary)

    print(f"Written {size} bytes to {compath} (offset range {min_off:#06x}-{max_off:#06x})")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.obj output.com", file=sys.stderr)
        sys.exit(1)
    obj2com(sys.argv[1], sys.argv[2])
