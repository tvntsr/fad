.PHONY: all
all:
	cd src && $(MAKE)


.PHONY: clean
clean:
	cd src && $(MAKE) clean

.PHONY: install
install:
	cp cfg/fad.conf /etc/
	cp cfg/fad.service /lib/systemd/system/
	chmod 644 /lib/systemd/system/fad.service
	systemctl daemon-reload
	systemctl enable fad
	cd src && $(MAKE) install

.PHONY: uninstall
uninstall:
	rm -f /etc/fad.conf
	systemctl stop fad
	systemctl disable fad
	systemctl daemon-reload
	rm -f /lib/systemd/system/fad.service
	cd src && $(MAKE) uninstall
