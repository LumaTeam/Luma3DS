import sys;
import os

f1 = 0
f2 = 0
f3 = 0

f1size = 0

with open(sys.argv[1], "rb+") as file1:
	with open(sys.argv[2], "rb") as file2:
		f1 = file1.read()
		f2 = file2.read()
		file1.seek(int(sys.argv[3], 16), os.SEEK_SET)
		file1.write(f2);
		