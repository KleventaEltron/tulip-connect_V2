import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument('--cc',default=os.environ.get('CC','cc'));a=p.parse_args()
r=Path(__file__).resolve().parents[2];cc=[a.cc]+(['cc'] if Path(a.cc).stem.lower()=='zig' else [])
with tempfile.TemporaryDirectory(prefix='logging-recovery-') as t:
    t=Path(t);source=(r/'src/app_logging_tasks.c').read_text()
    # Compile the complete app unchanged against controlled hardware headers.
    for name in re.findall(r'#include "([^"]+)"',source):
        path=t/name;path.parent.mkdir(parents=True,exist_ok=True)
        if name=='app_logging_tasks.h':path.write_text((r/'src/app_logging_tasks.h').read_text())
        else:path.write_text('#include "stubs.h"\n')
    (t/'xc.h').write_text('#include "stubs.h"\n')
    app=t/'app.c';app.write_text(source)
    exe=t/('test.exe' if os.name=='nt' else 'test')
    subprocess.run(cc+['-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I'+str(t),'-I'+str(r/'tests/logging_recovery'),str(app),str(r/'tests/logging_recovery/test_app.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
