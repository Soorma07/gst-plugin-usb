/*
 * Copyright (C) 2011 RidgeRun
 *
 */

#define DPOINT printf("Debug point %s %d\n", __FUNCTION__, __LINE__)

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <pthread.h>
#include <string.h>

#include "gstusbsrc.h"


GST_DEBUG_CATEGORY_STATIC (gst_usb_src_debug);
#define GST_CAT_DEFAULT gst_usb_src_debug

#define GST_USB_SRC_GET_STATE_LOCK(s) \
  (GST_USB_SRC(s)->state_lock)
#define GST_USB_SRC_STATE_LOCK(s) \
  (g_mutex_lock(GST_USB_SRC_GET_STATE_LOCK(s)))
#define GST_USB_SRC_STATE_UNLOCK(s) \
  (g_mutex_unlock(GST_USB_SRC_GET_STATE_LOCK(s)))

enum
{
  PROP_0,
  PROP_USBSYNC
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
static GstFlowReturn gst_usb_src_create 
(GstPushSrc * ps, GstBuffer ** buf);
static gboolean gst_usb_src_start (GstBaseSrc * bs);
static gboolean gst_usb_src_stop (GstBaseSrc * bs);
static GstStateChangeReturn gst_usb_src_change_state (GstElement *
						      element, GstStateChange transition);

void *gst_usb_src_down_event (void *src);	
static void close_down_event(void *param);
static gboolean gst_usb_src_send_caps(GstUsbSrc *s, GstCaps *caps);
static GstCaps *gst_usb_src_receive_caps(GstUsbSrc *s);

/* GObject vmethod implementations */

static void
gst_usb_src_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
				       "usbsrc",
				       "Hardware",
				       "Elements that receives data across an USB link",
				       "Michael Gruner <<michael.gruner@ridgerun.com>>"\
				       "\n\t\tDiego Dompe <<diego.dompe@ridgerun.com>>");
  
  gst_element_class_add_pad_template (element_class,
				      gst_static_pad_template_get (&src_factory));
}

/* initialize the usbsrc's class */
static void
gst_usb_src_class_init (GstUsbSrcClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *base_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_class = GST_PUSH_SRC_CLASS (klass);

  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_usb_src_debug, "usbsrc",
			   0, "USB source element");

  gobject_class->set_property = gst_usb_src_set_property;
  gobject_class->get_property = gst_usb_src_get_property; 

  gstelement_class->change_state =
    gst_usb_src_change_state;

  push_class->create = gst_usb_src_create;
  base_class->start = gst_usb_src_start;
  base_class->stop = gst_usb_src_stop;

  g_object_class_install_property (gobject_class, PROP_USBSYNC,
				   g_param_spec_boolean ("usbsync", "UsbSync", "Synchronize timestamps with src time",
							 TRUE, G_PARAM_READWRITE));    
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
  s->play=FALSE;
  s->state_lock = g_mutex_new ();
  s->sync = GST_CLOCK_TIME_NONE;
  s->usbsync = TRUE;
}

static void
gst_usb_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstUsbSrc *filter = GST_USB_SRC (object);

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
gst_usb_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstUsbSrc *filter = GST_USB_SRC (object);

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

static gboolean
gst_usb_src_start (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);
  
   
  switch (usb_gadget_new(s->gadget, GLEVEL0)){
  case GAD_EOK:
    break;  
  case ERR_GAD_DIR:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Cannot work on /dev/gadget dir. Make sure it exists"\
		       "and you have a gadgetfs mounted there."));
    return FALSE;
  case ERR_OPEN_FD:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Can't open gadget's file descriptor"));
    return FALSE;
  case ERR_NO_DEVICE:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("No asociated device found"));  
    return FALSE;
  case ERR_WRITE_FD:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Can't write to file descriptor"));
    return FALSE;
  case SHORT_WRITE_FD:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Short write in file descriptor, aborting..."));
    return FALSE;
  default:
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
		      ("Error initializing device"));
    return FALSE;
  }
  
  guint *notification = g_malloc(sizeof(guint)); 

  /* Poll for gadget connection */
  GST_DEBUG_OBJECT (s,"Waiting for gadget to connect...");
  while (s->gadget->connected != 1)
    g_usleep(1000); /* Wait 1 milisecond */
  GST_DEBUG_OBJECT (s,"Gadget connected!");
  
  /* Send sink the connection notification */
  notification[0] = GST_USB_CONNECTED;
  GST_DEBUG_OBJECT (s,"Notifying sink of connection status");
  if ( usb_gadget_transfer (s->gadget,
                            GAD_UP_EP,   
                            (unsigned char *) notification, 
			    sizeof(guint)) != GAD_EOK)
  {
    g_free(notification);	  							
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
      ("Error Establishing connection with sink"));
    return FALSE;
  }		

  /* Create a thread for downstream events */
  if (pthread_create (&(s->gadget->ev_down.thread), NULL,
		      (void *) gst_usb_src_down_event, (void *) bs) != 0){
    GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
      ("Unable to create down events thread, aborting.."));	  
    g_free(notification);
    return FALSE;
  }	

  GST_USB_SRC_STATE_UNLOCK(s);
  g_free(notification);
  return TRUE;
}

