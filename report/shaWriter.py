import hashlib
import re, fnmatch
import os, sys

# We find the pathname for this script
pathname = os.path.dirname(sys.argv[0])

# We find the current directory
dirList = os.listdir(os.path.abspath(pathname))

read_list = []

tempPattern = re.compile("(#|.#)")

tex = fnmatch.translate('*.tex')
texPattern = re.compile(tex)

for fName in dirList:
	if tempPattern.match(fName):
		continue
	
	if texPattern.match(fName) and fName != "version.tex":		
		read_list.append(fName)

text = ""

for fName in read_list:
	FILE = open(os.path.abspath(pathname) + os.sep + fName, "r")
	text += FILE.read()

# Find sha
m = hashlib.sha224(text)

# Write to version file
FILE = open("version.tex", "w")
FILE.write(m.hexdigest()[0:60])
