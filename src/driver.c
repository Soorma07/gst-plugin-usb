/* $(CROSS_COMPILE)cc -Wall       -g -o usb usb.c usbstring.c -lpthread
 *	-OR-
 * $(CROSS_COMPILE)cc -Wall -DAIO -g -o usb usb.c usbstring.c -lpthread -laio
 */

/*
 * this is an example pthreaded USER MODE driver implementing a
 * USB Gadget/Device with simple bulk source/sink functionality.
 * you could implement pda sync software this way, or some usb class
 * protocols (printers, test-and-measurement equipment, and so on).
 *
 * with hardware that also supports isochronous data transfers, this
 * can stream data using multi-buffering and AIO.  that's the way to
 * handle audio or video data, where on-time delivery is essential.
 *
 * needs "gadgetfs" and a supported USB device controller driver
 * in the kernel; this autoconfigures, based on the driver it finds.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <asm/byteorder.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>

#ifdef	AIO
/* this aio code works with libaio-0.3.106 */
#include <libaio.h>
#endif

#include "usbstring.h"
#include "driver.h"
#include "usbgadget_descriptors.h"

static int verbose;
static int pattern;

/*
 * Descriptors where here
 */

/* kernel drivers could autoconfigure like this too ... if
 * they were willing to waste the relevant code/data space.
 */

static int	HIGHSPEED;
static char	*DEVNAME;
static char	*EP_EVUP_NAME, *EP_EVDOWN_NAME, *EP_STREAM_NAME;

/* gadgetfs currently has no chunking (or O_DIRECT/zerocopy) support
 * to turn big requests into lots of smaller ones; so this is "small".
 */
#define	USB_BUFSIZE	(7 * 1024)

static enum usb_device_speed	current_speed;

static int autoconfig ()
{
	struct stat	statb;

	if (stat (DEVNAME = "musb_hdrc", &statb) == 0) {
		HIGHSPEED = 1;
		device_desc.bcdDevice = __constant_cpu_to_le16 (0x0107),

		fs_stream_desc.bEndpointAddress
			= hs_stream_desc.bEndpointAddress
			= USB_DIR_OUT | 2;
		EP_STREAM_NAME = "ep2out";
		fs_evup_desc.bEndpointAddress = hs_evup_desc.bEndpointAddress
			= USB_DIR_IN | 1;
		EP_EVUP_NAME = "ep1in";

		gst_usb_intf.bNumEndpoints = 3;
		fs_evdown_desc.bEndpointAddress
			= hs_evdown_desc.bEndpointAddress
			= USB_DIR_OUT | 1;
		EP_EVDOWN_NAME = "ep1out";
	} 
	else {
		DEVNAME = 0;
		return -ENODEV;
	}
	return 0;
}


/*-------------------------------------------------------------------------*/

/* full duplex data, with at least three threads: ep0, sink, and source */

static pthread_t	ep0;
static int		ep0_fd = -1;

static pthread_t	stream;
static int		stream_fd = -1;

static pthread_t	ev_up;
static int		ev_up_fd = -1;

static pthread_t	ev_down;
static int		ev_down_fd = -1;

// FIXME no status i/o yet

static void close_fd (void *fd_ptr)
{
	int	status, fd;

	fd = *(int *)fd_ptr;
	*(int *)fd_ptr = -1;

	fprintf(stderr, "Inside close fd\n");
	/* test the FIFO ioctls (non-ep0 code paths) */
	if (pthread_self () != ep0) {
		status = ioctl (fd, GADGETFS_FIFO_STATUS);
		if (status < 0) {
			/* ENODEV reported after disconnect */
			if (errno != ENODEV && errno != -EOPNOTSUPP)
				perror ("get fifo status");
		} else {
			fprintf (stderr, "fd %d, unclaimed = %d\n",
				fd, status);
			if (status) {
				status = ioctl (fd, GADGETFS_FIFO_FLUSH);
				if (status < 0)
					perror ("fifo flush");
			}
		}
	}

	if (close (fd) < 0)
		perror ("close");
}


/* you should be able to open and configure endpoints
 * whether or not the host is connected
 */
