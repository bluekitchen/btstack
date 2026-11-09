#!/usr/bin/env python3
"""Add generated pages to the HTML navigation and retain a combined PDF page."""

import json


def update_navigation(title, index, combined, pages):
    marker = "  - '" + title + "': " + index + "\n"
    with open("mkdocs.yml") as source:
        config = source.read()
    entries = "  - " + json.dumps(title) + ":\n"
    entries += "    - 'Overview': " + index + "\n"
    for page_title, path in pages:
        entries += "    - " + json.dumps(page_title) + ": " + path + "\n"
    if marker not in config:
        raise ValueError("Missing navigation entry: " + marker.strip())
    with open("mkdocs.yml", "w") as output:
        output.write(config.replace(marker, entries))
    with open("mkdocs-latex.yml") as source:
        config = source.read()
    if marker not in config:
        raise ValueError("Missing PDF navigation entry: " + marker.strip())
    with open("mkdocs-latex.yml", "w") as output:
        output.write(config.replace(marker, "  - " + json.dumps(title) + ": " + combined + "\n"))
