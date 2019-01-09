#! /usr/bin/python3

import re
import subprocess
import sys

addr2line="addr2line"
nm = "nm"

addr_to_symbols = {}
addr_to_timestamp = {}

def get_sym_addr(fname,sym_name):
    if fname not in addr_to_symbols:
        symbols = subprocess.check_output([nm,fname]).decode('ascii')
        addr_to_symbols[fname] = symbols.split('\n')

    pm = re.compile("(?P<addr>[0-9-a-f]+)\s+T\s+%s" % sym_name)
    for symbol in addr_to_symbols[fname]:
        m = pm.match(symbol)
        if m:
            return m.group('addr')
    return None

#"<file-name>(<sym-name>+offset) [address]
pm = re.compile(r'^(?P<gen>B|E)@(?P<time>0x[0-9a-f]+) (?P<fname>[^(]+)\((?P<sym_name>[^+]*)\+(?P<call_offset>[^)]+)\) \[0x[0-9a-f]+\]')

print("start_time,end_time,elapsed,elf,sym_name,call_addr,source")
with open("finstr.txt") as fin:
    for line in fin:
        m = pm.match(line)
        if not m:
            continue

        call_offset = m.group('call_offset')
        sym_name = m.group('sym_name')
        fname = m.group('fname')
        time = m.group('time')
        gen = m.group('gen')
        sym_addr = get_sym_addr(fname,sym_name)
        call_addr = "0x%x" % (int(sym_addr,16) + int(call_offset,16))
        if gen == 'B':
            if call_addr not in addr_to_timestamp:
                addr_to_timestamp[call_addr] = []
            addr_to_timestamp[call_addr].append(time)
            if not sorted(addr_to_timestamp[call_addr]):
                print("FATAL: non incremental timestamps: %s" % call_addr, file=sys.stderr)
                sys.exit(1)
        elif gen == 'E':
            cmd = [addr2line,"-pCf","-e",fname,call_addr]
            out = subprocess.check_output(cmd).decode('ascii').rstrip()
            if call_addr not in addr_to_timestamp:
                print("WARNING: end call_address(%s) doesn't have a begining" % call_addr, file=sys.stderr)
                continue
            start_time = addr_to_timestamp[call_addr].pop()
            elapsed = int(time,16) - int(start_time,16)
            print("%s,%s,%s,%s,%s,%s,%s" % (start_time,time,elapsed, fname, sym_name,call_addr, out))
