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
#include "usbgadget.h"
#include "usbgadget_descriptors.h"

//static int verbose;
static int pattern;


/* kernel drivers could autoconfigure like this too ... if
 * they were willing to waste the relevant code/data space.
 */

static int	HIGHSPEED;
//static char	*DEVNAME;

/* gadgetfs currently has no chunking (or O_DIRECT/zerocopy) support
 * to turn big requests into lots of smaller ones; so this is "small".
 */
#define	USB_BUFSIZE	(7 * 1024)

static enum usb_device_speed	current_speed;

static GADGET_EXIT_CODE autoconfig (usb_gadget *gadget)
{
  struct stat	statb;

  if (stat (gadget->DEVNAME = "musb_hdrc", &statb) == 0) 
  {	  
    HIGHSPEED = 1;
    device_desc.bcdDevice = __constant_cpu_to_le16 (0x0107),

    fs_stream_desc.bEndpointAddress
      = hs_stream_desc.bEndpointAddress
      = USB_DIR_OUT | 2;
    gadget->stream.NAME = "ep2out";
    fs_evup_desc.bEndpointAddress = hs_evup_desc.bEndpointAddress
      = USB_DIR_IN | 1;
    gadget->ev_up.NAME = "ep1in";

    gst_usb_intf.bNumEndpoints = 3;
    fs_evdown_desc.bEndpointAddress
      = hs_evdown_desc.bEndpointAddress
      = USB_DIR_OUT | 1;
    gadget->ev_down.NAME = "ep1out";
  } 
  else 
  {
    gadget->DEVNAME = 0;
    return ERR_NO_DEVICE;
  }
  return GAD_EOK;
}


/*-------------------------------------------------------------------------*/

/* full duplex data, with at least three threads: ep0, sink, and source */

// FIXME no status i/o yet

static void close_ep0 (void *param)
{
  int fd = *(int *)param;
  if (close (fd) < 0)
    /* 
     * TODO: Find a way to pass error handling to GSTREAMER
	 */
    perror ("close");
}

