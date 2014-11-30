#!/usr/bin/env python

import os
import dbus
import time
import sys
import socket
import signal

from dbus.mainloop.glib import DBusGMainLoop
import gobject

main_loop = gobject.MainLoop()

"""
run the DBus service like this:
XDG_DATA_DIRS=. ./lunadhtd
"""

def main(app_id, key):
	# create connections
	bus = dbus.SessionBus(mainloop=DBusGMainLoop())
	bus_name = 'org.manuel.LunaDHT'
	dht = bus.get_object(bus_name, '/org/manuel/LunaDHT')

	results = dht.get(app_id, key, dbus_interface='org.manuel.LunaDHT')
	if len(results) > 0:
		for res in results:
			print ''.join([chr(x) for x in res])
	else:
		print >>sys.stderr, "Nothing found :("

if __name__ == '__main__':
	if len(sys.argv) != 3:
		print >>sys.stderr, "Usage: %s <app_id> <key>" % sys.argv[0]
		print >>sys.stderr, 'eg. %s 4321 "What is the answer to the great question, of life, the universe and everything?"' % sys.argv[0]
	else:
		main(int(sys.argv[1]), sys.argv[2])
