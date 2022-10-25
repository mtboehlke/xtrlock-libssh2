/*
 * xtrlock.c
 *
 * X Transparent Lock
 *
 * Copyright (C)1993,1994 Ian Jackson
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <arpa/inet.h>
#include <libssh2.h>
#include <skalibs/socket.h>
#include <skalibs/strerr2.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xos.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <values.h>

#ifdef MULTITOUCH
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#endif

#include "lock.bitmap"
#include "mask.bitmap"
#include "patchlevel.h"

static char ipadr[sizeof(struct in6_addr)];
const char *PROG;

static Display *display;
static Window window;

#define TIMEOUTPERATTEMPT 30000
#define MAXGOODWILL  (TIMEOUTPERATTEMPT*5)
#define INITIALGOODWILL MAXGOODWILL
#define GOODWILLPORTION 0.3

#define SSHPORT 22

static struct passwd *pw;

static
int passwordok(const char *s)
{
	int sock, ret = 0;
	LIBSSH2_SESSION *session;
	if (libssh2_init(0)) {
		strerr_warnw1sys("libssh2_init");
		return ret;
	}
	if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		strerr_warnwu1sys("create socket");
		return ret;
	}
	if (socket_connect6(sock, ipadr, SSHPORT)) {
		strerr_warnwu1sys("connect to socket");
		return ret;
	}
	if (!(session = libssh2_session_init())) {
		strerr_warnwu1sys("initialize libssh2 session");
		return ret;
	}
	if (libssh2_session_handshake(session, sock)) {
		strerr_warn1sys("libssh2 handshake failed");
		return ret;
	}
	ret = libssh2_userauth_password(session, pw->pw_name, s);
	libssh2_session_disconnect(session, "xtrlock normal disconnect");
	libssh2_session_free(session);
	close(sock);
	libssh2_exit();
	return !ret;
}

#if MULTITOUCH
static XIEventMask evmask;

/* (Optimistically) attempt to grab multitouch devices which are not
 * intercepted via XGrabPointer. */
static void
handle_multitouch(Cursor cursor)
{
  XIDeviceInfo *info;
  int xi_ndevices;

  info = XIQueryDevice(display, XIAllDevices, &xi_ndevices);

  for (int i=0; i < xi_ndevices; i++) {
    XIDeviceInfo *dev = &info[i];

    for (int j=0; j < dev->num_classes; j++) {
      if (dev->classes[j]->type == XITouchClass &&
          dev->use == XISlavePointer) {
        XIGrabDevice(display, dev->deviceid, window, CurrentTime, cursor,
                     GrabModeAsync, GrabModeAsync, False, &evmask);
      }
    }
  }
  XIFreeDeviceInfo(info);
}
#endif

