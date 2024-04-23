import sqlite3

con = sqlite3.connect('data/.coverage')
cur = con.cursor()
res = cur.execute("SELECT * from arc")
print(res.fetchall())