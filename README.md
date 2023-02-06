## xtrlock-libssh2

Minimal X display lock program using SSH for password authentication

### Introduction

This is a fork of the Debian maintained version of Ian Jackson's excellent **xtrlock**. This version does not require the binary to be installed setuid or otherwise have elevated privileges. It does, however, require that an SSH server is listening for local connections.

### Details

For more details on how **xtrlock** grabs the input devices, see xtrlock(1).

For password authentication, **xtrlock** uses the supplied password to attempt to authenticate an SSH session with a server listening on the local host. Upon successful authentication, **xtrlock** will release the mouse and keyboard.

### Dependencies

* Xlib

* libssh2

* [skalibs](https://skarnet.org/software/skalibs) &gt;= 2.13.0.0

### Setup

If you are already running an SSH server that accepts password authentication, no further setup is required. If your server does not allow password authentication, or you don't want to listen for SSH connections on the LAN, you can run a dedicated server that only listens for local connections on the port of your choice. For example, using the lightweight SSH server dropbear(8):

	# dropbear -R -p localhost:port

where port is a TCP port number. When invoking **xtrlock**, use **-p** *port* to use your port instead of the default 22. You can also change the default port used by **xtrlock** at compile time by building with

	$ make CPPFLAGS=-DSSHPORT=port

You could also use a lightweight super-server, e.g. s6-tcpserver(8) together with dropbear in service program (inetd) mode:

	# s6-tcpserver ::1 port dropbear -R -i
