import argparse
import os
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument('--cc',default=os.environ.get('CC','cc'))
p.add_argument('--bench',action='store_true')
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
with tempfile.TemporaryDirectory(prefix='s2-coordinator-') as t:
    exe=Path(t)/('test.exe' if os.name=='nt' else 'test')
    sources=['src/app_s2_session_tasks.c','src/app_s2_tasks.c','src/files/s2/s2_session.c','src/files/s2/s2_https.c','src/files/s2/s2_token_store.c','src/files/cJSON.c','tests/app_s2_session/test_coordinator.c']
    subprocess.run(cc+['-std=c99','-DTULIP_S2_COMM_TEST_ENABLE='+('1' if a.bench else '0'),'-Wall','-Wextra','-Werror','-I'+str(r/'tests/app_s2_session/stubs'),'-I'+str(r/'src')]+[str(r/s) for s in sources]+['-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    subprocess.run([str(exe),"registration-failure"],check=True)
