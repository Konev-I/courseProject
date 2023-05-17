all: PICOFoxweb

clean:
	@rm -rf *.o
	@rm -rf PICOFoxweb

PICOFoxweb: main.o httpd.o
	gcc -o PICOFoxweb $^ -lpam -lpam_misc

main.o: main.c httpd.h
	gcc -c -o main.o main.c -lpam -lpam_misc

httpd.o: httpd.c httpd.h
	gcc -c -o httpd.o httpd.c -lpam -lpam_misc

install: PICOFoxweb
	useradd -c "PICOFoxweb user" -r -s /sbin/nologin -d /var/www/webroot labuser
	install -o root -g root -m 0755 PICOFoxweb /usr/local/sbin/
	install -o root -g root -m 0644 courseProject.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl restart courseProject.service
	mkdir -p /var/www
	cp -r webroot -t /var/www/
	chown -R labuser:labuser /var/www/webroot
	
uninstall:
	systemctl stop courseProject
	rm -rf /var/www/webroot
	rm -f /usr/local/sbin/PICOFoxweb
	rm -f /etc/systemd/system/courseProject.service
	systemctl daemon-reload
	userdel -f labuser

