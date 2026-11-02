import struct,zlib
from pathlib import Path
w,h=1920,1080
rows=[]
for y in range(h):
    left=(255,0,0) if y<h//2 else (0,0,255)
    right=(0,255,0) if y<h//2 else (255,255,0)
    row=bytes(left)*(w//2)+bytes(right)*(w//2)
    rows.append(b'\0'+row)
def chunk(kind,data):
    return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b'')
Path('tests/assets/corners-1920x1080.png').write_bytes(data)
