"""Static offline guide content and fragment checks."""
import pathlib,unittest
from html.parser import HTMLParser
ROOT=pathlib.Path(__file__).resolve().parents[1]
class GuideTest(unittest.TestCase):
 def test_requested_output_explanation_and_upgrade(self):
  source=(ROOT/'docs/中文使用教程.html').read_text()
  for text in ('连接成功不代表已开启试听或发送','Enable send or headphone preview before playing a sound','最小化到托盘','同一安装目录','每次启动仍默认关闭'):
   self.assertIn(text,source)
 def test_no_duplicate_ids_or_broken_local_fragments(self):
  class Parser(HTMLParser):
   def __init__(self):super().__init__();self.ids=[];self.links=[]
   def handle_starttag(self,tag,attrs):
    a=dict(attrs)
    if 'id' in a:self.ids.append(a['id'])
    if tag=='a' and a.get('href','').startswith('#'):self.links.append(a['href'][1:])
  p=Parser();p.feed((ROOT/'docs/中文使用教程.html').read_text())
  self.assertEqual(len(p.ids),len(set(p.ids)));self.assertTrue(set(p.links)<=set(p.ids))
if __name__=='__main__':unittest.main()
