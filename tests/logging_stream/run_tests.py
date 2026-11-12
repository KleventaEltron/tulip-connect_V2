import argparse
import http.client
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('CC','cc'));a=p.parse_args()
r=Path(__file__).resolve().parents[2];cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
s=(r/'src/files/logging.c').read_text()
def function(sig):
    start=s.index(sig);line=s[start:].splitlines()[0]
    if line.rstrip().endswith('}'):return line+'\n'
    m=re.search(r'\n}(?:\n|$)',s[start:]);assert m;return s[start:start+m.end()]
parts=[s[s.index('static LOG_HTTP_TX loggingTx;'):s.index('DEVICE_TYPE DEVICE_TYPE_REQUESTED')],
       s[s.index('static unsigned realtimeStage'):s.index('void getRealTimeDataStatussen')],
       s[s.index('static const uint32_t eep_addrs_8_bit_1'):s.index('bool readServerResponseUpdatedSettingsDone')]]
for sig in ('bool getUpdateSettingsFromServer(', 'static cJSON *loggingParseJson(', 'bool parse_update_settings_from_json(', 'void resetRxBuffer(', 'bool loggingConnectionReusable(', 'bool areUpdateSettingsAvailable(', 'bool readServerResponseDone(', 'bool parse_modbus_settings('):
    parts.append(function(sig))
with tempfile.TemporaryDirectory(prefix='logging-stream-') as t:
    t=Path(t);source=t/'stream.c'
    prototypes='bool loggingTransmitBusy(void);\nbool readServerResponseDone(void);\nbool loggingIoFailed(void);\n'
    source.write_text('#include "stubs.h"\n#include <assert.h>\n'+prototypes+'\n'.join(parts)+'\n'+(r/'tests/logging_stream/test_stream.c').read_text())
    exe=t/('test.exe' if os.name=='nt' else 'test')
    subprocess.run(cc+['-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(r/'tests/logging_stream'),'-I'+str(r/'src/files'),str(source),str(r/'src/files/logging_http.c'),str(r/'src/files/cJSON.c'),'-lm','-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    if result.returncode:print(result.stdout[-1000:],result.stderr);result.check_returncode()
parsed={}
for line in result.stdout.splitlines():
    if '_HEX ' in line:
        name,data=line.split(' ',1);wire=bytes.fromhex(data);_,rest=wire.split(b'\r\n',1)
        class Socket:
            def makefile(self,*args):return io.BytesIO(b'HTTP/1.1 200 OK\r\n'+rest)
        reply=http.client.HTTPResponse(Socket());reply.begin();parsed[name]=reply.read()
    else:print(line)
body=json.loads(parsed['REALTIME_HEX']);assert len(body['devices'])==17
assert [d['index'] for d in body['devices']]==list(range(17))
assert all(d['sample']==1 for d in body['devices']);assert parsed['GET_HEX']==b''
assert [d['index'] for d in json.loads(parsed['ALONE_HEX'])['devices']]==[0,1]
assert [d['index'] for d in json.loads(parsed['SPARSE_HEX'])['devices']]==[0,1,4,16]
print('Actual request wire bytes decoded independently: all 17 device objects intact and settings CSV matches value order.')

layout=json.loads((r/'tests/logging_stream/expected_layout.json').read_text())
expected=[]
for first,end in layout['ranges']:
    expected.extend(2 if address==0x100 else (address*3)&65535 for address in range(first,end))
for group in layout['groups']:
    for address in group['addresses']:
        value=1 if address==layout['silent'] else 0 if address==layout['boost'] else address+1000
        expected.append(value&255 if group['bits']==8 else value)
assert parsed['SETTINGS_HEX']==(''.join(str(v)+',' for v in expected)).encode()
print('Settings CSV independently matches the legacy layout fixture (including mirrored mode bits).')
