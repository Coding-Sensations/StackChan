from test_https_wire import qualified_context
import unittest,ssl,http.client
from unittest.mock import patch
from hermes_adapter import HTTPSWire,Unknown,Refused
TOKEN=b'fixture-only-deadline-credential'
class Clock:
 def __init__(self):self.now=0.
 def __call__(self):return self.now
class SlowFile:
 def __init__(self,sock,data,header_delay,body_delay):self.sock=sock;self.data=data;self.offset=0;self.split=data.index(b'\r\n\r\n')+4;self.header_delay=header_delay;self.body_delay=body_delay
 def read1(self,n):
  if self.offset>=len(self.data):return b''
  step=self.header_delay if self.offset<self.split else self.body_delay
  timeout=self.sock.timeout
  if step>timeout:self.sock.clock.now+=timeout;raise TimeoutError('fixture timeout')
  self.sock.clock.now+=step;value=self.data[self.offset:self.offset+1];self.offset+=len(value);return value
 def readline(self,n=-1):
  result=b''
  while len(result)!=n:
   value=self.read1(1)
   if not value:break
   result+=value
   if value==b'\n':break
  return result
 def read(self,n=-1):
  value=b''
  while len(value)!=n:
   item=self.read1(1)
   if not item:break
   value+=item
  return value
 def close(self):pass
class Sock:
 def __init__(self,clock,header_delay,body_delay):
  self.clock=clock;self.timeout=3.;self.sent=[];self.closed=False
  body=b'{"run_id":"run-1","status":"started"}'
  raw=b'HTTP/1.1 202 Accepted\r\nContent-Type: application/json\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body
  self.fp=SlowFile(self,raw,header_delay,body_delay)
 def settimeout(self,n):self.timeout=n
 def sendall(self,data):self.sent.append(bytes(data))
 def send(self,data):self.sent.append(bytes(data));return len(data)
 def makefile(self,*a,**kw):return self.fp
 def close(self):self.closed=True
class Tests(unittest.TestCase):
 def wire(self,header_delay,body_delay):
  clock=Clock();sock=Sock(clock,header_delay,body_delay)
  class Connection(http.client.HTTPSConnection):
   def connect(self):self.sock=sock
  return HTTPSWire('https://127.0.0.1',TOKEN,qualified_context(),connection_factory=Connection,clock=clock),clock,sock
 def call(self,wire,remaining=None):return wire.exchange('POST','/p/hermes/v1/runs',{'session_id':'session-1','input':'hello'},'operation-1',lambda:True,remaining=remaining)
 def test_header_slow_drip_stops_at_elapsed_budget(self):
  wire,clock,sock=self.wire(.2,0.)
  with self.assertRaises(Unknown):self.call(wire)
  self.assertLessEqual(clock.now,3.000001);self.assertTrue(sock.closed)
 def test_body_slow_drip_stops_at_elapsed_budget(self):
  wire,clock,sock=self.wire(0.,.2)
  with self.assertRaises(Unknown):self.call(wire)
  self.assertLessEqual(clock.now,3.000001);self.assertTrue(sock.closed)
 def test_owner_remaining_budget_shorter_than_transport_budget(self):
  wire,clock,sock=self.wire(.02,0.)
  with self.assertRaises(Unknown):self.call(wire,lambda:.05)
  self.assertLessEqual(clock.now,.050001);self.assertTrue(sock.closed)
 def test_fast_complete_native_http_parser_still_works(self):
  wire,clock,sock=self.wire(0.,0.);self.assertEqual(self.call(wire)['run_id'],'run-1');self.assertTrue(sock.closed)
 def test_ambient_dns_is_not_a_supported_route(self):
  with patch('socket.getaddrinfo',side_effect=AssertionError('DNS lookup')):
   with self.assertRaises(ValueError):HTTPSWire('https://ambient.invalid',TOKEN,qualified_context())
if __name__=='__main__':unittest.main()
