# hyena: randomized file delivery spotted butler

## Description
**hyena** is a CGI and FastCGI interface for randomized file delivery.

## Installing
**hyena** targets FreeBSD and OpenBSD as primary distributions, for
instructions on installing and running on Linux, see
[Installing on Linux](#installing-on-linux).

**hyena** requires the following libraries:
- [kcgi](https://kristaps.bsd.lv/kcgi/)
  - packaged on FreeBSD and OpenBSD as `kcgi`
  - needs to be compiled from source on Linux, see [Installing on Linux](#installing-on-linux)
- [libconfig](https://hyperrealm.github.io)
  - packaged on FreeBSD and OpenBSD as `libconfig`
  - packaged on Debian as `libconfig-dev`
- `bmake` on Linux systems.

Replacing `make` with `bmake` if you're on a Linux system:
```
$ ./configure
$ make
$ sudo make install
```

### Installing on Linux
Linux usage requires special care as `kcgi` seems to be broken as `seccomp` support
was pulled out of the library.

```
$ cd /tmp
$ wget https://kristaps.bsd.lv/kcgi/snapshots/kcgi.tgz
$ tar xf kcgi.tgz
$ cd kcgi-0.13.0
$ ./configure
$ sed -i 's/HAVE_SECCOMP_FILTER 1/HAVE_SECCOMP_FILTER 0/' config.h
$ bmake
$ bmake install
```

After following these steps, you can proceed to compile as normal.

## Deployment
**hyena** detects if it's being run as a CGI or FastCGI worker, and it can be
further configured with a configuration file commonly located at
`/usr/local/etc/hyena/hyena.conf`.

It is recommended to use `kfcgi` (from `kcgi`) to run as a FastCGI worker.
```
$ kfcgi -u www -s /var/run/hyena.sock \e
        -U www -p /usr/local/www \e
        -- /usr/local/bin/hyena
```

Some example configurations for NGINX and OpenBSD `httpd` are listed below.

### NGINX web server
```
location / {
    fastcgi_pass unix:/var/run/hyena.sock
    fastcgi_split_path_info (/)(.*);
    fastcgi_param PATH_INFO $fastcgi_path_info;
    include fastcgi_params;
}
```

### OpenBSD httpd
```
location "/*" {
    request strip 1
    fastcgi socket "/run/hyena.sock"
}
```
