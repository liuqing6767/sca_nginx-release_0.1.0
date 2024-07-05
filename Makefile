
build:
	$(MAKE) -f objs/Makefile

install:
	$(MAKE) -f objs/Makefile install

clean:
	rm -rf Makefile objs

upgrade:
	/usr/local/nginx/sbin/nginx -t

	# upgrade compatibility from 0.1.x to 0.2.x
	cp /usr/local/nginx/logs/nginx.pid /usr/local/nginx/logs/nginx.pid.oldbin

	kill -USR2 `cat /usr/local/nginx/logs/nginx.pid`
	sleep 1
	test -f /usr/local/nginx/logs/nginx.pid.oldbin

	# upgrade compatibility from 0.1.x to 0.2.x
	cp /usr/local/nginx/logs/nginx.pid /usr/local/nginx/logs/nginx.pid.newbin

	kill -WINCH `cat /usr/local/nginx/logs/nginx.pid.oldbin`
