/*
 * Copyright (C) 2011 RidgeRun
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <pthread.h>

#include "gstusbsrc.h"


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
static GstCaps *gst_usb_src_get_caps (GstBaseSrc * bs);
static GstFlowReturn gst_usb_src_create 
    (GstPushSrc * ps, GstBuffer ** buf);
static gboolean gst_usb_src_start (GstBaseSrc * bs);
static gboolean gst_usb_src_stop (GstBaseSrc * bs);
static gboolean gst_usb_src_event(GstBaseSrc *bs, GstEvent *event);

void *gst_usb_src_down_event (void *src);	
static void close_down_event(void *param);
static gboolean gst_usb_src_send_caps(GstUsbSrc *s, GstCaps *caps);
static GstCaps *gst_usb_src_receive_caps(GstUsbSrc *s);
static GstEvent *gst_usb_src_receive_event(GstUsbSrc *s);


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
  base_class->start = gst_usb_src_start;
  base_class->stop = gst_usb_src_stop;
  //~ base_class->event = gst_usb_src_event;
}

/* initialize the new element
 */
static void
gst_usb_src_init (GstUsbSrc * s,
    GstUsbSrcClass * gclass)
{
  /* Initialize data protocol library */	
  gst_dp_init();	
  gst_base_src_set_live (GST_BASE_SRC (s), TRUE);
  s->gadget = g_malloc(sizeof(usb_gadget));
  s->caps=NULL;
  s->emptycaps=TRUE;
  s->busy=TRUE;
}

static void
gst_usb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //~ GstUsbSrc *src = GST_USB_SRC (object);

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
  //~ GstUsbSrc *src = GST_USB_SRC (object);

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
  guint *notification = g_malloc(sizeof(guint)); 
   
  switch (usb_gadget_new(s->gadget, GLEVEL2))
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
  while (s->gadget->connected != 1)
    g_usleep(1000); /* Wait 1 milisecond */
  GST_WARNING("Gadget connected!");
  
  /* Send sink the connection notification */
  notification[0] = GST_USB_CONNECTED;
  GST_WARNING("Notifying sink of connection status");
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                            (unsigned char *) notification, 
		  			        sizeof(guint)) != GAD_EOK)
  {
    g_free(notification);	  							
    GST_WARNING("Error Establishing connection with sink");
	return FALSE;
  }		
  g_free(notification);
  
  
  /* Create a thread for downstream events */
  if (pthread_create (&(s->gadget->ev_down.thread), NULL,
	 (void *) gst_usb_src_down_event, (void *) bs) != 0)
  {
    GST_WARNING("Unable to create down events thread, aborting..");	  
    return FALSE;
  }	
  s->busy = FALSE;
  return TRUE;
}

static gboolean
gst_usb_src_stop (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
    
  usb_gadget_free(s->gadget);
  g_free(s->gadget);
  return TRUE;
}

#define PRINTERR(ret) switch(ret) \
                       { \
						  case ERR_OPEN_FD:\
                            GST_WARNING("Error opening file descriptor for transfer");\
                            break;\
                          case ERR_READ_FD:\
                            GST_WARNING("Can't write to file descriptor");\
                            break;\
						  case SHORT_READ_FD:\
                            GST_WARNING("File descriptor short read, aborting.");\
	                        break;\
	                      default:\
                            break;\
                       }	   
					   
