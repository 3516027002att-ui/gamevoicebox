import importlib.util,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class DeliveryReportTest(unittest.TestCase):
 def test_fullpack_report_uses_measured_integrity(self):
  text=(ROOT/'scripts/write_final_report.py').read_text()
  self.assertIn("assets/integrity.json",text)
  self.assertIn('GameVoiceBox-Source-1.2.1.zip',text)
  self.assertNotIn('81,503 /37,617 /124,134',text)
  self.assertIn('不设置任何默认全局快捷键',text)
  self.assertNotIn('未上传Drive或对外发布',text)
if __name__=='__main__':unittest.main()
