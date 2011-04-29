#ifndef __USB_HOST_H__
#define __USB_HOST_H__

/*
 * Copyright (C) 2011 RidgeRun
 */

#include <libusb.h>
#include <stdlib.h>
#include <stdio.h>


/**
 * Endpoint addresses
 */
typedef enum _EP_ADDRESS
{
  /** Endpoint 1 configured for IN direcion */
  EP1_IN  = 0x81,
  
  /** Endpoint 1 configured for OUT direcion */
  EP1_OUT = 0x01,
  
  /** Endpoint 2 configured for IN direcion */
  EP2_IN  = 0x82,
  
  /** Endpoint 2 configured for OUT direcion */
  EP2_OUT = 0x02,
  
  /** Endpoint 3 configured for IN direcion */
  EP3_IN  = 0x83,
  
  /** Endpoint 3 configured for OUT direcion */
  EP3_OUT = 0x03,
} EP_ADRESS;

/**
 * Exit codes for error handling
 */
typedef enum _HOST_EXIT_CODE
{
  /** Everything ok */	
  EOK,
  
  /** Error openind usb context */
  ERR_INIT,
  
  /** Unable to find desired device */
  ERR_FOUND,
  
  /** Error opening the device */
  ERR_OPEN,
  
  /** Error during transfer */
  ERR_TRANSFER
  
} HOST_EXIT_CODE;

/**
 * Levels of verbosity to implement.
 */
typedef enum _VERBOSE
{
  /** No messages printed */	
  LEVEL0,
  
  /** Errors printed to stderr */
  LEVEL1,
  
  /** Errors and warnings to stderr */
  LEVEL2,
  
  /** Infos printed to stdout, errors and warnings to stderr */
  LEVEL3
  
} VERBOSE;


/**
 * Simple device struct.
 */
typedef struct _usb_host
{
  /** Pointer to a libusb context */	
  libusb_context *ctx;
  
  /** Pointer to a libusb device handle */
  libusb_device_handle *devh;
  
  /** Integer used to store the amount of trasfered data */
  int transferred;
  
} usb_host;

 /**
  * \brief Object constructor.
  * \param host Object to create.
  * \param v Verbosity level of the context. See #_VERBOSE.
  * \return Code with the return status.
  */
extern HOST_EXIT_CODE usb_host_new(usb_host *host, VERBOSE v);


 /**
  * \brief Method to open the desired device.
  * \param host Object in wich the desired device will be opened.
  * \param vendor_id Vendor ID of the device to be opened.
  * \param product_id Product ID of the device to be opened.
  * \return Code with the return status.
  */ //TODO: add a description field
extern HOST_EXIT_CODE usb_host_device_open(usb_host *host, 
								uint16_t vendor_id,
								uint16_t product_id);

/**
 * \brief Method to transfer data bulk data.
 * \param host Object that contains an opened device.
 * \param endp Endpoint address to write to or read from.
 * \param buffer Buffer containing the data to transfer.
 * \param length Length in bytes of the data to transfer.
 * \param timeout Time in milliseconds to the transfer to give up.
 * \return Code with the transfer status.
 */
extern HOST_EXIT_CODE usb_host_device_transfer(usb_host *host, 
								  EP_ADRESS endp, 
								  unsigned char *buffer,
								  int length,
								  unsigned int timeout);

 /**
  * \brief Object destructor.
  * \param host Usb host device to free.
  */
extern void usb_host_free(usb_host *host);

#endif /* __USB_HOST_H__ */

