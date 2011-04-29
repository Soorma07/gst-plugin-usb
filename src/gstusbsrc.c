/*
 * Copyright (C) 2011 RidgeRun
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstusbsrc.h"
#include "driver.h"

GST_DEBUG_CATEGORY_STATIC (gst_usb_src_debug);
#define GST_CAT_DEFAULT gst_usb_src_debug

enum
{
  PROP_0,
};

/* the capabilities of the inputs and outputs.
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstUsbSrc, gst_usb_src, GstPushSrc,
    GST_TYPE_PUSH_SRC);

static void gst_usb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_usb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_usb_src_set_caps (GstBaseSrc * bs, GstCaps * caps);
static GstCaps *gst_usb_src_get_caps (GstBaseSrc * bs);
static GstFlowReturn gst_usb_src_create 
    (GstPushSrc * ps, GstBuffer ** buf);
static gboolean gst_usb_src_start (GstBaseSrc * bs);
static gboolean gst_usb_src_stop (GstBaseSrc * bs);


/* GObject vmethod implementations */

static void
gst_usb_src_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "usbsrc",
    "Hardware",
    "Elements that receives data across an USB link",
    "Michael Gruner <<michael.gruner@ridgerun.com>>\
	\n\t\tDiego Dompe <<diego.dompe@ridgerun.com>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

/* initialize the usbsrc's class */
static void
gst_usb_src_class_init (GstUsbSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseSrcClass *base_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_class = GST_PUSH_SRC_CLASS (klass);

  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_usb_src_debug, "usbsrc",
      0, "USB source element");

  gobject_class->set_property = gst_usb_src_set_property;
  gobject_class->get_property = gst_usb_src_get_property;
  push_class->create = gst_usb_src_create;
  base_class->get_caps = gst_usb_src_get_caps;
  base_class->set_caps = gst_usb_src_set_caps;
  base_class->start = gst_usb_src_start;
  base_class->stop = gst_usb_src_stop;
}

/* initialize the new element
 */
static void
gst_usb_src_init (GstUsbSrc * src,
    GstUsbSrcClass * gclass)
{
  /* Initialize data protocol library */	
  gst_dp_init();	
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
}

