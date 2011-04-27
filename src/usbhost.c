/*
 * 		GStreamer
 * 		Copyright (C) 2011 RidgeRun
 *      Michael Grüner <michael.gruner@ridgerun.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#include "usbhost.h"

HOST_EXIT_CODE usb_host_new(usb_host *host, VERBOSE v)
{
  if (libusb_init ( &(host->ctx) ) == 1)  /* 0 on success LIBUSB_ERROR on error */
  {
	return ERR_INIT;
  }	
  libusb_set_debug (host->ctx, v); /* Set level of verbosity */
 
  return EOK;
}

HOST_EXIT_CODE usb_host_device_open(usb_host *host, uint16_t vendor_id,
						uint16_t product_id)
{
	
  host->devh = libusb_open_device_with_vid_pid(host->ctx,
											   vendor_id,
											   product_id);
  if (host->devh == NULL)
    return ERR_OPEN;			
  
  return EOK;								   
}

HOST_EXIT_CODE usb_host_device_transfer(usb_host *host, 
								  EP_ADRESS endp, 
								  unsigned char *buffer,
								  int length,
								  unsigned int timeout)
{
  int r = libusb_bulk_transfer(host->devh, (unsigned char) endp, 
                               buffer, length, &(host->transferred),
							   timeout);
							   
  if (r != 0 && host->transferred != length)
  {
	return ERR_TRANSFER; 
  }
  
  return EOK;
}								  
								  

void usb_host_free(usb_host *device)
{
  libusb_close (device->devh);	
  libusb_exit (device->ctx);
}