static GstFlowReturn
gst_usb_src_create (GstPushSrc * ps, GstBuffer ** buf)
{
  GstUsbSrc *s = GST_USB_SRC (ps);
  guint *size = g_malloc(sizeof(guint)), ret;
  unsigned char *header = NULL;
  guint8 *data, *payload, paylength;
  GstCaps *caps;
  
  //~ g_print("create\n");
  /* Ask for the size of the header */
  if ( (ret=usb_gadget_transfer (s->gadget,
                                   GAD_STREAM_EP, 
                                   (unsigned char *) size, 
		  			               sizeof(guint)))  != GAD_EOK )
  {	
    g_free(size);
	PRINTERR(ret)
    return GST_FLOW_ERROR;
  }
  
  /* Alocate memory for the header */
    header = (void *) g_malloc(size[0]);
  /* Ask for the header */
  if ( (ret=usb_gadget_transfer (s->gadget, 
                                   GAD_STREAM_EP,  
                                  (unsigned char *) header, 
    		        			    size[0])) != GAD_EOK)	
  {												
    g_free(size);
	g_free(header);
	PRINTERR(ret)
    return GST_FLOW_ERROR;
  }
	
  /* Create the buffer using gst data protocol */
  *buf = gst_dp_buffer_from_header (size[0], header);

  /* Now read the data */
  data = g_malloc((*buf)->size);
  /* Ask for the header */
  if ( (ret=usb_gadget_transfer (s->gadget,
                                   GAD_STREAM_EP,   
                                  (unsigned char *) data, 
    		        			    (*buf)->size)) != GAD_EOK)	
  {	
	g_free(data);  											
    g_free(size);
	g_free(header);
	PRINTERR(ret)
	gst_buffer_unref(*buf);
    return GST_FLOW_ERROR;
  }	
  /* Insert data on GstBuffer */
  gst_buffer_set_data(*buf, data, (*buf)->size);
  
  /* GDP doesn't send caps so receive them manually */ 
  g_free(header);
  /* Ask for the size of the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_STREAM_EP,   
                           (unsigned char *) size, 
		  			       sizeof(guint)) != GAD_EOK)
  {	
	g_free(size);  		
    return GST_FLOW_ERROR;
  }

  /* Alocate memory for the header */
  header = (void *) g_malloc(size[0]);
    
  /* Ask for the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_STREAM_EP,   
                           (unsigned char *) header, 
  					       size[0]) != GAD_EOK)	
  {	
	g_free(size);
	g_free(header);  											
	return GST_FLOW_ERROR;
  }

  /* Check for the payload type to be a caps header */
  if (GST_READ_UINT16_BE(header+4) != GST_DP_PAYLOAD_CAPS)
  {
	GST_WARNING("Received header is not a cap's header");  
	g_free(size);
	g_free(header);  
    return GST_FLOW_ERROR;
  }

  /* Get the size of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  payload = g_malloc(paylength);
  
  /* Ask for the payload */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_STREAM_EP, 
                           (unsigned char *) payload, 
  					       paylength) != GAD_EOK)
  {
	g_free(size);  
	g_free(payload);
	g_free(header);  
    return GST_FLOW_ERROR;
  }
  
  /* Finally get caps from header and payload */
  caps = gst_dp_caps_from_packet (size[0],
                                  header,
                                  payload);
  	
  gst_buffer_set_caps(*buf, caps);
  /* Free vectors */
  //~ gst_caps_unref(caps);
  //~ g_print("create end\n");
  g_free(payload);
  g_free(data);
  g_free(header);
  g_free(size); 

  return GST_FLOW_OK;
}

static GstCaps *
gst_usb_src_get_caps (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);  
  
  if(s->gadget->connected != 1)
    return NULL;
	
  while (s->emptycaps)
    g_usleep(1000);
  
  return s->caps;		  
}


/* Down events thread */
void *gst_usb_src_down_event (void *src)
{
  GstBaseSrc *bs = (GstBaseSrc *)src;	
  GstUsbSrc *s = GST_USB_SRC (bs);	
  guint *notification = g_malloc(sizeof(guint));	
  GstCaps *caps;
  
  pthread_cleanup_push (close_down_event, (void *) notification);
  
  while (TRUE)
  {
	/* Create a cancellation test point */  
	pthread_testcancel();  
	
	g_print("src: Receiving notification\n");

    /* Receive an event (internal polling) */
    if (usb_gadget_transfer(s->gadget, 
	  							  GAD_DOWN_EP, 
		  						  (unsigned char *) notification,
			  					  sizeof(guint)) != GAD_EOK)
    { 
      GST_WARNING("Error receving downstream event");
	  continue;    							      
    }
	g_print("src: Oh noooooo\n");
	/* Wait until gadget is free */
	while (s->busy)
      g_usleep(1000);	
    s->busy = TRUE;
    switch (notification[0])
    { /* Sink is asking for our allowed caps */
	  case GST_USB_GET_CAPS:
	  {		  
		notification[0] = GST_USB_CAPS;
		g_print("src: Received a get caps\n");
		if ( usb_gadget_transfer (s->gadget,
                                 GAD_UP_EP,   
                                 (unsigned char *) notification, 
		  			             sizeof(guint)) != GAD_EOK)
          GST_WARNING("Error returning caps");	

		g_print("src: Returning my caps\n");
		if (gst_pad_is_linked(GST_BASE_SRC_PAD(bs)))
		  caps = gst_pad_peer_get_caps (GST_BASE_SRC_PAD(bs));
		else
		  caps = gst_caps_copy (gst_pad_get_pad_template_caps\
		                       (GST_BASE_SRC_PAD(bs)));
		if (caps == NULL)
		  GST_WARNING("Caps are null");
		else
		  GST_WARNING("Caps are not null");					   
		g_print("src: Found caps\n");					   
	    if (!gst_usb_src_send_caps(s, caps))
		  GST_WARNING("Error sending caps");
		g_print("src: Caps sent\n");
		gst_caps_unref(caps);
		break;
	  }
	  /* Sink is sending a set of caps for src to set */
	  case GST_USB_SET_CAPS:
	    g_print("src: Received a set caps\n");
	    s->caps = gst_usb_src_receive_caps(s);
		s->emptycaps = FALSE;
		g_print("src: Received caps" );
		break;
	  /* Sink is sending us an event */
	  case GST_USB_EVENT:
	  {
		GstEvent *event;
	    g_print("src: Event received \n");
		event = gst_usb_src_receive_event(s);	
		if (gst_pad_send_event (GST_BASE_SRC_PAD(bs),
                                event))
		  GST_WARNING("Error publishing event");
		gst_event_unref(event);  
		break;
	  }  							
	  default:
	    GST_WARNING("Unknown downstream event");
		break;	  
    }
	s->busy = FALSE;
  }
  /* It should never reach this point */
  pthread_cleanup_pop (1);	 	  
}	

