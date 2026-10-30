"""Validate an offline candidate; never install or execute it on hardware."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--private-inputs', type=Path, required=True)
    p.add_argument('--assets', type=Path, required=True)
    p.add_argument('--compile-only', action='store_true')
    a = p.parse_args()
    spec = importlib.util.spec_from_file_location('pe', HERE.parent/'ad104-msi/tools/pe_format.py')
    pe = importlib.util.module_from_spec(spec);spec.loader.exec_module(pe)
    binary = a.output/'RTX4080S-EXPERIMENTAL.EFI'
    pe.validate_pe(binary.read_bytes())
    tests={}
    expected={
        'transaction':'90 real core + compat + UEFI port scenarios passed; all PCI/MMIO/DMA/firmware execution simulated.',
        'guards':'17 admission cases PASS; all hardware operations simulated.',
        'ownership':'X870E ownership: 32 scenarios, 634531 checks; captured graph/code, all effects simulated.',
        'app':'app flow: 296 checks; 15 scenarios, all effects simulated.',
        'boot-io':'boot I/O: 571 checks; 8 cases, real file bytes, all EFI services simulated.',
    }
    for name in ('transaction','guards','ownership','app','boot-io'):
        path=a.output/f'{name}.log'
        if not a.compile_only:
            log=path.read_text()
            if not log.strip() or 'FAIL' in log or log.strip().splitlines()[-1] != expected[name]:
                raise ValueError('Missing or failed test log: '+name)
            tests[name]={'last_line':log.strip().splitlines()[-1], 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    sources={}
    for directory in (HERE,HERE.parent/'ad104-msi'):
        for path in sorted(directory.rglob('*')):
            if path.suffix in ('.c','.h','.py','.ps1','.cs') and not any(part in ('build','local','__pycache__') for part in path.relative_to(directory).parts):
                sources[path.relative_to(ROOT).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
    manifest=dict(profile='rtx4080s-msi-x870e',version='0.1.0-experimental',efi_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        unlock_implemented=True,gpu_write_capability=True,hardware_tested=False,performance_validated=False,
        real_gpu_executed_by_build=False,automatic_unlock=False,requires_key='U',default='stock Windows after 30 seconds',
        host_tests_passed=not a.compile_only,tests=tests,source_sha256=sources,
        private_profile_sha256=hashlib.sha256((a.private_inputs/'rtx4080s_boot_profile.h').read_bytes()).hexdigest())
    (a.output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name,item in tests.items():print(name+': '+item['last_line'])
    print('PE verified. Not hardware-qualified. No USB/ESP/boot variables modified.')

if __name__ == '__main__':
    main()
