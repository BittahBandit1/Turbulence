import importlib.util
import tempfile
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('mobile_common', ROOT/'scripts/mobile_common.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Sources(unittest.TestCase):
 def test_missing_or_wrong_game_code_rejected(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp)
   with self.assertRaises(ValueError):module.generated_sources(root)
   dol=root/'generated';rel=root/'rel';dol.mkdir();rel.mkdir()
   for directory in [dol,rel]:
    (directory/'generated.h').touch();(directory/'generated_smc.txt').touch()
    (directory/'chunks').mkdir();(directory/'chunks/test.c').touch()
   (dol/'main.dol').write_bytes(b'not Sonic Riders');(rel/'rel_image.bin').touch()
   with self.assertRaises(ValueError):module.generated_sources(root,dol,rel)
 def test_personal_sources_detected(self):
  if not (ROOT/'extracted').exists():self.skipTest('No personal game files')
  generated,rel=module.generated_sources(ROOT)
  self.assertTrue((generated/'main.dol').is_file())
  self.assertTrue((rel/'rel_image.bin').is_file())
if __name__=='__main__':unittest.main()
