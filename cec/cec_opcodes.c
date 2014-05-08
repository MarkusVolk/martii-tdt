/*
 * 
 * (c) 2010 konfetti, schischu
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/io.h>

#include "cec_worker.h"
#include "cec_opcodes.h"
#include "cec_opcodes_def.h"
#include "cec_internal.h"
#include "cec_proc.h"
#include "cec_rc.h"
#include "cec_dev.h"
#include "cec_debug.h"

extern int activemode;

extern long stmhdmiio_get_cec_address(unsigned int * arg);

static unsigned char isFirstKiss = 0;

static unsigned char logicalDeviceTypeChoicesIndex = 0;


static const unsigned char logicalDeviceTypeChoices[][2] =  { 
  { 1 << DEVICE_TYPE_DVD, DEVICE_TYPE_DVD1 }, 
  { 1 << DEVICE_TYPE_DVD, DEVICE_TYPE_DVD2 }, 
  { 1 << DEVICE_TYPE_DVD, DEVICE_TYPE_DVD3 }, 
  { 1 << DEVICE_TYPE_STB, DEVICE_TYPE_STB1 }, 
  { 1 << DEVICE_TYPE_STB, DEVICE_TYPE_STB2 }, 
  { 1 << DEVICE_TYPE_STB, DEVICE_TYPE_STB3 },
  { 1 << DEVICE_TYPE_REC, DEVICE_TYPE_REC1 },
  { 1 << DEVICE_TYPE_REC, DEVICE_TYPE_REC2 },
  { 0xFF, DEVICE_TYPE_UNREG }
 };

extern char *deviceName;
static unsigned char logicalDeviceType = DEVICE_TYPE_DVD1;
extern unsigned char deviceType;

static unsigned short ActiveSource = 0x0000;

////////////////////////////////////

unsigned char getIsFirstKiss(void)
{
  return isFirstKiss;
}

void setIsFirstKiss(unsigned char kiss)
{
  isFirstKiss = kiss;
  if(isFirstKiss == 0)
  {
    if (activemode)
      sendReportPhysicalAddress();
  }
}

unsigned char getLogicalDeviceType(void)
{
  return logicalDeviceType;
}

unsigned char getDeviceType(void)
{
  return deviceType;
}

unsigned short getPhysicalAddress(void)
{
  unsigned int value = 0;
  stmhdmiio_get_cec_address(&value);
  
  return value & 0xffff;
}

//=================

unsigned short getActiveSource(void)
{
  return ActiveSource;
}

void setActiveSource(unsigned short addr)
{
  if(ActiveSource != addr) {
    ActiveSource = addr;
    setUpdatedActiveSource();
  }
}

//-----------------------------------------

void parseMessage(unsigned char src, unsigned char dst, unsigned int len, unsigned char buf[])
{
#define NAME_SIZE 100
  char name[NAME_SIZE];
  unsigned char responseBuffer[SEND_BUF_SIZE];
  int responseLen = 0;

  memset(name, 0, NAME_SIZE);
  memset(responseBuffer, 0, SEND_BUF_SIZE);

  switch(buf[0])
  {
    case ROUTING_CHANGE: 
    case ROUTING_INFORMATION: 
    case ACTIVE_SOURCE: 
    case GIVE_PHYSICAL_ADDRESS: 
    case STANDBY: 
      break;
    default: 
		if (!activemode)
			return;
  }

  switch(buf[0])
  {
    case ROUTING_CHANGE:
	  if (len > 4)
			  setActiveSource((buf[3]<<8) | buf[4]);
      break;

    case ROUTING_INFORMATION: 
    case ACTIVE_SOURCE: 
	  if (len > 2)
			  setActiveSource((buf[1]<<8) | buf[2]);

    case GIVE_PHYSICAL_ADDRESS: 
      sendReportPhysicalAddress();
      break;

    case STANDBY: 
      setUpdatedStandby();
      break;

    case GIVE_DECK_STATUS: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = DECK_STATUS;
      responseBuffer[responseLen++] = DECK_INFO_PLAY;
      break;

    case USER_CONTROL_PRESSED :
	  if (len > 1)
			  input_inject(buf[1], 1);
      break;

    case USER_CONTROL_RELEASED: 
      input_inject(0xFFFF, 0);
      break;

    case GIVE_OSD_NAME: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = SET_OSD_NAME;
      responseLen += strlen(deviceName);
      if (responseLen > CEC_MAX_DATA_LEN)
		responseLen = CEC_MAX_DATA_LEN;
      strncpy(responseBuffer + 2, deviceName, responseLen - 2);
      break;

    case SET_STREAM_PATH: 
      if((len > 2) && (((buf[1]<<8) | buf[2]) == getPhysicalAddress())) // If we are the active source
    case REQUEST_ACTIVE_SOURCE: 
      {
        unsigned short physicalAddress = getPhysicalAddress();
        responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | BROADCAST;
        responseBuffer[responseLen++] = ACTIVE_SOURCE;
        responseBuffer[responseLen++] = (((physicalAddress >> 12)&0xf) << 4) | ((physicalAddress >> 8)&0xf);
        responseBuffer[responseLen++] = (((physicalAddress >>  4)&0xf) << 4) | ((physicalAddress >> 0)&0xf);
      }
      break;

    case VENDOR_COMMAND: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = FEATURE_ABORT;
      responseBuffer[responseLen++] = VENDOR_COMMAND;
      responseBuffer[responseLen++] = ABORT_REASON_UNRECOGNIZED_OPCODE;
      break;

    case GIVE_DEVICE_VENDOR_ID: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | BROADCAST;
      responseBuffer[responseLen++] = DEVICE_VENDOR_ID;
      // http://standards.ieee.org/develop/regauth/oui/oui.txt
#if defined(UFS912) || defined(UFS913) // Kathrein
      responseBuffer[responseLen++] = 0x00;
      responseBuffer[responseLen++] = 0xD0;
      responseBuffer[responseLen++] = 0x55;
#else
#ifdef ATEVIO7500
      responseBuffer[responseLen++] = 0x00;
      responseBuffer[responseLen++] = 0x1E;
      responseBuffer[responseLen++] = 0xB8;
#else
      responseBuffer[responseLen++] = 'D';
      responseBuffer[responseLen++] = 'B';
      responseBuffer[responseLen++] = 'X';
#endif
#endif
      break;

    case MENU_REQUEST: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = MENU_STATUS;
      responseBuffer[responseLen++] = MENU_STATE_ACTIVATE;
      break;

    case GIVE_DEVICE_POWER_STATUS: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = REPORT_POWER_STATUS;
      responseBuffer[responseLen++] = POWER_STATUS_ON;
      break;

    case GET_CEC_VERSION: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = CEC_VERSION;
      responseBuffer[responseLen++] = CEC_VERSION_V13A;
      break;

    case VENDOR_COMMAND_WITH_ID: 
      responseBuffer[responseLen++] = (getLogicalDeviceType() << 4) | src;
      responseBuffer[responseLen++] = FEATURE_ABORT;
      responseBuffer[responseLen++] = VENDOR_COMMAND_WITH_ID;
      responseBuffer[responseLen++] = ABORT_REASON_UNRECOGNIZED_OPCODE;
      break;

    default:
      break;
  }
  if (responseLen)
      sendMessage(responseLen, responseBuffer);
}

void parseRawMessage(unsigned int len, unsigned char buf[], const char *hint)
{
  int ic;

  // Header
  unsigned char src = buf[0] >> 4;
  unsigned char dst = buf[0] & 0x0F;

  unsigned int dataLen = len - 1;

  // Copy-paste to http://www.cec-o-matic.com/ to get readable output
  printk("[CEC] %s Data: ", hint);
  for(ic = 0; ic < len; ic++)
    printk("%s%02x", ic ? ":" : "", buf[ic]);

  if (dataLen > CEC_MAX_DATA_LEN)
  {
    printk("[CEC] %s: Message too long (%d > %d)\n", hint, dataLen, CEC_MAX_DATA_LEN);
    return;
  }

  if (dataLen > 0)
  {
    printk("\n");
    parseMessage(src, dst, dataLen, buf + 1);
    if (!activemode)
      AddMessageToBuffer(buf, len);
  } else {
	dprintk(4, " (PING)\n");
    //Lets check if the ping was send from or id, if so this means that 
    //the deviceId pinged ist already been taken
    if(src == dst && src == getLogicalDeviceType())
    {
      if(getLogicalDeviceType() != DEVICE_TYPE_UNREG)
        sendPingWithAutoIncrement();
    }
  }
}

//================
// Higher Level Functions

void sendReportPhysicalAddress(void) 
{
  unsigned short physicalAddress = getPhysicalAddress();
  unsigned char responseBuffer[SEND_BUF_SIZE];

  responseBuffer[0] = (getLogicalDeviceType() << 4) | BROADCAST;
  responseBuffer[1] = REPORT_PHYSICAL_ADDRESS;
  responseBuffer[2] = (((physicalAddress >> 12)&0xf) << 4) | ((physicalAddress >> 8)&0xf);
  responseBuffer[3] = (((physicalAddress >>  4)&0xf) << 4) | ((physicalAddress >> 0)&0xf);
  responseBuffer[4] = getDeviceType(); 
  sendMessage(5, responseBuffer);
}

void setSourceAsActive(void) 
{
  unsigned short physicalAddress = getPhysicalAddress();
  unsigned char responseBuffer[SEND_BUF_SIZE];

  responseBuffer[0] = (getLogicalDeviceType() << 4) | BROADCAST;
  responseBuffer[1] = ACTIVE_SOURCE;
  responseBuffer[2] = (((physicalAddress >> 12)&0xf) << 4) | ((physicalAddress >> 8)&0xf);
  responseBuffer[3] = (((physicalAddress >>  4)&0xf) << 4) | ((physicalAddress >> 0)&0xf);
  sendMessage(4, responseBuffer);
}

void wakeTV(void)
{
  unsigned char responseBuffer[SEND_BUF_SIZE];

  responseBuffer[0] = (getLogicalDeviceType() << 4) | DEVICE_TYPE_TV;
  responseBuffer[1] = IMAGE_VIEW_ON;
  sendMessage(2, responseBuffer);
}

void sendInitMessage(void)
{
  unsigned char responseBuffer[SEND_BUF_SIZE];
  // Determine if TV is On
  
  responseBuffer[0] = (getLogicalDeviceType() << 4) | DEVICE_TYPE_TV;
  responseBuffer[1] = GIVE_DEVICE_POWER_STATUS;
  sendMessage(2, responseBuffer);
}

void sendPingWithAutoIncrement(void)
{
  unsigned char responseBuffer[1];
  char ldt = 1 << deviceType;

  dprintk(1, "%s %d\n", __func__, __LINE__);
  // advance to the first matching device type
  while (!(ldt & logicalDeviceTypeChoices[logicalDeviceTypeChoicesIndex][0]))
    logicalDeviceTypeChoicesIndex++;
  printk("[CEC] sendPingWithAutoIncrement - 1\n");
  setIsFirstKiss(1);

  logicalDeviceType = logicalDeviceTypeChoices[logicalDeviceTypeChoicesIndex++][1];
  responseBuffer[0] = (logicalDeviceType << 4) | (logicalDeviceType & 0xF);
  dprintk(1, "%s %d\n", __func__, __LINE__);
  sendMessage(1, responseBuffer);
  dprintk(1, "%s %d\n", __func__, __LINE__);
  // advance to the next matching device type
  while (!(ldt & logicalDeviceTypeChoices[logicalDeviceTypeChoicesIndex][0]))
    logicalDeviceTypeChoicesIndex++;
}

void sendOneTouchPlay(void)
{
  unsigned short physicalAddress = getPhysicalAddress();
  unsigned char responseBuffer[SEND_BUF_SIZE];

  dprintk(1, "%s %d\n", __func__, __LINE__);
  responseBuffer[0] = (getLogicalDeviceType() << 4) | DEVICE_TYPE_TV;
  responseBuffer[1] = IMAGE_VIEW_ON;
  dprintk(1, "%s %d\n", __func__, __LINE__);
  sendMessage(2, responseBuffer);
  dprintk(1, "%s %d\n", __func__, __LINE__);
  udelay(10000);
  dprintk(1, "%s %d\n", __func__, __LINE__);

  responseBuffer[0] = (getLogicalDeviceType() << 4) | BROADCAST;
  responseBuffer[1] = ACTIVE_SOURCE;
  responseBuffer[2] = (((physicalAddress >> 12)&0xf) << 4) | ((physicalAddress >> 8)&0xf);
  responseBuffer[3] = (((physicalAddress >> 4)&0xf) << 4)  | ((physicalAddress >> 0)&0xf);
  dprintk(1, "%s %d\n", __func__, __LINE__);
  sendMessage(4, responseBuffer);
  dprintk(1, "%s %d\n", __func__, __LINE__);
}

void sendSystemStandby(int deviceId)
{
  unsigned char responseBuffer[SEND_BUF_SIZE];

  responseBuffer[0] = (getLogicalDeviceType() << 4) | (deviceId & 0xF);
  responseBuffer[1] = STANDBY;
  sendMessage(2, responseBuffer);
}

// vim:ts=4
