from pathlib import Path
import hashlib
import importlib.util
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('report', ROOT/'tools/profile_size_report.py')
report=importlib.util.module_from_spec(spec);spec.loader.exec_module(report)

def table():
    entry=struct.pack('<HBBII16sI',0x50aa,0,0x10,0x20000,0x4f0000,b'ota_0',0)
    entry+=struct.pack('<HBBII16sI',0x50aa,0,0x11,0x510000,0x4f0000,b'ota_1',0)
    return entry+struct.pack('<H',0xebeb)+b'\xff'*14+hashlib.md5(entry).digest()+b'\xff'*32

class Report(unittest.TestCase):
    def test_generated_wifi_in_other_component_needs_no_main_definitions(self):
        selection={'excluded_sources':[],'definitions':{'STACKCHAN_BUILD_MUSIC':0}}
        entries=[{'file':'/src/main/'+path,'arguments':['c++','-DSTACKCHAN_BUILD_MUSIC=0'],
                  'output':'esp-idf/main/CMakeFiles/__idf_main.dir/'+path+'.obj'}
                 for path in ['companion/boot.cc','apps/stock_shell/stock_apps.cc']]
        entries.append({'file':'/build/esp-idf/main/stock_agent_backend/stock_wifi_manager.cc',
                        'arguments':['c++'],
                        'output':'esp-idf/78__esp-wifi-connect/CMakeFiles/__idf_78__esp-wifi-connect.dir/__/main/stock_agent_backend/stock_wifi_manager.cc.obj'})
        self.assertEqual(report.audit_commands(selection,entries),2)

    def test_actual_partition_bounds_and_image_measurement(self):
        result=report.measure(b'x'*1024,table(),baseline=b'x'*2048)
        self.assertEqual(result['image_bytes'],1024)
        self.assertEqual(result['spare_bytes'],0x4f0000-1024)
        self.assertEqual(result['delta_from_baseline_bytes'],-1024)
        self.assertFalse(result['firmware_boot_verified'])

    def test_invalid_table_overflow_empty_and_threshold_fail(self):
        bad=bytearray(table());bad[10]^=1
        for image,part,minimum in [(b'',table(),0),(b'x'*0x4f0001,table(),0),
                (b'x',bytes(bad),0),(b'x',b'\xff'*32,0),
                (b'x',table(),0x4f0000),(b'x',table()[:33],0)]:
            with self.subTest(size=len(image)),self.assertRaises(ValueError):
                report.measure(image,part,min_spare=minimum)

    def test_stale_excluded_source_or_wrong_definitions_rejected(self):
        selection={'excluded_sources':['apps/instruments/instrument_apps.cc'],
                   'definitions':{'STACKCHAN_BUILD_MUSIC':0}}
        entries=[{'file':'/src/main/companion/boot.cc','arguments':['c++','-DSTACKCHAN_BUILD_MUSIC=0'],'output':'esp-idf/main/CMakeFiles/__idf_main.dir/boot.obj'},
                 {'file':'/src/main/apps/stock_shell/stock_apps.cc','arguments':['c++','-DSTACKCHAN_BUILD_MUSIC=0'],'output':'esp-idf/main/CMakeFiles/__idf_main.dir/stock.obj'}]
        self.assertEqual(report.audit_commands(selection,entries),2)
        with self.assertRaises(ValueError):
            report.audit_commands(selection,entries+[{'file':'/src/main/apps/instruments/instrument_apps.cc','arguments':[]}])
        entries[0]['arguments']=['c++','-DSTACKCHAN_BUILD_MUSIC=1']
        with self.assertRaises(ValueError):report.audit_commands(selection,entries)

    def test_duplicate_define_and_missing_mandatory_source_rejected(self):
        selection={'excluded_sources':[],'definitions':{'STACKCHAN_BUILD_MUSIC':0}}
        entries=[{'file':'/src/main/companion/boot.cc','arguments':['c++','-DSTACKCHAN_BUILD_MUSIC=0','-DSTACKCHAN_BUILD_MUSIC=1'],'output':'esp-idf/main/CMakeFiles/__idf_main.dir/boot.obj'}]
        with self.assertRaises(ValueError):report.audit_commands(selection,entries)

if __name__=='__main__':unittest.main()
