/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>	/* we share subpixel information */
#include <string.h>
#include <stdlib.h>

static char *program_name;

static char *direction[5] = {
    "normal", 
    "left", 
    "inverted", 
    "right",
    "\n"};

/* subpixel order */
static char *order[6] = {
    "unknown",
    "horizontal rgb",
    "horizontal bgr",
    "vertical rgb",
    "vertical bgr",
    "no subpixels"};

static char *connection[3] = {
    "connected",
    "disconnected",
    "unknown connection"};

static void
usage(void)
{
    fprintf(stderr, "usage: %s [options]\n", program_name);
    fprintf(stderr, "  where options are:\n");
    fprintf(stderr, "  -display <display> or -d <display>\n");
    fprintf(stderr, "  -help\n");
    fprintf(stderr, "  -o <normal,inverted,left,right,0,1,2,3>\n");
    fprintf(stderr, "            or --orientation <normal,inverted,left,right,0,1,2,3>\n");
    fprintf(stderr, "  -q        or --query\n");
    fprintf(stderr, "  -s <size>/<width>x<height> or --size <size>/<width>x<height>\n");
    fprintf(stderr, "  -r <rate> or --rate <rate>\n");
    fprintf(stderr, "  -v        or --version\n");
    fprintf(stderr, "  -x        (reflect in x)\n");
    fprintf(stderr, "  -y        (reflect in y)\n");
    fprintf(stderr, "  --screen <screen>\n");
    fprintf(stderr, "  --verbose\n");

    exit(1);
    /*NOTREACHED*/
}

