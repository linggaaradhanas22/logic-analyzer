"""Validate the compiled HardwareX class-report PDFs and their build inputs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re

import pymupdf

ROOT = Path(__file__).resolve().parents[1]
LATEX = ROOT / "docs" / "hardwarex" / "latex"
BUILD = ROOT / "build" / "latex-hardwarex"
REQUIRED_HEADINGS = (
    "Hardware in context",
    "Hardware description",
    "Design files summary",
    "Bill of materials summary",
    "Build instructions",
    "Operation instructions",
    "Validation and characterization",
)
FORBIDDEN = (
    "Open source license",
    "Source file repository",
    "OSHWA",
    "HardwareX article template",
    "Before you complete this template",
    "Please replace",
    "Write a short description",
    "Max. 200 words",
    "Add at least 3 keywords",
    "Article title",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compact(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check_pdf(variant: str, manifest: dict) -> dict:
    pdf_path = LATEX / f"hardwarex-{variant}.pdf"
    require(pdf_path.is_file(), f"Missing PDF: {pdf_path}")
    key = str(pdf_path.relative_to(ROOT)).replace("\\", "/")
    require(sha256(pdf_path) == manifest["pdf_sha256"][key],
            f"PDF differs from manifest: {pdf_path}")
    log_path = BUILD / f"hardwarex-{variant}.log"
    require(log_path.is_file(), f"Missing compile log: {log_path}")
    log = log_path.read_text(encoding="utf-8", errors="replace")
    require("Overfull \\hbox" not in log, f"{variant}: overfull hbox")
    require("Missing character" not in log, f"{variant}: missing character")
    require(not re.search(r"(?m)^!", log), f"{variant}: TeX error in log")
    require("Undefined control sequence" not in log,
            f"{variant}: undefined control sequence")

    doc = pymupdf.open(pdf_path)
    require(len(doc) >= 15, f"{variant}: unexpectedly short PDF ({len(doc)} pages)")
    image_count = 0
    for page in doc:
        require(abs(page.rect.width - 595.28) < 0.2 and
                abs(page.rect.height - 841.89) < 0.2,
                f"{variant}: page is not A4")
        for font in page.get_fonts(full=True):
            require(bool(doc.extract_font(font[0])[3]),
                    f"{variant}: font not embedded: {font[3]}")
        for block in page.get_text("dict")["blocks"]:
            bbox = block["bbox"]
            x0, y0, x1, y1 = bbox
            if block.get("type") == 1:
                image_count += 1
            if block.get("type") == 0:
                block_text = compact(" ".join(
                    span["text"] for line in block.get("lines", [])
                    for span in line.get("spans", [])))
                if block_text.isdigit() and y1 > page.rect.height - 60:
                    continue
            require(x0 >= 40 and y0 >= 40 and
                    x1 <= page.rect.width - 40 and y1 <= page.rect.height - 40,
                    f"{variant}: content outside 1.5 cm margins: {bbox}")
    require(image_count >= 8,
            f"{variant}: expected at least 8 images (hardware + 4 diagrams + 3 UART), got {image_count}")
    video = "https://youtu.be/1xx84Ndi4vQ?si=ldsuYEWvYVqu2u6-"
    require(any(link.get("uri") == video for page in doc for link in page.get_links()),
            f"{variant}: demonstration video hyperlink missing")
    require("Video demonstration" in compact(doc[1].get_text()) and
            any(link.get("uri") == video for link in doc[1].get_links()),
            f"{variant}: specifications-table video row or hyperlink missing")
    page_text = [page.get_text() for page in doc]
    full = "\n".join(page_text)
    flat = compact(full)
    for heading in REQUIRED_HEADINGS:
        require(heading in flat, f"{variant}: missing heading: {heading}")
    for phrase in ("UART demonstration", "Close-up of the decoded UART frame",
                   "Live UART decoding in slot 0", "4C 41 2D 53 33 0A",
                   "Rp 13,954,700", "Weekly project activity summary",
                   "linggaaradhanas@gmail.com", "KCCI",
                    "Specifications table", "References", "Logic analyzer hardware assembly"):
        require(phrase in flat, f"{variant}: missing report content: {phrase}")
    for phrase in FORBIDDEN:
        require(phrase not in full, f"{variant}: template/journal field leaked: {phrase}")
    require("I2C" in flat or "I\u00b2C" in flat, f"{variant}: I2C mention missing")

    first = compact(page_text[0])
    lingga = "Lingga Aradhana Sahadewa"
    alvin = "Alvin Hadar"
    bagas = "Bagas Budhi Permana"
    if variant == "embedded":
        require(lingga in first, "embedded: Lingga missing from title page")
        require(alvin not in full and bagas not in full,
                "embedded: FPGA co-author leaked into the document")
        require("sole author and contributor" in flat,
                "embedded: sole-contributor statement missing")
    else:
        require(all(name in first for name in (lingga, alvin, bagas)),
                "fpga: all three authors must appear on the title page")
        require(first.index(lingga) < first.index(alvin) < first.index(bagas),
                "fpga: author order is not Lingga, Alvin, Bagas")
        require("lead author and contributor" in flat,
                "fpga: lead-contributor statement missing")
        require("Alvin Hadar developed the RTL capture FIFO/buffer and sample packing" in flat
                and "Bagas Budhi Permana developed trigger and edge detection" in flat,
                 "fpga: RTL credit statement missing")
        require("sole author and contributor" not in flat,
                "fpga: sole-contributor statement leaked")
    return {
        "pages": len(doc),
        "images": image_count,
        "a4": True,
        "embedded_fonts": True,
        "title_authors": [name for name in (lingga, alvin, bagas) if name in first],
        "headings": list(REQUIRED_HEADINGS),
        "no_overfull_or_missing_glyphs": True,
        "no_journal_fields_or_template_instructions": True,
    }


def main() -> int:
    manifest_path = LATEX / "latex_build_manifest.json"
    require(manifest_path.is_file(), f"Missing build manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest.get("engine") == "tectonic", "Manifest engine is not Tectonic")
    require("Tectonic" in manifest.get("engine_version", ""), "Unexpected engine version")
    require(set(manifest.get("variants", [])) == {"embedded", "fpga"},
            "Manifest variants incomplete")
    for key, digest in manifest["source_sha256"].items():
        path = ROOT / key
        require(path.is_file(), f"Build input missing: {key}")
        require(sha256(path) == digest, f"Build input changed after build: {key}")
    results = {variant: check_pdf(variant, manifest) for variant in ("embedded", "fpga")}
    result = {
        "engine": manifest["engine_version"],
        "variants": results,
        "build_inputs_verified": True,
        "compiled_pdfs_verified": True,
    }
    (LATEX / "latex_validation.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
