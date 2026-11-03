"""Read actual image/table/compile metadata; never builds, connects or flashes."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import struct
from build_profile import resolve, parse_selection


def measure(image, partitions, baseline=None, min_spare=0):
    slots=[];checksum=False;end=False
    if len(partitions)%32 or not image or min_spare<0:
        raise ValueError('Empty image or invalid partition/threshold input')
    for offset in range(0,len(partitions),32):
        entry=partitions[offset:offset+32]
        magic=int.from_bytes(entry[:2],'little')
        if magic==0xebeb:
            if checksum or entry[2:16]!=b'\xff'*14 or entry[16:]!=hashlib.md5(partitions[:offset]).digest():
                raise ValueError('Partition checksum mismatch')
            checksum=True;continue
        if entry==b'\xff'*32:
            end=True;continue
        if checksum or end or magic!=0x50aa:
            raise ValueError('Invalid partition table entry')
        _,kind,subtype,start,size,_,flags=struct.unpack('<HBBII16sI',entry)
        if kind==0 and 0x10<=subtype<=0x1f:
            if not size or start%0x10000 or flags&~1:
                raise ValueError('Invalid OTA application geometry')
            slots.append({'subtype':subtype,'offset':start,'bytes':size})
    if not checksum or not end or len(slots)<2:
        raise ValueError('Checksummed table with two preserved OTA slots required')
    if len({slot['subtype'] for slot in slots})!=len(slots):
        raise ValueError('Duplicate OTA subtype')
    ordered=sorted(slots,key=lambda slot:slot['offset'])
    if any(a['offset']+a['bytes']>b['offset'] for a,b in zip(ordered,ordered[1:])):
        raise ValueError('Overlapping OTA slots')
    size=min(slot['bytes'] for slot in slots);spare=size-len(image)
    if spare<min_spare:
        raise ValueError('Image exceeds slot or requested minimum spare')
    return {'image_bytes':len(image),'image_sha256':hashlib.sha256(image).hexdigest(),
            'slot_bytes':size,'ota_slots':slots,'spare_bytes':spare,
            'spare_percent':round(100*spare/size,3),
            'under_ten_percent_spare':spare*10<size,
            'delta_from_baseline_bytes':len(image)-len(baseline) if baseline is not None else None,
            'firmware_boot_verified':False,
            'scope':'byte measurement only; image validity, runtime heap and physical behavior require separate qualification'}


def audit_commands(selection, entries):
    excluded=selection['excluded_sources'];definitions=selection['definitions'];seen=set();count=0
    for entry in entries:
        file=Path(entry['file']).as_posix()
        for path in excluded:
            if file.endswith('/main/'+path):
                raise ValueError('Excluded source remains in compile graph: '+path)
        args=entry.get('arguments') or shlex.split(entry['command'])
        output=entry.get('output')
        if output is None and '-o' in args:
            index=args.index('-o')
            if index+1<len(args):output=args[index+1]
        # Generated main-directory files also belong to other IDF components
        # (e.g. the adapted Wi-Fi manager). Definitions are PRIVATE to __idf_main.
        if not output or '/CMakeFiles/__idf_main.dir/' not in ('/'+str(output)):
            continue
        for key,value in definitions.items():
            actual=[arg for arg in args if arg.startswith('-D'+key+'=') or arg=='-D'+key]
            if actual!=['-D'+key+'='+str(value)]:
                raise ValueError('Missing, duplicate or stale profile compile definition: '+key)
        for required in ['apps/stock_shell/stock_apps.cc','companion/boot.cc']:
            if file.endswith('/main/'+required):seen.add(required)
        count+=1
    if len(seen)!=2:raise ValueError('Mandatory stock shell/Mira sources absent from compile graph')
    return count


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ['image','partitions','selection','compile-commands']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--baseline',type=Path)
    parser.add_argument('--min-spare-bytes',type=int,default=0)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        raw=args.selection.read_bytes();selection=parse_selection(raw.decode())
        expected=resolve(selection['profile'],
                         {'schema':1,'features':selection['enabled']} if selection['profile']=='custom' else None)
        if selection!=expected:raise ValueError('Resolved selection does not match the source-owned schema')
        result=measure(args.image.read_bytes(),args.partitions.read_bytes(),
                       args.baseline.read_bytes() if args.baseline else None,args.min_spare_bytes)
        result['audited_main_compile_entries']=audit_commands(selection,json.loads(args.compile_commands.read_text()))
        result['selection_sha256']=hashlib.sha256(raw).hexdigest()
        result['profile']=selection['profile'];result['enabled']=selection['enabled']
    except (OSError,ValueError,KeyError,TypeError) as error:
        parser.exit(2,'Size/profile report rejected: '+str(error)+'\n')
    args.output.write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
    print(f"Image {result['image_bytes']} bytes; spare {result['spare_bytes']} bytes ({result['spare_percent']}%)")


if __name__=='__main__':main()