static int
ep_config (char *name, const char *label,
	struct usb_endpoint_descriptor *fs,
	struct usb_endpoint_descriptor *hs
)
{
	int		fd, status;
	char	buf [USB_BUFSIZE];

	/* open and initialize with endpoint descriptor(s) */
	fd = open (name, O_RDWR);
	if (fd < 0) {
		status = -errno;
		fprintf (stderr, "%s open %s error %d (%s)\n",
			label, name, errno, strerror (errno));
		return status;
	}

	/* one (fs or ls) or two (fs + hs) sets of config descriptors */
	*(__u32 *)buf = 1;	/* tag for this format */
	memcpy (buf + 4, fs, USB_DT_ENDPOINT_SIZE);
	if (HIGHSPEED)
		memcpy (buf + 4 + USB_DT_ENDPOINT_SIZE,
			hs, USB_DT_ENDPOINT_SIZE);
			
	status = write (fd, buf, 4 + USB_DT_ENDPOINT_SIZE
			+ (HIGHSPEED ? USB_DT_ENDPOINT_SIZE : 0));
	if (status < 0) {
		status = -errno;
		fprintf (stderr, "%s config %s error %d (%s)\n",
			label, name, errno, strerror (errno));
		close (fd);
		return status;
	} else if (verbose) {
		unsigned long	id;

		id = pthread_self ();
		fprintf (stderr, "%s start %ld fd %d\n", label, id, fd);
	}
	return fd;
}

#define stream_open(name) \
	ep_config(name,__FUNCTION__, &fs_stream_desc, &hs_stream_desc)
#define ev_up_open(name) \
	ep_config(name,__FUNCTION__, &fs_evup_desc, &hs_evup_desc)
#define ev_down_open(name) \
	ep_config(name,__FUNCTION__, &fs_evdown_desc, &hs_evdown_desc)	
	


static unsigned long fill_in_buf(void *buf, unsigned long nbytes)
{
#ifdef	DO_PIPE
	/* pipe stdin to host */
	nbytes = fread (buf, 1, nbytes, stdin);
	if (nbytes == 0) {
		if (ferror (stdin))
			perror ("read stdin");
		if (feof (stdin))
			errno = ENODEV;
	}
#else
	switch (pattern) {
	unsigned	i;

	default:
		// FALLTHROUGH
	case 0:		/* endless streams of zeros */
		memset (buf, 0, nbytes);
		break;
	case 1:		/* mod63 repeating pattern */
		for (i = 0; i < nbytes; i++)
			((__u8 *)buf)[i] = (__u8) (i % 63);
		break;
	}
#endif
	return nbytes;
}

static int empty_out_buf(void *buf, unsigned long nbytes)
{
	unsigned len;

#ifdef	DO_PIPE
	/* pipe from host to stdout */
	len = fwrite (buf, nbytes, 1, stdout);
	if (len != nbytes) {
		if (ferror (stdout))
			perror ("write stdout");
	}
#else
	unsigned	i;
	__u8		expected, *data;

	for (i = 0, data = buf; i < nbytes; i++, data++) {
		switch (pattern) {
		case 0:
			expected = 0;
			break;
		case 1:
			expected = i % 63;
			break;
		default:	/* no verify */
			i = nbytes - 1;
			continue;
		}
		if (*data == expected)
			continue;
		fprintf (stderr, "bad OUT byte %d, expected %02x got %02x\n",
				i, expected, *data);
		for (i = 0, data = 0; i < nbytes; i++, data++) {
			if (0 == (i % 16))
				fprintf (stderr, "%4d:", i);
			fprintf (stderr, " %02x", *data);
			if (15 == (i % 16))
				fprintf (stderr, "\n");
		}
		return -1;
	}
	len = i;
#endif
	memset (buf, 0, nbytes);
	return len;
}

