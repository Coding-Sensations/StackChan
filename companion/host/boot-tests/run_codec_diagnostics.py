#!/usr/bin/env python3
"""Finite offline codec diagnostics; no services, credentials or device IO.
Host Opus is real. The firmware worker is real. Xtensa decoder/I2S are doubles.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,sys,tempfile,wave
repo=Path(__file__).resolve().parents[3]
project=repo.parents[1]
tests=Path(__file__).resolve().parent
sys.dont_write_bytecode=True
sys.path[:0]=[str(project/'bridge/local-endpoint'),str(repo/'companion/host/streaming')]
from codec import OpusCodec
from streaming_session import encode_frame
cue=project/'.tools/native-call-preparation/transport-speaker-cue.wav'
assert hashlib.sha256(cue.read_bytes()).hexdigest()=='8c70a511b8e9dc570c277e3286343ed52077315158485a106c494557364d22b9'
with wave.open(str(cue),'rb') as w:
    assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(1,2,24000,118844)
    pcm=w.readframes(w.getnframes())
packets=[]
with OpusCodec(24000) as codec:
    for start in range(0,len(pcm),2880):
        packet=codec.encode(pcm[start:start+2880].ljust(2880,b'\0'))
        decoded=codec.decode(packet)
        assert len(decoded)==2880
        packets.append(packet)
assert len(packets)==83
with tempfile.TemporaryDirectory(prefix='stackchan-codec-diagnostics-') as tmp:
    tmp=Path(tmp);framed=tmp/'cue.scav'
    with framed.open('wb') as f:
        for seq,packet in enumerate(packets):
            wire=encode_frame(1,seq,True,packet);f.write(struct.pack('>I',len(wire))+wire)
    executable=tmp/'worker'
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(tests/'codec-fixture'),'-I'+str(repo/'firmware/main'),str(tests/'test_codec_faults.cpp'),str(repo/'firmware/main/companion/codec_tasks.cc'),'-o',str(executable)],check=True)
    scratch=tmp/'scratch'
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(tests/'codec-fixture'),'-I'+str(repo/'firmware/main'),str(tests/'test_codec_scratch.cpp'),str(repo/'firmware/main/companion/codec_tasks.cc'),'-o',str(scratch)],check=True)
    subprocess.run([str(scratch)],check=True,timeout=10)
    backpressure=tmp/'backpressure'
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DCODEC_FIXTURE_REAL_TASKS','-I'+str(tests/'codec-fixture'),'-I'+str(repo/'firmware/main'),str(tests/'test_codec_backpressure.cpp'),str(repo/'firmware/main/companion/codec_tasks.cc'),'-o',str(backpressure)],check=True)
    for mode in ['delayed_consumer','timeout','interrupt','stop','epoch_change','late_resume']:
        subprocess.run([str(backpressure),mode],check=True,timeout=10)
    for mode in ['decode_error','short_pcm','partial_input','overflow','encoder_open','decoder_open','resampler_open','encode_error','sequence_gap']:
        subprocess.run([str(executable),mode],check=True,timeout=10)
    subprocess.run([str(executable),'cue',str(framed)],check=True,timeout=10)
print(json.dumps(dict(result='passed',cue_frames=118844,cue_seconds=118844/24000,opus_packets=83,all_host_decoded_bytes=2880,worker_actual=True,host_opus_actual=True,xtensa_decoder_actual=False,codec_driver_actual=False,hardware=False,network=False)))
