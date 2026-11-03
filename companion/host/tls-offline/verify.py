"""Offline only: no sockets, device access, key generation, or production secrets."""
import argparse, hashlib, json, pathlib, ssl, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
DEFAULT=ROOT.parents[1]/'.tools/sdk/esp-idf-v5.5.4/components/mbedtls/mbedtls/framework/data_files'
FILES=('test-ca.crt','test-ca2.crt','server1.crt','server1.key','server7-expired.crt','test-int-ca.crt','server7.key')
def handshake(client_context, server_context, host):
    ci,co,si,so=[ssl.MemoryBIO() for _ in range(4)]
    client=client_context.wrap_bio(ci,co,server_side=False,server_hostname=host)
    server=server_context.wrap_bio(si,so,server_side=True)
    ready=[False,False]
    for _ in range(50):
        for i,peer in enumerate((client,server)):
            if ready[i]: continue
            try: peer.do_handshake(); ready[i]=True
            except ssl.SSLWantReadError: pass
        if co.pending:si.write(co.read())
        if so.pending:ci.write(so.read())
        if all(ready):return
    raise AssertionError('in-memory handshake did not finish')
def context(ca):
    client=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    # Official legacy SDK fixtures contain SHA-1 signatures. This compatibility
    # setting applies only to these in-memory test contexts, never firmware.
    client.set_ciphers('DEFAULT:@SECLEVEL=0')
    client.load_verify_locations(cafile=str(ca));client.check_hostname=True
    return client
def run(fixtures):
    assert all((fixtures/f).is_file() for f in FILES),'official SDK fixtures missing'
    with tempfile.TemporaryDirectory(prefix='companion-offline-tls-') as temp:
        temp=pathlib.Path(temp)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(pathlib.Path(__file__).with_name('test_local_tls.cpp')),'-o',str(temp/'policy')],check=True)
        subprocess.run([str(temp/'policy')],check=True)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(pathlib.Path(__file__).with_name('test_local_tls_lifecycle.cpp')),'-o',str(temp/'lifecycle')],check=True)
        subprocess.run([str(temp/'lifecycle')],check=True)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(pathlib.Path(__file__).with_name('test_tls_writers.cpp')),'-o',str(temp/'writers')],check=True)
        subprocess.run([str(temp/'writers')],check=True)
        tls_tests=pathlib.Path(__file__).resolve().parent
        transport=temp/'context'
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(tls_tests/'sdk-fixture'),'-I'+str(ROOT/'firmware/main'),'-I'+str(ROOT/'firmware/managed_components/78__esp-ml307/include'),str(tls_tests/'test_local_tls_context.cpp'),str(ROOT/'firmware/main/companion/local_tls.cc'),'-o',str(transport)],check=True)
        for mode in ['overlap','invalid_fd','closed_fd','idle','callback','callback_stop','read_retry']:
            subprocess.run([str(transport),mode],check=True,timeout=5)
        server=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        server.set_ciphers('DEFAULT:@SECLEVEL=0')
        server.load_cert_chain(str(fixtures/'server1.crt'),str(fixtures/'server1.key'))
        handshake(context(fixtures/'test-ca.crt'),server,'PolarSSL Server 1')
        cases=[('wrong CA',context(fixtures/'test-ca2.crt'),server,'PolarSSL Server 1'),
               ('wrong hostname',context(fixtures/'test-ca.crt'),server,'192.168.1.12')]
        (temp/'expired-chain.pem').write_bytes((fixtures/'server7-expired.crt').read_bytes()+(fixtures/'test-int-ca.crt').read_bytes())
        expired=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        expired.set_ciphers('DEFAULT:@SECLEVEL=0')
        expired.load_cert_chain(str(temp/'expired-chain.pem'),str(fixtures/'server7.key'))
        cases.append(('expired certificate',context(fixtures/'test-ca2.crt'),expired,'localhost'))
        for label,client,peer,host in cases:
            try:handshake(client,peer,host)
            except ssl.SSLCertVerificationError as exc:
                if label=='expired certificate':assert exc.verify_code==10,(label,exc.verify_message)
            else:raise AssertionError(label+' was accepted')
    print(json.dumps({'offline':True,'network':False,'policy':'pass','certificate_cases':['valid chain','wrong CA','wrong hostname','expired certificate'],
        'backend':ssl.OPENSSL_VERSION,'fixtures':{f:hashlib.sha256((fixtures/f).read_bytes()).hexdigest() for f in FILES}},indent=2))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--fixtures',type=pathlib.Path,default=DEFAULT)
    run(parser.parse_args().fixtures)
