#!/usr/bin/env python3
# writes, per level, the texel rects of the textures the rooms use (tile, x0, y0, x1, y1) as json
import struct,sys,json,glob,os
out={}
for path in sorted(glob.glob(sys.argv[1]+'/*.PHD')):
    d=open(path,'rb').read(); o=0
    def u16():
        global o; v=struct.unpack_from('<H',d,o)[0]; o+=2; return v
    def u32():
        global o; v=struct.unpack_from('<I',d,o)[0]; o+=4; return v
    u32(); nt=u32(); o+=nt*65536; u32()
    used=set()
    for r in range(u16()):
        o+=16; n=u32(); start=o; end=o+n*2
        nv=u16(); o+=nv*8
        for k in range(u16()):
            used.add(struct.unpack_from('<5H',d,o)[4]&0x7FFF); o+=10
        for k in range(u16()):
            used.add(struct.unpack_from('<4H',d,o)[3]&0x7FFF); o+=8
        o=end
        n=u16(); o+=n*32; z=u16(); x=u16(); o+=z*x*8; o+=2; n=u16(); o+=n*18; n=u16(); o+=n*18; o+=4
    for sz in (2,2,4,32,6,8,2,4,2,18,32):
        n=u32(); o+=n*sz
    n=u32(); ot=[]
    for i in range(n):
        a,t=struct.unpack_from('<HH',d,o); c=struct.unpack_from('<16B',d,o+4); o+=20
        pts=[(c[k*4+1],c[k*4+3]) for k in range(4) if c[k*4+1] or c[k*4+3] or k<3]
        ot.append((t,pts))
    for sz in (16,8,16,16):
        n=u32(); o+=n*sz
    nb=u32(); o+=nb*20; n=u32(); o+=n*2; o+=nb*12
    n=u32(); w=struct.unpack_from('<%dH'%n,d,o); k=1
    for r in range(w[0]):
        c=w[k]+1; used.update(w[k+1:k+1+c]); k+=1+c
    rects=[]
    for i in sorted(used):
        if i>=len(ot): continue
        t,pts=ot[i]; xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
        rects.append((t,min(xs),min(ys),max(xs)+1,max(ys)+1))
    out[os.path.basename(path)[:-4]]=rects
    print(os.path.basename(path), nt, len(used), len(ot))
json.dump(out,open(sys.argv[2],'w'))
