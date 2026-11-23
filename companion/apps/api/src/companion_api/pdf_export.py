from __future__ import annotations

from typing import Iterable


def _escape_pdf_text(text: str) -> str:
    ascii_text = text.encode("latin-1", errors="replace").decode("latin-1")
    return ascii_text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def build_simple_pdf(title: str, lines: Iterable[str]) -> bytes:
    """Generates a valid standalone multi-line PDF 1.4 report with zero native dependencies."""
    all_lines = [title, "=" * min(72, len(title)), *lines]
    # Paginate cleanly at 48 lines per A4 page
    pages: list[list[str]] = [all_lines[i : i + 48] for i in range(0, len(all_lines), 48)] or [[]]

    objects: list[bytes] = []
    # 1: Catalog, 2: Pages, 3: Font (Helvetica), then per-page (Page + Content)
    page_ids: list[int] = []
    next_id = 4
    page_objs: list[tuple[int, int, bytes]] = []

    for page_lines in pages:
        content_ops = ["BT", "/F1 10 Tf", "50 785 Td", "14 TL"]
        for idx, line in enumerate(page_lines):
            if idx == 0:
                content_ops.append(f"({_escape_pdf_text(line)}) Tj")
            else:
                content_ops.append(f"T* ({_escape_pdf_text(line)}) Tj")
        content_ops.append("ET")
        stream_bytes = "\n".join(content_ops).encode("latin-1")
        page_id = next_id
        content_id = next_id + 1
        next_id += 2
        page_ids.append(page_id)
        page_objs.append((page_id, content_id, stream_bytes))

    kids = " ".join(f"{pid} 0 R" for pid in page_ids)
    objects.append(b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n")
    objects.append(f"2 0 obj\n<< /Type /Pages /Kids [{kids}] /Count {len(page_ids)} >>\nendobj\n".encode("ascii"))
    objects.append(b"3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n")

    for page_id, content_id, stream_bytes in page_objs:
        objects.append(
            f"{page_id} 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
            f"/Resources << /Font << /F1 3 0 R >> >> /Contents {content_id} 0 R >>\nendobj\n".encode("ascii")
        )
        objects.append(
            f"{content_id} 0 obj\n<< /Length {len(stream_bytes)} >>\nstream\n".encode("ascii")
            + stream_bytes
            + b"\nendstream\nendobj\n"
        )

    out = bytearray(b"%PDF-1.4\n")
    offsets: list[int] = [0]
    for obj in objects:
        offsets.append(len(out))
        out.extend(obj)

    xref_pos = len(out)
    out.extend(f"xref\n0 {len(offsets)}\n".encode("ascii"))
    out.extend(b"0000000000 65535 f \n")
    for off in offsets[1:]:
        out.extend(f"{off:010d} 00000 n \n".encode("ascii"))
    out.extend(
        f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref_pos}\n%%EOF\n".encode("ascii")
    )
    return bytes(out)
