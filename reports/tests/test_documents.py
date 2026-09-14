"""Check report links, source listings, and the provenance of measured data."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DocumentTests(unittest.TestCase):
    def test_all_local_links_resolve(self):
        for path in [ROOT / "README.md", *ROOT.glob("report_*.md")]:
            text = re.sub(r"```.*?```", "", path.read_text(), flags=re.S)
            for target in re.findall(r"\]\(([^)]+)\)", text):
                name = target.split("#")[0]
                if name and not name.startswith(("http:", "https:", "mailto:")):
                    self.assertTrue((path.parent / name).exists(), (path.name, target))

    def test_source_listings_match_compilable_files(self):
        total = 0
        for path in ROOT.glob("report_*.md"):
            for name, code in re.findall(r"### (pcm/[^\n]+\.c)\n\n```c\n(.*?)\n```", path.read_text(), re.S):
                self.assertEqual(code, (ROOT / name).read_text().rstrip(), (path.name, name))
                total += 1
        self.assertEqual(total, 11)

    def test_measurements_match_source_snapshot(self):
        data = json.loads((ROOT / "assets/results.json").read_text())
        for name, expected in data["source_sha256"].items():
            self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), expected, name)

    def test_timing_tables_match_measurements(self):
        data = json.loads((ROOT / "assets/results.json").read_text())
        dft = (ROOT / "report_dft.md").read_text()
        fft = (ROOT / "report_fft.md").read_text()
        for row in data["fourier"]["timings"]:
            n, m = row["N"], row["median_s"]
            self.assertIn(f'| {n:,} | {m["DFT"]*1000:.6f} | {m["IDFT"]*1000:.6f} |', dft)
            self.assertIn(f'| {n:,} | {m["DFT"]*1000:.6f} | {m["IDFT"]*1000:.6f} | '
                          f'{m["FFT"]*1e6:.3f} | {m["IFFT"]*1e6:.3f} | {m["DFT"]/m["FFT"]:,.1f} |', fft)

    def test_pdf_exports_match_markdown(self):
        manifest = json.loads((ROOT / "pdf/manifest.json").read_text())
        self.assertEqual(len(manifest), 8)
        for name, entry in manifest.items():
            pdf = ROOT / "pdf" / name
            markdown = ROOT / (pdf.stem + ".md")
            self.assertEqual(hashlib.sha256(markdown.read_bytes()).hexdigest(), entry["markdown_sha256"], name)
            self.assertEqual(hashlib.sha256(pdf.read_bytes()).hexdigest(), entry["pdf_sha256"], name)


if __name__ == "__main__":
    unittest.main()