static gboolean
gst_usb_src_stop (GstBaseSrc * bs)
{
  GstUsbSrc *s = GST_USB_SRC (bs);

  if (pthread_cancel(s->gadget->ev_down.thread))
  {
    GST_WARNING_OBJECT(s,"Problem closing USB events thread");
  }
  usb_gadget_free(s->gadget);
  g_free(s->gadget);
  return TRUE;
}

#define PRINTERR(ret,s) switch(ret) \
                        { \
	                  case ERR_OPEN_FD:\
                            GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),\
			      ("Error opening file descriptor for transfer"));\
                            break;\
                          case ERR_READ_FD:\
                            GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),\
			      ("Can't write to file descriptor"));\
                            break;\
			  case SHORT_READ_FD:\
                            GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),\
			      ("File descriptor short read, aborting."));\
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
  guint8 *data;

  //GST_USB_SRC_STATE_LOCK(s);
  /* Ask for the size of the header */
  if ( (ret=usb_gadget_transfer (s->gadget,
                                 GAD_STREAM_EP, 
                                (unsigned char *) size, 
		  		 sizeof(guint)))  != GAD_EOK )
  {	
    g_free(size);
    PRINTERR(ret,s)
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
    PRINTERR(ret,s)
    return GST_FLOW_ERROR;
  }
	
  /* Create the buffer using gst data protocol */

  *buf = gst_dp_buffer_from_header (size[0], header);
  data = GST_BUFFER_DATA(*buf);
  /* Now read the data */

  /* Ask for the header */
  if ((ret=usb_gadget_transfer (s->gadget,
                                GAD_STREAM_EP,   
                               (unsigned char *) data, 
    	                       (*buf)->size)) != GAD_EOK)	
  {	

    g_free(data);
    g_free(size);
    g_free(header);
    PRINTERR(ret,s)
    return GST_FLOW_ERROR;
  }	

  /* Synchronize with sink's timestamps */
  if (s->usbsync)
    GST_BUFFER_TIMESTAMP(*buf) += s->sync;

  //GST_USB_SRC_STATE_UNLOCK(s);
  g_free(header);
  g_free(size); 

  return GST_FLOW_OK;
}

/* Down events thread */
void *gst_usb_src_down_event (void *src)
{
  GstBaseSrc *bs = GST_BASE_SRC(src);	
  GstUsbSrc *s = GST_USB_SRC(bs);	
  
  guint *notification = g_malloc(sizeof(guint));	
  GstCaps *caps;
  
  pthread_cleanup_push (close_down_event, (void *) notification);
  
  while (TRUE)
  {
    /* Create a cancellation test point */  
    pthread_testcancel();  
    /* Receive an event (internal polling) */
    if (usb_gadget_transfer(s->gadget, 
	                    GAD_DOWN_EP, 
		  	   (unsigned char *) notification,
			    sizeof(guint)) != GAD_EOK)
    { 
      GST_WARNING_OBJECT(s,"Error receving downstream event");
      continue;    							      
    }
    /* Wait until gadget is free */
    GST_USB_SRC_STATE_LOCK(s);
    switch (notification[0])
    { 
      /* Sink is asking for our allowed caps */
      case GST_USB_GET_CAPS:
      {		  
	notification[0] = GST_USB_CAPS;
	GST_DEBUG_OBJECT (s,"Received a get caps");
	/* Send a caps message */
	if ( usb_gadget_transfer (s->gadget,
                                 GAD_UP_EP,   
                                 (unsigned char *) notification, 
				  sizeof(guint)) != GAD_EOK)
	{			
	  GST_USB_SRC_STATE_UNLOCK(s);
          GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
                           ("Error returning caps"));	
        }
	if (gst_pad_is_linked(GST_BASE_SRC_PAD(bs)))
	  caps = gst_pad_peer_get_caps (GST_BASE_SRC_PAD(bs));
	else
	  caps = gst_caps_copy (gst_pad_get_pad_template_caps\
		                       (GST_BASE_SRC_PAD(bs)));	   
	if (!gst_usb_src_send_caps(s, caps))
	{
	  GST_USB_SRC_STATE_UNLOCK(s);
	  GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
			    ("Error sending caps"));
	}
	GST_DEBUG_OBJECT (s,"Caps sent\n");
	gst_caps_unref(caps);
	break;
      }
      /* Sink is sending a set of caps for src to set */
      case GST_USB_SET_CAPS:{
	GstCaps *caps;
        GST_DEBUG_OBJECT (s,"Received a set caps");
	caps = gst_usb_src_receive_caps(s);
	s->play = TRUE;
	if (!gst_pad_set_caps (GST_BASE_SRC_PAD(bs), caps)){
	  GST_ELEMENT_ERROR(s,STREAM,FAILED,(NULL),
			    ("Error setting caps"));
	}
	else{
	  GST_DEBUG_OBJECT (s,"Caps set correctly");
	}
	break;
      }
      default:
	GST_WARNING_OBJECT(s,"Unknown downstream event");
	break;	  
    }
    GST_USB_SRC_STATE_UNLOCK(s);
  }
  /* It should never reach this point */
  pthread_cleanup_pop (1);	 	  
}	