static void *simple_stream_thread (void *param)
{
	char		*name = (char *) param;
	int		status;
	char		buf [USB_BUFSIZE];

	status = stream_open (name);
	if (status < 0)
		return 0;
	stream_fd = status;
	pthread_cleanup_push (close_fd, &stream_fd);
	do {
		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		pthread_testcancel ();
		errno = 0;
		status = read (stream_fd, buf, sizeof buf);

		if (status < 0)
			break;
		status = empty_out_buf (buf, status);
	} while (status > 0);
	
	if (status == 0) {
		if (verbose)
			fprintf (stderr, "done %s\n", __FUNCTION__);
	} else if (verbose > 2 || errno != ESHUTDOWN) /* normal disconnect */
		perror ("write");
	fflush (stdout);
	fflush (stderr);
	pthread_cleanup_pop (1);

	return 0;
}

static void *simple_ev_up_thread (void *param)
{
	char		*name = (char *) param;
	int		status;
	char		buf [USB_BUFSIZE];

	status = ev_up_open (name);
	if (status < 0)
		return 0;
	ev_up_fd = status;

	/* synchronous reads of endless streams of data */
	pthread_cleanup_push (close_fd, &ev_up_fd);
	do {
		unsigned long	len;

		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		pthread_testcancel ();

		len = fill_in_buf (buf, sizeof buf);
		if (len > 0)
			status = write (ev_up_fd, buf, len);
		else
			status = 0;

	} while (status > 0);
	if (status == 0) {
		if (verbose)
			fprintf (stderr, "done %s\n", __FUNCTION__);
	} else if (verbose > 2 || errno != ESHUTDOWN) /* normal disconnect */
		perror ("read");
	fflush (stdout);
	fflush (stderr);
	pthread_cleanup_pop (1);

	return 0;
}

static void *simple_ev_down_thread (void *param)
{
	char		*name = (char *) param;
	int		status;
	char		buf [USB_BUFSIZE];

	status = ev_down_open (name);
	if (status < 0)
		return 0;
	ev_down_fd = status;

	/* synchronous reads of endless streams of data */
	pthread_cleanup_push (close_fd, &ev_down_fd);
	do {
		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		pthread_testcancel ();
		errno = 0;
		status = read (ev_down_fd, buf, sizeof buf);

		if (status < 0)
			break;
		status = empty_out_buf (buf, status);
	} while (status > 0);
	if (status == 0) {
		if (verbose)
			fprintf (stderr, "done %s\n", __FUNCTION__);
	} else if (verbose > 2 || errno != ESHUTDOWN) /* normal disconnect */
		perror ("read");
	fflush (stdout);
	fflush (stderr);
	pthread_cleanup_pop (1);

	return 0;
}

static void *(*stream_thread) (void *);
static void *(*ev_up_thread) (void *);
static void *(*ev_down_thread) (void *);
static void *(*ep0_thread) (void *);


static void start_io ()
{
	sigset_t	allsig, oldsig;

	sigfillset (&allsig);
	errno = pthread_sigmask (SIG_SETMASK, &allsig, &oldsig);
	if (errno < 0) {
		perror ("set thread signal mask");
		return;
	}

	/* is it true that the LSB requires programs to disconnect
	 * from their controlling tty before pthread_create()?
	 * why?  this clearly doesn't ...
	 */
	if (pthread_create (&stream, 0,
			stream_thread, (void *) EP_STREAM_NAME) != 0) {
		perror ("can't create stream thread");
		goto cleanup;
	}

	if (pthread_create (&ev_up, 0,
			ev_up_thread, (void *) EP_EVUP_NAME) != 0) {
		perror ("can't create up events thread");
		pthread_cancel (stream);
		stream = ep0;
		goto cleanup;
	}
	
	if (pthread_create (&ev_down, 0,
			ev_down_thread, (void *) EP_EVDOWN_NAME) != 0) {
		perror ("can't create down events thread");
		pthread_cancel (stream);
		pthread_cancel (ev_up);
		stream = ep0;
		ev_up = ep0;
		goto cleanup;
	}

	/* give the other threads a chance to run before we report
	 * success to the host.
	 * FIXME better yet, use pthread_cond_timedwait() and
	 * synchronize on ep config success.
	 */
	sched_yield ();

cleanup:
	errno = pthread_sigmask (SIG_SETMASK, &oldsig, 0);
	if (errno != 0) {
		perror ("restore sigmask");
		exit (-1);
	}
}

