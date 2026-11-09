import contextlib, importlib.util, io, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pacing_report',ROOT/'scripts/pacing-report.py')
report=importlib.util.module_from_spec(spec);spec.loader.exec_module(report)
class Report(unittest.TestCase):
 def test_negative_valid_timestamp_and_dropped_callback(self):
  with tempfile.TemporaryDirectory() as folder:
   path=Path(folder)/'pacing.csv'
   path.write_text('1,1,1,100,0,-1.000,0,2\n2,2,1,200,0,-1.000,0,3\nglass,-1.000\n')
   output=io.StringIO()
   with contextlib.redirect_stdout(output):report.main(path)
   self.assertIn('unsubmitted 1,',output.getvalue())
   self.assertIn('1 reported not displayed',output.getvalue())
 def test_single_display_interval(self):
  with tempfile.TemporaryDirectory() as folder:
   path=Path(folder)/'pacing.csv';path.write_text('glass,0.000\nglass,16.683\n')
   with contextlib.redirect_stdout(io.StringIO()) as output:report.main(path)
   self.assertIn('100.0%',output.getvalue())
if __name__=='__main__':unittest.main()