static void close_down_event(void *param)
{
  GST_WARNING("Closing down events thread");		
  guint *notification = (guint *) param;	
  g_free(notification);	
}


static gboolean gst_usb_src_send_caps(GstUsbSrc *s, GstCaps *caps)
{
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint8 *header, *payload;
  guint *length, paylength;	
  
  length = g_malloc(sizeof(guint));

  /* Make a package from the given caps */	
  gdp->packet_from_caps(caps,
                        GST_DP_HEADER_FLAG_NONE,
                        &length[0],
                        &header,
                        &payload);
  g_print("src: Inside send caps\n");
  /* Send the size of the header */
  if (usb_gadget_transfer(s->gadget, 
						  GAD_UP_EP, 
						  (unsigned char *) length,
						  sizeof(guint)) != GAD_EOK)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	return FALSE;								  
  }
  g_print("src: Sent length %d\n", length[0]);
  /* Now send the header */
  if (usb_gadget_transfer(s->gadget, 
						  GAD_UP_EP, 
						  (unsigned char *) header,
						  length[0]) != GAD_EOK)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	return FALSE;								  
  }
  
  /* Get the length of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  g_print("src: Sent header with paylength %d\n", paylength);
  /* Send the payload */
  if (usb_gadget_transfer(s->gadget, 
						  GAD_UP_EP, 
						  (unsigned char *) payload,
						  paylength) != GAD_EOK)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	return FALSE;								  
  }
  g_print("src: Sent payload %d\n", paylength);
  g_free(length);
  g_free(header);
  g_free(payload);
  gst_dp_packetizer_free (gdp);
  return TRUE;
}

static GstCaps *gst_usb_src_receive_caps(GstUsbSrc *s)
{
  GstCaps *caps;
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;
  
  /* Wait unitl gadget is connected */
  if (s->gadget->connected != 1)
  {
    g_free(length);
	return FALSE;
  }
  
  /* Ask for the size of the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP,   
                           (unsigned char *) length, 
		  			       sizeof(guint)) != GAD_EOK)
  {	
	g_free(length);  		
    return FALSE;
  }

  g_print("Received size of header %d\n", length[0]);

  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP,   
                           (unsigned char *) header, 
  					       length[0]) != GAD_EOK)	
  {	
	g_free(length);
	g_free(header);  											
	return FALSE;
  }

  /* Check for the payload type to be a caps header */
  if (GST_READ_UINT16_BE(header+4) != GST_DP_PAYLOAD_CAPS)
  {
	GST_WARNING("Received header is not a cap's header");  
	g_free(length);
	g_free(header);  
    return FALSE;
  }

  /* Get the size of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  payload = g_malloc(paylength);
  
  /* Ask for the payload */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP, 
                           (unsigned char *) payload, 
  					       paylength) != GAD_EOK)
  {
	g_free(length);  
	g_free(payload);
	g_free(header);  
    return FALSE;
  }
  
  /* Finally get caps from header and payload */
  caps = gst_dp_caps_from_packet (length[0],
                                  header,
                                  payload);
								  
  GST_WARNING("Succesfully received caps");
  /* Free allocated vectors */
  g_free(length);  
  g_free(payload);
  g_free(header);  
  
  return caps;
}

