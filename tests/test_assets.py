"""Independent source hashes, full-decode equivalence, and catalog contracts."""
import hashlib
import json
import pathlib
import subprocess
import tempfile
import unittest
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXPECTED = [
    ('cao', '操操操', '操操操.MP3', '712f2e46c9e76a16c512901f0d54d39659c25cc8b19697d60986f4ee47f57602', 81503),
    ('kale', '卡了', '卡了.MP3', '3354de4a4445627a768eb58b8c7d869e5b2a59cab6491578c1d0b08c9da0f1a9', 37617),
    ('vamos', 'vamos!', 'vamos!.MP3', '36fd243d0a1e9505c54450266170c22b2ad82934716200af9923ca13f12fad65', 124134),
    ('jiana', '加纳', '加纳.mp3', '25feba4280b9dd0bec7dadbebc198e720b4de18b8f864e168cb46e65e4a7e3e4', 442480),
    ('kajian', '卡键', '卡键.mp3', 'c7f82e13a7133585ea0edde2700e4cc6bf6ad288678f3b7cb11514121eaecefb', 34560),
    ('tunzidan', '吞子弹了', '吞子弹了.mp3', 'e904040c4da84dd9c5698ec71323bd08866cc8f0d15e9b9aaed2c1059755b3f9', 47926),
    ('yare', '呀嘞呀嘞', '呀嘞呀嘞.mp3', '7804280d9f2541e189d46e394054d8ae7287c3a1727bbf9cf6385952e1030168', 67968),
    ('yare_slow', '呀嘞呀嘞（缓慢版）', '呀嘞呀嘞（缓慢版）.mp3', '3cf4d0ef491eddec80e0a969691e733fb3abeccdc64db9012c9dd8867d5d2944', 81792),
    ('kuandai', '宽带', '宽带.mp3', '98aa3af0d20dfd5c163475eaf9ae9ad83a370a71f40ad2b61c0fc308ecece0c4', 98082),
    ('yizi', '椅子', '椅子.mp3', '9d64da607a657098ab78b2aa052abdc801dcbacae4afc020fb3591a0888c8868', 104769),
    ('meibanfa', '没办法', '没办法.mp3', '62f4960edba6fec86e73c61afffec5746d7638cfda46c4f9fc8ad086e64f5e37', 242974),
    ('xiaoditu', '看小地图去了', '看小地图去了.mp3', '5680fb0ac11cab05a277fad344c21c9c0348cfbcf351e1c9ae129fc42b1d0444', 79134),
    ('shezhi', '设置', '设置.mp3', '1f9eb9261a1f02b68baf12b8bf7602f7970a8af83e3a95490d656b7610994434', 52385),
    ('ziben', '资本', '资本.mp3', 'b47284fb88bdb9832f36c797694bd6c4abd046f981e932e29ced8f06b6d3f46a', 55728),
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class AssetsTest(unittest.TestCase):
    def test_catalog_contains_all_fourteen_in_compatible_order_without_default_keys(self):
        path = ROOT / 'assets/catalog.json'
        self.assertTrue(path.is_file(), '14-clip catalog is missing')
        catalog = json.loads(path.read_text(encoding='utf-8'))
        self.assertEqual(catalog['schema_version'], 1)
        self.assertEqual(catalog['default_stop_hotkey'], '')
        self.assertEqual(catalog['product'], {'name': '开麦匣', 'english_name': 'GameVoiceBox', 'version': '1.2.0'})
        self.assertEqual([(c['id'], c['label'], c['source_filename'], c['source_sha256'], c['expected_frames']) for c in catalog['clips']], EXPECTED)
        self.assertTrue(all(c['default_hotkey'] == '' for c in catalog['clips']))
        self.assertEqual(catalog['provenance']['archive_name'], '荣誉区1.09版本.zip')
        self.assertEqual(catalog['provenance']['archive_sha256'], '17f5c2b1d9badf57c3700676cd24f32a171867f57fa473478dec9fb3e9d86f0e')
        self.assertEqual(sum(c['contains_video'] for c in catalog['clips']), 4)
        self.assertEqual(sum(c['extension_mismatch'] for c in catalog['clips']), 8)

    def test_integrity_has_fourteen_complete_runtime_wavs(self):
        integrity = json.loads((ROOT / 'assets/integrity.json').read_text(encoding='utf-8'))
        self.assertEqual(len(integrity), 14, 'runtime integrity still has the three-clip baseline')
        self.assertEqual([c['clip'] for c in integrity], [c[0] for c in EXPECTED])
        for row, (clip_id, _, filename, source_hash, frames) in zip(integrity, EXPECTED):
            with self.subTest(clip=clip_id):
                source = ROOT / 'assets/original' / filename
                runtime = ROOT / 'assets' / f'{clip_id}.wav'
                self.assertTrue(source.is_file(), 'untouched source is missing')
                self.assertEqual(sha256(source), source_hash)
                self.assertEqual(row['source'], filename)
                self.assertEqual(row['source_sha256'], source_hash)
                self.assertEqual(row['wav_sha256'], sha256(runtime))
                with wave.open(str(runtime), 'rb') as wav:
                    self.assertEqual((wav.getframerate(), wav.getnchannels(), wav.getsampwidth(), wav.getnframes(), wav.getcomptype()), (48000, 2, 2, frames, 'NONE'))
                    pcm = wav.readframes(frames)
                self.assertEqual(row['pcm_sha256'], hashlib.sha256(pcm).hexdigest())
                self.assertEqual(row['data_bytes'], frames * 4)
                self.assertEqual(row['wav_bytes'], runtime.stat().st_size)
                self.assertAlmostEqual(row['duration_seconds'], frames / 48000, places=8)
        first_three = {
            'cao': '9181c62df52a3498a53ccfb2bb86a8d018e54c9204a9a28a283d43354d39c258',
            'kale': '0bf11008f620597ef536be43107b535405539bf0fc3788449e1e3424873aae22',
            'vamos': '8c2dad3cc34d8cd728afc87042af1d97839a8887ece42e6d95b9d2c305f529f1',
        }
        for clip_id, expected_hash in first_three.items():
            self.assertEqual(sha256(ROOT / 'assets' / f'{clip_id}.wav'), expected_hash)

    def test_source_media_is_source_only_and_metadata_is_sanitized(self):
        self.assertEqual(sorted(p.name for p in (ROOT / 'assets').glob('*.wav')), sorted(f'{row[0]}.wav' for row in EXPECTED))
        self.assertFalse(any(p.suffix.lower() in {'.mp3', '.mp4', '.aac', '.m4a'} for p in (ROOT / 'assets').iterdir()))
        original = ROOT / 'assets/original'
        self.assertTrue(original.is_dir(), 'source-only original directory is missing')
        self.assertEqual(sorted(p.name for p in original.iterdir()), sorted(row[2] for row in EXPECTED))
        for path in (ROOT / 'assets').glob('*.json'):
            text = path.read_text(encoding='utf-8')
            self.assertNotIn('/workspace/', text)
            self.assertNotIn('drive.google.com', text)
            self.assertNotIn('drive_id', text)
            self.assertNotIn('local_path', text)

    def test_every_wav_equals_a_fresh_full_first_audio_stream_decode(self):
        with tempfile.TemporaryDirectory() as directory:
            for clip_id, _, filename, _, _ in EXPECTED:
                with self.subTest(clip=clip_id):
                    source = ROOT / 'assets/original' / filename
                    self.assertTrue(source.is_file(), 'full-decode source is missing')
                    derived = pathlib.Path(directory) / f'{clip_id}.wav'
                    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', str(source), '-map', '0:a:0', '-ar', '48000', '-ac', '2', '-c:a', 'pcm_s16le', '-fflags', '+bitexact', '-flags:a', '+bitexact', str(derived)], check=True)
                    self.assertEqual(sha256(derived), sha256(ROOT / 'assets' / f'{clip_id}.wav'))

    def test_generated_c_header_ids_labels_and_invalid_bounds(self):
        header = ROOT / 'src/app/clip_catalog.h'
        self.assertTrue(header.is_file(), 'generated C catalog is missing')
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            program = root / 'catalog.c'
            program.write_text('#include <stdio.h>\n#include <string.h>\n#include "clip_catalog.h"\nint main(void) {\nif (CLIP_COUNT != 14 || strcmp(clip_catalog_id(-1), "") || strcmp(clip_catalog_label(-1), "") || strcmp(clip_catalog_id(CLIP_COUNT), "") || strcmp(clip_catalog_label(CLIP_COUNT), "")) return 1;\nfor (int i=0;i<CLIP_COUNT;i++) printf("%s\\t%s\\n",clip_catalog_id(i),clip_catalog_label(i));\nreturn 0;\n}\n', encoding='utf-8')
            executable = root / 'catalog'
            subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(header.parent), str(program), '-o', str(executable)], check=True)
            lines = subprocess.check_output([str(executable)]).decode('utf-8').splitlines()
            self.assertEqual(lines, [f'{row[0]}\t{row[1]}' for row in EXPECTED])

    def test_official_zip_and_all_files_unchanged(self):
        integrity = json.loads((ROOT / 'vendor/integrity.json').read_text(encoding='utf-8'))
        self.assertEqual(len(integrity['files']), 31)
        self.assertEqual(sha256(ROOT / 'vendor/VBCABLE_Driver_Pack45.zip'), integrity['zip_sha256'])
        for name, entry in integrity['files'].items():
            path = ROOT / 'vendor/official' / name
            self.assertEqual(path.stat().st_size, entry['size'])
            self.assertEqual(sha256(path), entry['sha256'])

    def test_user_docs_describe_fourteen_clips_and_no_default_global_keys(self):
        tutorial = (ROOT / 'docs/中文使用教程.html').read_text(encoding='utf-8')
        checklist = (ROOT / 'docs/Windows验收清单.txt').read_text(encoding='utf-8')
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        for text in (tutorial, checklist, readme):
            self.assertIn('GameVoiceBox', text)
            self.assertIn('默认不设置', text)
            self.assertNotIn('RongyuSoundboardNative', text)
            self.assertNotIn('Rongyu Soundboard Native', text)
        for _, label, _, _, frames in EXPECTED:
            self.assertIn(label, tutorial)
            self.assertIn(f'{frames / 48000:.2f}', tutorial)
            self.assertIn(label, checklist)
        self.assertIn('%LOCALAPPDATA%\\Programs\\GameVoiceBox', tutorial)
        self.assertIn('%APPDATA%\\GameVoiceBox\\settings.ini', tutorial)
        self.assertIn('原始文件仅保留在源代码包', tutorial)

    def test_voice_and_driver_rights_are_not_relicensed_as_project_source(self):
        license_path = ROOT / 'LICENSE.md'
        notice_path = ROOT / 'docs/THIRD_PARTY_NOTICES.md'
        self.assertTrue(license_path.is_file(), 'source/voice rights notice missing')
        self.assertTrue(notice_path.is_file(), 'third-party notice missing')
        license_text = license_path.read_text(encoding='utf-8')
        notices = notice_path.read_text(encoding='utf-8')
        self.assertIn('尚未指定开源许可证', license_text)
        self.assertIn('语音素材', license_text)
        self.assertIn('原权利人', license_text)
        self.assertIn('Donationware', notices)
        self.assertIn('https://vb-audio.com/Services/licensing.htm', notices)
        self.assertIn('31', notices)


if __name__ == '__main__':
    unittest.main()
