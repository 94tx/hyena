HYENA(7) - Miscellaneous Information Manual

# NAME

**hyena** - randomized file delivery spotted butler

# DESCRIPTION

**hyena**
is a CGI and FastCGI interface for randomized file delivery over the internet.

# INSTALLING

**hyena**
targets FreeBSD, OpenBSD and (hopefully) Linux, and requires
[kcgi](https://kristaps.bsd.lv/kcgi/)
and
[libconfig](https://hyperrealm.github.io/libconfig/)
.

	./configure
	make
	make install

On FreeBSD and OpenBSD,
**hyena**
is statically linked to make it easy to run in
chroot(2)
.

# DEPLOYMENT

**hyena**
automatically detects if it's being run as a CGI or FastCGI worker.

**hyena**
can be configured using a configuration file commonly located at
*/usr/local/etc/hyena/hyena.conf*

The configuration file provided in the source tree can be
used as an example, as it provides all the possible options the program can
be configured with.

## nginx

Use
kfcgi(8)
to run
**hyena**
:

	kfcgi -u www -s /var/run/hyena.sock \
		-U www -p /usr/local/www \
		-- /usr/local/bin/hyena

Configure
nginx(8)
to serve
**hyena**
at the desired location, for example:

	location / {
		fastcgi_pass unix:/var/run/hyena.sock
		fastcgi_split_path_info (/)(.*);
		fastcgi_param PATH_INFO $fastcgi_path_info;
		include fastcgi_params;
	}

## OpenBSD httpd

Use
kfcgi(8)
to run
**hyena**
:

	kfcgi -u www -s /var/www/run/hyena.sock \
		-U www -p /usr/local/www \
		-- /usr/local/bin/hyena

Configure
httpd(8)
to serve
**hyena**
at the desired location, for example:

	location "/*" {
		request strip 1
		fastcgi socket "/run/hyena.sock"
	}

# AUTHORS

wolf &lt;[wolf@fastmail.co.uk](mailto:wolf@fastmail.co.uk)&gt;

Debian - May 31, 2021
