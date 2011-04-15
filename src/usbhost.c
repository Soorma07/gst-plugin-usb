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

int usb_host_new(usb_host *host, VERBOSE v)
{
  if (libusb_init ( &(host->ctx) ) == 1)  /* 0 on success LIBUSB_ERROR on error */
  {
    fprintf(stderr, "Cannot initialize usb context.\n");
	return EXIT_FAILURE;
  }	
  libusb_set_debug (host->ctx, v); /* Set level of verbosity */
 
  return EXIT_SUCCESS;
}

int usb_host_device_open(usb_host *host)
{
  libusb_device **list;
  
  /* Generate a list with all the conected devices */
  ssize_t cnt = libusb_get_device_list(NULL, &list);
  ssize_t i = 0;
  
  if (cnt < 0)
  {
    fprintf(stderr, "No devices conected.\n");
	return EXIT_FAILURE;  
  }
  
  for (i = 0; i < cnt; i++) 
  {
    libusb_device *device = list[i];
    if (0/*Add desired function*/) 
	{
      host->dev = device;
      break;
    }
  }
  
  if (host->dev) 
  {
    if ( libusb_open(host->dev, &(host->devh)) )
    {
      fprintf(stderr, "Cannot open desired device.\n");
	  return EXIT_FAILURE;    
    }
  }

  libusb_free_device_list(list, 1);	
  return EXIT_SUCCESS;  
}

int usb_host_device_transfer(usb_host *host, 
								  EP stream, 
								  unsigned char *buffer,
								  int length,
								  unsigned int timeout)
{
  int r = libusb_bulk_transfer(host->devh, stream, buffer, 
							   length, &(host->transferred),
							   timeout);
							   
  if (r != 0 && host->transferred != length)
  {
    fprintf(stderr, "Transfer failed.\n");
	return EXIT_FAILURE; 
  }
  
  return EXIT_SUCCESS;
}								  
								  

void usb_host_free(usb_host *device)
{
  libusb_exit (device->ctx);
}

