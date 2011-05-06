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
static gboolean gst_usb_sink_event(GstBaseSink *sink, GstEvent *event);

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
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_usb_sink_event);	  

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
  s->silent = TRUE;

  s->host = g_malloc(sizeof(usb_host));
  s->caps = NULL;
  s->emptycaps = TRUE;
  s->busy = TRUE;
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
gst_usb_sink_get_caps (GstBaseSink * bs)
{
  GstUsbSink *s = GST_USB_SINK (bs);  
  guint *notification = g_malloc(sizeof(guint));

  if (s->host->connected != 1)
    return NULL;
		
  /* Wait for device to finish tasks */
  while (s->busy)
    g_usleep(1000);	
  s->busy = TRUE;
  
  s->emptycaps = TRUE;
  notification[0] = GST_USB_GET_CAPS; /* Ask for src's caps */	 
  
  if (usb_host_device_transfer(s->host, 
								  EP1_OUT, 
								  (unsigned char *) notification,
								  sizeof(guint),
								  0) == ERR_TRANSFER)
  {   
	g_free(notification);	
	GST_WARNING("Error sending get caps notification");
	s->busy = FALSE;
	return NULL;						  
  } 
  
  //~ caps = gst_usb_sink_receive_caps(s);
  g_free(notification);	
  s->busy=FALSE;
 
  GST_WARNING("Waiting for caps");
  /* Wait until up_events thread fills the caps */
  while (s->emptycaps)
    g_usleep(1000);	
  
  GST_WARNING("Caps received");
  
  return s->caps;
}

static gboolean
gst_usb_sink_set_caps (GstBaseSink * bs, GstCaps * caps)
{
  GstUsbSink *s = GST_USB_SINK (bs);
  guint *notification =	g_malloc(sizeof(guint)), ret=TRUE;
  
  if (s->host->connected != 1)
    return FALSE;
  
  /* Wait for host to finish tasks */
  while (s->busy)
    g_usleep(1000);
  s->busy = TRUE;
  
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
	GST_WARNING("Error sending caps");
	s->busy = FALSE;
	return FALSE;						  
  } 
  
  ret = gst_usb_sink_send_caps(s, caps);
  s->busy = FALSE;

  return TRUE;
}

static GstFlowReturn gst_usb_sink_render (GstBaseSink *bs, 
										  GstBuffer *buffer)
{
  /* TODO:
  * Each received buffer will call this function.
  * Send this buffer across usb link
  */
  GstUsbSink *s = GST_USB_SINK (bs);
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint *length; 
  guint8 *header;
  
  
  g_print("Render in\n");
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

#define TIMEOUT 10

static gboolean gst_usb_sink_start (GstBaseSink *bs)
{
  GstUsbSink *s = GST_USB_SINK (bs);   

  /* Init usb context */
  if (usb_host_new(s->host, LEVEL0) != EOK)
  {
	GST_WARNING("Failed opening usb context!");
    return FALSE;
  }
  GST_WARNING("Success opening usb context.");
  
  /* Give a little time to gadget to connect */
  GST_WARNING("Searching for a gadget device");
  for (;;)
  {
    /* Usb host object, vendor ID, product ID */
    if (usb_host_device_open(s->host, 0x0525, 0xa4a4)==EOK)
    {
      GST_WARNING("Found a gadget device.");
      goto success;	  
    }
  }
  GST_WARNING("Error opening usb device!");
  return FALSE;
  
success:  
  s->busy = FALSE;
  /* Create the up events thread to receive connection form gadget */
  if (pthread_create (&(s->host->up_events), NULL,
	 (void *) gst_usb_sink_up_event, (void *) bs) != 0)
  {
    GST_WARNING("Unable to create up events thread, aborting..");	  
    return FALSE;
  }
  
  /* Waiting for the connected notification on the events thread*/
  while (s->host->connected != 1)
    g_usleep(1000); /* Wait a millisecond */
  GST_WARNING("Connection stablished");
   	
  return TRUE;
}

static gboolean gst_usb_sink_stop (GstBaseSink *bs)
{
  GstUsbSink *s = GST_USB_SINK (bs); 
  
  /* Init usb context */
  GST_WARNING("Closing usb device.");
  /* Cancel main events thread */
  pthread_cancel (s->host->up_events);
  
  usb_host_free(s->host);
  g_free(s->host);
  return TRUE;
}

static gboolean gst_usb_sink_event (GstBaseSink *sink, GstEvent *event)
{
  
  /* TODO:
   * Events need to be handled across usb link!
   */
   /* Gstreamer is telling us to send the message */
   //if (GST_EVENT_TYPE (event) == GST_EVENT_SINK_MESSAGE )
   //{
	   /* Send event here */
	 //  return TRUE;
   //} 
  return TRUE;
}


/* Up events thread */
void *gst_usb_sink_up_event (void *sink)
{
  GstBaseSink *bs = (GstBaseSink *)sink;	
  GstUsbSink *s = GST_USB_SINK (bs);
  guint *notification = g_malloc(sizeof(guint)), ret;	
  //~ GstCaps *caps;
  
  pthread_cleanup_push (close_up_event, (void *) notification);
  
  while (TRUE)
  {
    //~ g_print("sink: Receiving notification\n");	  
	  
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
				  				 1)) == ERR_TRANSFER)
    { 
	  /* FIXME: it has received 5 bytes when timeout occurs*/	
	  if (ret == 5)
	    continue;	
      GST_WARNING("Error receving upstream event");
	  continue;    							      
    }
	/* Wait until device is free */
	while (s->busy)
      g_usleep(1000);	
	s->busy = TRUE; 	
    switch (notification[0])
    { /* Src is asking for our allowed caps */
	  //~ case GST_USB_GET_CAPS:
	  //~ {   
		//~ notification[0] = GST_USB_CAPS;
		//~ g_print("sink: Received a get caps\n"); 
		//~ if (usb_host_device_transfer(s->host, 
							         //~ EP1_OUT, 
							         //~ (unsigned char *) notification,
							         //~ sizeof(guint),
							         //~ 0) == ERR_TRANSFER)
          //~ GST_WARNING("Error returning caps");	
//~ 
		//~ g_print("sink: Returning my caps\n");
		//~ if (gst_pad_is_linked(GST_BASE_SINK_PAD(bs)))
		  //~ caps = gst_pad_peer_get_caps (GST_BASE_SINK_PAD(bs));
		//~ else
		  //~ caps = gst_caps_copy (gst_pad_get_pad_template_caps\
		                       //~ (GST_BASE_SINK_PAD(bs)));
		//~ g_print("sink: Found caps\n");					   
	    //~ if (!gst_usb_sink_send_caps(s, caps))
		  //~ GST_WARNING("Error sending caps");
		//~ g_print("sink: Caps sent\n");  
		//~ gst_caps_unref(caps);
		//~ break;
	  //~ }
	  //~ 
	  //~ /* Sink is sending a set of caps for src to set */
	  //~ case GST_USB_SET_CAPS:
	  	//~ g_print("sink: Received a set caps\n"); 
	    //~ caps = gst_usb_sink_receive_caps(s);
		//~ if (!gst_pad_set_caps(GST_BASE_SINK_PAD(bs),caps))
		  //~ GST_WARNING("Unable to set received caps");
		//~ gst_caps_unref(caps);
		//~ s->prueba=TRUE;
		//~ break;
	  
	  /* Src is returning his possible caps */		
	  case GST_USB_CAPS:
	  	g_print("sink: Received a caps\n"); 
	    s->caps = gst_usb_sink_receive_caps(s);
		if (s->caps == NULL)
		  GST_WARNING("Caps are null");
		else
		  GST_WARNING("Caps are not null");
		GST_WARNING("Caps received from src");
		s->emptycaps = FALSE;
		break;
      
	  /* Gadget has finished connecting */
	  case GST_USB_CONNECTED:
	    GST_WARNING("Received connection notice from src");
		s->host->connected = 1;
		break;			  	
	  default:
	    GST_WARNING("Unknown downstream event");
		break;	  	
    }
	s->busy = FALSE;
  }
  /* It should never reach this point */
  pthread_cleanup_pop (1);	 	  
}	