static void stop_io ()
{
	if (!pthread_equal (stream, ep0)) {
		pthread_cancel (stream);
		if (pthread_join (stream, 0) != 0)
			perror ("can't join stream thread");
		stream = ep0;
	}

	if (!pthread_equal (ev_up, ep0)) {
		pthread_cancel (ev_up);
		if (pthread_join (ev_up, 0) != 0)
			perror ("can't join up events thread");
		ev_up = ep0;
	}
	
	if (!pthread_equal (ev_down, ep0)) {
		pthread_cancel (ev_down);
		if (pthread_join (ev_down, 0) != 0)
			perror ("can't join down events thread");
		ev_down = ep0;
	}
	fprintf(stderr, "**Threads distroyed\n");
}

/*-------------------------------------------------------------------------*/

static char *
build_config (char *cp, const struct usb_endpoint_descriptor **ep)
{
	struct usb_config_descriptor *c;
	int i;

	c = (struct usb_config_descriptor *) cp;

	memcpy (cp, &config, config.bLength);
	cp += config.bLength;
	memcpy (cp, &gst_usb_intf, gst_usb_intf.bLength);
	cp += gst_usb_intf.bLength;

	for (i = 0; i < gst_usb_intf.bNumEndpoints; i++) {
		memcpy (cp, ep [i], USB_DT_ENDPOINT_SIZE);
		cp += USB_DT_ENDPOINT_SIZE;
	}
	c->wTotalLength = __cpu_to_le16 (cp - (char *) c);
	return cp;
}

static int init_device (void)
{
	char	buf [4096], *cp = &buf [0];
	int		fd;
	int		status;

	status = autoconfig ();
	
if (status < 0) {
		fprintf (stderr, "?? don't recognize /dev/gadget bulk device\n");
		return status;
	}

	fd = open (DEVNAME, O_RDWR);
	if (fd < 0) {
		perror (DEVNAME);
		return -errno;
	}

	*(__u32 *)cp = 0;	/* tag for this format */
	cp += 4;

	/* write full then high speed configs */
	cp = build_config (cp, fs_eps);
	if (HIGHSPEED)
		cp = build_config (cp, hs_eps);

	/* and device descriptor at the end */
	memcpy (cp, &device_desc, sizeof device_desc);
	cp += sizeof device_desc;

	status = write (fd, &buf [0], cp - &buf [0]);
	if (status < 0) {
		perror ("write dev descriptors");
		close (fd);
		return status;
	} else if (status != (cp - buf)) {
		fprintf (stderr, "dev init, wrote %d expected %d\n",
				status, cp - buf);
		close (fd);
		return -EIO;
	}
	return fd;
}

