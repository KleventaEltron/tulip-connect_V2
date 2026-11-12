import argparse
import base64
import http.client
import importlib.util
import json
import os
from pathlib import Path
import secrets
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading

p=argparse.ArgumentParser()
p.add_argument('--cc',default=os.environ.get('CC','cc'))
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
spec=importlib.util.spec_from_file_location('bench_server',r/'tools/s2_comm_test/server.py')
bench=importlib.util.module_from_spec(spec);spec.loader.exec_module(bench)

def exact(sock,n):
    data=b''
    while len(data)<n:
        chunk=sock.recv(n-len(data))
        assert chunk,'closed peer'
        data+=chunk
    return data

def send(sock,obj,opcode=1):
    data=json.dumps(obj,separators=(',',':')).encode() if opcode==1 else obj
    mask=secrets.token_bytes(4)
    head=bytes([128|opcode])+(bytes([128|len(data)]) if len(data)<126 else b'\xfe'+struct.pack('!H',len(data)))
    sock.sendall(head+mask+bytes(v^mask[i%4] for i,v in enumerate(data)))

def receive(sock):
    while True:
        a,b=exact(sock,2);assert a&128 and not b&128
        n=b&127
        if n==126:n=struct.unpack('!H',exact(sock,2))[0]
        assert n<4096
        data=exact(sock,n)
        if a&15==9:send(sock,data,10);continue
        assert a&15==1
        return json.loads(data)

with tempfile.TemporaryDirectory(prefix='s2-messages-') as temp:
    temp=Path(temp);exe=temp/('messages.exe' if os.name=='nt' else 'messages')
    subprocess.run(cc+['-std=c99','-DTULIP_S2_COMM_TEST_ENABLE=1','-Wall','-Wextra','-Werror','-I'+str(r/'src'),'-I'+str(r/'src/files/s2'),str(r/'src/files/s2/s2_test_messages.c'),str(r/'src/files/cJSON.c'),str(r/'tests/s2_messages/test_messages.c'),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    bench.initialize(temp,'127.0.0.1','tulip-s2-test.invalid',port,header_path=temp/'fixture.h')
    server=bench.create_server(temp,'127.0.0.1')
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    cfg=server.state.config
    context=ssl.create_default_context(cafile=str(temp/'ca.pem'))
    context.maximum_version=ssl.TLSVersion.TLSv1_2
    def connect():return context.wrap_socket(socket.create_connection(('127.0.0.1',port),timeout=5),server_hostname=cfg['hostname'])
    def post(path,bearer,body):
        raw=json.dumps(body).encode() if body is not None else b''
        with connect() as sock:
            wire=(f'POST {path} HTTP/1.1\r\nHost: {cfg["hostname"]}:{port}\r\nAuthorization: Bearer {bearer}\r\nContent-Type: application/json\r\nContent-Length: {len(raw)}\r\nConnection: close\r\n\r\n').encode()+raw
            sock.sendall(wire);reply=http.client.HTTPResponse(sock);reply.begin()
            return reply.status,json.loads(reply.read())
    try:
        request={'clientNodeId':cfg['client'],'serverNodeId':cfg['server'],'supportedS2MessageVersions':[bench.VERSION],'supportedCommunicationProtocols':['WebSocket']}
        assert post('/v1/initiateSession','invalid',request)[0]==401
        status,init=post('/v1/initiateSession',cfg['initial_token'],request);assert status==200
        status,confirm=post('/v1/confirmAccessToken',init['accessToken'],None);assert status==200
        assert post('/v1/initiateSession',cfg['initial_token'],request)[0]==401
        before=(temp/'state.json').read_bytes()
        try:
            bench.reset_device(temp)
        except RuntimeError:
            pass
        else:
            raise AssertionError('reset must refuse a running server')
        assert (temp/'state.json').read_bytes()==before
        # State survives server object reload without printing credentials.
        assert bench.State(temp).data['active']==init['accessToken']
        with connect() as sock:
            key=base64.b64encode(secrets.token_bytes(16)).decode()
            sock.sendall((f'GET /s2 HTTP/1.1\r\nHost: {cfg["hostname"]}:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: {key}\r\nAuthorization: Bearer {confirm["websocketToken"]}\r\n\r\n').encode())
            headers=b''
            while not headers.endswith(b'\r\n\r\n'):headers+=exact(sock,1)
            assert headers.startswith(b'HTTP/1.1 101')
            proc=subprocess.Popen([str(exe),'interactive'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
            def cycle(message=None):
                if message is not None:
                    proc.stdin.write(json.dumps(message)+'\n');proc.stdin.flush()
                passed=False
                while True:
                    line=proc.stdout.readline().strip()
                    assert line,'C engine exited'
                    if line=='#READY':return passed
                    if line=='#PASS':passed=True;continue
                    obj=json.loads(line);server.state.check[obj['message_type']].validate(obj);send(sock,obj)
            try:
                cycle()
                for i in range(30):
                    if cycle(receive(sock)):break
                else:raise AssertionError('two-way exchange did not complete')
                assert server.state.passed.wait(3),'server did not confirm pass'
                send(sock,struct.pack('!H',1000),8)
            finally:
                proc.stdin.close();proc.wait(timeout=5);assert proc.returncode==0
    finally:
        server.shutdown();server.server_close();thread.join(timeout=5)
    config_before=(temp/'config.json').read_bytes()
    certificate_before=(temp/'server.pem').read_bytes()
    bench.reset_device(temp)
    assert (temp/'config.json').read_bytes()==config_before
    assert (temp/'server.pem').read_bytes()==certificate_before
    backups=list(temp.glob('state-backup-*.json'));assert len(backups)==1
    assert json.loads(backups[0].read_text())['active']==init['accessToken']
    restarted=bench.State(temp)
    assert restarted.data['active']==cfg['initial_token'] and restarted.data['pending'] is None
    assert restarted.initiate(init['accessToken'],request)[0]==401
    assert restarted.initiate(cfg['initial_token'],request)[0]==200
    print('Bench reset backup, running-server exclusion and initial-token resynchronization passed.')
    print('Actual C diagnostic engine exchanged schema-valid S2 messages with local TLS/HTTPS/WebSocket CEM; both sides confirmed PASS. No pump-control module linked.')
