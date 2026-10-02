#!/usr/bin/env python3
"""Compile the actual hold_ms body with host libc; no Windows runtime needed."""
import pathlib
import re
import subprocess
import tempfile

source = (pathlib.Path(__file__).resolve().parents[2] / 'dxsmoke.c').read_text()
defines = '\n'.join(re.findall(r'^#define HOLD_.*$', source, re.M))
body = re.search(r'DWORD hold_ms\(void\)\n\{.*?\n\}', source, re.S).group()
with tempfile.TemporaryDirectory() as tmp:
    c = pathlib.Path(tmp) / 'hold.c'
    c.write_text('#define _POSIX_C_SOURCE 200809L\n#include <stdlib.h>\n#include <assert.h>\n'
                 'typedef unsigned long DWORD;\n' + defines + '\n' + body + '''
int main(void) {
    unsetenv("DXSMOKE_HOLD"); assert(hold_ms() == 750);
    const char *inputs[] = {"", "invalid", "0", "199", "200", "7000", "20000", "20001", "99999999999999999999999"};
    unsigned long expected[] = {750, 200, 200, 200, 200, 7000, 20000, 20000, 20000};
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        setenv("DXSMOKE_HOLD", inputs[i], 1); assert(hold_ms() == expected[i]);
    }
}
''')
    exe = str(pathlib.Path(tmp) / 'hold')
    subprocess.run(['cc', '-Wall', '-Wextra', str(c), '-o', exe], check=True)
    subprocess.run([exe], check=True)
print('hold_ms: unset, empty, invalid, lower/upper boundaries, overflow passed')
