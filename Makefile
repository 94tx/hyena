include Makefile.configure

LIBS = kcgi kcgi-json libconfig
LDLIBS != pkg-config --libs --static $(LIBS)
CFLAGS != pkg-config --cflags --static $(LIBS)
CFLAGS += -W -Wall -Wextra -pedantic -std=c11
.ifdef STATIC
LDFLAGS += $(LDADD_STATIC)
.endif

CFLAGS += -DPREFIX=\"$(PREFIX)\"

hyena: hyena.c hyena.h compats.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ hyena.c compats.o $(LDLIBS)

install: hyena
	$(INSTALL_PROGRAM) hyena $(DESTDIR)$(BINDIR)
uninstall:
	rm $(DESTDIR)$(BINDIR)hyena
clean:
	rm -f hyena compats.o tags
distclean: clean
	rm -f config.h config.log Makefile.configure

.PHONY: install uninstall clean
