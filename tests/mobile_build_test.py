import importlib.util
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
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
 def test_native_rel_sources_accepted(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);dol=root/'dol';rel=root/'rel';dol.mkdir();rel.mkdir()
   for directory in [dol,rel]:
    (directory/'generated.h').touch();(directory/'generated_smc.txt').touch()
    (directory/'chunks').mkdir()
   (dol/'main.dol').write_bytes(b'test DOL');(dol/'chunks/test.c').touch()
   (rel/'generated.h').write_text('#define DOLRECOMP_BACKEND_LLVM 1')
   (rel/'rel_image.bin').write_bytes(b'test REL');(rel/'chunks/test.o').touch()
   with patch.object(module,'DOL_SHA256',module.sha256(b'test DOL').hexdigest()):
    self.assertEqual(module.generated_sources(root,dol,rel),(dol.resolve(),rel.resolve()))
    (rel/'chunks/test.o').unlink()
    with self.assertRaises(ValueError):module.generated_sources(root,dol,rel)
 def test_android_function_chunks_use_separate_cache(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);source=root/'input';source.mkdir();(source/'main.dol').write_bytes(b'test')
   compiler=root/'lib/DolRecomp/build-llvm/dolrecomp';compiler.parent.mkdir(parents=True);compiler.touch()
   # A fixed-partition cache must never satisfy the optimized build.
   old=root/'build/llvm-android/dol/generated';old.mkdir(parents=True);(old/'generated.h').touch()
   def generate(command,**kwargs):
    output=Path(command[-1])/'generated';output.mkdir();(output/'generated.h').touch()
   with patch.object(module.subprocess,'run',side_effect=generate) as run:
    output=module.llvm_main(root,source,'aarch64-unknown-linux-android26','android')
    self.assertEqual(output,root/f'build/llvm-android-functions-v2-{module.sha256(b"").hexdigest()[:12]}/dol/generated')
    self.assertTrue((output/'.generation-complete').is_file())
    env=run.call_args.kwargs['env']
    self.assertEqual(env['DOLRECOMP_LLVM_FUNCTION_CHUNKS'],'1')
    self.assertEqual(env['DOLRECOMP_LLVM_FUNCTION_CHUNK_MAX'],'4096')
    self.assertNotIn('--semantics',run.call_args.args[0])
    self.assertEqual((output/'main.dol').read_bytes(),b'test')
    module.llvm_main(root,source,'aarch64-unknown-linux-android26','android')
    self.assertEqual(run.call_count,1)
   compiler.write_bytes(b'changed compiler')
   with patch.object(module.subprocess,'run',side_effect=generate) as run:
    changed=module.llvm_main(root,source,'aarch64-unknown-linux-android26','android')
    self.assertNotEqual(changed,output)
    self.assertEqual(run.call_count,1)
   # A header written before an interrupted generation is not a valid cache.
   (changed/'.generation-complete').unlink()
   def regenerate(command,**kwargs):
    (Path(command[-1])/'generated/generated.h').touch()
   with patch.object(module.subprocess,'run',side_effect=regenerate) as run:
    module.llvm_main(root,source,'aarch64-unknown-linux-android26','android')
    self.assertEqual(run.call_count,1)
 def test_personal_sources_detected(self):
  if not (ROOT/'extracted').exists():self.skipTest('No personal game files')
  generated,rel=module.generated_sources(ROOT)
  self.assertTrue((generated/'main.dol').is_file())
  self.assertTrue((rel/'rel_image.bin').is_file())
if __name__=='__main__':unittest.main()
