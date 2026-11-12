import argparse
import http.client
import io
import os
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument('--cc',default=os.environ.get('CC','cc'))
a=p.parse_args();root=Path(__file__).resolve().parents[2]
cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
with tempfile.TemporaryDirectory(prefix='logging-http-') as t:
    exe=Path(t)/('test.exe' if os.name=='nt' else 'test')
    subprocess.run(cc+['-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'src/files'),str(root/'src/files/logging_http.c'),str(root/'tests/logging_http/test_http.c'),'-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    if result.returncode:print(result.stdout,result.stderr);result.check_returncode()
for line in result.stdout.splitlines():
    if line.startswith('WIRE_HEX '):
        request=bytes.fromhex(line.split(' ',1)[1]);_,rest=request.split(b'\r\n',1)
        class Socket:
            def makefile(self,*args):return io.BytesIO(b'HTTP/1.1 200 OK\r\n'+rest)
        parsed=http.client.HTTPResponse(Socket());parsed.begin();assert parsed.read()==b'x'*8192
    else:print(line)
print('Generated chunked request independently decoded by Python HTTP parser.')
