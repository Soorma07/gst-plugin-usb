#ifndef __GST_USB_MESSAGES_H__
#define __GST_USB_MESSAGES_H__

/** 
 * Intercommunication messages
 */
typedef enum _GST_USB_MESSAGE
{
  /** Local process needs caps from remote process */
  GST_USB_GET_CAPS,
  
  /** Local process is going to send caps for remote process to set it */
  GST_USB_SET_CAPS,
  
  /** The following transfers are incoming caps */
  GST_USB_CAPS,
  
  /** Remote device has succesfully connected */
  GST_USB_CONNECTED
  	
} GST_USB_MESSAGE;	  


#endif /* __GST_USB_MESSAGES_H__ */
