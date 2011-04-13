/*
 * GStreamer
 * Copyright (C) 2011 RidgeRun
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstusbsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_usb_sink_debug);
#define GST_CAT_DEFAULT gst_usb_sink_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstUsbSink, gst_usb_sink, GstBaseSink,
    GST_TYPE_BASE_SINK);

static void gst_usb_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_usb_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_usb_sink_set_caps 
    (GstBaseSink * bsink, GstCaps * caps);
static GstCaps * gst_usb_sink_get_caps (GstBaseSink * bsink);
static GstFlowReturn gst_usb_sink_render 
    (GstBaseSink *sink, GstBuffer *buffer);
static gboolean gst_usb_sink_start (GstBaseSink *sink);
static gboolean gst_usb_sink_stop (GstBaseSink *sink);
	



/* GObject vmethod implementations */

static void
gst_usb_sink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "usbsink",
    "Hardware",
    "Elements that sends data across an USB link",
    "Michael Grüner <<michael.gruner@ridgerun.com>>\n\t\tDiego Dompe <<diego.dompe@ridgerun.com>>");

    gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the usbsink's class */
static void
gst_usb_sink_class_init (GstUsbSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;

  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_usb_sink_debug, "usbsink",
      0, "USB sink element");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;

  gobject_class->set_property = gst_usb_sink_set_property;
  gobject_class->get_property = gst_usb_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
		  
  /* Using basesink class
   */
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_usb_sink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_usb_sink_set_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_usb_sink_render);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_usb_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_usb_sink_stop);	  
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_usb_sink_init (GstUsbSink * filter,
    GstUsbSinkClass * gclass)
{
  filter->silent = TRUE;
}

static void
gst_usb_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUsbSink *filter = GST_USB_SINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_usb_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUsbSink *filter = GST_USB_SINK (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

static GstCaps *
gst_usb_sink_get_caps (GstBaseSink * bsink)
{
  /* TODO:
   * this function is called when asked for the caps of the sink.
   * Ask here for the caps across the usb link.
   */	
  return NULL;
}

static gboolean
gst_usb_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  /* TODO:
   * this function is called when the sink pad caps are being set.
   * Set the caps of the usb src across usb link.
   */

  return TRUE;
}

static GstFlowReturn gst_usb_sink_render (GstBaseSink *sink, 
										  GstBuffer *buffer)
{
  /* TODO:
  * Each received buffer will call this function.
  * Send this buffer across usb link
  */
  return GST_FLOW_OK;
}

static gboolean gst_usb_sink_start (GstBaseSink *sink)
{
  /* TODO:
  * Init usb device here!
  */
  return TRUE;
}

static gboolean gst_usb_sink_stop (GstBaseSink *sink)
{
  /*TODO:
   * Free usb device here!
   */
  return TRUE;
}

