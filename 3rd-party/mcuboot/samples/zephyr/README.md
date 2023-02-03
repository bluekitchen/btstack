# Zephyr sample application.

In order to successfully deploy an application using mcuboot, it is
necessary to build at least one other binary: the application itself.
It is beyond the scope of this documentation to describe what an
application is able to do, however a working example is certainly
useful.

Please see the comments in the Makefile in this directory for more
details on how to build and test this application.

Note that this sample uses the "ninja" build tool, which can be
installed on most systems using the system package manager, e.g., for
a Debian-based distro:

```
$ sudo apt-get install ninja
```

or in Fedora:

```
$ sudo dnf install ninja
```