static void handle_control (int fd, struct usb_ctrlrequest *setup)
{
	int		status, tmp;
	__u8		buf [256];
	__u16		value, index, length;

	value = __le16_to_cpu(setup->wValue);
	index = __le16_to_cpu(setup->wIndex);
	length = __le16_to_cpu(setup->wLength);

	if (verbose)
		fprintf (stderr, "SETUP %02x.%02x "
				"v%04x i%04x %d\n",
			setup->bRequestType, setup->bRequest,
			value, index, length);

	/*
	if ((setup->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD)
		goto special;
	*/

	switch (setup->bRequest) {	/* usb 2.0 spec ch9 requests */
	case USB_REQ_GET_DESCRIPTOR:
		if (setup->bRequestType != USB_DIR_IN)
			goto stall;
		switch (value >> 8) {
		case USB_DT_STRING:
			tmp = value & 0x0ff;
			if (verbose > 1)
				fprintf (stderr,
					"... get string %d lang %04x\n",
					tmp, index);
			if (tmp != 0 && index != strings.language)
				goto stall;
			status = usb_gadget_get_string (&strings, tmp, buf);
			if (status < 0)
				goto stall;
			tmp = status;
			if (length < tmp)
				tmp = length;
			status = write (fd, buf, tmp);
			if (status < 0) {
				if (errno == EIDRM)
					fprintf (stderr, "string timeout\n");
				else
					perror ("write string data");
			} else if (status != tmp) {
				fprintf (stderr, "short string write, %d\n",
					status);
			}
			break;
		default:
			goto stall;
		}
		return;
	case USB_REQ_SET_CONFIGURATION:
		if (setup->bRequestType != USB_DIR_OUT)
			goto stall;
		if (verbose)
			fprintf (stderr, "CONFIG #%d\n", value);

		/* Kernel is normally waiting for us to finish reconfiguring
		 * the device.
		 *
		 * Some hardware can't, notably older PXA2xx hardware.  (With
		 * racey and restrictive config change automagic.  PXA 255 is
		 * OK, most PXA 250s aren't.  If it has a UDC CFR register,
		 * it can handle deferred response for SET_CONFIG.)  To handle
		 * such hardware, don't write code this way ... instead, keep
		 * the endpoints always active and don't rely on seeing any
		 * config change events, either this or SET_INTERFACE.
		 */
		switch (value) {
		case CONFIG_VALUE:
			if (verbose)
				fprintf (stderr, "Starting io\n");
			start_io ();
			break;
		case 0:
			if (verbose)
				fprintf (stderr, "Stoping io\n");
			stop_io ();
			break;
		default:
			/* kernel bug -- "can't happen" */
			fprintf (stderr, "? illegal config\n");
			goto stall;
		}

		/* ... ack (a write would stall) */
		status = read (fd, &status, 0);
		if (status)
			perror ("ack SET_CONFIGURATION");
		return;
	case USB_REQ_GET_INTERFACE:
		if (setup->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE)
				|| index != 0
				|| length > 1)
			goto stall;

		/* only one altsetting in this driver */
		buf [0] = 0;
		status = write (fd, buf, length);
		if (status < 0) {
			if (errno == EIDRM)
				fprintf (stderr, "GET_INTERFACE timeout\n");
			else
				perror ("write GET_INTERFACE data");
		} else if (status != length) {
			fprintf (stderr, "short GET_INTERFACE write, %d\n",
				status);
		}
		return;
	case USB_REQ_SET_INTERFACE:
		if (setup->bRequestType != USB_RECIP_INTERFACE
				|| index != 0
				|| value != 0)
			goto stall;

		/* just reset toggle/halt for the interface's endpoints */
		status = 0;
		if (ioctl (stream_fd, GADGETFS_CLEAR_HALT) < 0) {
			status = errno;
			perror ("reset source fd");
		}
		if (ioctl (ev_up_fd, GADGETFS_CLEAR_HALT) < 0) {
			status = errno;
			perror ("reset sink fd");
		}
		/* FIXME eventually reset the status endpoint too */
		if (status)
			goto stall;

		/* ... and ack (a write would stall) */
		status = read (fd, &status, 0);
		if (status)
			perror ("ack SET_INTERFACE");
		return;
	default:
		goto stall;
	}

stall:
	if (verbose)
		fprintf (stderr, "... protocol stall %02x.%02x\n",
			setup->bRequestType, setup->bRequest);

	/* non-iso endpoints are stalled by issuing an i/o request
	 * in the "wrong" direction.  ep0 is special only because
	 * the direction isn't fixed.
	 */
	if (setup->bRequestType & USB_DIR_IN)
		status = read (fd, &status, 0);
	else
		status = write (fd, &status, 0);
	if (status != -1)
		fprintf (stderr, "can't stall ep0 for %02x.%02x\n",
			setup->bRequestType, setup->bRequest);
	else if (errno != EL2HLT)
		perror ("ep0 stall");
}

static void signothing (int sig, siginfo_t *info, void *ptr)
{
	/* NOP */
	if (verbose > 2)
		fprintf (stderr, "%s %d\n", __FUNCTION__, sig);
}

static const char *speed (enum usb_device_speed s)
{
	switch (s) {
	case USB_SPEED_LOW:	return "low speed";
	case USB_SPEED_FULL:	return "full speed";
	case USB_SPEED_HIGH:	return "high speed";
	default:		return "UNKNOWN speed";
	}
}

/*-------------------------------------------------------------------------*/

/* control thread, handles main event loop  */

#define	NEVENT		5
#define	LOGDELAY	(15 * 60)	/* seconds before stdout timestamp */

