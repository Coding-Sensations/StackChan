import unittest,ssl,json
from hermes_adapter import HTTPSWire,Unknown,Refused
TOKEN=b'fixture-credential-only-never-real'
def qualified_context():
 context=ssl.create_default_context()
 context.minimum_version=ssl.TLSVersion.TLSv1_2
 return context
class Response:
 status=202
 def __init__(self,data=b'{"run_id":"run-1","status":"started"}',headers=None):self.data=data;self.headers=headers or {'Content-Type':'application/json'}
 def getheader(self,key,default=None):return self.headers.get(key,default)
 def read(self,n):return self.data[:n]
class Connection:
 def __init__(self,*a,**kw):self.calls=[];self.response=Response();self.on_connect=lambda:None;self.on_request=lambda:None;self.on_close=lambda:None;self.failclose=False
 def connect(self):self.calls.append('connect');self.on_connect()
 def request(self,method,path,body,headers):self.calls.append((method,path,body,dict(headers)));self.on_request()
 def getresponse(self):self.calls.append('response');return self.response
 def close(self):self.calls.append('close');self.on_close();
class Tests(unittest.TestCase):
 def setUp(self):
  self.context=qualified_context();self.c=Connection();self.valid=True
  self.w=HTTPSWire('https://127.0.0.1:443',TOKEN,self.context,connection_factory=lambda *a,**kw:self.c)
 def call(self):return self.w.exchange('POST','/p/robot-hermes/v1/runs',{'session_id':'session-1','input':'hi'},'operation-1',lambda:self.valid)
 def test_real_wire_encodes_authenticated_bounded_request(self):
  self.assertEqual(self.call()['run_id'],'run-1');request=self.c.calls[1];self.assertEqual(request[0:2],('POST','/p/robot-hermes/v1/runs'));self.assertEqual(json.loads(request[2]),{'session_id':'session-1','input':'hi'});self.assertEqual(request[3]['Idempotency-Key'],'operation-1');self.assertEqual(request[3]['Authorization'],'Bearer '+TOKEN.decode());self.assertEqual(self.c.calls[-1],'close');self.assertNotIn(TOKEN.decode(),repr(self.w))
 def test_plaintext_or_credential_url_or_unverified_tls_refused(self):
  for origin in ['http://127.0.0.1:8642','https://user:pass@127.0.0.1','https://127.0.0.1?token=x','https://127.0.0.1/other']:
   with self.assertRaises(ValueError):HTTPSWire(origin,TOKEN,self.context)
  self.context.check_hostname=False
  with self.assertRaises(ValueError):HTTPSWire('https://127.0.0.1',TOKEN,self.context)
 def test_revocation_during_connect_prevents_request(self):
  self.c.on_connect=lambda:setattr(self,'valid',False)
  with self.assertRaises(Refused):self.call()
  self.assertEqual(self.c.calls,['connect','close'])
 def test_revocation_after_write_is_unknown_no_retry(self):
  self.c.on_request=lambda:setattr(self,'valid',False)
  with self.assertRaises(Unknown):self.call()
  self.assertNotIn('response',self.c.calls)
 def test_redirect_and_http_error_never_follow_or_expose_error(self):
  for status in [301,302,401,409,500]:
   self.setUp();self.c.response.status=status
   with self.assertRaises(Unknown):self.call()
   self.assertEqual(sum(isinstance(c,tuple) for c in self.c.calls),1)
 def test_duplicate_nonfinite_oversize_malformed_or_wrong_type_refused(self):
  for data in [b'{"run_id":"one","run_id":"two"}',b'{"v":NaN}',b'x'*65537,b'bad',b'[]']:
   self.setUp();self.c.response=Response(data)
   with self.assertRaises(Unknown):self.call()
 def test_cleanup_callback_revocation_cannot_return_success(self):
  self.c.on_close=lambda:setattr(self,'valid',False)
  with self.assertRaises(Unknown):self.call()
 def test_cleanup_failure_retains_owner_and_prevents_new_request(self):
  def fail():raise RuntimeError('fixture secret not exposed')
  self.c.on_close=fail
  with self.assertRaises(Unknown):self.call()
  with self.assertRaises(Refused):self.call()
  self.c.on_close=lambda:None;self.w.close()
  with self.assertRaises(Refused):self.call()
 def test_close_wipes_owned_credential_and_future_operations_inert(self):
  secret=self.w._secret;self.w.close();self.assertEqual(set(secret),{0})
  with self.assertRaises(Refused):self.call()


class FixtureTlsPolicy(unittest.TestCase):
 def test_fixture_pins_minimum_when_platform_policy_is_unspecified(self):
  from unittest.mock import patch
  ambient=ssl.create_default_context();ambient.minimum_version=ssl.TLSVersion.MINIMUM_SUPPORTED
  with self.assertRaises(ValueError):HTTPSWire('https://127.0.0.1',TOKEN,ambient)
  with patch.object(ssl,'create_default_context',return_value=ambient):context=qualified_context()
  self.assertEqual(context.minimum_version,ssl.TLSVersion.TLSv1_2)
  self.assertEqual(context.verify_mode,ssl.CERT_REQUIRED);self.assertTrue(context.check_hostname)
  HTTPSWire('https://127.0.0.1',TOKEN,context,connection_factory=Connection).close()

if __name__=='__main__':unittest.main()
