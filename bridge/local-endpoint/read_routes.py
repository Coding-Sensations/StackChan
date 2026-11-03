"""Read public CoreAudio device properties only. Never starts IO/sets properties."""
import ctypes as C,json
P=C.c_void_p
class Address(C.Structure):_fields_=[('selector',C.c_uint32),('scope',C.c_uint32),('element',C.c_uint32)]
def four(s):return int.from_bytes(s.encode(),'big')
ca=C.CDLL('/System/Library/Frameworks/CoreAudio.framework/CoreAudio')
cf=C.CDLL('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
size=ca.AudioObjectGetPropertyDataSize;size.argtypes=[C.c_uint32,C.POINTER(Address),C.c_uint32,P,C.POINTER(C.c_uint32)];size.restype=C.c_int32
get=ca.AudioObjectGetPropertyData;get.argtypes=[C.c_uint32,C.POINTER(Address),C.c_uint32,P,C.POINTER(C.c_uint32),P];get.restype=C.c_int32
cf.CFStringGetCString.argtypes=[P,P,C.c_long,C.c_uint32];cf.CFStringGetCString.restype=C.c_bool
cf.CFRelease.argtypes=[P];cf.CFRelease.restype=None

def prop(obj,selector,scope='glob'):
    a=Address(four(selector),four(scope),0);n=C.c_uint32()
    if size(obj,C.byref(a),0,None,C.byref(n)) or n.value>65536:return None
    buf=(C.c_ubyte*n.value)()
    if get(obj,C.byref(a),0,None,C.byref(n),buf):return None
    return bytes(buf[:n.value])
def string(obj,key):
    raw=prop(obj,key)
    if not raw or len(raw)!=C.sizeof(P):return None
    ref=P(int.from_bytes(raw,'little'));out=C.create_string_buffer(1024)
    try:return out.value.decode() if cf.CFStringGetCString(ref,out,len(out),0x08000100) else None
    finally:cf.CFRelease(ref)
def uint(obj,key,scope='glob'):
    raw=prop(obj,key,scope);return int.from_bytes(raw,'little') if raw and len(raw)==4 else None

def main():
    devices=prop(1,'dev#');assert devices and len(devices)%4==0
    result=[]
    for i in range(0,len(devices),4):
        device=int.from_bytes(devices[i:i+4],'little');name=string(device,'lnam')
        if name not in ('Pro Tools Audio Bridge 2-A','Pro Tools Audio Bridge 2-B'):continue
        rate=prop(device,'nsrt')
        item={'name':name,'uid':string(device,'uid '),'nominal_rate':C.c_double.from_buffer_copy(rate).value if rate and len(rate)==8 else None,'alive':uint(device,'livn'),'buffer_frames':uint(device,'fsiz'),'scopes':{}}
        for scope in ('inpt','outp'):
            streams=prop(device,'stm#',scope)
            item['scopes'][scope]={'stream_count':len(streams)//4 if streams is not None else None,'latency_frames':uint(device,'ltnc',scope),'safety_offset_frames':uint(device,'saft',scope)}
        result.append(item)
    print(json.dumps({'read_only':True,'devices':result,'routing_verified':False,'IO_started':False,'properties_changed':False},indent=2))
if __name__=='__main__':main()