static void
gst_usb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUsbSrc *src = GST_USB_SRC (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_usb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUsbSrc *src = GST_USB_SRC (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

static gboolean
gst_usb_src_start (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
  /*
   * TODO: start USB here!
   */
   
  switch (usb_gadget_new(&(s->gadget), GLEVEL2))
  {
	case GAD_EOK:
	  break;  
    case ERR_GAD_DIR:
	  GST_WARNING("Cannot work on /dev/gadget dir. Make sure it exists\
	    and you have a gadgetfs mounted there.");
	  return FALSE;
	case ERR_OPEN_FD:
	  GST_WARNING("Can't open gadget's file descriptor");
	  return FALSE;
	case ERR_NO_DEVICE:
	  GST_WARNING("No asociated device found");  
	  return FALSE;
	case ERR_WRITE_FD:
	  GST_WARNING("Can't write to file descriptor");
	  return FALSE;
	case SHORT_WRITE_FD:
	  GST_WARNING("Short write in file descriptor, aborting...");
	  return FALSE;
    default:
      GST_WARNING("Error initializing device\n");
	  return FALSE;
  }
  
  /* Poll for gadget connection */
  GST_WARNING("Waiting for gadget to connect...");
  while (s->gadget.connected != 1)
    g_usleep(1000); /* Wait 1 milisecond */
  GST_WARNING("Gadget connected!");	
  return TRUE;
}

static gboolean
gst_usb_src_stop (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
    
  usb_gadget_free(&(s->gadget));
  return TRUE;
}

static GstFlowReturn
gst_usb_src_create (GstPushSrc * ps, GstBuffer ** buf)
{
  GstUsbSrc *s = GST_USB_SRC (ps);
  guint *size = g_malloc(sizeof(guint)), ret = GAD_EOK;
  unsigned char *header = NULL;

  
  /* Ask for the size of the header */
  if ( (ret = usb_gadget_transfer (&(s->gadget), 
                                   (unsigned char *) size, 
		  			               sizeof(guint)) ) == GAD_EOK )
  {
    g_print("src: %d\n", size[0]);				
    /* Alocate memory for the header */
    header = (void *) g_malloc(size[0]);
    
	/* Ask for the header */
    if ( (ret = usb_gadget_transfer (&(s->gadget), 
                         (unsigned char *) header, 
    					    size[0])) == GAD_EOK)	
	{												
      /* Create the buffer using gst data protocol */
	  g_print("src2: %d\n", header[0]);
      *buf = gst_dp_buffer_from_header (size[0], header);
	
	  g_print("SRC: Buffer size %d\n", (*buf)->size);
	  return GST_FLOW_OK;
    }
  }
  
  /* Free vectors */
  if (header != NULL)
    g_free(header);
  g_free(size);
  
  switch (ret)
  {
    case ERR_OPEN_FD:
      GST_WARNING("Error opening file descriptor for transfer");
	  return GST_FLOW_ERROR;
    case ERR_READ_FD:
      GST_WARNING("Can't write to file descriptor");
	  return GST_FLOW_ERROR;
    case SHORT_READ_FD:
      GST_WARNING("File descriptor short read, aborting.");
	  return GST_FLOW_ERROR;
	default:
      break;	   
  }	  

  /* It should never reach here */
  return GST_FLOW_OK;
}

static GstCaps *
gst_usb_src_get_caps (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
  GstCaps *caps;
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;
  
  g_free(length);
  return NULL;
  
  /* Ask for the size of the header */
  if ( usb_gadget_transfer (&(s->gadget), 
                           (unsigned char *) length, 
		  			       sizeof(guint)) != GAD_EOK)
  {	
	g_free(length);  		
    return NULL;
  }
  
  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
  if ( usb_gadget_transfer (&(s->gadget), 
                           (unsigned char *) header, 
  					       length[0]) != GAD_EOK)	
  {	
	g_free(length);
	g_free(header);  											
	return NULL;
  }
  
  /* Check for the payload type to be a caps header */
  if (*((guint16 *)(header+4)) != GST_DP_PAYLOAD_CAPS)
  {
	g_free(length);
	g_free(header);  
    return NULL;
  }
  
  /* Get the size of the payload */
  paylength = *((guint16 *)(header+6));
  payload = g_malloc(paylength);
  
  /* Ask for the payload */
  if ( usb_gadget_transfer (&(s->gadget), 
                           (unsigned char *) payload, 
  					       paylength) != GAD_EOK)
  {
	g_free(length);  
	g_free(payload);
	g_free(header);  
    return NULL;
  }
  
  /* Finally get caps from header and payload */
  caps = gst_dp_caps_from_packet (length[0],
                                  header,
                                  payload);
								  
  /* Free allocated vectors */
  g_free(length);  
  g_free(payload);
  g_free(header);  
  return caps;								  
  
#if 0
 
  GST_DEBUG ("width = %d, height=%d", width, height);
  return gst_caps_new_simple ("video/x-raw-rgb",
      "bpp", G_TYPE_INT, xcontext->bpp,
      "depth", G_TYPE_INT, xcontext->depth,
      "endianness", G_TYPE_INT, xcontext->endianness,
      "red_mask", G_TYPE_INT, xcontext->r_mask_output,
      "green_mask", G_TYPE_INT, xcontext->g_mask_output,
      "blue_mask", G_TYPE_INT, xcontext->b_mask_output,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1,
      "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1,
      NULL);
#endif
}

static gboolean
gst_usb_src_set_caps (GstBaseSrc * bs, GstCaps * caps)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
#if 0
  GstStructure *structure;
  const GValue *new_fps;

  /* If not yet opened, disallow setcaps until later */
  if (!s->xcontext)
    return FALSE;

  /* The only thing that can change is the framerate downstream wants */
  structure = gst_caps_get_structure (caps, 0);
  new_fps = gst_structure_get_value (structure, "framerate");
  if (!new_fps)
    return FALSE;

  /* Store this FPS for use when generating buffers */
  s->fps_n = gst_value_get_fraction_numerator (new_fps);
  s->fps_d = gst_value_get_fraction_denominator (new_fps);

  GST_DEBUG_OBJECT (s, "peer wants %d/%d fps", s->fps_n, s->fps_d);
#endif
  return TRUE;
}

