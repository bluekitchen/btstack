#!/usr/bin/env python3
#
# Filter coverage reported by lcov/genhtml
#
# Copyright 2012 BlueKitchen GmbH
#
from lxml import html
import sys
import os

coverage_html_path = "coverage-html/index.html"

summary = {}

def update_category(name, value):
    old = 0
    if name in summary:
        old = summary[name]
    summary[name] = old + value

def list_category_table():
    row = "%-11s |%11s |%11s |%11s"

    print( row % ('', 'Hit', 'Total', 'Coverage'))
    print("------------|------------|------------|------------")

    categories = [ 'Line', 'Function', 'Branch'];
    for category in categories:
        hit   = summary[category + "_hit"]
        total = summary[category + "_total"]
        coverage = 100.0 * hit / total
        print ( row % ( category, hit, total, "%.1f" % coverage))

filter = sys.argv[1:]

print("\nParsing HTML Coverage Report")

tree = html.parse(coverage_html_path)
files = tree.xpath("//td[@class='coverFile']")
for file in files:
    row = file.getparent()
    children = row.getchildren()
    path = children[0].text_content()
    lineCov     = children[3].text_content()
    functionCov = children[5].text_content()
    branchCov   = children[7].text_content()
    (lineHit,     lineTotal)     = [int(x) for x in lineCov.split("/")]
    (functionHit, functionTotal) = [int(x) for x in functionCov.split("/")]
    (branchHit,   branchTotal)   = [int(x) for x in branchCov.split("/")]
    # print(path, lineHit, lineTotal, functionHit, functionTotal, branchHit, branchTotal)

    # filter
    if path in filter:
        print("- Skipping " + path)
        continue

    print("- Adding   " + path)
    update_category('Line_hit', lineHit)
    update_category('Line_total', lineTotal)
    update_category('Function_hit', functionHit)
    update_category('Function_total', functionTotal)
    update_category('Branch_hit', branchHit)
    update_category('Branch_total', branchTotal)

print("")
list_category_table()