static void *simple_ep0_thread (void *param)
{
	int			fd = *(int*) param;
	//time_t			now, last;
	struct pollfd		ep0_poll;

	stream = ev_up = ev_down = ep0 = pthread_self ();
	pthread_cleanup_push (close_fd, param);
	
	/*
	 * Signals will be handled by GStreamer
	 */
	
	ep0_poll.fd = fd;
	ep0_poll.events = POLLIN | POLLOUT | POLLHUP;

	/* event loop */
	//last = 0;

	for (;;) {
		int				tmp;
		struct usb_gadgetfs_event	event [NEVENT];
		int				connected = 0;
		int				i, nevent;

		/* Use poll() to test that mechanism, to generate
		 * activity timestamps, and to make it easier to
		 * tweak this code to work without pthreads.  When
		 * AIO is needed without pthreads, ep0 can be driven
		 * instead using SIGIO.
		 */

		tmp = poll(&ep0_poll, 1, -1);
		
		/*
		 * TODO:
		 * Are these lines somehow necessary?
		 * Think they are for suspend state
		 */
/*		if (verbose) {

			time (&now);
			if ((now - last) > LOGDELAY) {
				char		timebuf[26];

				last = now;
				ctime_r (&now, timebuf);
				printf ("\n** %s", timebuf);
			}
		} */
		if (tmp < 0) {
			/* exit path includes EINTR exits */
			perror("poll");
			break;
		}

		tmp = read (fd, &event, sizeof event);
		if (tmp < 0) {
			if (errno == EAGAIN) {
				sleep (1);
				continue;
			}
			perror ("ep0 read after poll");
			goto done;
		}
		nevent = tmp / sizeof event [0];
		if (nevent != 1 && verbose)
			fprintf (stderr, "read %d ep0 events\n",
				nevent);

		for (i = 0; i < nevent; i++) {
			switch (event [i].type) {
			case GADGETFS_NOP:
				if (verbose)
					fprintf (stderr, "NOP\n");
				break;
			case GADGETFS_CONNECT:
				connected = 1;
				current_speed = event [i].u.speed;
				if (verbose)
					fprintf (stderr,
						"CONNECT %s\n",
					    speed (event [i].u.speed));
				break;
			case GADGETFS_SETUP:
				connected = 1;
				handle_control (fd, &event [i].u.setup);
				break;
			case GADGETFS_DISCONNECT:
				connected = 0;
				current_speed = USB_SPEED_UNKNOWN;
				if (verbose)
					fprintf(stderr, "DISCONNECT\n");
				stop_io ();
				break;
			case GADGETFS_SUSPEND:
				// connected = 1;
				if (verbose)
					fprintf (stderr, "SUSPEND\n");
				break;
			default:
				fprintf (stderr,
					"* unhandled event %d\n",
					event [i].type);
			}
		}
		continue;
done:
		fflush (stdout);
		if (connected)
			stop_io ();
		break;
	}
	if (verbose)
		fprintf (stderr, "done\n");
	fflush (stdout);

	pthread_cleanup_pop (1);
	return 0;
}

/*-------------------------------------------------------------------------*/

int
usb_gadget_new ()
{
	
	verbose=3;
    
	stream_thread = simple_stream_thread;
	ev_up_thread = simple_ev_up_thread;
	ev_down_thread = simple_ev_down_thread;
	ep0_thread = simple_ep0_thread;

	if (chdir ("/dev/gadget") < 0) {
		return ERR_GAD_DIR;
	}
	
	ep0_fd = init_device ();
	if (ep0_fd < 0)
		return ERR_INI_DEV;
	fprintf (stderr, "/dev/gadget/%s ep0 configured\n",
		DEVNAME); // ERASE ME!
	fflush (stderr);
	
	if (pthread_create (&ep0, NULL,
			(void *) ep0_thread, (void *) &ep0_fd) != 0) {
		perror ("can't create stream thread");
	} else
		fprintf(stdout, "**Thread created\n");
	//(void) ep0_thread (&fd);
	return EOK; 
}

int usb_gadget_free()
{
    stop_io();
	pthread_cancel (ep0);
	if (pthread_join (ep0, 0) != 0)
	  perror ("can't join up events thread");	
	return EOK;
}

