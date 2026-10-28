import copy
import unittest
from review import assess


class HostReviewTests(unittest.TestCase):
    def setUp(self):
        self.inventory = dict(schema=1, board=dict(manufacturer='Micro-Star International Co., Ltd.',
                              product='X870E GAMING PLUS WIFI (MS-7E70)'), bios='1.A62',
                              adapters=[dict(pci=dict(vendor='10DE', device='2702', subsystem_vendor='1462',
                              subsystem_device='5118', revision='A1')), dict(pci=dict(vendor='1002',device='164E'))],
                              secure_boot_enabled=True)

    def test_matching_host_is_diagnostic_only(self):
        result=assess(self.inventory)
        self.assertTrue(result['can_prepare_diagnostic'])
        self.assertFalse(result['can_activate_unlock'])
        self.assertEqual(result['capture_origin'],'absent')

    def test_changed_board_bios_gpu_and_missing_igpu(self):
        cases=[]
        data=copy.deepcopy(self.inventory);data['board']['product']='OTHER';cases.append(data)
        data=copy.deepcopy(self.inventory);data['bios']='1.A63';cases.append(data)
        data=copy.deepcopy(self.inventory);data['adapters'][0]['pci']['subsystem_device']='4702';cases.append(data)
        data=copy.deepcopy(self.inventory);data['adapters'].pop();cases.append(data)
        data=copy.deepcopy(self.inventory);data['adapters'].append(data['adapters'][0]);cases.append(data)
        for data in cases:
            with self.subTest(data=data):self.assertFalse(assess(data)['can_prepare_diagnostic'])

    def test_missing_secure_boot_is_unknown(self):
        del self.inventory['secure_boot_enabled']
        self.assertTrue(any('enabled or unknown' in s for s in assess(self.inventory)['remaining_work']))

    def test_write_manifest_rejected(self):
        with self.assertRaises(ValueError):
            assess(self.inventory,{},dict(profile='rtx4080s-msi',gpu_write_capability=True))


if __name__=='__main__':unittest.main()
