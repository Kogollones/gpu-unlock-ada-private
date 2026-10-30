"""Generate hash-pinned AD102 assets offline from the repository's verified inputs."""
import argparse
import hashlib
import importlib.util
import json
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'firmware'/f'{name}.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    import zstandard
    manifest = json.loads((ROOT/'firmware/artifacts-610.43.03.json').read_text())
    for item in (manifest['source_artifacts'][0], manifest['source_artifacts'][2]):
        with (a.inputs/item['filename']).open('rb') as f:
            if hashlib.file_digest(f, 'sha256').hexdigest() != item['sha256']:
                raise ValueError('Input hash mismatch: '+item['filename'])
    gen = module('generate_v67')
    ext = module('extract_boot_assets')
    package = a.inputs/manifest['source_artifacts'][2]['filename']
    wanted = {'.PKGINFO', gen.FIRMWARE_PREFIX+'gsp_ga10x.bin', manifest['source_artifacts'][2]['notice']['path']}
    found = {}
    with package.open('rb') as f, zstandard.ZstdDecompressor().stream_reader(f) as reader:
        with tarfile.open(fileobj=reader, mode='r|') as archive:
            for item in archive:
                if item.name not in wanted:
                    continue
                if item.name in found or not item.isfile() or item.size > 128*1024*1024:
                    raise ValueError('Invalid package member')
                found[item.name] = archive.extractfile(item).read()
    if set(found) != wanted or f'pkgver = {gen.PACKAGE_VERSION}' not in found['.PKGINFO'].decode().splitlines():
        raise ValueError('Package metadata or members do not match')
    blobs = {
        'gsp_ga10x.bin': found[gen.FIRMWARE_PREFIX+'gsp_ga10x.bin'],
        gen.AD102_PAYLOAD_NAME: gen.build_ad102_payload(),
        'NVIDIA-LICENSE': found[manifest['source_artifacts'][2]['notice']['path']],
    }
    pins = {
        'gsp_ga10x.bin': gen.GSP_SHA256,
        gen.AD102_PAYLOAD_NAME: gen.AD102_PAYLOAD_SHA256,
        'NVIDIA-LICENSE': manifest['source_artifacts'][2]['notice']['sha256'],
    }
    sources = {}
    with tarfile.open(a.inputs/manifest['source_artifacts'][0]['filename'], 'r:xz') as archive:
        for item in archive:
            name = item.name.rsplit('/', 1)[-1]
            if name not in ext.ADA_PINNED:
                continue
            if name in sources or not item.isfile() or item.size > 16*1024*1024:
                raise ValueError('Invalid generated source member')
            data = archive.extractfile(item).read()
            if hashlib.sha256(data).hexdigest() != ext.ADA_PINNED[name]:
                raise ValueError('Generated source hash mismatch')
            sources[name] = data.decode('ascii')
    if set(sources) != set(ext.ADA_PINNED):
        raise ValueError('Missing generated source')
    blobs['booter_load_ad102.bin'] = ext.build_bundle(sources['g_bindata_kgspGetBinArchiveBooterLoadUcode_AD102.c'])
    boot = sources['g_bindata_kgspGetBinArchiveGspRmBoot_AD102.c']
    blobs['gsp_boot_ad102.bin'] = ext._array_bytes(boot, 'UCODE_IMAGE_PROD')[0]
    blobs['gsp_boot_desc_ad102.bin'] = ext._array_bytes(boot, 'UCODE_DESC_PROD')[0]
    pins.update(json.loads((ROOT/'firmware/ada-610.43.03.json').read_text())['derived_assets'])
    for name, data in blobs.items():
        if hashlib.sha256(data).hexdigest() != pins[name]:
            raise ValueError('Output hash mismatch: '+name)
    a.output.mkdir(parents=True, exist_ok=True)
    for name, data in blobs.items():
        (a.output/name).write_bytes(data)
    print('Verified five generated assets and NVIDIA license; no firmware executed.')

if __name__ == '__main__':
    main()
