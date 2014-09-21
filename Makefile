all:
	gdbus-codegen --interface-prefix=org.manuel \
		--annotate "org.manuel.LunaDHT.get()[key]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.get()[results]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.put()[key]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.put()[value]" org.gtk.GDBus.C.ForceGVariant true \
		--generate-c-code network-bindings network.xml

	mkdir -p ./glib-2.0/schemas
	glib-compile-schemas --targetdir=./glib-2.0/schemas .

	gcc -g -c -Wall `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` network-bindings.c
	gcc -g -c -Wall `pkg-config --cflags glib-2.0` dbus.c
	gcc -g -c -Wall `pkg-config --cflags glib-2.0` settings.c
	g++ -g -c -Wall `pkg-config --cflags libevent` -I /usr/include dht.cpp
	gcc -g -c -Wall main.c
	gcc -g -c -Wall safe_assert.c

	g++ -g -Wall -o lunadhtd \
		network-bindings.o dbus.o dht.o main.o safe_assert.o settings.o \
		libcage/src/*.o \
		`pkg-config --libs openssl glib-2.0 gio-2.0 gio-unix-2.0 libevent`
