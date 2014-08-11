all:
	gdbus-codegen --interface-prefix=luna --generate-c-code network-bindings network.xml

	gcc -g -c `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` network-bindings.c
	gcc -g -c `pkg-config --cflags glib-2.0` dbus.c
	g++ -g -c `pkg-config --cflags libevent` -I /usr/include dht.cpp
	gcc -g -c main.c

	g++ -g network-bindings.o dbus.o dht.o main.o libcage/src/*.o `pkg-config --libs openssl glib-2.0 gio-2.0 gio-unix-2.0 libevent`