static void close_up_event(void *param)
{
  GST_WARNING("Closing up events thread");	
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
						   0) == ERR_TRANSFER)
  {	
	g_free(length);  		
    return NULL;
  }

  g_print("sink: Received length %d\n", length[0]);
  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
 if ( usb_host_device_transfer (s->host,
                           EP1_IN,   
                           (unsigned char *) header, 
  					       length[0], 
						   0) == ERR_TRANSFER)
  {	
	g_free(length);
	g_free(header);  											
	return NULL;
  }
  

  /* Check for the payload type to be a caps header */
  if (GST_READ_UINT16_BE(header+4) != GST_DP_PAYLOAD_CAPS)
  {
	GST_WARNING("Received header is not a cap's header");  
	g_free(length);
	g_free(header);  
    return NULL;
  }

  /* Get the size of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  payload = g_malloc(paylength);
  g_print("sink: Received header with paylength %d\n", paylength);
  /* Ask for the payload */
 if ( usb_host_device_transfer (s->host,
                           EP1_IN,   
                           (unsigned char *) payload, 
  					       paylength, 
						   0) == ERR_TRANSFER)
  {
	g_free(length);  
	g_free(payload);
	g_free(header);  
    return NULL;
  }
  
  g_print("sink: Received payload\n");
  /* Finally get caps from header and payload */
  caps = gst_dp_caps_from_packet (length[0],
                                  header,
                                  payload);
								  
  /* Free allocated vectors */
  g_free(length);  
  g_free(payload);
  g_free(header);  
  g_print("sink: Exit recevied caps\n");
  return caps;	
} 

static gboolean gst_usb_sink_send_caps(GstUsbSink *s, GstCaps *caps)
{
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;	
  
  g_print("sink: Inside sending caps 1\n");
  /* If host is not initialized try get caps later */
  //~ if (s->host->connected != 1)
    //~ return FALSE;	 
  
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
								  0) == ERR_TRANSFER)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	return FALSE;								  
  }
  g_print("sink: Sent header %d\n", length[0]);
  /* Now send the header */
  if (usb_host_device_transfer(s->host, 
								  EP1_OUT, 
								  (unsigned char *) header,
								  length[0],
								  0) == ERR_TRANSFER)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	return FALSE;								  
  }
  g_print("sink: Sent header\n");
  /* Get the length of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  
  /* Send the payload */
  if (usb_host_device_transfer(s->host, 
								  EP1_OUT, 
								  (unsigned char *) payload,
								  paylength,
								  0) == ERR_TRANSFER)
  {   
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
  GST_WARNING("Caps sent");
  return TRUE;
}
