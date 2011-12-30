/**
** This file is part of fsusb_demo
**
** fsusb_demo is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License as
** published by the Free Software Foundation; either version 2 of the
** License, or (at your option) any later version.
**
** fsusb_demo is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with fsusb_picdem; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
** 02110-1301, USA
*/

/**
A USB interface to the Microchip PICDEM FS USB Demo board

Manuel Bessler, m.bessler AT gmx.net, 20050619

Based on usb_pickit.c :
Orion Sky Lawlor, olawlor@acm.org, 2003/8/3
Mark Rages, markrages@gmail.com, 2005/4/1

Simple modification by Xiaofan Chen, 20050811
Email: xiaofanc AT gmail DOT com
Note: I am in the process of trying to port it to Windows 
using libusb_win32 and MinGW.
By the way, I do not know where I got the original source.

Notes by Xiaofan Chen, 20050812
Okay I got the reply from the author. The code comes from here:
http://www.varxec.net/picdem_fs_usb/ 
Now the code works with Win32. Tested on Windows XP SP2.

Notes by Xiaofan Chen, 20070922
Minor modifications: to detach the kernel driver if it is
calimed by some newer kernel ldusb module.
http://www.opensubscriber.com/message/linux-usb-devel@lists.sourceforge.net/7160138.html
Also I remove the check of uid: better to use udev rules so that we do not need
to run as root.
*/
#include <usb.h> /* libusb header */
#include <unistd.h> /* for geteuid */
#include <stdio.h>
#include <string.h>

#define READ_VERSION     0x00
#define READ_VERSION_LEN 0x02

#define ID_BOARD         0x31
#define UPDATE_LED       0x32
#define SET_TEMP_REAL    0x33
#define RD_TEMP          0x34
#define SET_TEMP_LOGGING 0x35
#define RD_TEMP_LOGGING  0x36
#define RD_POT           0x37
#define RESET            0xFF

//#define USB_DEBUG

/* PICkit USB values */ /* From Microchip firmware */
const static int vendorID=0x04d8; // Microchip, Inc
const static int productID=0x000c; // PICDEM FS USB demo app
const static int configuration=1; /*  Configuration 1*/
const static int interface=0;	/* Interface 0 */

/* change by Xiaofan */
/* libusb-win32 requires the correct endpoint address
   including the direction bits. This is the most important
   differece between libusb-win32 and libusb.
*/  
//const static int endpoint=1; /* first endpoint for everything */
const static int endpoint_in=0x81; /* endpoint 0x81 address for IN */
const static int endpoint_out=1; /* endpoint 1 address for OUT */

const static int timeout=2000; /* timeout in ms */

/* PICDEM FS USB max packet size is 64-bytes */
const static int reqLen=64;
typedef unsigned char byte;

void bad(const char *why) {
	fprintf(stderr,"Fatal error> %s\n",why);
	exit(17);
}

/****************** Internal I/O Commands *****************/

/** Send this binary string command. */
static void send_usb(struct usb_dev_handle * d, int len, const byte * src)
{
   int r = usb_interrupt_write(d, endpoint_out, (char *)src, len, timeout);
//   if( r != reqLen )
   if( r < 0 )
   {
	  perror("usb PICDEM FS USB write"); bad("USB write failed"); 
   }
}

/** Read this many bytes from this device */
static void recv_usb(struct usb_dev_handle * d, int len, byte * dest)
{
//   int i;
   int r = usb_interrupt_read(d, endpoint_in, (char *) dest, len, timeout);
   if( r != len )
   {
	  perror("usb PICDEM FS USB read"); bad("USB read failed"); 
   }
}

void picdem_fs_usb_read_version(struct usb_dev_handle * d)
{
   byte answer[reqLen];
   byte question[reqLen];
   question[0] = READ_VERSION;
   question[1] = READ_VERSION_LEN;
   send_usb(d, 2, question);
   recv_usb(d, 2+READ_VERSION_LEN, answer);
   if( (int)answer[0] == READ_VERSION &&
	   (int)answer[1] == READ_VERSION_LEN )
	  printf("answer was correct!\n");
   printf("Onboard firmware version is %d.%d\n",
		  (int)answer[3],(int)answer[2]);
}

void picdem_fs_usb_led(struct usb_dev_handle * d, int lednum, int onoff)
{
   if( lednum < 3 || lednum > 4 )
	  return;
    byte answer[reqLen];
   byte question[reqLen];
   question[0] = UPDATE_LED;
   question[1] = lednum;
   question[2] = (onoff==0) ? 0 : 1;
   send_usb(d, 3, question);
   recv_usb(d, 1, answer);
   printf("LED #%i is now %s\n", lednum, (onoff==0) ? "off" : "on");
}

void picdem_fs_usb_readtemp(struct usb_dev_handle * d)
{
   byte answer[reqLen];
   byte question[reqLen];
   int rawval,rawtemp;
   float degCtemp;
   question[0] = RD_TEMP;
   send_usb(d, 1, question);
   recv_usb(d, 3, answer);
   rawval = answer[1] + (answer[2]<<8);
   
   /* improved temperature conversion by Bill Freeman */
   if( rawval & (1 << 15) )
         rawval -= 1 << 16;
			 
/*   if( (rawval>>15) & 0x01 )
	  rawval = (~rawval) + 1; */
   rawtemp = (rawval)>>3;
   degCtemp = rawtemp * 0.0625;
   printf("Temperature now is %f degC (raw: %i, rawval: %i)\n", degCtemp, rawtemp, rawval);
}