static void close_fd (void *param)
{
  int	status, fd;
  
  fd = *(int *)param;     	  

  /* test the FIFO ioctls (non-ep0 code paths) */
  status = ioctl (fd, GADGETFS_FIFO_STATUS);
  if (status < 0) 
  {
    /* ENODEV reported after disconnect */
    if (errno != ENODEV && errno != -EOPNOTSUPP)
      /* 
	   * TODO: Find a way to pass error handling to GSTREAMER
	   */
	  perror ("get fifo status");	
  } else 
  {
    if (status) 
    {
      status = ioctl (fd, GADGETFS_FIFO_FLUSH);
      if (status < 0)
        /* 
		 * TODO: Find a way to pass error handling to GSTREAMER
		 */
        perror ("fifo flush");
    }
  }
  if (close (fd) < 0)
    /* 
     * TODO: Find a way to pass error handling to GSTREAMER
	 */
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

#define DO_PIPE
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
  
  int  status;
  char  buf [USB_BUFSIZE];

  usb_gadget *gadget = (usb_gadget *)param;
  int verbose = gadget->verbosity;
  char *name = gadget->stream.NAME;
  
  status = stream_open (name);
  if (status < 0)
    perror("stream fd open");
  
  gadget->stream.fd = status;
  pthread_cleanup_push (close_fd, (void *)&(gadget->stream.fd));
	
  do 
  {
    /* original LinuxThreads cancelation didn't work right
     * so test for it explicitly.
     */
    pthread_testcancel();
	
    errno = 0;
    status = read (gadget->stream.fd, buf, sizeof buf);
	    
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
	int		status;
	char		buf [USB_BUFSIZE];

  usb_gadget *gadget = (usb_gadget *)param;
  int verbose = gadget->verbosity;
  char *name = gadget->ev_up.NAME;
	status = ev_up_open (name);
	if (status < 0)
		return 0;
	gadget->ev_up.fd = status;
	/* synchronous reads of endless streams of data */
	pthread_cleanup_push (close_fd, (void *)&(gadget->ev_up.fd));
	do {
		unsigned long	len;

		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		pthread_testcancel ();

		len = fill_in_buf (buf, sizeof buf);
		if (len > 0)
			status = write (gadget->ev_up.fd, buf, len);
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
	int		status;
	char		buf [USB_BUFSIZE];

  usb_gadget *gadget = (usb_gadget *)param;
  int verbose = gadget->verbosity;    
 char *name = gadget->ev_down.NAME;
	status = ev_down_open (name);
	if (status < 0)
		return 0;
	gadget->ev_down.fd = status;
	/* synchronous reads of endless streams of data */
	pthread_cleanup_push (close_fd, (void *) &(gadget->ev_down.fd));
	do {
		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		pthread_testcancel ();
		errno = 0;
		status = read (gadget->ev_down.fd, buf, sizeof buf);

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

static void start_io (usb_gadget *gadget)
{
  sigset_t	allsig, oldsig;
  int status; 
  
  /* Disable interrupts during transfers */
  sigfillset (&allsig);
  /* Store original signal mask in oldsig to recover later */ 
  errno = pthread_sigmask (SIG_SETMASK, &allsig, &oldsig);
  if (errno < 0) 
    perror("sigmask");


  /* is it true that the LSB requires programs to disconnect
   * from their controlling tty before pthread_create()?
   * why?  this clearly doesn't ...
   */
  //~ if (pthread_create (&(gadget->stream.thread), 0,
            //~ gadget->stream.func, (void *) gadget) != 0)      
  //~ {
	//~ fprintf (stderr, "Unable to create stream thread\n");  
    //~ goto cleanup;
  //~ }
  
  /* ***************************************/
  status = stream_open (gadget->stream.NAME);
  if (status < 0)
    perror("stream fd open");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Stream file descriptor opened\n");
  gadget->stream.fd = status;
  /* ***************************************/    
  
  //~ if (pthread_create (&(gadget->ev_up.thread), 0,
            //~ gadget->ev_up.func, (void *) gadget) != 0) 
  //~ {
	//~ fprintf (stderr, "Unable to create upstream events thread\n"); 
	//~ /* Cancel already created thread */       
    //~ pthread_cancel (gadget->stream.thread);
    //~ gadget->stream.thread = gadget->ep0.thread;
    //~ goto cleanup;
  //~ }
  
  /* ***************************************/
  status = ev_up_open (gadget->ev_up.NAME);
  if (status < 0)
    perror("upstream events fd open");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Up events file descriptor opened\n");
  gadget->ev_up.fd = status;
  /* ***************************************/
	//~ 
  //~ if (pthread_create (&(gadget->ev_down.thread), 0,
            //~ gadget->ev_down.func, (void *) gadget) != 0) 
  //~ {
    //~ fprintf (stderr, "Unable to create downstream events thread\n");  
	//~ /* Cancel already created thread */
    //~ pthread_cancel (gadget->stream.thread);
    //~ pthread_cancel (gadget->ev_up.thread);
    //~ gadget->stream.thread = gadget->ep0.thread;
    //~ gadget->ev_up.thread = gadget->ep0.thread;
    //~ goto cleanup;
  //~ }
  
  /* ***************************************/
  status = ev_down_open (gadget->ev_down.NAME);
  if (status < 0)
    perror("downstream events fd open");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Down events file descriptor opened\n");
  gadget->ev_down.fd = status;
  gadget->connected=1;
  /* ***************************************/

  //~ /* give the other threads a chance to run before we report
   //~ * success to the host.
   //~ * FIXME better yet, use pthread_cond_timedwait() and
   //~ * synchronize on ep config success.
   //~ */
  //~ sched_yield ();

  errno = pthread_sigmask (SIG_SETMASK, &oldsig, 0);
  if (errno != 0) 
    perror("sigmask");
}

static void stop_io (usb_gadget *gadget)
{
  	
  //~ if (!pthread_equal (gadget->stream.thread, gadget->ep0.thread)) 
  //~ {
    //~ pthread_cancel (gadget->stream.thread);
    //~ if (pthread_join (gadget->stream.thread, 0) != 0)
      //~ fprintf(stderr, "Unable to join threads");   
    //~ gadget->stream.thread = gadget->ep0.thread;
  //~ }
  
  /* ***************************************************/
  if (close (gadget->stream.fd) < 0)
    /* 
     * TODO: Find a way to pass error handling to GSTREAMER
	 */
    perror ("close");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Stream file descriptor closed\n");  
  /* ****************************************************/

  //~ if (!pthread_equal (gadget->ev_up.thread, gadget->ep0.thread)) 
  //~ {
    //~ pthread_cancel (gadget->ev_up.thread);
    //~ if (pthread_join (gadget->ev_up.thread, 0) != 0)
      //~ fprintf(stderr, "Unable to join threads");
    //~ gadget->ev_up.thread = gadget->ep0.thread;
  //~ }
  
  /* ***************************************************/
  if (close (gadget->ev_up.fd) < 0)
    /* 
     * TODO: Find a way to pass error handling to GSTREAMER
	 */
    perror ("close");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Upstream events file descriptor closed\n");  
  /* ****************************************************/
	//~ 
  //~ if (!pthread_equal (gadget->ev_down.thread, gadget->ep0.thread)) 
  //~ {
    //~ pthread_cancel (gadget->ev_down.thread);
    //~ if (pthread_join (gadget->ev_down.thread, 0) != 0)
      //~ fprintf(stderr, "Unable to join threads");
    //~ gadget->ev_down.thread = gadget->ep0.thread;
  //~ }
  
  /* ***************************************************/
  if (close (gadget->ev_down.fd) < 0)
    /* 
     * TODO: Find a way to pass error handling to GSTREAMER
	 */
    perror ("close");
  else
    if (gadget->verbosity > GLEVEL1)
	  printf("Downstream events file descriptor closed\n");  
  /* ****************************************************/
  gadget->connected=0;	
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

  for (i = 0; i < gst_usb_intf.bNumEndpoints; i++) 
  {
    memcpy (cp, ep [i], USB_DT_ENDPOINT_SIZE);
    cp += USB_DT_ENDPOINT_SIZE;
  }
  c->wTotalLength = __cpu_to_le16 (cp - (char *) c);
  return cp;
}

static int init_device (usb_gadget *gadget)
{
  char	buf [4096], *cp = &buf [0];
  int	fd;
  int	status;

  status = autoconfig (gadget);
	
  if (status == ERR_NO_DEVICE)
    return status;

  fd = open (gadget->DEVNAME, O_RDWR);
  if (fd < 0)
    return ERR_OPEN_FD;

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
  if (status < 0) 
  {
    close (fd);
    return ERR_WRITE_FD;
  } else if (status != (cp - buf)) 
  {
    close (fd);
	return SHORT_WRITE_FD;
  }
  return fd;
}

static void handle_control (usb_gadget *gadget, struct usb_ctrlrequest *setup)
{
  int   status, tmp;
  __u8  buf [256];
  __u16 value, index, length;
  int fd = gadget->ep0.fd;
  int verbose = gadget->verbosity;
  
  value = __le16_to_cpu(setup->wValue);
  index = __le16_to_cpu(setup->wIndex);
  length = __le16_to_cpu(setup->wLength);

  if (verbose)
    printf ("SETUP %02x.%02x "
            "v%04x i%04x %d\n",
            setup->bRequestType, setup->bRequest,
            value, index, length);

  switch (setup->bRequest) 
  {	/* usb 2.0 spec ch9 requests */
    case USB_REQ_GET_DESCRIPTOR:
      if (setup->bRequestType != USB_DIR_IN)
        goto stall;
      switch (value >> 8) 
	  {
        case USB_DT_STRING:
          tmp = value & 0x0ff;
          if (verbose > 1)
            printf ("... get string %d lang %04x\n", tmp, index);
          if (tmp != 0 && index != strings.language)
            goto stall;
          status = usb_gadget_get_string (&strings, tmp, buf);
          if (status < 0)
            goto stall;
          tmp = status;
          if (length < tmp)
            tmp = length;
          status = write (fd, buf, tmp);
          if (status < 0) 
            if (errno == EIDRM)
              fprintf (stderr, "string timeout\n");
            else
			  perror ("write string data");
          else 
            if (status != tmp) 
              fprintf (stderr, "short string write, %d\n", status);
          break;
        default:
          goto stall;
      }
      return;
    case USB_REQ_SET_CONFIGURATION:
      if (setup->bRequestType != USB_DIR_OUT)
        goto stall;
      if (verbose)
        printf ("CONFIG #%d\n", value);

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
      switch (value) 
      {
        case CONFIG_VALUE:
          if (verbose)
            printf ("Starting io\n");
          start_io (gadget);
          break;
        case 0:
          if (verbose)
            printf ("Stoping io\n");
          stop_io (gadget);
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
      if (status < 0) 
      {
        if (errno == EIDRM)
          fprintf (stderr, "GET_INTERFACE timeout\n");
      else
          perror ("write GET_INTERFACE data");
      } else 
      if (status != length) 
      {
        fprintf (stderr, "short GET_INTERFACE write, %d\n", status);
      }
      return;
    case USB_REQ_SET_INTERFACE:
      if (setup->bRequestType != USB_RECIP_INTERFACE
                || index != 0
                || value != 0)
        goto stall;

      /* just reset toggle/halt for the interface's endpoints */
      status = 0;
      if (ioctl (gadget->stream.fd, GADGETFS_CLEAR_HALT) < 0) 
      {
        status = errno;
        perror ("reset source fd");
      }
      if (ioctl (gadget->ev_up.fd, GADGETFS_CLEAR_HALT) < 0) 
      {
        status = errno;
        perror ("reset up events fd");
      }
	  if (ioctl (gadget->ev_down.fd, GADGETFS_CLEAR_HALT) < 0) 
      {
        status = errno;
        perror ("reset down events fd");
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


static const char *speed (enum usb_device_speed s)
{
  switch (s) 
  {
    case USB_SPEED_LOW:	 return "low speed";
	case USB_SPEED_FULL: return "full speed";
	case USB_SPEED_HIGH: return "high speed";
	default:             return "UNKNOWN speed";
  }
}

/*-------------------------------------------------------------------------*/

/* control thread, handles main event loop  */

#define	NEVENT		5
#define	LOGDELAY	(15 * 60)	/* seconds before stdout timestamp */

static void *simple_ep0_thread (void *param)
{
  usb_gadget *gadget = (usb_gadget*) param;
  int connected=0, fd = gadget->ep0.fd;
  int verbose = gadget->verbosity;
  struct pollfd	ep0_poll;

  gadget->stream.thread = gadget->ev_up.thread = gadget->ev_down.thread = gadget->ep0.thread = pthread_self ();
  pthread_cleanup_push (close_ep0, (void *) &(gadget->ep0.fd));
  /*
   * Signals will be handled by GStreamer
   */
	
  ep0_poll.fd = fd;
  ep0_poll.events = POLLIN | POLLOUT | POLLHUP;

  /* event loop */

  for (;;) 
  {
    int tmp;
    struct usb_gadgetfs_event	event [NEVENT];
    int	i, nevent;

    /* Use poll() to test that mechanism, to generate
     * activity timestamps, and to make it easier to
     * tweak this code to work without pthreads.  When
     * AIO is needed without pthreads, ep0 can be driven
     * instead using SIGIO.
     */

    tmp = poll(&ep0_poll, 1, -1);

    if (tmp < 0) 
    {
      /* exit path includes EINTR exits */
      perror("poll");
      break;
    }

    tmp = read (fd, &event, sizeof event);
    if (tmp < 0) 
    {
      if (errno == EAGAIN) 
      {
        /* This means a subsequent request may succeed */
		sleep (1);
        continue;
      }
      perror ("ep0 read");
      goto done;
    }
	/* Many events can be read from a single read () */
    nevent = tmp / sizeof event [0];

    for (i = 0; i < nevent; i++) 
    {
      switch (event [i].type) 
      {
        case GADGETFS_NOP:
          if (verbose)
            printf ("NOP\n");
            break;
        case GADGETFS_CONNECT:
          connected = 1;
          current_speed = event [i].u.speed;
          if (verbose)
            printf ("CONNECT %s\n", speed (event [i].u.speed));
          break;
        case GADGETFS_SETUP:
          connected = 1;
	  handle_control (gadget, &event [i].u.setup);
          break;
        case GADGETFS_DISCONNECT:
	  connected = 0;
          current_speed = USB_SPEED_UNKNOWN;
          if (verbose)
            printf("DISCONNECT\n");
            stop_io (gadget);
          break;
        case GADGETFS_SUSPEND:
          //connected = 1;
          if (verbose)
            printf ("SUSPEND\n");
          break;
        default:
          printf ("* unhandled event %d\n", event [i].type);
      }
    }
    continue;
  done:
    fflush (stdout);
    if (connected)
      stop_io (gadget);
    break;
  }
  pthread_cleanup_pop (1);
  return 0;
}

/*-------------------------------------------------------------------------*/

GADGET_EXIT_CODE usb_gadget_new (usb_gadget *gadget, VERBOSITY v)
{
  gadget->verbosity = v;
  gadget->stream.func = simple_stream_thread;
  gadget->ev_up.func = simple_ev_up_thread;
  gadget->ev_down.func = simple_ev_down_thread;
  gadget->ep0.func = simple_ep0_thread;
  gadget->connected=0;
  
  if (chdir ("/dev/gadget") < 0)
    return ERR_GAD_DIR;
	
  gadget->ep0.fd = init_device (gadget);
  if (gadget->ep0.fd < 0)
    return gadget->ep0.fd;
	
  if (pthread_create (&(gadget->ep0.thread), NULL,
	 (void *) gadget->ep0.func, (void *) gadget) != 0)
    return ERR_THRD;  

  return GAD_EOK; 
}

GADGET_EXIT_CODE usb_gadget_free (usb_gadget *gadget)
{
  /* Sub threads are canceled here */	
  stop_io(gadget);
  /* Cancel main events thread */
  pthread_cancel (gadget->ep0.thread);
  if (pthread_join (gadget->ep0.thread, 0) != 0)
    return 	ERR_JN_THRD;
  return GAD_EOK;
}

int usb_gadget_transfer (usb_gadget *gadget, 
			 GAD_EP_ADDRESS endp,
                         unsigned char *buffer,
			 int length){
  int  status;
  
  int verbose = gadget->verbosity;
  
  errno = 0;
  switch (endp)
  {
    case GAD_STREAM_EP:
	  status = read (gadget->stream.fd, buffer, length);
      if (status < 0)
        return ERR_READ_FD;
	  break;
	case GAD_DOWN_EP:
	  status = read (gadget->ev_down.fd, buffer, length);
      if (status < 0)
        return ERR_READ_FD;
	  break; 
	case GAD_UP_EP:
	  status = write (gadget->ev_up.fd, buffer, length);
      if (status < 0)
        return ERR_WRITE_FD;
	  break;      
	default:
	  return ERR_NO_DEVICE;  	  
  }	  	

  if (status == 0) 
  {
    if (verbose)
      printf ("Done %s\n", __FUNCTION__);
  } else if (status < length) /* Test for short read */
      return SHORT_READ_FD;
	  
  
  /* Is it better to return the amount of bytes read? */
  return GAD_EOK;
}
