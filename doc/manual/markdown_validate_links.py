#!/usr/bin/env python3
"""Validate internal Markdown and HTML links used by the BTstack Manual.

MkDocs 1.5 validates anchors created from headings, but does not recognize the
legacy ``<a name=...>`` anchors used for code listings. This validator covers
both forms and fails the build for a missing local anchor.
"""

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


MARKDOWN_LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^\s)]+)(?:\s+[^)]*)?\)")
HTML_LINK_RE = re.compile(r"<a\s+[^>]*?href=[\"']([^\"']+)[\"'][^>]*>", re.IGNORECASE)
HTML_ANCHOR_RE = re.compile(r"<a\s+[^>]*?(?:name|id)=[\"']([^\"']+)[\"'][^>]*>", re.IGNORECASE)
EXPLICIT_ANCHOR_RE = re.compile(r"\{#([^\s}]+)")
HEADING_RE = re.compile(r"^\s{0,3}#{1,6}\s+(.+?)\s*$")


def heading_slug(text):
    """Return the basic Python-Markdown/MkDocs heading slug."""
    text = re.sub(r"\{#[^}]+\}", "", text)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"[*_~]", "", text)
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE).strip().lower()
    return re.sub(r"[-\s]+", "-", text)


def anchors_in(path):
    anchors = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        anchors.update(HTML_ANCHOR_RE.findall(line))
        anchors.update(EXPLICIT_ANCHOR_RE.findall(line))
        heading = HEADING_RE.match(line)
        if heading:
            slug = heading_slug(heading.group(1))
            if slug:
                anchors.add(slug)
    return anchors


def local_fragment_target(url, source, docs_root):
    parsed = urlsplit(url)
    if parsed.scheme or parsed.netloc or not parsed.fragment:
        return None
    if parsed.path.startswith("/"):
        return None

    fragment = unquote(parsed.fragment)
    target = source if not parsed.path else (source.parent / unquote(parsed.path)).resolve()
    try:
        target.relative_to(docs_root)
    except ValueError:
        return None
    if target.suffix != ".md":
        return None
    return target, fragment


def links_in(path):
    text = path.read_text(encoding="utf-8")
    for line_number, line in enumerate(text.splitlines(), start=1):
        for match in MARKDOWN_LINK_RE.finditer(line):
            yield line_number, match.group(1)
        for match in HTML_LINK_RE.finditer(line):
            yield line_number, match.group(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--input", default="docs", help="generated Markdown folder")
    args = parser.parse_args()

    docs_root = Path(args.input).resolve()
    markdown_files = sorted(docs_root.rglob("*.md"))
    anchors = {path: anchors_in(path) for path in markdown_files}
    failures = []

    for source in markdown_files:
        for line_number, url in links_in(source):
            resolved = local_fragment_target(url, source, docs_root)
            if not resolved:
                continue
            target, fragment = resolved
            if target not in anchors:
                failures.append((source, line_number, url, "target Markdown file does not exist"))
            elif fragment not in anchors[target]:
                failures.append((source, line_number, url, "anchor does not exist"))

    if failures:
        for source, line_number, url, reason in failures:
            print("%s:%d: error: %s: %s" % (source.relative_to(docs_root), line_number, reason, url))
        return 1

    print("Validated internal Markdown anchors in %d files." % len(markdown_files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
