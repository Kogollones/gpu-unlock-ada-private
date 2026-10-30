"""Extract the reviewed owner capture for offline tests, without widening admission."""
import argparse
import hashlib
import importlib.util
import json
import struct
from pathlib import Path

CAPTURE_SHA256 = '7daae82ad44c299929eb37c3421e680939e508b861e413d0526fcbbd2113c487'
HERE = Path(__file__).resolve().parent

def guid(raw):
    b = bytes.fromhex(raw)
    a,c,d = struct.unpack('<IHH', b[:8])
    return '{'+f'0x{a:x},0x{c:x},0x{d:x},'+'{'+','.join(hex(x) for x in b[8:])+'}}'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    raw = a.capture.read_bytes()
    if hashlib.sha256(raw).hexdigest() != CAPTURE_SHA256:
        raise ValueError('This fixture exporter requires the reviewed October 30 capture')
    cap = json.loads(raw)
    spec = importlib.util.spec_from_file_location('probe', HERE.parent/'ad104-msi/diagnostic/tools/analyze.py')
    probe = importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
    result = probe.analyze(cap, json.loads(a.manifest.read_text(encoding='utf-8-sig'))['build_id'], 'rtx4080s-msi')
    if not all(result[k] for k in ('identity_observed','physical_prom_complete','platform_graph_complete','topology_complete')):
        raise ValueError('Incomplete fixture')
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output/'physical-prom.bin').write_bytes(bytes.fromhex(cap['physical_prom']['hex']))
    for agent in cap['platform']['gpu_agents']:
        (a.output/f"driver-{agent['agent']}.bin").write_bytes(bytes.fromhex(agent['code']['hex']))
    data = bytes.fromhex(cap['config256'])
    (a.output/'rtx4080s_captured.h').write_text('/* Private captured PCI fixture, no firmware. */\nstatic const UINT8 CapturedConfig[256]={'+','.join(hex(x) for x in data)+'};\n')
    lines = ['/* Private captured graph; handle IDs are test labels only. */','static const EDGE CapturedEdges[]={']
    handle = lambda n: 'NULL' if n is None else f'(EFI_HANDLE)(UINTN){n+1}'
    for h in cap['platform']['handles']:
        for e in h['active_opens']:
            lines.append('{'+','.join([handle(h['id']),handle(e['agent']),handle(e['controller']),guid(e['protocol']),str(int(e['attributes'],16)),str(e['count'])])+'},')
    lines += ['};']
    (a.output/'rtx4080s_graph.h').write_text('\n'.join(lines)+'\n')
    print('Verified private PCI/graph fixtures and ROM/driver bytes; no hardware accessed.')

if __name__ == '__main__':
    main()
