#ifndef __DRIVER_H__
#define __DRIVER_H__



typedef enum _GADGET_EXIT_CODE
{
  /** Everything OK */	
  EOK,	
  
  /** Error changing to gadget directory */
  ERR_GAD_DIR,
  
  /** Error initializing gadget device */
  ERR_INI_DEV
  
  	
} GADGET_EXIT_CODE;

extern int usb_gadget_new(void);

extern int usb_gadget_free(void);


#endif /* __DRIVER_H__ */
