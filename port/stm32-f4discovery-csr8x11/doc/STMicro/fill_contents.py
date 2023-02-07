import sqlite3
import re
import os
from collections import OrderedDict

svd_path = os.path.dirname(__file__)
# Put database file in the same folder and provide name here
db_path = svd_path + "/cube-finder-db.db"
# Table containing part numbers
pn_table = "cpn"
# Column containing part numbers
pn_column = "cpn"

def regexp(expr, item):
    reg = re.compile(expr)
    return reg.search(item) is not None

files = os.listdir(svd_path)
regexes = OrderedDict()
for file in files:
    if file[-4:] == ".svd":
        name=file[0:-4]
        if (name.find("_") != -1):
            name = name[0:name.find("_")]
        # exclude DISCOVERY since otherwise some part numbers match with regular expression
        regexes[file] = "^(?=^" + name.replace("x","[A-Z0-9]") + "[A-Z0-9]*$)(?=^((?!DISCOVERY).)*$).*$"

regexes = OrderedDict(sorted(regexes.items(), key=lambda item: item[1]))

con = sqlite3.connect(db_path)
con.create_function("REGEXP", 2, regexp)
cursor = con.cursor()
contents = [];
for file, regex in regexes.items():
    res = str(cursor.execute("SELECT GROUP_CONCAT(mcus,', ') FROM (SELECT DISTINCT SUBSTRING(cpn, 1, 11) as mcus FROM (SELECT " + pn_column + " FROM " + pn_table + " WHERE " + pn_column + " REGEXP '" + regex + "'))").fetchone())
    if res == "(None,)":
        contents.append(file)
    else:
        contents.append(res[2:-3] + ", " + file)

with open(svd_path + '/Contents.txt', 'w') as f:
    for line in contents:
        f.write(f"{line}\n")

con.close()