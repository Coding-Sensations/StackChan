"""Bounded mono PCM16LE wrappers around installed public Opus/FFmpeg APIs.
State is connection-local. No audio device or network access occurs here.
"""
import ctypes as C
import ctypes.util
import os
import sys

P=C.c_void_p
U8=C.POINTER(C.c_ubyte)
I16=C.POINTER(C.c_int16)


def load_library(name):
    """Resolve installed libraries without assuming one macOS architecture/prefix."""
    option = 'STACKCHAN_' + name.upper() + '_LIBRARY'
    override = os.environ.get(option)
    if override is not None:
        if not os.path.isabs(override):
            raise OSError(option + ' must name an absolute library path')
        try:
            return C.CDLL(override)
        except OSError as error:
            raise OSError('Cannot load ' + option + ': ' + str(error)) from error
    discovered = ctypes.util.find_library(name)
    candidates = [discovered] if discovered else []
    if sys.platform == 'darwin':
        candidates += [prefix + '/lib' + name + '.dylib'
                       for prefix in ('/opt/homebrew/lib', '/usr/local/lib')]
    else:
        candidates.append('lib' + name + '.so')
    errors = []
    for candidate in dict.fromkeys(candidates):
        try:
            return C.CDLL(candidate)
        except OSError as error:
            errors.append(str(error))
    raise OSError('Cannot load ' + name + '; set ' + option +
                  ' to its absolute installed library path: ' + '; '.join(errors))


def bind(lib,name,result,args):
    f=getattr(lib,name);f.restype=result;f.argtypes=args;return f


