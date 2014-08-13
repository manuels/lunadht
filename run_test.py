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

def main():
	# start process
	env = dict(os.environ)
	env.update({
		'G_MESSAGES_DEBUG': 'all',
	})
	proc_id = os.spawnve(os.P_NOWAIT, './a.out', [], env)
	time.sleep(1)

	# create connections
	bus = dbus.SessionBus(mainloop=DBusGMainLoop())
	bus_name = 'org.manuel.LunaDHT'
	dht = bus.get_object(bus_name,
	                     '/org/manuel/LunaDHT')

	#print dht.join("localhost", 7786,
	print dht.join("::1", 7786,
		dbus_interface='org.manuel.LunaDHT')

	time.sleep(5)

	for i in range(5):
		print dht.put(0xDEAD, "foo\0", "bar\0", 60*60,
			dbus_interface='org.manuel.LunaDHT')
		time.sleep(1)

		print dht.get(0xDEAD, "foo\0",
			dbus_interface='org.manuel.LunaDHT')
		time.sleep(1)

	main_loop.run()

if __name__ == '__main__':
	main()
