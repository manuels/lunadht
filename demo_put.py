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

def main(app_id, key, value):
	# create connections
	bus = dbus.SessionBus(mainloop=DBusGMainLoop())
	bus_name = 'org.manuel.LunaDHT'
	dht = bus.get_object(bus_name, '/org/manuel/LunaDHT')

	ttl = 60*60 # one hour
	res = dht.put(app_id, key, value, ttl, dbus_interface='org.manuel.LunaDHT')

if __name__ == '__main__':
	if len(sys.argv) != 4:
		print >>sys.stderr, "Usage: %s <app_id> <key> <value>" % sys.argv[0]
		print >>sys.stderr, 'eg. %s 4321 "What is the answer to the great question, of life, the universe and everything?" "42"' % sys.argv[0]
		print sys.argv
	else:
		main(int(sys.argv[1]), sys.argv[2], sys.argv[3])
