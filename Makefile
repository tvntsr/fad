.PHONY: all
all:
	cd src && $(MAKE)


.PHONY: clean
clean:
	cd src && $(MAKE) clean

.PHONY: install
install:
	cp cfg/fad.conf /etc/
	cd src && $(MAKE) install

.PHONY: uninstall
uninstall:
	rm -f /etc/fad.conf
	cd src && $(MAKE) uninstall