int
main (int argc, char **argv)
{
    Display       *dpy;
    XRRScreenResources	*sr;
    XRRScreenSize *sizes;
    XRRScreenConfiguration *sc;
    int		nsize;
    int		nrate;
    short		*rates;
    Window	root;
    Status	status = RRSetConfigFailed;
    int		rot = -1;
    int		verbose = 0, query = 0;
    Rotation	rotation, current_rotation, rotations;
    XEvent	event;
    XRRScreenChangeNotifyEvent *sce;    
    char          *display_name = NULL;
    int 		i, j;
    SizeID	current_size;
    short		current_rate;
    int		rate = -1;
    int		size = -1;
    int		dirind = 0;
    int		setit = 0;
    int		screen = -1;
    int		version = 0;
    int		event_base, error_base;
    int		reflection = 0;
    int		width = 0, height = 0;
    int		have_pixel_size = 0;
    int		ret = 0;
    int		major_version, minor_version;

    program_name = argv[0];
    if (argc == 1) query = 1;
    for (i = 1; i < argc; i++) {
	if (!strcmp ("-display", argv[i]) || !strcmp ("-d", argv[i])) {
	    if (++i>=argc) usage ();
	    display_name = argv[i];
	    continue;
	}
	if (!strcmp("-help", argv[i])) {
	    usage();
	    continue;
	}
	if (!strcmp ("--verbose", argv[i])) {
	    verbose = 1;
	    continue;
	}

	if (!strcmp ("-s", argv[i]) || !strcmp ("--size", argv[i])) {
	    if (++i>=argc) usage ();
	    if (sscanf (argv[i], "%dx%d", &width, &height) == 2)
		have_pixel_size = 1;
	    else {
		size = atoi (argv[i]);
		if (size < 0) usage();
	    }
	    setit = 1;
	    continue;
	}

	if (!strcmp ("-r", argv[i]) || !strcmp ("--rate", argv[i])) {
	    if (++i>=argc) usage ();
	    rate = atoi (argv[i]);
	    if (rate < 0) usage();
	    setit = 1;
	    continue;
	}

	if (!strcmp ("-v", argv[i]) || !strcmp ("--version", argv[i])) {
	    version = 1;
	    continue;
	}

	if (!strcmp ("-x", argv[i])) {
	    reflection |= RR_Reflect_X;
	    setit = 1;
	    continue;
	}
	if (!strcmp ("-y", argv[i])) {
	    reflection |= RR_Reflect_Y;
	    setit = 1;
	    continue;
	}
	if (!strcmp ("--screen", argv[i])) {
	    if (++i>=argc) usage ();
	    screen = atoi (argv[i]);
	    if (screen < 0) usage();
	    continue;
	}
	if (!strcmp ("-q", argv[i]) || !strcmp ("--query", argv[i])) {
	    query = 1;
	    continue;
	}
	if (!strcmp ("-o", argv[i]) || !strcmp ("--orientation", argv[i])) {
	    char *endptr;
	    if (++i>=argc) usage ();
	    dirind = strtol(argv[i], &endptr, 0);
	    if (*endptr != '\0') {
		for (dirind = 0; dirind < 4; dirind++) {
		    if (strcmp (direction[dirind], argv[i]) == 0) break;
		}
		if ((dirind < 0) || (dirind > 3))  usage();
	    }
	    rot = dirind;
	    setit = 1;
	    continue;
	}
	usage();
    }
    if (verbose) query = 1;

    dpy = XOpenDisplay (display_name);

    if (dpy == NULL) {
	fprintf (stderr, "Can't open display %s\n", XDisplayName(display_name));
	exit (1);
    }
    
    XRRQueryVersion (dpy, &major_version, &minor_version);
    if (!(major_version > 1 || minor_version >= 2))
    {
	fprintf (stderr, "Randr version too old (need 1.2 or better)\n");
	exit (1);
    }
	
    if (screen < 0)
	screen = DefaultScreen (dpy);
    if (screen >= ScreenCount (dpy)) {
	fprintf (stderr, "Invalid screen number %d (display has %d)\n",
		 screen, ScreenCount (dpy));
	exit (1);
    }

    root = RootWindow (dpy, screen);

    sr = XRRGetScreenResources (dpy, root);

    printf ("timestamp: %ld\n", sr->timestamp);
    printf ("configTimestamp: %ld\n", sr->configTimestamp);
    for (i = 0; i < sr->ncrtc; i++) {
	printf ("\tcrtc: 0x%x\n", sr->crtcs[i]);
    }
    for (i = 0; i < sr->noutput; i++) {
	XRROutputInfo	*xoi;
	
	printf ("\toutput: 0x%x\n", sr->outputs[i]);
	xoi = XRRGetOutputInfo (dpy, sr, sr->outputs[i]);
	printf ("\t\tname: %s\n", xoi->name);
	printf ("\t\ttimestamp: %d\n", xoi->timestamp);
	printf ("\t\tcrtc: 0x%x\n", xoi->crtc);
	printf ("\t\tconnection: %s\n", connection[xoi->connection]);
	printf ("\t\tsubpixel_order: %s\n", order[xoi->subpixel_order]);
	XRRFreeOutputInfo (xoi);
    }
    for (i = 0; i < sr->nmode; i++) {
	printf ("\tmode: 0x%x\n", sr->modes[i].id);
	printf ("\t\tname: %s\n", sr->modes[i].name);
	printf ("\t\twidth: %d\n", sr->modes[i].width);
	printf ("\t\theight: %d\n", sr->modes[i].height);
	printf ("\t\tmmWidth: %d\n", sr->modes[i].mmWidth);
	printf ("\t\tmmHeight: %d\n", sr->modes[i].mmHeight);
	printf ("\t\tdotClock: %d\n", sr->modes[i].dotClock);
	printf ("\t\thSyncStart: %d\n", sr->modes[i].hSyncStart);
	printf ("\t\thSyncEnd: %d\n", sr->modes[i].hSyncEnd);
	printf ("\t\thTotal: %d\n", sr->modes[i].hTotal);
	printf ("\t\tvSyncStart: %d\n", sr->modes[i].vSyncStart);
	printf ("\t\tvSyncEnd: %d\n", sr->modes[i].vSyncEnd);
	printf ("\t\tvTotal: %d\n", sr->modes[i].vTotal);
	printf ("\t\tmodeFlags: 0x%x\n", sr->modes[i].modeFlags);
    }
    if (sr == NULL) 
    {
	fprintf (stderr, "Cannot get screen resources\n");
	exit (1);
    }

    XRRFreeScreenResources (sr);
    return(ret);
}