static void close_down_event(void *param)
{
  GST_INFO ("Closing down events thread");		
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

  /* Send the size of the header */
  if (usb_gadget_transfer(s->gadget, 
			  GAD_UP_EP, 
			  (unsigned char *) length,
			  sizeof(guint)) != GAD_EOK){   
    g_free(length);
    g_free(header);
    g_free(payload);
    gst_dp_packetizer_free (gdp);
    return FALSE;								  
  }

  /* Now send the header */
  if (usb_gadget_transfer(s->gadget, 
			  GAD_UP_EP, 
			  (unsigned char *) header,
			  length[0]) != GAD_EOK){   
    g_free(length);
    g_free(header);
    g_free(payload);
    gst_dp_packetizer_free (gdp);
    return FALSE;								  
  }
  
  /* Get the length of the payload */
  paylength = GST_READ_UINT32_BE(header+6);

  /* Send the payload */
  if (usb_gadget_transfer(s->gadget, 
			  GAD_UP_EP, 
			  (unsigned char *) payload,
			  paylength) != GAD_EOK){   
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


static GstCaps *gst_usb_src_receive_caps(GstUsbSrc *s)
{
  GstCaps *caps;
  guint8 *header, *payload;
  guint *length = g_malloc(sizeof(guint)), paylength;
  
  /* Wait unitl gadget is connected */
  if (s->gadget->connected != 1)
  {
    g_free(length);
    return NULL;
  }
  
  /* Ask for the size of the header */
  if ( usb_gadget_transfer (s->gadget,
			    GAD_DOWN_EP,   
			    (unsigned char *) length, 
			    sizeof(guint)) != GAD_EOK){	
    g_free(length);  		
    return NULL;
  }

  /* Alocate memory for the header */
  header = (void *) g_malloc(length[0]);
    
  /* Ask for the header */
  if ( usb_gadget_transfer (s->gadget,
			    GAD_DOWN_EP,   
			    (unsigned char *) header, 
			    length[0]) != GAD_EOK){	
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
  if ( usb_gadget_transfer (s->gadget,
			    GAD_DOWN_EP, 
			    (unsigned char *) payload, 
			    paylength) != GAD_EOK){
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

static GstStateChangeReturn
gst_usb_src_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstUsbSrc *src = GST_USB_SRC (element);
  guint *notification = g_malloc(sizeof(guint));   
 										
  /* Handle ramp-up state changes */
  switch (transition) 
  {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /* Wait until caps are set */
      while (!src->play){
	g_usleep(1000);
      }

      /* Send sink the play notification */
      notification[0] = GST_USB_PLAY;
      GST_DEBUG_OBJECT (src,"Notifying sink play status");
      if ( usb_gadget_transfer (src->gadget,
				GAD_UP_EP,   
				(unsigned char *) notification, 
				sizeof(guint)) != GAD_EOK)
	{
	g_free(notification);	  							
	GST_ELEMENT_ERROR(src,STREAM,FAILED,(NULL),
			  ("Error Establishing connection with sink"));
	return GST_STATE_CHANGE_FAILURE;
      }
      src->sync= gst_util_get_timestamp()- gst_element_get_base_time(element);
      GST_DEBUG_OBJECT(src, "Estimated %" GST_TIME_FORMAT " for time sync", GST_TIME_ARGS(src->sync));
      break;
    default:
      break;
  }

  /* Pass state changes to base class */
  ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  /* Handle ramp-down state changes */
  switch (transition) 
  {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
/*   TODO: send stop notification here if needed */
      break;
    default:
      break;
  }
  g_free(notification);
  return ret;
}


