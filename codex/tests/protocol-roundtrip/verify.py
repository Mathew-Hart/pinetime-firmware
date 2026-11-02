#!/usr/bin/env python3
"""Cross-language wire check: Python phone encoder -> actual C++ watch decoder."""
from pathlib import Path
from decimal import Decimal, ROUND_HALF_UP
import hashlib, importlib.util, json, os, shlex, subprocess, sys
TEST=Path(__file__).resolve().parent
ROOT=TEST.parent.parent
SOURCE=ROOT.parent/'src'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def scaled(value,n=1): return int((Decimal(str(value))*n).quantize(Decimal(1),rounding=ROUND_HALF_UP))
def main():
    spec=importlib.util.spec_from_file_location('gps_wire_roundtrip_bridge',ROOT/'companion/gps_bridge.py')
    module=importlib.util.module_from_spec(spec);sys.modules[spec.name]=module;spec.loader.exec_module(module)
    (TEST/'output').mkdir(exist_ok=True)
    binary=TEST/'output/decode'
    subprocess.run(shlex.split(os.environ.get('CXX','clang++'))+['-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(SOURCE),str(TEST/'decode.cpp'),'-o',str(binary)],check=True)
    fixtures=json.loads((ROOT/'companion/protocol-fixtures.json').read_text())
    cases=[]
    for case in fixtures['cases']:
        sample=case['sample']; packet=module.encode(module.Sample(**sample))
        assert packet.hex()==case['hex'] and len(packet)==20,case['name']
        flags=sum(bit for field,bit in [('latitude',1),('altitude',2),('used',4),('in_view',8),('hdop',16)] if sample.get(field) is not None)
        expected={'fix':sample['fix'],'flags':flags,'used':sample.get('used') if sample.get('used') is not None else 255,
          'view':sample.get('in_view') if sample.get('in_view') is not None else 255,'age':sample['age'],
          'lat':scaled(sample.get('latitude') or 0,10**7),'lon':scaled(sample.get('longitude') or 0,10**7),
          'alt':scaled(sample.get('altitude') or 0),'hdop':scaled(sample['hdop'],100) if sample.get('hdop') is not None else 65535}
        decoded=json.loads(subprocess.check_output([str(binary),packet.hex()],text=True))
        assert decoded==expected,(case['name'],decoded,expected)
        cases.append(case['name'])
    result={'result':'PASS','cases':cases,'cross_language_cases':len(cases),'actual_python_encoder_and_cpp_decoder':True,
      'sanitizers':['ASan','UBSan'],'source_sha256':{str(p.relative_to(ROOT.parent)):sha(p) for p in [SOURCE/'components/ble/GpsData.h',ROOT/'companion/gps_bridge.py',ROOT/'companion/protocol-fixtures.json',TEST/'decode.cpp',TEST/'verify.py']},
      'uploaded':False,'phone_actions':[]}
    (TEST/'test-receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='source_sha256'},indent=2))
if __name__=='__main__': main()
