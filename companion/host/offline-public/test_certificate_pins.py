"""Public owner-pin enforcement with synthetic bytes; no TLS or device IO."""
import hashlib
import unittest
from test_public_config import module


class CertificatePins(unittest.TestCase):
    def config(self):
        config=module()
        config.DEVICE='synthetic-device';config.HOST='192.168.42.10'
        config.URI='wss://192.168.42.10:18443/companion/audio'
        config.CA_HASH='a'*64;config.EXPIRY=2000
        config.CERT_HASH=hashlib.sha256(b'synthetic certificate').hexdigest()
        config.CERT_BEGIN=1000;config.CERT_END=2000
        return config

    def test_exact_bytes_and_validity_are_required(self):
        self.config().require_server_certificate(b'synthetic certificate',1000,2000)

    def test_replacement_certificate_with_identical_dates_is_rejected(self):
        with self.assertRaises(ValueError):
            self.config().require_server_certificate(b'different synthetic certificate',1000,2000)

    def test_each_validity_pin_is_independently_required(self):
        for begin,end in ((999,2000),(1000,2001),(True,2000),(1000,2000.0)):
            with self.subTest(begin=begin,end=end),self.assertRaises(ValueError):
                self.config().require_server_certificate(b'synthetic certificate',begin,end)

    def test_unconfigured_or_oversized_certificate_is_rejected(self):
        config=self.config();config.CERT_HASH=''
        with self.assertRaises(ValueError):config.require_server_certificate(b'synthetic certificate',1000,2000)
        with self.assertRaises(ValueError):self.config().require_server_certificate(b'x'*8193,1000,2000)


if __name__=='__main__':unittest.main()
