#!/usr/bin/env python

#	vdautomount 0.1
#	easy-to-use wrapper for vdfuse
#	requires vdfuse >= v80
#
#	This program is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 2 of the License, or
#	(at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program.  If not, see <http://www.gnu.org/licenses/>.

VERSION = "0.1"

### DEFAULT OPTIONS

# if your vdfuse is not in your PATH change this to the path to your vdfuse
# executable
VDFUSE_COMMAND = "vdfuse"

### END OPTIONS

import os, sys
from optparse import OptionParser
from vboxapi import VirtualBoxManager

def main (args):
	parser = OptionParser (prog="vdautomount",
		usage="%prog [options] machine-name-or-uuid mountpoint", version="%prog " + globals()["VERSION"])
	parser.add_option ("-p", help="path to vdfuse",
		type="string", default=globals()["VDFUSE_COMMAND"], metavar="vdfuse")
	parser.add_option ("-r", help="readonly", action="store_true")
	parser.add_option ("-g", help="run in foreground", action="store_true")
	parser.add_option ("-v", help="verbose", action="store_true")
	parser.add_option ("-d", help="debug", action="store_true")
	parser.add_option ("-a", help="allow all users to read disk", action="store_true")
	parser.add_option ("-w", help="allow all users to read and write to disk", action="store_true")
	parser.add_option ("-m", help="specify which disk to mount, required if machine has more than one disk",
		type="int", default=-1, metavar="NUMBER")
	
	options, args = parser.parse_args (args=args)
	
	if len (args) != 2:
		parser.error ("invalid machine specifier or mountpoint")
	
	spec = args[0]
	mountpoint = args[1]
	vbm = VirtualBoxManager (None, None)
	
	if not (os.access (mountpoint, os.R_OK | os.W_OK) and os.path.isdir (mountpoint)):
		parser.error ("mountpoint cannot be accessed or is not a directory")
	
	try:
		machine = vbm.vbox.getMachine (spec)
	except:
		try:
			machine = vbm.vbox.findMachine (spec)
		except:
			parser.error ("couldn't find machine \"%s\"" % spec)
	
	mediums = [x.medium for x in machine.getMediumAttachments () if x.type == 3]
	
	if len (mediums) == 1:
		medium = mediums[0]
	elif options.m != -1:
		medium = mediums[options.m - 1]
	else:
		ss = sys.stdout
		sys.stdout = sys.stderr
		print "Multiple disks on machine:"
		for index, medium in enumerate (mediums):
			print "%d:\tbase:\t%s" % (index + 1, medium.base.location)
			if medium.id != medium.base.id:
				print "\tsnap:\t%s" % medium.location
		sys.stdout = ss
		parser.exit (2, "%s: specify the disk number with the -m option\n" % parser.get_prog_name ())
	
	paths = []
	while True:
		paths.append (medium.location)
		if medium.parent:
			medium = medium.parent
		else:
			break
	
	paths.reverse ()
	base = paths[0]
	diffs = paths[1:]
	
	if len (diffs) > 100:
		parser.error ("too many levels of snapshots")
	
	args = [options.p]
	for option, value in options.__dict__.iteritems ():
		if option in ("p", "m"):
			continue
		if value:
			args.append ("-" + option)
	
	args.append ("-f")
	args.append (base.encode ("UTF-8"))
	
	for x in diffs:
		args.append ("-s")
		args.append (x.encode ("UTF-8"))
	
	args.append (mountpoint)
	
	try:
		os.execvp (options.p, args)
	except OSError as e:
		parser.error ("error running vdfuse. wrong path (-p) ?")
	
if __name__ == "__main__":
	main (sys.argv[1:])
