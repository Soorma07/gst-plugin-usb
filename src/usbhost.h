#ifndef __USB_HOST_H__
#define __USB_HOST_H__

/*
 * Copyright (C) 2011 RidgeRun
 */

#include </home/mgruner/ti-dvsdk_dm3730-evm_4_02_00_06/linux-devkit/arm-none-linux-gnueabi/usr/include/libusb-1.0/libusb.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * Enpoint address.
 */
typedef unsigned char EP;

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
  
  /** Pointer to a libusb device */
  libusb_device *dev;
  
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
extern int usb_host_new(usb_host *host, VERBOSE v);


 /**
  * \brief Method to open the desired device.
  * \param host Object in wich the desired device will be opened.
  * \return Code with the return status.
  */ //TODO: add a description field
extern int usb_host_device_open(usb_host *host);

/**
 * \brief Method to transfer data bulk data.
 * \param host Object that contains an opened device.
 * \param buffer Buffer containing the data to transfer.
 * \param length Length of the data to transfer.
 * \param timeout Time in milliseconds to the transfer to give up.
 * \return Code with the transfer status.
 */
extern int usb_host_device_transfer(usb_host *host, 
								  EP stream, 
								  unsigned char *buffer,
								  int length,
								  unsigned int timeout);

 /**
  * \brief Object destructor.
  * \param host Usb host device to free.
  */
extern void usb_host_free(usb_host *host);

#endif /* __USB_HOST_H__ */

