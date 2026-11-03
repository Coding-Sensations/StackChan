"""Research-only missing-model contract. This is never a model loader or readiness gate."""
import json
from pathlib import Path

def check_missing_model(manifest):
    if manifest.get('schema_version') != 1 or manifest.get('phrase') != 'Hey Mira':
        raise ValueError('unexpected schema/phrase')
    for flag in ['available','runtime_enabled','network_capture_authorized']:
        if manifest.get(flag) is not False:
            raise ValueError(flag + ' must remain false')
    for field in ['model_file','model_sha256']:
        if field not in manifest or manifest[field] is not None:
            raise ValueError(field + ' must be explicitly absent')
    if manifest.get('required_pcm') != {'sample_rate_hz':16000,'channels':1,'signed_bits':16}:
        raise ValueError('PCM contract mismatch')
    if not isinstance(manifest.get('reason'),str) or not manifest['reason'].strip():
        raise ValueError('absence reason required')
    return {'contract_valid':True,'available':False,'runtime_enabled':False,'inference_executed':False}

if __name__ == '__main__':
    print(json.dumps(check_missing_model(json.loads(Path(__file__).with_name('hey-mira-unavailable.json').read_text())),indent=2))
