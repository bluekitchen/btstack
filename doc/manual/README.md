# BTstack Manual

This directory builds the BTstack Manual as HTML and PDF. The authored
Markdown lives in `docs-template/`; the build scripts add generated API,
example, port, and GATT documentation.

## Prerequisites

Install the Python dependencies listed in `requirements.txt`. Building the PDF
also requires Pandoc, WeasyPrint, pdfLaTeX, and qpdf.

## Build

```sh
make html   # HTML site in btstack/
make pdf    # PDF in btstack.pdf
make        # both
```

`make docs` first creates `docs-markdown/` from the templates and generated
content, then produces the processed `docs/` input used by both output formats.
The HTML build uses MkDocs and post-processes code blocks. The PDF build
collects the Markdown into `latex/btstack_generated.md`, renders it with
Pandoc and WeasyPrint, creates the cover page with pdfLaTeX, and combines both
PDFs with qpdf.

Use `make clean` to remove generated documentation and build outputs before a
full rebuild.
