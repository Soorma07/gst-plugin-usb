/*
 * GStreamer
 * Copyright (C) 2011 RidgeRun
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>
#include <pthread.h>
#include "gstusbsink.h"


GST_DEBUG_CATEGORY_STATIC (gst_usb_sink_debug);
#define GST_CAT_DEFAULT gst_usb_sink_debug

#define GST_USB_SINK_GET_STATE_LOCK(s) \
  (GST_USB_SINK(s)->state_lock)
#define GST_USB_SINK_STATE_LOCK(s) \
  (g_mutex_lock(GST_USB_SINK_GET_STATE_LOCK(s)))
#define GST_USB_SINK_STATE_UNLOCK(s) \
  (g_mutex_unlock(GST_USB_SINK_GET_STATE_LOCK(s)))


enum
{
  PROP_0,
  PROP_USBSYNC
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
static GstStateChangeReturn gst_usb_sink_change_state (GstElement *
    element, GstStateChange transition);

/* Extra functions */
void *gst_usb_sink_up_event (void *sink);	
static void close_up_event(void *param);
static GstCaps * gst_usb_sink_receive_caps(GstUsbSink *s);
static gboolean gst_usb_sink_send_caps(GstUsbSink *s, GstCaps *caps);


/* GObject vmethod implementations */

static void
gst_usb_sink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "usbsink",
    "Hardware",
    "Elements that sends data across an USB link",
    "Michael Gruner <<michael.gruner@ridgerun.com>>\n\t\tDiego Dompe <<diego.dompe@ridgerun.com>>");

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

  gstelement_class->change_state =
    GST_DEBUG_FUNCPTR (gst_usb_sink_change_state);
		  
  /* Using basesink class
   */
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_usb_sink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_usb_sink_set_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_usb_sink_render);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_usb_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_usb_sink_stop);	

    g_object_class_install_property (gobject_class, PROP_USBSYNC,
				     g_param_spec_boolean ("usbsync", "UsbSync", "Synchronize timestamps with src time",
							   TRUE, G_PARAM_READWRITE));    
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_usb_sink_init (GstUsbSink * s,
    GstUsbSinkClass * gclass)
{	
  /* Initialize the data protocol library */	
  gst_dp_init();	
  s->usbsync = TRUE;

  s->play=FALSE;
  s->host = g_malloc(sizeof(usb_host));
  s->caps = NULL;
  s->emptycaps = TRUE;
  s->state_lock = g_mutex_new ();	  
}

static void
gst_usb_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUsbSink *filter = GST_USB_SINK (object);

  switch (prop_id) {
    case PROP_USBSYNC:
      filter->usbsync = g_value_get_boolean (value);
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
    case PROP_USBSYNC:
      g_value_set_boolean (value, filter->usbsync);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

static GstCaps *
gst_usb_sink_get_caps (GstBaseSink * bs)
{
  GstUsbSink *s = GST_USB_SINK (bs);  
  guint *notification = g_malloc(sizeof(guint));

  /* If device is not connected try later */
  if (s->host->connected != 1)
    return NULL;
		
  /* Wait for device to finish tasks */
  GST_USB_SINK_STATE_LOCK(s);
  
  s->emptycaps = TRUE;
  notification[0] = GST_USB_GET_CAPS; /* Ask for src's caps */	 
  
  if (usb_host_device_transfer(s->host, 
			       EP1_OUT, 
 			       (unsigned char *) notification,
		               sizeof(guint),
			       0) == ERR_TRANSFER)
  {   
    g_free(notification);
    GST_USB_SINK_STATE_UNLOCK(s);
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
    ("Error sending get caps notification"));
    return NULL;						  
  } 
  
  g_free(notification);	
  GST_USB_SINK_STATE_UNLOCK(s);
 
  GST_DEBUG_OBJECT(s, "Waiting for caps");
  /* Wait until up_events thread fills the caps */
  while (s->emptycaps)
    g_usleep(1000);	
  
  GST_DEBUG_OBJECT(s, "Caps received");
  
  return s->caps;
}

static gboolean
gst_usb_sink_set_caps (GstBaseSink * bs, GstCaps * caps)
{
  GstUsbSink *s = GST_USB_SINK (bs);
  guint *notification =	g_malloc(sizeof(guint));
  gboolean ret=TRUE;
  
  /* If device is not connected try later */
  if (s->host->connected != 1)
    return FALSE;
  
  /* Wait for host to finish tasks */
  GST_USB_SINK_STATE_LOCK(s);
  
  /* Ask the src to set the following caps */
  notification[0] = GST_USB_SET_CAPS; 

  /* Notify that these caps are needed to be set */	
  if (usb_host_device_transfer(s->host, 
                               EP1_OUT, 
                              (unsigned char *) notification,
                               sizeof(guint),
                               0) == ERR_TRANSFER)
  {   
    g_free(notification);
    GST_USB_SINK_STATE_UNLOCK(s);
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Error sending caps"));
    return FALSE;						  
  } 
  
  ret = gst_usb_sink_send_caps(s, caps);
  GST_USB_SINK_STATE_UNLOCK(s);

  return TRUE;
}