class OpusCodec:
    def __init__(self,rate):
        if sys.byteorder!='little' or rate not in (16000,24000,48000):raise ValueError('unsupported PCM format')
        self.rate=rate;self.encoder=self.decoder=None
        self.lib=load_library('opus')
        self.create_encoder=bind(self.lib,'opus_encoder_create',P,[C.c_int,C.c_int,C.c_int,C.POINTER(C.c_int)])
        self.create_decoder=bind(self.lib,'opus_decoder_create',P,[C.c_int,C.c_int,C.POINTER(C.c_int)])
        self.enc=bind(self.lib,'opus_encode',C.c_int,[P,I16,C.c_int,U8,C.c_int])
        self.dec=bind(self.lib,'opus_decode',C.c_int,[P,U8,C.c_int,I16,C.c_int,C.c_int])
        self.destroy_encoder=bind(self.lib,'opus_encoder_destroy',None,[P])
        self.destroy_decoder=bind(self.lib,'opus_decoder_destroy',None,[P])
        self.encoder_ctl=bind(self.lib,'opus_encoder_ctl',C.c_int,[P,C.c_int])
        self.decoder_ctl=bind(self.lib,'opus_decoder_ctl',C.c_int,[P,C.c_int])
        error=C.c_int();self.encoder=self.create_encoder(rate,1,2049,C.byref(error))
        if not self.encoder or error.value:raise RuntimeError('Opus encoder creation failed')
        self.decoder=self.create_decoder(rate,1,C.byref(error))
        if not self.decoder or error.value:self.close();raise RuntimeError('Opus decoder creation failed')

    def _live(self):
        if not self.encoder or not self.decoder:raise RuntimeError('codec closed')

    def encode(self,pcm):
        self._live()
        if not isinstance(pcm,bytes) or len(pcm)%2 or len(pcm)//2 not in tuple(self.rate*n//400 for n in (1,2,4,8,16,24)):
            raise ValueError('PCM must contain one legal Opus frame (2.5..60ms)')
        samples=(C.c_int16*(len(pcm)//2)).from_buffer_copy(pcm);out=(C.c_ubyte*4096)()
        n=self.enc(self.encoder,samples,len(samples),out,len(out))
        if n<0:raise ValueError('Opus encode error '+str(n))
        return bytes(out[:n])

    def decode(self,packet):
        self._live()
        if not isinstance(packet,bytes) or not 1<=len(packet)<=4096:raise ValueError('packet size')
        data=(C.c_ubyte*len(packet)).from_buffer_copy(packet);out=(C.c_int16*(self.rate*120//1000))()
        n=self.dec(self.decoder,data,len(data),out,len(out),0)
        if n<0:raise ValueError('Opus decode error '+str(n))
        return C.string_at(out,n*2)

    def reset(self):
        self._live()
        # Opus RESET_STATE can retain automatically selected encoder mode.
        # A fresh session drops all prior encoding/decoding choices as well.
        rate=self.rate;self.close();self.__init__(rate)

    def close(self):
        if self.encoder:self.destroy_encoder(self.encoder);self.encoder=None
        if self.decoder:self.destroy_decoder(self.decoder);self.decoder=None

    def __enter__(self):return self
    def __exit__(self,*args):self.close()


class Layout(C.Structure):
    _fields_=[('order',C.c_int),('nb_channels',C.c_int),('mask',C.c_uint64),('opaque',P)]


class Resampler:
    def __init__(self,input_rate,output_rate):
        if sys.byteorder!='little' or input_rate not in (16000,24000,48000) or output_rate not in (16000,24000,48000):raise ValueError('rate')
        self.input_rate=input_rate;self.output_rate=output_rate;self.ctx=P()
        self.lib=load_library('swresample');self.av=load_library('avutil')
        self.layout_default=bind(self.av,'av_channel_layout_default',None,[C.POINTER(Layout),C.c_int])
        self.alloc=bind(self.lib,'swr_alloc_set_opts2',C.c_int,[C.POINTER(P),C.POINTER(Layout),C.c_int,C.c_int,C.POINTER(Layout),C.c_int,C.c_int,C.c_int,P])
        self.init=bind(self.lib,'swr_init',C.c_int,[P])
        self.bound=bind(self.lib,'swr_get_out_samples',C.c_int,[P,C.c_int])
        self.convert=bind(self.lib,'swr_convert',C.c_int,[P,C.POINTER(U8),C.c_int,C.POINTER(U8),C.c_int])
        self.free=bind(self.lib,'swr_free',None,[C.POINTER(P)])
        self._new()

    def _new(self):
        layout=Layout();self.layout_default(C.byref(layout),1)
        if self.alloc(C.byref(self.ctx),C.byref(layout),1,self.output_rate,C.byref(layout),1,self.input_rate,0,None)<0 or self.init(self.ctx)<0:
            self.close();raise RuntimeError('resampler creation failed')

    def _convert(self,pcm,flush=False):
        if not self.ctx:raise RuntimeError('resampler closed')
        count=len(pcm)//2;capacity=self.bound(self.ctx,count)
        if not 0<=capacity<=self.output_rate:raise RuntimeError('resampler output bound')
        out=(C.c_ubyte*max(2,capacity*2))();outputs=(U8*1)(C.cast(out,U8))
        data=(C.c_ubyte*len(pcm)).from_buffer_copy(pcm) if pcm else None
        inputs=(U8*1)(C.cast(data,U8)) if data is not None else None
        n=self.convert(self.ctx,outputs,capacity,None if flush else inputs,count)
        if n<0:raise ValueError('resampler error '+str(n))
        return bytes(out[:n*2])

    def process(self,pcm):
        if not isinstance(pcm,bytes) or not len(pcm) or len(pcm)%2 or len(pcm)>self.input_rate*120//1000*2:raise ValueError('PCM bound')
        return self._convert(pcm)

    def flush(self):return self._convert(b'',True)
    def reset(self):
        if not self.ctx:raise RuntimeError('resampler closed')
        self.close();self._new()
    def close(self):
        if self.ctx:self.free(C.byref(self.ctx))
    def __enter__(self):return self
    def __exit__(self,*args):self.close()
