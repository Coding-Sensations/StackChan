"""Read-only sidecar authority view over HomeLab's existing SQLite store.
Always consult durable state; do not cache pairing credentials across processes.
Owner issuance/revocation remains in HomeLab's existing owner-authorized API.
"""
from hashlib import sha256


class DurableAudioRegistry:
    def __init__(self, store):self.store = store
    def get(self, device_id):
        return next((device for device in self.store.load_devices() if device.id == device_id), None)
    def authenticate(self, credential):
        if type(credential) is not str or not 1 <= len(credential) <= 256:return None
        device_id = self.store.device_for_credential(sha256(credential.encode()).hexdigest())
        if device_id is None:return None
        device = self.get(device_id)
        return device_id if device is not None and device.status == 'active' else None