void picdem_fs_usb_readpot(struct usb_dev_handle * d)
{
   byte answer[reqLen];
   byte question[reqLen];
   int rawval;
   question[0] = RD_POT;
   send_usb(d, 1, question);
   recv_usb(d, 3, answer);
   rawval = answer[1] + (answer[2]<<8);
   printf("Potentiometer now reads %i\n", rawval);
}

void picdem_fs_usb_reset(struct usb_dev_handle * d)
{
//   byte answer[reqLen];
   byte question[reqLen];
   question[0] = RESET;
   send_usb(d, 1, question);
//   recv_usb(d, 3, answer);
   printf("Board resetted\n");
}

/* debugging: enable debugging error messages in libusb */
extern int usb_debug;

/* Find the first USB device with this vendor and product.
   Exits on errors, like if the device couldn't be found.
*/
struct usb_dev_handle *usb_picdem_fs_usb_open(void)
{
  struct usb_device * device;
  struct usb_bus * bus;

/* change by Xiaofan: it is better not to run as root. It is
better to use udev rules to set up the correct perimissions */
/*
#ifndef WIN32  
  if( geteuid()!=0 )
	 bad("This program must be run as root, or made setuid root");
#endif  
*/
#ifdef USB_DEBUG
  usb_debug=255; 
#endif

  printf("Locating Microchip(tm) PICDEM(tm) FS USB Demo Board (vendor 0x%04x/product 0x%04x)\n", vendorID, productID);
  /* (libusb setup code stolen from John Fremlin's cool "usb-robot") */

  // added the two debug lines  
//  usb_set_debug(255);
//  printf("setting USB debug on by adding usb_set_debug(255) \n");
  //End of added codes

  usb_init();
  usb_find_busses();
  usb_find_devices();

/* change by Xiaofan */  
/* libusb-win32: not using global variable like usb_busses*/
/*  for (bus=usb_busses;bus!=NULL;bus=bus->next) */  
  for (bus=usb_get_busses();bus!=NULL;bus=bus->next) 
  {
	 struct usb_device * usb_devices = bus->devices;
	 for( device=usb_devices; device!=NULL; device=device->next )
	 {
		if( device->descriptor.idVendor == vendorID &&
			device->descriptor.idProduct == productID )
		{
		   struct usb_dev_handle *d;
		   printf("Found USB PICDEM FS USB Demo Board as device '%s' on USB bus %s\n",
				   device->filename,
				   device->bus->dirname);
		   d = usb_open(device);
		   if( d )
		   { /* This is our device-- claim it */
//			  byte retData[reqLen];

/* change by Xiaofan */ 
/* to detach the kernel driver if it is
calimed by some newer kernel ldusb module.
http://www.opensubscriber.com/message/linux-usb-devel@lists.sourceforge.net/7160138.html
*/
#ifdef LINUX
			{
				int retval;
				char dname[32] = {0};
				retval = usb_get_driver_np(d, 0, dname, 31);
				if (!retval)
					usb_detach_kernel_driver_np(d, 0);
				}
#endif

			  if( usb_set_configuration(d, configuration) ) 
			  {
				 bad("Error setting USB configuration.\n");
			  }
			  if( usb_claim_interface(d, interface) ) 
			  {
				 bad("Claim failed-- the USB PICDEM FS USB is in use by another driver.\n");
			  }
			  printf("Communication established.\n");
			  picdem_fs_usb_read_version(d);
			  return d;
		   }
		   else 
			  bad("Open failed for USB device");
		}
		/* else some other vendor's device-- keep looking... */
	 }
  }
  bad("Could not find USB PICDEM FS USB Demo Board--\n"
      "you might try lsusb to see if it's actually there.");
  return NULL;
}

/* Change by Xiaofan */
/* Please also refer to README */
void show_usage(void)
{
  printf("fsusb_demo: Software for PICDEM Full Speed USB demo board\n");
  printf("fsusb_demo --ledon n, n=3 or 4: ON LED D3 or D4\n");
  printf("fsusb_demo --ledff n, n=3 or 4: Off LED D3 or D4\n");
  printf("fsusb_demo --readtemp: read out temperature\n");
  printf("fsusb_demo --readpot: read out poti value\n");
  printf("fsusb_demo --reset: to reset the PICDEM FS USB demo board\n");
}

int main(int argc, char ** argv) 
{
   struct usb_dev_handle * picdem_fs_usb = usb_picdem_fs_usb_open();
   if(argc < 2 || argc > 3) {
    show_usage();
    exit(1);
  } 
  if(argc == 3){
	  if( 0 == strcmp(argv[1],"--ledon") )
	  {
		 picdem_fs_usb_led(picdem_fs_usb, atoi(argv[2]), 1);
	  }
	  else if( 0 == strcmp(argv[1],"--ledoff") )
	  {
		 picdem_fs_usb_led(picdem_fs_usb, atoi(argv[2]), 0);	 
	  }
	 }
   
   if( argc == 2 ){
	  if(  0 == strcmp(argv[1],"--readtemp") )
	  {
		 picdem_fs_usb_readtemp(picdem_fs_usb);
	  }
	  else if(  0 == strcmp(argv[1],"--readpot") )
	  {
		 picdem_fs_usb_readpot(picdem_fs_usb);
	  }
	  else if(  0 == strcmp(argv[1],"--reset") )
	  {
		 picdem_fs_usb_reset(picdem_fs_usb);
	  }
	}
   usb_close(picdem_fs_usb);
   return 0;
}
