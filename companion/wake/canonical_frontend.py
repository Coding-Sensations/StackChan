"""Bounded offline/runtime feature contract for the frozen literal trial.
Public pymicro-features interface only; no model load/invoke, audio IO or activation.
"""
import hashlib
import importlib.metadata
from pathlib import Path
import numpy as np
import micro_features_cpp
from pymicro_features import MicroFrontend

BINDING_SHA256='c3d25353f132f26f8468426ec2d6e5c1924e4b9173240ef228d12109369861ad'
MODEL_SHA256='801a98307fb6c2b1a568de83f078a61bd1885e1a2c0aa34d621a910fa1681f77'
RATE=16000
WINDOW_SAMPLES=480
STEP_SAMPLES=160
FEATURES=40
MODEL_STRIDE=3
MAX_FEED_SAMPLES=512
AVAILABLE=False
RUNTIME_ENABLED=False

def expected_frame_count(samples):
    if isinstance(samples,bool) or not isinstance(samples,int) or samples<0:raise ValueError('sample count')
    return max(0,1+(samples-WINDOW_SAMPLES)//STEP_SAMPLES)

def quantize_raw(raw):
    """Exact saturated model-specific affine map: raw*(5/128)/(26/255)-128.
    Positive nearest rounding; no unsaturated half ties occur for uint16 range.
    """
    array=np.asarray(raw)
    if array.dtype!=np.uint16:raise ValueError('raw frontend features must be uint16')
    integer=((array.astype(np.uint32)*1275+1664)//3328).astype(np.int32)-128
    return np.clip(integer,-128,127).astype(np.int8)

def scaled_to_raw(scaled):
    array=np.asarray(scaled,dtype=np.float32)
    if not np.isfinite(array).all() or np.any(array<0) or np.any(array>65535*(5/128)):raise ValueError('invalid scaled feature')
    recovered=np.rint(array.astype(np.float64)*25.6).astype(np.uint16)
    if not np.array_equal(array,recovered.astype(np.float32)*np.float32(5/128)):raise ValueError('feature does not lie on canonical raw grid')
    return recovered

class CanonicalFrontend:
    """Retains native stream state across calls; emits every complete feature.
    Caller must reset on audio-owner revocation, discontinuity or generation change.
    Never emits padded/partial EOF frames. This class is not a consent/runtime gate.
    """
    def __init__(self):
        if importlib.metadata.version('pymicro-features')!='2.0.2':raise RuntimeError('unqualified binding version')
        if hashlib.sha256(Path(micro_features_cpp.__file__).read_bytes()).hexdigest()!=BINDING_SHA256:raise RuntimeError('unqualified binding bytes')
        self._native=MicroFrontend()
        self.samples=0;self.frames=0;self._pending=np.empty(0,dtype=np.int16)
    def reset(self):
        if hasattr(self,'_pending'):self._pending.fill(0)
        self._native.reset();self.samples=0;self.frames=0;self._pending=np.empty(0,dtype=np.int16)
    def feed(self,pcm):
        array=np.asarray(pcm)
        if array.dtype!=np.int16 or array.ndim!=1 or len(array)>MAX_FEED_SAMPLES:
            self.reset();raise ValueError('bounded signed16mono samples required')
        pending=np.concatenate((self._pending,array))
        self._pending.fill(0)
        rows=[];consumed=0
        try:
            # Pinned public C binding requires at least160 samples per call.
            # Retain <160 trailing samples; they cannot complete another frame.
            while len(pending)-consumed>=STEP_SAMPLES:
                block=pending[consumed:consumed+STEP_SAMPLES]
                output=self._native.process_samples(np.ascontiguousarray(block,dtype='<i2').tobytes())
                read=output.samples_read
                if isinstance(read,bool) or not isinstance(read,int) or read!=STEP_SAMPLES:raise RuntimeError('native progress invalid')
                consumed+=read
                if output.features:
                    if len(output.features)!=FEATURES:raise RuntimeError('native feature width mismatch')
                    rows.append(scaled_to_raw(np.asarray(output.features,dtype=np.float32)))
            self._pending=pending[consumed:].copy()
            self.samples+=len(array);self.frames+=len(rows)
            if self.frames!=expected_frame_count(self.samples):raise RuntimeError('frame count mismatch')
            return np.asarray(rows,dtype=np.uint16).reshape(-1,FEATURES)
        except Exception:
            self.reset();raise
        finally:
            pending.fill(0)

def complete_clip_features(pcm,chunk_samples=160):
    array=np.asarray(pcm)
    if array.dtype!=np.int16 or array.ndim!=1:raise ValueError('clip format')
    if isinstance(chunk_samples,bool) or not isinstance(chunk_samples,int) or not 1<=chunk_samples<=MAX_FEED_SAMPLES:raise ValueError('chunk bound')
    frontend=CanonicalFrontend();rows=[]
    for offset in range(0,len(array),chunk_samples):
        rows.extend(frontend.feed(array[offset:offset+chunk_samples]))
    return np.asarray(rows,dtype=np.uint16).reshape(-1,FEATURES)

class StrideAssembler:
    def __init__(self):self.reset()
    def reset(self):
        if hasattr(self,'_rows'):self._rows.fill(0)
        self._rows=np.zeros((MODEL_STRIDE,FEATURES),dtype=np.int8);self._used=0
    def push(self,raw):
        array=np.asarray(raw)
        if array.dtype!=np.uint16 or array.shape!=(FEATURES,):self.reset();raise ValueError('one raw feature frame required')
        self._rows[self._used]=quantize_raw(array);self._used+=1
        if self._used<MODEL_STRIDE:return None
        result=self._rows.copy().reshape(1,MODEL_STRIDE,FEATURES);self.reset();return result

def validate_model_contract(contract,model_bytes):
    """Reject another model/metadata before model-specific quantization is selected."""
    expected={'model_sha256':MODEL_SHA256,'input_shape':[1,3,40],'input_type':'int8','input_scale_float32':float(np.float32(26/255)),'input_zero_point':-128}
    if hashlib.sha256(model_bytes).hexdigest()!=MODEL_SHA256:raise ValueError('model bytes mismatch')
    if any(contract.get(key)!=value for key,value in expected.items()):raise ValueError('model metadata mismatch')
    return True
