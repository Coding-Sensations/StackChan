import copy
import json
from pathlib import Path
import unittest
from check_unavailable import check_missing_model

class MissingModelTests(unittest.TestCase):
    def setUp(self):
        self.m=json.loads(Path(__file__).with_name('hey-mira-unavailable.json').read_text())
    def test_absence_never_implies_readiness(self):
        self.assertEqual(check_missing_model(self.m),{'contract_valid':True,'available':False,'runtime_enabled':False,'inference_executed':False})
    def test_fabricated_readiness_rejected(self):
        for field in ['available','runtime_enabled','network_capture_authorized']:
            with self.subTest(field=field):
                m=copy.deepcopy(self.m); m[field]=True
                with self.assertRaises(ValueError): check_missing_model(m)
    def test_fake_asset_rejected(self):
        for field in ['model_file','model_sha256']:
            m=copy.deepcopy(self.m); m[field]='synthetic'
            with self.assertRaises(ValueError): check_missing_model(m)
    def test_missing_fields_rejected(self):
        for field in ['available','runtime_enabled','network_capture_authorized','model_file','model_sha256']:
            m=copy.deepcopy(self.m); del m[field]
            with self.assertRaises(ValueError): check_missing_model(m)
    def test_wrong_pcm_rejected(self):
        for value in [24000,48000,0]:
            m=copy.deepcopy(self.m); m['required_pcm']['sample_rate_hz']=value
            with self.assertRaises(ValueError): check_missing_model(m)
    def test_wrong_phrase_rejected(self):
        m=copy.deepcopy(self.m); m['phrase']='Hey Jarvis'
        with self.assertRaises(ValueError): check_missing_model(m)

if __name__=='__main__': unittest.main()
