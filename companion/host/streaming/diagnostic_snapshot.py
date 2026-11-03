"""SCFD v1 public firmware diagnostics. Import performs no I/O.

Mirrors companion/diagnostics.h: fixed 176-byte, big-endian fields, CRC32
IEEE over the first 172 bytes. Times are raw modulo-32-bit milliseconds;
stack high water is SDK bytes (zero means unsampled). No console/text fields.
"""
import struct
import zlib

MAGIC=b'SCFD'
FRAME_SIZE=176
NO_FAULT=0xffffffff
STAGES=frozenset(range(36))
FAULTS=frozenset(range(1,27))
class SnapshotError(ValueError):pass

def _fault(words,start):
    task,reason,detail,timestamp=words[start:start+4]
    if task==NO_FAULT:
        if reason or detail or timestamp:raise SnapshotError('invalid_snapshot')
        return None
    if task not in range(4) or reason not in FAULTS:raise SnapshotError('invalid_snapshot')
    signed=detail if detail<0x80000000 else detail-0x100000000
    return dict(task=task,reason=reason,detail=signed,time_ms=timestamp)
def _tasks(words,start):
    result=[]
    for task in range(4):
        stage,timestamp,highwater=words[start+task*3:start+task*3+3]
        if stage not in STAGES:raise SnapshotError('invalid_snapshot')
        result.append(dict(task=task,stage=stage,time_ms=timestamp,stack_highwater_bytes=highwater))
    return result

def decode_snapshot(frame):
    if type(frame) is not bytes or len(frame)!=FRAME_SIZE:raise SnapshotError('invalid_snapshot')
    magic,version,flags,length=struct.unpack_from('>4sBBH',frame)
    if magic!=MAGIC or version!=1 or flags not in (0,1,2,3) or length!=FRAME_SIZE:raise SnapshotError('invalid_snapshot')
    if zlib.crc32(frame[:-4])!=struct.unpack_from('>I',frame,172)[0]:raise SnapshotError('invalid_snapshot')
    words=struct.unpack_from('>41I',frame,8)
    if not words[0] or words[1]>64:raise SnapshotError('invalid_snapshot')
    result=dict(version=1,back_observed=bool(flags&2),boot_nonce=words[0],reset_reason=words[1],uptime_ms=words[2],tick_count=words[3],heartbeat_success=words[4],heartbeat_failure=words[5],dropped_updates=words[6],first_fault=_fault(words,7),tasks=_tasks(words,11),prior=None)
    if flags&1:
        if words[1] in (0,1,9) or not words[23] or words[23]==words[0] or words[24]>64:raise SnapshotError('invalid_snapshot')
        result['prior']=dict(boot_nonce=words[23],reset_reason=words[24],first_fault=_fault(words,25),tasks=_tasks(words,29))
    elif tuple(words[23:])!=(0,0,NO_FAULT,0,0,0,*([0]*12)):
        raise SnapshotError('invalid_snapshot')
    return result
