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
		'XDG_DATA_DIRS': '.',
	})
	pid1 = os.spawnve(os.P_NOWAIT, './lunadhtd', '-b org.manuel.LunaDHTTest -p 17768'.split(), env)
	pid2 = os.spawnve(os.P_NOWAIT, './lunadhtd', [], env)
	time.sleep(1)

	# create connections
	bus = dbus.SessionBus(mainloop=DBusGMainLoop())
	bus_name = 'org.manuel.LunaDHT'
	dht = bus.get_object(bus_name, '/org/manuel/LunaDHT')

	dht.join("::1", 7786, dbus_interface='org.manuel.LunaDHT')

	for i in range(5):
		dht.put(0xDEAD, "foo\0", "bar\0", 60*60,
			dbus_interface='org.manuel.LunaDHT')
		time.sleep(1)

		res = dht.get(0xDEAD, "foo\0",
			dbus_interface='org.manuel.LunaDHT')
		if len(res) > 0:
			actual_result = ''.join([chr(x) for x in res[0]])
			expected_result = "bar\0"

			if actual_result == expected_result:
				print 'Test suceeded.'

				os.kill(pid1, signal.SIGTERM)
				os.kill(pid2, signal.SIGTERM)
				os.wait()
				os.wait()
				return

	raise 'Test failed.'

	main_loop.run()

if __name__ == '__main__':
	main()
