import argparse
import os
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument('--cc',default=os.environ.get('CC','cc'))
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
with tempfile.TemporaryDirectory(prefix='s2-see-') as t:
    exe=Path(t)/('test.exe' if os.name=='nt' else 'test')
    subprocess.run(cc+['-std=c99','-Wall','-Wextra','-Werror',
        '-I'+str(r/'tests/s2_token_store_same54/stubs'),'-I'+str(r/'src'),'-I'+str(r/'src/files/s2'),
        str(r/'src/files/s2/s2_token_store_same54.c'),str(r/'src/files/s2/s2_token_store.c'),
        str(r/'tests/s2_token_store_same54/test_same54.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
