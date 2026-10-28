"""Report port evidence gaps without executing firmware or generating a write profile."""
import argparse
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('probe_analyzer', HERE.parent/'ad104-msi/diagnostic/tools/analyze.py')
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)


def assess(inventory, capture=None, manifest=None, mock=False):
    board = inventory.get('board', {})
    pci = [a.get('pci') or {} for a in inventory.get('adapters', [])]
    target = dict(vendor='10DE', device='2702', subsystem_vendor='1462', subsystem_device='5118', revision='A1')
    host_matches = (inventory.get('schema') == 1
                    and board.get('manufacturer') == 'Micro-Star International Co., Ltd.'
                    and board.get('product') == 'X870E GAMING PLUS WIFI (MS-7E70)'
                    and inventory.get('bios') == '1.A62'
                    and sum(p == target for p in pci) == 1
                    and sum(p.get('vendor') == '1002' and p.get('device') == '164E' for p in pci) == 1)
    gaps = []
    if not host_matches:
        gaps.append('Windows motherboard, BIOS or GPU identity differs from this development profile.')
    if inventory.get('secure_boot_enabled') is not False:
        gaps.append('Unsigned diagnostic boot policy is unresolved: Secure Boot is enabled or unknown.')
    evidence = None
    if capture is None:
        gaps.append('No preboot capture supplied: ROM, register state and firmware ownership are unknown.')
    else:
        if (not manifest or manifest.get('profile') != 'rtx4080s-msi'
                or manifest.get('gpu_write_capability') is not False
                or manifest.get('hardware_tested') is not False):
            raise ValueError('Expected the exact diagnostic kit manifest')
        evidence = analyzer.analyze(capture, manifest['build_id'], 'rtx4080s-msi')
        evidence.pop('stable_fingerprint', None)  # Avoid duplicating paths in the summary.
        for key, message in (
            ('identity_observed', 'Unique target identity is missing.'),
            ('register_snapshot_complete', 'Register snapshot is incomplete; inspect the recorded refusal.'),
            ('physical_prom_complete', 'Physical PROM capture is incomplete.'),
            ('platform_graph_complete', 'Firmware driver ownership graph is incomplete.'),
            ('topology_complete', 'GOP topology is incomplete or ambiguous.'),
        ):
            if not evidence[key]: gaps.append(message)
        if not evidence['driver_code_captures_verified']:
            gaps.append('No loaded GPU driver code was captured and hash-checked.')
        if capture.get('config256') is None:
            gaps.append('Extended PCI configuration is missing.')
        if not evidence.get('amd_integrated_gops'):
            gaps.append('No AMD 164e GOP was observed; Windows cabling does not prove firmware output routing.')
        if mock:
            gaps.append('Capture is simulated host-test data and cannot qualify this hardware.')
    gaps += ['X870E/AD103 write transaction and multi-display release/cleanup still require implementation and replay.',
             'Actual Windows handoff, numerical correctness and matched performance remain untested.']
    return dict(schema=1, host_profile_matches=host_matches, can_prepare_diagnostic=host_matches,
                can_activate_unlock=False, hardware_unlock_validated=False,
                capture_origin='host-mock' if mock else ('operator-supplied, unauthenticated' if capture else 'absent'),
                capture_evidence=evidence, remaining_work=gaps)


def read_json(path):
    if path.stat().st_size > 8*1024*1024:
        raise ValueError('Input exceeds the diagnostic report size limit')
    return json.loads(path.read_text(encoding='utf-8-sig'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--mock', action='store_true')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if bool(args.capture) != bool(args.manifest) or (args.mock and not args.capture):
        parser.error('Supply --capture and --manifest together; --mock requires a capture')
    try:
        report = assess(read_json(args.inventory), read_json(args.capture) if args.capture else None,
                        read_json(args.manifest) if args.manifest else None, args.mock)
        text = json.dumps(report, indent=2)+'\n'
        if args.output:
            with args.output.open('x', encoding='utf-8') as out: out.write(text)
        else: print(text, end='')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.error(str(exc))


if __name__ == '__main__':
    main()
