# http://security.stackexchange.com/questions/24444/what-is-the-most-hardened-set-of-options-for-gcc-compiling-c-c

CFLAGS=-D _FORTIFY_SOURCE=2 -O2 -Wall -g -Werror -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -fPIE -ftrapv -Wl,-z,relro,-z,now
BUILD=lunadht-0.0.1-0

all: build

build:
	gdbus-codegen --interface-prefix=org.manuel \
		--annotate "org.manuel.LunaDHT.get()[key]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.get()[results]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.put()[key]" org.gtk.GDBus.C.ForceGVariant true \
		--annotate "org.manuel.LunaDHT.put()[value]" org.gtk.GDBus.C.ForceGVariant true \
		--generate-c-code network-bindings network.xml

	#mkdir -p ./glib-2.0/schemas
	#glib-compile-schemas --targetdir=./glib-2.0/schemas .

	gcc -c ${CFLAGS} `pkg-config --cflags glib-2.0 gio-2.0 gio-unix-2.0` network-bindings.c
	gcc -c ${CFLAGS} `pkg-config --cflags glib-2.0` dbus.c
	gcc -c ${CFLAGS} `pkg-config --cflags glib-2.0` settings.c
	g++ -c ${CFLAGS} `pkg-config --cflags libevent` -I /usr/include dht.cpp
	gcc -c ${CFLAGS} main.c
	gcc -c ${CFLAGS} safe_assert.c

	g++ ${CFLAGS} -o lunadhtd \
		network-bindings.o dbus.o dht.o main.o safe_assert.o settings.o \
		libcage/src/*.o \
		`pkg-config --libs openssl glib-2.0 gio-2.0 gio-unix-2.0 libevent`

packaging: build
	rm -Rf ${BUILD}

	mkdir -p ${BUILD}/DEBIAN
	cp debian/control ${BUILD}/DEBIAN/

	mkdir -p ${BUILD}/usr/bin/
	mkdir -p ${BUILD}/usr/share/glib-2.0/schemas/
	mkdir -p ${BUILD}/usr/share/dbus-1/services/

	cp lunadhtd ${BUILD}/usr/bin/
	cp nodes.gschema.xml ${BUILD}/usr/share/glib-2.0/schemas/
	cp org.manuel.lunadht.service ${BUILD}/usr/share/dbus-1/services/

	dpkg-deb --build ${BUILD}
