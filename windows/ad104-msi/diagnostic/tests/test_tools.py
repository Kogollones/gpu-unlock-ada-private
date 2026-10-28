import copy
import importlib.util
import json
import os
from pathlib import Path
import struct
import unittest

ROOT=Path(__file__).resolve().parents[1]
BINARY=Path(os.environ.get('PROBE_TEST_BINARY',ROOT/'build/AD104-PROBE.EFI'))


def load(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/f'{name}.py')
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


verify=load('verify')
analyze=load('analyze')
EXPECTED_PROFILE=json.loads((ROOT/'build/host-capture.json').read_text()).get('diagnostic_profile','ad104-msi')


def analyze_capture(data, build_id):
    return analyze.analyze(data, build_id, EXPECTED_PROFILE)


class ToolTests(unittest.TestCase):
    def test_pe_accepts_built_image(self):
        verify.validate_pe(BINARY.read_bytes())

    def test_pe_rejects_mutations(self):
        original=BINARY.read_bytes()
        pe=struct.unpack_from('<I',original,0x3c)[0]
        opt=pe+24
        cases=[(0,b'ZZ'),(pe+4,struct.pack('<H',0x14c)),
               (opt+68,struct.pack('<H',3)),(opt+16,b'\0'*4),
               (opt+112+8,struct.pack('<II',0x1000,64)),
               (opt+112+5*8,b'\0'*8),(opt+112+9*8,struct.pack('<II',0x1000,64)),
               (opt+112+14*8,struct.pack('<II',0x1000,64)),
               (0x3c,struct.pack('<I',0xffffffff))]
        for offset,patch in cases:
            with self.subTest(offset=offset):
                data=bytearray(original);data[offset:offset+len(patch)]=patch
                with self.assertRaises(ValueError):
                    verify.validate_pe(data)
        with self.assertRaises(ValueError):
            verify.validate_pe(original[:100])

    def test_source_rejects_forbidden_calls(self):
        for call in ('pci->Pci.Write(', 'pci->Mem.Read(', 'pci->Map(',
                     'bs->DisconnectController(', 'rs->SetVariable(', 'gop->SetMode('):
            with self.subTest(call=call),self.assertRaises(ValueError):
                verify.validate_source(call)

    def test_capture_accepts_mock_as_inventory_only(self):
        data=json.loads((ROOT/'build/host-capture.json').read_text())
        result=analyze_capture(data,data['build_id'])
        self.assertTrue(result['identity_observed'])
        self.assertEqual(result['target_gops'],1)
        self.assertFalse(result['unlock_ready'])
        self.assertFalse(result['hardware_support_validated'])
        self.assertTrue(result['register_snapshot_complete'])
        self.assertEqual(len(data['cached_rom_hex']),300032)
        self.assertTrue(result['physical_prom_complete'])
        self.assertEqual(len(data['config256']),512)

    def test_capture_rejects_mutations(self):
        data=json.loads((ROOT/'build/host-capture.json').read_text())
        with self.assertRaises(ValueError):
            analyze_capture(data,'incorrect-build')
        for field in ('gpu_writes','unlock_attempted','hardware_support_validated'):
            changed=copy.deepcopy(data);changed[field]=True
            with self.subTest(field=field),self.assertRaises(ValueError):
                analyze_capture(changed,data['build_id'])
        changed=copy.deepcopy(data);changed['target_id_count']=2
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])

        changed=copy.deepcopy(data);changed['display_devices'][0]['config64']='00'*64
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])
        changed=copy.deepcopy(data);changed['cached_rom_hex']='00'+changed['cached_rom_hex'][2:]
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])
        changed=copy.deepcopy(data);changed['register_snapshot']['read_attempts']=999
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])
        changed=copy.deepcopy(data);changed['physical_prom']['hex']='00'+changed['physical_prom']['hex'][2:]
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])
    def test_driver_code_hash(self):
        import hashlib
        data=json.loads((ROOT/'build/host-capture.json').read_text())
        data['platform']={'available':True,'handles':[], 'handle_count':0,'edge_count':0,
                         'gpu_agents':[{'code':{'length':1,'hex':'00','loaded_sha256':hashlib.sha256(b'\0').hexdigest()}}],
                         'complete':False,'atomic_snapshot':False,'release_authorized':False}
        self.assertEqual(analyze_capture(data,data['build_id'])['driver_code_captures_verified'],1)
        data['platform']['gpu_agents'][0]['code']['hex']='01'
        with self.assertRaises(ValueError):
            analyze_capture(data,data['build_id'])

    def test_profiles_cannot_be_substituted(self):
        data=json.loads((ROOT/'build/host-capture.json').read_text())
        other='ad104-msi' if EXPECTED_PROFILE == 'rtx4080s-msi' else 'rtx4080s-msi'
        for profile in ('unknown-profile', other):
            with self.subTest(profile=profile), self.assertRaises(ValueError):
                analyze.analyze(data,data['build_id'],profile)
        changed=copy.deepcopy(data);changed['diagnostic_profile']='unknown-profile'
        with self.assertRaises(ValueError):
            analyze_capture(changed,data['build_id'])

    def test_two_target_monitors_and_one_integrated_monitor(self):
        data=json.loads((ROOT/'build/host-capture-three-monitors.json').read_text())
        result=analyze_capture(data,data['build_id'])
        self.assertEqual(result['target_gops'],2)
        self.assertEqual(result['other_display_gops'],1)
        self.assertTrue(result['topology_complete'])
        self.assertFalse(result['unlock_ready'])

    def test_completed_capture_cannot_contradict_hardware_guards(self):
        original=json.loads((ROOT/'build/host-capture.json').read_text())
        cases=[]
        data=copy.deepcopy(original)
        data['register_snapshot']['samples'][0]['value']='0x192000a1'
        data['register_snapshot']['final_chip']='0x192000a1';cases.append(data)
        data=copy.deepcopy(original);data['register_snapshot']['samples'][1]['value']='0xbadf0000';cases.append(data)
        data=copy.deepcopy(original);data['config256']='00'*256;cases.append(data)
        data=copy.deepcopy(original);data['display_devices'][0]['location']['bus']=2;cases.append(data)
        for offset,value in ((4,7),(19,0x90),(8,0xa2)):
            data=copy.deepcopy(original)
            config=bytearray.fromhex(data['display_devices'][0]['config64']);config[offset]=value
            data['display_devices'][0]['config64']=config.hex()
            data['config256']=config.hex()+data['config256'][128:];cases.append(data)
        for data in cases:
            with self.subTest(case=cases.index(data)), self.assertRaises(ValueError):
                analyze_capture(data,data['build_id'])

    def test_amd_gop_routing_and_failed_or_duplicate_gops(self):
        data=json.loads((ROOT/'build/host-capture-three-monitors.json').read_text())
        data['display_devices'][1]['vendor']='0x1002'
        data['display_devices'][1]['device_id']='0x164e'
        result=analyze_capture(data,data['build_id'])
        self.assertEqual(result['amd_integrated_gops'],1)
        self.assertTrue(result['console_on_target'])
        failed=copy.deepcopy(data);failed['gops'][0]['protocol_status']='0x8000000000000007'
        self.assertFalse(analyze_capture(failed,data['build_id'])['topology_complete'])
        data['gops'][2]['path']=copy.deepcopy(data['gops'][0]['path'])
        self.assertFalse(analyze_capture(data,data['build_id'])['topology_complete'])


if __name__=='__main__':
    unittest.main()