static GstFlowReturn gst_usb_sink_render (GstBaseSink *bs, 
					  GstBuffer *buffer)
{
  GstUsbSink *s = GST_USB_SINK (bs);
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint *length; 
  guint8 *header;
  
  /* Syncronize timestamps */
  if (s->usbsync)
    GST_BUFFER_TIMESTAMP(buffer) -= s->sync;
  
  

  /* Start transfer */  
  length = g_malloc(sizeof(guint));
  gdp->header_from_buffer(buffer,
                          GST_DP_HEADER_FLAG_NONE,
			  &length[0],
                          &header);

  /* Send as first byte the header size */
  if (usb_host_device_transfer(s->host, 
                               EP2_OUT, 
                              (unsigned char *) length,
                               sizeof(guint),
                               0) == ERR_TRANSFER)
  {
    g_free(length);
    g_free(header);
    gst_dp_packetizer_free (gdp);
    return GST_FLOW_ERROR;								  
  }
  /* Now send the header */									 
  if (usb_host_device_transfer(s->host, 
                               EP2_OUT, 
                              (unsigned char *) header,
                               length[0],
                               0) == ERR_TRANSFER)
  {   
    g_free(length);
    g_free(header);
    gst_dp_packetizer_free (gdp);
    return GST_FLOW_ERROR;								  
  }
  /* Now send the buffer */									 
  if (usb_host_device_transfer(s->host, 
                               EP2_OUT, 
                              (unsigned char *) buffer->data,
                               buffer->size,
                               0) == ERR_TRANSFER)
  {   
    g_free(length);
    g_free(header);
    gst_dp_packetizer_free (gdp);
    return GST_FLOW_ERROR;								  
  }
  g_free(length);
  g_free(header);
  gst_dp_packetizer_free (gdp); 

  return GST_FLOW_OK;
}

/* Use this to define a search timeout, currently there's no*/
#define TIMEOUT 10

static gboolean gst_usb_sink_start (GstBaseSink *bs)
{
  GstUsbSink *s = GST_USB_SINK (bs);   

  /* Init usb context */
  if (usb_host_new(s->host, LEVEL3) != EOK)
  {
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
            ("Failed opening usb context!"));
    return FALSE;
  }
  GST_DEBUG_OBJECT(s, "Success opening usb context.");
  
  /* Give a little time to gadget to connect */
  GST_DEBUG_OBJECT(s, "Searching for a gadget device");
  /* TODO: see if a timeout is required */
  for (;;)
  {
    /* Usb host object, vendor ID, product ID */
    if (usb_host_device_open(s->host, 0x0525, 0xa4a4)==EOK)
    {
      GST_DEBUG_OBJECT(s, "Found a gadget device.");
      goto success;	  
    }
  }
  GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
    ("Error opening usb device!"));
  return FALSE;
  
success:  
  GST_USB_SINK_STATE_UNLOCK(s);
  /* Create the up events thread to receive connection form gadget */
  if (pthread_create (&(s->host->up_events), NULL,
	 (void *) gst_usb_sink_up_event, (void *) bs) != 0)
  {
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
      ("Unable to create up events thread, aborting.."));	  
    return FALSE;
  }
  
  /* Waiting for the connected notification on the events thread*/
  while (s->host->connected != 1)
    g_usleep(1000); /* Wait a millisecond */
  GST_DEBUG_OBJECT(s, "Connection stablished");
   	
  return TRUE;
}

static gboolean gst_usb_sink_stop (GstBaseSink *bs)
{
  GstUsbSink *s = GST_USB_SINK (bs); 

  /* Init usb context */
  GST_DEBUG_OBJECT(s, "Closing usb device");
  /* Cancel main events thread */
  pthread_cancel (s->host->up_events);
  usb_host_free(s->host);
  g_free(s->host);

  return TRUE;
}