static GstEvent *gst_usb_src_receive_event(GstUsbSrc *s)
{
  GstEvent *event;
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;
  
  /* Wait unitl gadget is connected */
  if (s->gadget->connected != 1)
  {
    g_free(length);
	return FALSE;
  }
  
  /* Ask for the size of the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP,   
                           (unsigned char *) length, 
		  			       sizeof(guint)) != GAD_EOK)
  {	
	g_free(length);  		
    return FALSE;
  }

  g_print("Received size of header %d\n", length[0]);

  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP,   
                           (unsigned char *) header, 
  					       length[0]) != GAD_EOK)	
  {	
	g_free(length);
	g_free(header);  											
	return FALSE;
  }

  /* Check for the payload type to be a caps header */
  /* A different payload type is encoded starting from 
   * GST_DP_PAYLOAD_EVENT_NONE=64. Ignore the rest of
   * the bits to check for a generic event*/
  if (GST_READ_UINT16_BE(header+4) < GST_DP_PAYLOAD_EVENT_NONE)
  {
	GST_WARNING("Received header is not a event's header");  
	g_free(length);
	g_free(header);  
    return FALSE;
  }

 
  /* Get the size of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  payload = g_malloc(paylength);
  
  /* Ask for the payload */
  if ( usb_gadget_transfer (s->gadget,
                           GAD_DOWN_EP, 
                           (unsigned char *) payload, 
  					       paylength) != GAD_EOK)
  {
	g_free(length);  
	g_free(payload);
	g_free(header);  
    return FALSE;
  }
  
  /* Finally get caps from header and payload */
  event = gst_dp_event_from_packet (length[0],
                                  header,
                                  payload);
								  
  GST_WARNING("Succesfully received event");
  /* Free allocated vectors */
  g_free(length);  
  g_free(payload);
  g_free(header);  
  
  return event;
}

static gboolean gst_usb_src_event (GstBaseSrc *bs, GstEvent *event)
{
   /* Gstreamer is telling us to send the message */
  GstUsbSrc *s = GST_USB_SRC (bs); 
  GstDPPacketizer *gdp = gst_dp_packetizer_new (GST_DP_VERSION_0_2);
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;	
  guint *notification = g_malloc(sizeof(guint));
  
  g_print("src: Posted event");
  while (s->gadget->connected != 1)
    g_usleep(1000);
  //~ if (gst_pad_push_event (GST_BASE_SRC_PAD(bs), event))
  //~ {
    //~ GST_WARNING("Event not processed");
    //~ g_free(length);
	//~ gst_event_unref(event);
	//~ return FALSE;	  
  //~ }	
  /*
   * FIXME: found that some events gave problems to GDP.
   * Think they must be less than 63 because of flags.
   */  
  if (GST_EVENT_TYPE(event) == GST_EVENT_TAG)
  {
    GST_WARNING("Event not handled by GDP");
    g_free(length);
	
	return FALSE;	 
  }	
  
  while (s->busy)
    g_usleep(1000);
  s->busy = TRUE;	
  
  notification[0] = GST_USB_EVENT;
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                           (unsigned char *) notification, 
		  			        sizeof(guint)) != GAD_EOK)
  {
	s->busy = TRUE;
    g_free(notification);	
	g_free(length);  								
    GST_WARNING("Error sending event");
	return FALSE;
  }	
  g_free(notification);
  
  g_print("src: Inside sending events\n");
  
  
   
  /* Make a package from the given caps */	
  gdp->packet_from_event(event,
                        GST_DP_HEADER_FLAG_NONE,
                        &length[0],
                        &header,
                        &payload);
  

  /* Send the size of the header */
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                           (unsigned char *) length, 
		  			        sizeof(guint)) != GAD_EOK)
  { 
	s->busy = FALSE;    
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	GST_WARNING("Error sending event");
	return FALSE;								  
  }
  g_print("sink: Sent header %d\n", length[0]);
  /* Now send the header */
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                           (unsigned char *) header, 
		  			        sizeof(guint)) != GAD_EOK)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	GST_WARNING("Error sending event");
	return FALSE;								  
  }
  g_print("sink: Sent header\n");
  /* Get the length of the payload */
  paylength = GST_READ_UINT32_BE(header+6);
  
  /* Send the payload */
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                           (unsigned char *) payload, 
		  			        sizeof(guint)) != GAD_EOK)
  {   
    g_free(length);
    g_free(header);
	g_free(payload);
    gst_dp_packetizer_free (gdp);
	GST_WARNING("Error sending event");
	return FALSE;								  
  }
  
  s->busy = FALSE;
  g_free(length);
  g_free(header);
  g_free(payload);
  gst_dp_packetizer_free (gdp);
  //~ GST_WARNING("Event sent");
  return TRUE;

}