int
main(int argc, char **argv)
{
  PROG = argv[0];
  if (inet_pton(AF_INET6, "::1", ipadr) < 0)
    strerr_dief1sys(1, "inet_pton");
  XEvent ev;
  KeySym ks;
  char cbuf[10], rbuf[128]; /* shadow appears to suggest 127 a good value here */
  int clen, rlen=0;
  long goodwill= INITIALGOODWILL, timeout= 0;
  XSetWindowAttributes attrib;
  Cursor cursor;
  Pixmap csr_source,csr_mask;
  XColor csr_fg, csr_bg, dummy, black;
  int ret, screen, blank = 0, fork_after = 0;
  struct timeval tv;
  int tvt, gs;

  if (getenv("WAYLAND_DISPLAY"))
	strerr_warnw1sys("Wayland X server detected: xtrlock"
		" cannot intercept all user input. See xtrlock(1).");

  while (argc > 1) {
    if ((strcmp(argv[1], "-b") == 0)) {
      blank = 1;
      argc--;
      argv++;
    } else if ((strcmp(argv[1], "-f") == 0)) {
      fork_after = 1;
      argc--;
      argv++;
    } else
		strerr_die3x(1, "xtrlock (version", program_version, ");"
			" usage: xtrlock [-b] [-f]");
  }

  errno=0;
  if (!(pw = getpwuid(getuid())))
	strerr_diefu1sys(1, "determine user information");

  display= XOpenDisplay(0);

  if (display==NULL)
	strerr_diefu1x(1, "open display");

#ifdef MULTITOUCH
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  int xi_major = 2, xi_minor = 2, xi_opcode, xi_error, xi_event;

  if (!XQueryExtension(display, INAME, &xi_opcode, &xi_event, &xi_error))
	strerr_dief1x(1, "No X Input extension");

  if (XIQueryVersion(display, &xi_major, &xi_minor) != Success ||
      xi_major * 10 + xi_minor < 22)
	strerr_dief1x(1, "Need XI 2.2");

  evmask.mask = mask;
  evmask.mask_len = sizeof(mask);
  memset(mask, 0, sizeof(mask));
  evmask.deviceid = XIAllDevices;
  XISetMask(mask, XI_HierarchyChanged);
  XISelectEvents(display, DefaultRootWindow(display), &evmask, 1);
#endif

  attrib.override_redirect= True;

  if (blank) {
    screen = DefaultScreen(display);
    attrib.background_pixel = BlackPixel(display, screen);
    window= XCreateWindow(display,DefaultRootWindow(display),
                          0,0,DisplayWidth(display, screen),DisplayHeight(display, screen),
                          0,DefaultDepth(display, screen), CopyFromParent, DefaultVisual(display, screen),
                          CWOverrideRedirect|CWBackPixel,&attrib);
    XAllocNamedColor(display, DefaultColormap(display, screen), "black", &black, &dummy);
  } else {
    window= XCreateWindow(display,DefaultRootWindow(display),
                          0,0,1,1,0,CopyFromParent,InputOnly,CopyFromParent,
                          CWOverrideRedirect,&attrib);
  }

  XSelectInput(display,window,KeyPressMask|KeyReleaseMask);

  csr_source= XCreateBitmapFromData(display,window,lock_bits,lock_width,lock_height);
  csr_mask= XCreateBitmapFromData(display,window,mask_bits,mask_width,mask_height);

  ret = XAllocNamedColor(display,
                        DefaultColormap(display, DefaultScreen(display)),
                        "steelblue3",
                        &dummy, &csr_bg);
  if (ret==0)
    XAllocNamedColor(display,
                    DefaultColormap(display, DefaultScreen(display)),
                    "black",
                    &dummy, &csr_bg);

  ret = XAllocNamedColor(display,
                        DefaultColormap(display,DefaultScreen(display)),
                        "grey25",
                        &dummy, &csr_fg);
  if (ret==0)
    XAllocNamedColor(display,
                    DefaultColormap(display, DefaultScreen(display)),
                    "white",
                    &dummy, &csr_bg);



  cursor= XCreatePixmapCursor(display,csr_source,csr_mask,&csr_fg,&csr_bg,
                              lock_x_hot,lock_y_hot);

  XMapWindow(display,window);

  /*Sometimes the WM doesn't ungrab the keyboard quickly enough if
   *launching xtrlock from a keystroke shortcut, meaning xtrlock fails
   *to start We deal with this by waiting (up to 100 times) for 10,000
   *microsecs and trying to grab each time. If we still fail
   *(i.e. after 1s in total), then give up, and emit an error
   */

  gs=0; /*gs==grab successful*/
  for (tvt=0 ; tvt<100; tvt++) {
    ret = XGrabKeyboard(display,window,False,GrabModeAsync,GrabModeAsync,
			CurrentTime);
    if (ret == GrabSuccess) {
      gs=1;
      break;
    }
    /*grab failed; wait .01s*/
    tv.tv_sec=0;
    tv.tv_usec=10000;
    select(1,NULL,NULL,NULL,&tv);
  }
  if (gs==0)
	strerr_diefu1x(1, "grab keyboard");

  if (XGrabPointer(display,window,False,(KeyPressMask|KeyReleaseMask)&0,
               GrabModeAsync,GrabModeAsync,None,
               cursor,CurrentTime)!=GrabSuccess) {
    XUngrabKeyboard(display,CurrentTime);
    strerr_diefu1x(1, "grab pointer");
  }

#ifdef MULTITOUCH
  handle_multitouch(cursor);
#endif

  if (fork_after) {
    pid_t pid = fork();
    if (pid < 0)
		strerr_diefu1sys(1, "fork");
    else if (pid > 0)
      exit(0);
  }

  for (;;) {
    XNextEvent(display,&ev);
    switch (ev.type) {
    case KeyPress:
      if (ev.xkey.time < timeout) { XBell(display,0); break; }
      clen= XLookupString(&ev.xkey,cbuf,9,&ks,0);
      switch (ks) {
      case XK_Escape: case XK_Clear:
        rlen=0; break;
      case XK_Delete: case XK_BackSpace:
        if (rlen>0) rlen--;
        break;
      case XK_Linefeed: case XK_Return: case XK_KP_Enter:
        if (rlen==0) break;
        rbuf[rlen]=0;
        if (passwordok(rbuf)) goto loop_x;
        XBell(display,0);
        rlen= 0;
        if (timeout) {
          goodwill+= ev.xkey.time - timeout;
          if (goodwill > MAXGOODWILL) {
            goodwill= MAXGOODWILL;
          }
        }
        timeout= -goodwill*GOODWILLPORTION;
        goodwill+= timeout;
        timeout+= ev.xkey.time + TIMEOUTPERATTEMPT;
        break;
      default:
        if (clen != 1) break;
        /* allow space for the trailing \0 */
	if (rlen < (sizeof(rbuf) - 1)){
	  rbuf[rlen]=cbuf[0];
	  rlen++;
	}
        break;
      }
      break;
#if MULTITOUCH
    case GenericEvent:
      if (ev.xcookie.extension == xi_opcode &&
          XGetEventData(display,&ev.xcookie) &&
          ev.xcookie.evtype == XI_HierarchyChanged) {
        handle_multitouch(cursor);
      }
      break;
#endif
    default:
      break;
    }
  }
 loop_x:
  exit(0);
}
