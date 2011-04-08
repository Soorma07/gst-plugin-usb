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

GST_DEBUG_CATEGORY_STATIC (gst_usbsink_debug);
#define GST_CAT_DEFAULT gst_usbsink_debug

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

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (Gstusbsink, gst_usbsink, GstElement,
    GST_TYPE_ELEMENT);

static void gst_usbsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_usbsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_usbsink_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_usbsink_chain (GstPad * pad, GstBuffer * buf);

/* GObject vmethod implementations */

static void
gst_usbsink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "usbsink",
    "Hardware",
    "Elements that sends data across an USB link",
    "Michael Grüner <<michael.gruner@ridgerun.com>>\n\t\tDiego Dompe <<diego.dompe@ridgerun.com>>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the usbsink's class */
static void
gst_usbsink_class_init (GstusbsinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_usbsink_debug, "usbsink",
      0, "USB sink element");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_usbsink_set_property;
  gobject_class->get_property = gst_usbsink_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_usbsink_init (Gstusbsink * filter,
    GstusbsinkClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_usbsink_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_usbsink_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  filter->silent = FALSE;
}

static void
gst_usbsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstusbsink *filter = GST_USBSINK (object);

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
gst_usbsink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstusbsink *filter = GST_USBSINK (object);

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

/* this function handles the link with other elements */
static gboolean
gst_usbsink_set_caps (GstPad * pad, GstCaps * caps)
{
  Gstusbsink *filter;
  GstPad *otherpad;

  filter = GST_USBSINK (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_usbsink_chain (GstPad * pad, GstBuffer * buf)
{
  Gstusbsink *filter;

  filter = GST_USBSINK (GST_OBJECT_PARENT (pad));

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
}