/* Up events thread */
void *gst_usb_sink_up_event (void *sink)
{
  GstBaseSink *bs = (GstBaseSink *)sink;	
  GstUsbSink *s = GST_USB_SINK (bs);
  guint *notification = g_malloc(sizeof(guint)), ret;	
  
  pthread_cleanup_push (close_up_event, (void *) notification);
  
  while (TRUE)
  {
    /* Create a cancellation test point */  
    pthread_testcancel();  
    
    /* Receive an event (internal polling) */
    /*
     *  FIXME: If there is no timeout process sleeps
     *  and blocks all other threads. Made a workaround
     *  setting timeout to 1 millisecond. Find a way to 
     *  fix this
     */
    if ((ret = usb_host_device_transfer(s->host, 
	  				EP1_IN, 
		  			(unsigned char *) notification,
			  		sizeof(guint),
				  	1)) == ERR_TRANSFER){ 
      continue;	
    }
    /* Wait until device is free */
    GST_USB_SINK_STATE_LOCK(s);
    switch (notification[0]){ 	  
      /* Src is returning his possible caps */		
    case GST_USB_CAPS:
      GST_DEBUG_OBJECT(s, "Received a caps"); 
      s->caps = gst_usb_sink_receive_caps(s);
      GST_DEBUG_OBJECT(s, "Caps received from src");
      s->emptycaps = FALSE;
      break;
      /* Gadget has finished connecting */
    case GST_USB_CONNECTED:
      GST_DEBUG_OBJECT(s, "Received connection notice from src");
      s->host->connected = 1;
      break;	
      /* Gadget is ready to play */
    case GST_USB_PLAY:
      GST_DEBUG_OBJECT(s, "Received play notice from src");
      s->play = TRUE;
      break;	
/*     TODO: Add the stop notification here if needed */
    default:
      GST_WARNING_OBJECT(s, "Unknown downstream event");
      break;	  	
    }
    GST_USB_SINK_STATE_UNLOCK(s);
  }
  /* It should never reach this point */
  pthread_cleanup_pop (1);	 	  
}	

static void close_up_event(void *param)
{
  GST_INFO("Closing up events thread");	
  guint *notification = (guint *) param;	
  g_free(notification);
}

static GstCaps* gst_usb_sink_receive_caps(GstUsbSink *s)
{
  GstCaps *caps;
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;
  	
	/* Ask for the size of the header */
  if ( usb_host_device_transfer (s->host,
				 EP1_IN,   
				 (unsigned char *) length, 
				 sizeof(guint), 
				 0) == ERR_TRANSFER){	
    g_free(length);  		
    return NULL;
  }

  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
  if ( usb_host_device_transfer (s->host,
				 EP1_IN,   
				 (unsigned char *) header, 
				 length[0], 
				 0) == ERR_TRANSFER){	
    g_free(length);
    g_free(header);  											
    return NULL;
  }
  

  /* Check for the payload type to be a caps header */
  if (GST_READ_UINT16_BE(header+4) != GST_DP_PAYLOAD_CAPS){
    GST_WARNING_OBJECT(s, "Received header is not a cap's header");  
    g_free(length);
    g_free(header);  
    return NULL;
  }

  /* Get the size of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  payload = g_malloc(paylength);

  /* Ask for the payload */
  if ( usb_host_device_transfer (s->host,
				 EP1_IN,   
				 (unsigned char *) payload, 
				 paylength, 
				 0) == ERR_TRANSFER){
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
} 

static gboolean gst_usb_sink_send_caps(GstUsbSink *s, GstCaps *caps)
{
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;	
   
  /* Make a package from the given caps */	
  gdp->packet_from_caps(caps,
                        GST_DP_HEADER_FLAG_NONE,
                        &length[0],
                        &header,
                        &payload);
  
  /* Send the size of the header */
  if (usb_host_device_transfer(s->host, 
			       EP1_OUT, 
			       (unsigned char *) length,
			       sizeof(guint),
			       0) == ERR_TRANSFER){   
    g_free(length);
    g_free(header);
    g_free(payload);
    gst_dp_packetizer_free (gdp);
    return FALSE;								  
  }

  /* Now send the header */
  if (usb_host_device_transfer(s->host, 
			       EP1_OUT, 
			       (unsigned char *) header,
			       length[0],
			       0) == ERR_TRANSFER){   
    g_free(length);
    g_free(header);
    g_free(payload);
    gst_dp_packetizer_free (gdp);
    return FALSE;								  
  }

  /* Get the length of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  
  /* Send the payload */
  if (usb_host_device_transfer(s->host, 
			       EP1_OUT, 
			       (unsigned char *) payload,
			       paylength,
			       0) == ERR_TRANSFER){   
    g_free(length);
    g_free(header);
    g_free(payload);
    gst_dp_packetizer_free (gdp);
    return FALSE;								  
  }
  
  g_free(length);
  g_free(header);
  g_free(payload);
  gst_dp_packetizer_free (gdp);
  return TRUE;
}

static GstStateChangeReturn
gst_usb_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstUsbSink *sink = GST_USB_SINK (element);
  guint *notification = g_malloc(sizeof(guint));

  switch (transition) {
    
  case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
    while (!sink->play)
      g_usleep(10);
    sink->sync= gst_util_get_timestamp()- gst_element_get_base_time(element);
    GST_DEBUG_OBJECT(sink, "Estimated %" GST_TIME_FORMAT " for time sync", GST_TIME_ARGS(sink->sync));
  }
    break;
  default:
    break;
    
  }
  
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  
  switch (transition) {
  case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    
    break;
  default:
    break;
  }
  
  g_free(notification);

  return ret;
}
