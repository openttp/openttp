//
//
// The MIT License (MIT)
//
// Copyright (c) 2016  Michael J. Wouters
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Based on code written by Brent A. Crosby www.crystalfontz.com, brent@crystalfontz.com
//
// Modification history
//

#include "Sys.h"
#include "Debug.h"

#include  <stdlib.h>
#include  <termios.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <errno.h>
#include  <ctype.h>
#include  <stdio.h>

#include <cstring>
#include <iostream>

#include "CFA635.h"

static const char *command_names[36]=
   {" 0 = Ping",
    " 1 = Read Version",
    " 2 = Write Flash",
    " 3 = Read Flash",
    " 4 = Store Boot State",
    " 5 = Reboot",
    " 6 = Clear LCD",
    " 7 = LCD Line 1",	//not a valid command for the 635 module
    " 8 = LCD Line 2",  //not a valid command for the 635 module
    " 9 = LCD CGRAM",
    "10 = Read LCD Memory",
    "11 = Place Cursor",
    "12 = Set Cursor Style",
    "13 = Contrast",
    "14 = Backlight",
    "15 = Read Fans",
    "16 = Set Fan Rpt.",
    "17 = Set Fan Power",
    "18 = Read DOW ID",
    "19 = Set Temp. Rpt",
    "20 = DOW Transaction",
    "21 = Set Live Display",
    "22 = Direct LCD Command",
    "23 = Set Key Event Reporting",
    "24 = Read Keypad, Polled Mode",
    "25 = Set Fan Fail-Safe",
    "26 = Set Fan RPM Glitch Filter",
    "27 = Read Fan Pwr & Fail-Safe",
    "28 = Set ATX switch functionality",
    "29 = Watchdog Host Reset",
    "30 = Rd Rpt",
    "31 = Send Data to LCD (row,col)",
    "32 = Key Legends",
    "33 = Set Baud Rate",
    "34 = Set/Configure GPIO",
    "35 = Read GPIO & Configuration"};
static const char *error_names[36]=
   {"Error: Ping",
    "Error: Version",
    "Error: Write Flash",
    "Error: Read Flash",
    "Error: Store Boot State",
    "Error: Reboot",
    "Error: Clear LCD",
    "Error: Set LCD 1",
    "Error: Set LCD 2",
    "Error: Set LCD CGRAM",
    "Error: Read LCD Memory",
    "Error: Place LCD Cursor",
    "Error: LCD Cursor Style",
    "Error: Contrast",
    "Error: Backlight",
    "Error: Query Fan",
    "Error: Set Fan Rept.",
    "Error: Set Fan Power",
    "Error: Read DOW ID",
    "Error: Set Temp. Rept.",
    "Error: DOW Transaction",
    "Error: Setup Live Disp",
    "Error: Direct LCD Command",
    "Error: Set Key Event Reporting",
    "Error: Read Keypad, Polled Mode",
    "Error: Set Fan Fail-Safe",
    "Error: Set Fan RPM Glitch Filter",
    "Error: Read Fan Pwr & Fail-Safe",
    "Error: Set ATX switch functionality",
    "Error: Watchdog Host Reset",
    "Error: Read  Reporting/ATX/Watchdog",
    "Error: Send Data to LCD (row,col)",
    "Error: Key Legends",
    "Error: Set Baud Rate",
    "Error: Set/Configure GPIO",
    "Error: Read GPIO & Configuration"};
static const char *key_names[21]=
   {"KEY_NONE",
    "KEY_UP_PRESS",
    "KEY_DOWN_PRESS",
    "KEY_LEFT_PRESS",
    "KEY_RIGHT_PRESS",
    "KEY_ENTER_PRESS",
    "KEY_EXIT_PRESS",
    "KEY_UP_RELEASE",
    "KEY_DOWN_RELEASE",
    "KEY_LEFT_RELEASE",
    "KEY_RIGHT_RELEASE",
    "KEY_ENTER_RELEASE",
    "KEY_EXIT_RELEASE",
    "KEY_UL_PRESS",
    "KEY_UR_PRESS",
    "KEY_LL_PRESS",
    "KEY_LR_PRESS",
    "KEY_UL_RELEASE",
    "KEY_UR_RELEASE",
    "KEY_LL_RELEASE",
    "KEY_LR_RELEASE"};

		
CFA635::CFA635()
{
}

CFA635::~CFA635()
{
	Dout(dc::trace,"CFA635::~CFA635()");
}


word CFA635::crc(ubyte *bufptr,word len,word seed)
{
	 //CRC lookup table to avoid bit-shifting loops.
  static const word crcLookupTable[256] =
    {0x00000,0x01189,0x02312,0x0329B,0x04624,0x057AD,0x06536,0x074BF,
     0x08C48,0x09DC1,0x0AF5A,0x0BED3,0x0CA6C,0x0DBE5,0x0E97E,0x0F8F7,
     0x01081,0x00108,0x03393,0x0221A,0x056A5,0x0472C,0x075B7,0x0643E,
     0x09CC9,0x08D40,0x0BFDB,0x0AE52,0x0DAED,0x0CB64,0x0F9FF,0x0E876,
     0x02102,0x0308B,0x00210,0x01399,0x06726,0x076AF,0x04434,0x055BD,
     0x0AD4A,0x0BCC3,0x08E58,0x09FD1,0x0EB6E,0x0FAE7,0x0C87C,0x0D9F5,
     0x03183,0x0200A,0x01291,0x00318,0x077A7,0x0662E,0x054B5,0x0453C,
     0x0BDCB,0x0AC42,0x09ED9,0x08F50,0x0FBEF,0x0EA66,0x0D8FD,0x0C974,
     0x04204,0x0538D,0x06116,0x0709F,0x00420,0x015A9,0x02732,0x036BB,
     0x0CE4C,0x0DFC5,0x0ED5E,0x0FCD7,0x08868,0x099E1,0x0AB7A,0x0BAF3,
     0x05285,0x0430C,0x07197,0x0601E,0x014A1,0x00528,0x037B3,0x0263A,
     0x0DECD,0x0CF44,0x0FDDF,0x0EC56,0x098E9,0x08960,0x0BBFB,0x0AA72,
     0x06306,0x0728F,0x04014,0x0519D,0x02522,0x034AB,0x00630,0x017B9,
     0x0EF4E,0x0FEC7,0x0CC5C,0x0DDD5,0x0A96A,0x0B8E3,0x08A78,0x09BF1,
     0x07387,0x0620E,0x05095,0x0411C,0x035A3,0x0242A,0x016B1,0x00738,
     0x0FFCF,0x0EE46,0x0DCDD,0x0CD54,0x0B9EB,0x0A862,0x09AF9,0x08B70,
     0x08408,0x09581,0x0A71A,0x0B693,0x0C22C,0x0D3A5,0x0E13E,0x0F0B7,
     0x00840,0x019C9,0x02B52,0x03ADB,0x04E64,0x05FED,0x06D76,0x07CFF,
     0x09489,0x08500,0x0B79B,0x0A612,0x0D2AD,0x0C324,0x0F1BF,0x0E036,
     0x018C1,0x00948,0x03BD3,0x02A5A,0x05EE5,0x04F6C,0x07DF7,0x06C7E,
     0x0A50A,0x0B483,0x08618,0x09791,0x0E32E,0x0F2A7,0x0C03C,0x0D1B5,
     0x02942,0x038CB,0x00A50,0x01BD9,0x06F66,0x07EEF,0x04C74,0x05DFD,
     0x0B58B,0x0A402,0x09699,0x08710,0x0F3AF,0x0E226,0x0D0BD,0x0C134,
     0x039C3,0x0284A,0x01AD1,0x00B58,0x07FE7,0x06E6E,0x05CF5,0x04D7C,
     0x0C60C,0x0D785,0x0E51E,0x0F497,0x08028,0x091A1,0x0A33A,0x0B2B3,
     0x04A44,0x05BCD,0x06956,0x078DF,0x00C60,0x01DE9,0x02F72,0x03EFB,
     0x0D68D,0x0C704,0x0F59F,0x0E416,0x090A9,0x08120,0x0B3BB,0x0A232,
     0x05AC5,0x04B4C,0x079D7,0x0685E,0x01CE1,0x00D68,0x03FF3,0x02E7A,
     0x0E70E,0x0F687,0x0C41C,0x0D595,0x0A12A,0x0B0A3,0x08238,0x093B1,
     0x06B46,0x07ACF,0x04854,0x059DD,0x02D62,0x03CEB,0x00E70,0x01FF9,
     0x0F78F,0x0E606,0x0D49D,0x0C514,0x0B1AB,0x0A022,0x092B9,0x08330,
     0x07BC7,0x06A4E,0x058D5,0x0495C,0x03DE3,0x02C6A,0x01EF1,0x00F78};

  //Initial CRC value is 0x0FFFF.
  register word newCrc;
  newCrc=seed;
  //This algorithim is based on the IrDA LAP example.
  while(len--)
    newCrc = (newCrc >> 8) ^ crcLookupTable[(newCrc ^ *bufptr++) & 0xff];
  //Make this crc match the one's complement that is sent in the packet.
  return(~newCrc);
}

ubyte CFA635::packetReceived(void)
{
	Sync_Read_Buffer();
  ubyte i;
  //First off, there must be at least 4 bytes available in the input stream
  //for there to be a valid command in it (command, length, no data, CRC).
  if((i=BytesAvail())<4)
    return(0);
  //The "peek" stuff allows us to look into the RS-232 buffer without
  //removing the data.
  Sync_Peek_Pointer();
  //Only commands 0 through MAX_COMMAND are valid.
  if(MAX_COMMAND<(0x3F&(incoming_command.command=PeekByte())))
    {
    //Throw out one byte of garbage. Next pass through should re-sync.
    GetByte();
    return(0);
    }
  //There is a valid command byte. Get the data_length. The data length
  //must be within reason.
  if(MAX_DATA_LENGTH<(incoming_command.data_length=PeekByte()))
    {
    //Throw out one byte of garbage. Next pass through should re-sync.
    GetByte();
    return(0);
    }
  //Now there must be at least incoming_command.data_length+sizeof(CRC) bytes
  //still available for us to continue.
  if((int)PeekBytesAvail()<(incoming_command.data_length+2))
    //It looked like a valid start of a packet, but it does not look
    //like the complete packet has been received yet.
    return(0);
  //There is enough data to make a packet. Transfer over the data.
  for(i=0;i<incoming_command.data_length;i++)
    incoming_command.data[i]=PeekByte();
  //Now move over the CRC.
  incoming_command.CRC.as_bytes[0]=PeekByte();
  incoming_command.CRC.as_bytes[1]=PeekByte();
  //Now check the CRC.
  if(incoming_command.CRC.as_word==
     crc((ubyte *)&incoming_command,incoming_command.data_length+2,0xFFFF))
    {
    //This is a good packet. I'll be horn swaggled. Remove the packet
    //from the serial buffer.
    AcceptPeekedData();
    //Let our caller know that incoming_command has good stuff in it.
    return(1);
    }
  //The CRC did not match. Toss out one byte of garbage. Next pass through
  //should re-sync.
  GetByte();
  return(0);
}

void CFA635::send_packet(void)
{
	 //In order to send the entire packet in one call to "write()", we
  //need to place the CRC packed against the data. If data_length
  //happens to be MAX_DATA_LENGTH, then the position of the CRC is
  //outgoing_response.CRC (like you would expect), but if data_length
  //is less, the CRC moves into the data area, packed into the first
  //unused data position.
  //
  //Create a pointer to the first unused data positions.
  word *packed_CRC_position;
  packed_CRC_position=
    (word *)&outgoing_response.data[outgoing_response.data_length];
  *packed_CRC_position=
    crc((ubyte *)&outgoing_response,outgoing_response.data_length+2,0xFFFF);
  //command, length, data[data_length], crc, crc
  SendData((ubyte *)&outgoing_response,outgoing_response.data_length+4);
}

void CFA635::ShowReceivedPacket(void)
{
  int
    i;
  //Terminate the incoming data so C handles it well in case it is a string.
  incoming_command.data[incoming_command.data_length]=0;
  char expanded[400];
  expanded[0]=0;
  for(i=0;i<incoming_command.data_length;i++)
    {
    if(isprint(incoming_command.data[i]))
      {
      int
        current_length=strlen(expanded);
      expanded[current_length]=incoming_command.data[i];
      expanded[current_length+1]=0;
      }
    else
      {
      char
        bin_number[5];
      sprintf(bin_number,"\\%03d",incoming_command.data[i]);
      strcat(expanded,bin_number);
      }
    }

  //key
	#ifdef CWDEBUG
  if(incoming_command.command==0x80)
	{
		char buf[1024];
		snprintf(buf,1023,"C=%d(key:%s),L=%d,D=\"%s\",CRC=0x%04X\n",
     incoming_command.command,incoming_command.data[0]<=20?
       key_names[incoming_command.data[0]]:
         "HUH?",
     incoming_command.data_length,
     expanded,
     incoming_command.CRC);
		Dout(dc::trace,"CFA635::ShowReceivedPacket()");
		Dout(dc::trace,buf);
	}
  else
	{
		//any other packet types
		char buf[1024];
		snprintf(buf,1023,"C=%d(%s),L=%d,D=\"%s\",CRC=0x%04X",
      incoming_command.command,
      ((incoming_command.command&0xC0)==0xC0)?
        error_names[0x3F&incoming_command.command]:
          command_names[0x3F&incoming_command.command],
      incoming_command.data_length,
      expanded,
      incoming_command.CRC);
		Dout(dc::trace,"CFA635::ShowReceivedPacket()");
		Dout(dc::trace,buf);
	}
	#endif
}
//
//
//
int CFA635::Serial_Init(const char *devname, int baud_rate)
{
  int
    brate;
  struct
    termios term;

  //open device
  handle = open(devname, O_RDWR | O_NOCTTY | O_NONBLOCK);

  if(handle <= 0)
	{
		Dout(dc::warning,"CFA635::Serial_Init() open() failed");
		return(1);
	}

  //get baud rate constant from numeric value
  switch (baud_rate)
  {
    case 19200:
      brate=B19200;
      break;
    case 115200:
      brate=B115200;
      break;
    default:
      Dout(dc::warning,"CFA635::Serial_Init() invalid baud rate: " << baud_rate <<
				" (must be 19200 or 115200)");
      return(2);
  }
  //get device struct
  if(tcgetattr(handle, &term) != 0)
  {
    Dout(dc::warning,"CFA635::Serial_Init() tcgetattr() failed");
    return(3);
	}

  //input modes
  term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INPCK|ISTRIP|INLCR|IGNCR|ICRNL
                  |IXON|IXOFF);
  term.c_iflag |= IGNPAR;

  //output modes
  term.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONOCR|ONLRET|OFILL
                  |OFDEL|NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);

  //control modes
  term.c_cflag &= ~(CSIZE|PARENB|PARODD|HUPCL|CRTSCTS);
  term.c_cflag |= CREAD|CS8|CSTOPB|CLOCAL;

  //local modes
  term.c_lflag &= ~(ISIG|ICANON|IEXTEN|ECHO);
  term.c_lflag |= NOFLSH;

  //set baud rate
  cfsetospeed(&term, brate);
  cfsetispeed(&term, brate);

  //set new device settings
  if(tcsetattr(handle, TCSANOW, &term)  != 0)
  {
    Dout(dc::warning,"CFA635::Serial_Init() tcsetattr() failed");
    return(4);
  }

  ReceiveBufferHead=ReceiveBufferTail=0;

  Dout(dc::trace,"CFA635::Serial_Init() success");
  return(0);
}

void CFA635::Uninit_Serial()
{
  close(handle);
  handle=0;
}

void CFA635::USendByte(unsigned char datum)
{
  int bytes_written=0;

  if(handle)
	{
    if((bytes_written=write(handle, &datum, 1)) != 1)
		{
      Dout(dc::warning,"CFA635::USendByte(): system call write() return error");
			Dout(dc::warning,"CFA635::USendByte(): Asked for 1 byte to be written, but " << bytes_written <<  "reported as written.");
		}
	}
  else
	{
    Dout(dc::warning,"CFA635::USendByte() handle is null");
	}
}

void CFA635::SendData(unsigned char *data,int length)
{
  int bytes_written=0;


	if(handle)
	{
		if((bytes_written=write(handle, data, length)) != length)
		{
			Dout(dc::warning,"CFA635::SendByte(): system call write() return error");
			Dout(dc::warning,"CFA635::SendByte(): Asked for "<< length << " bytes to be written, but " << bytes_written <<  "reported as written.");
		}
	}
	else
	{
		Dout(dc::warning,"CFA635::SendByte() handle is null");
	}
}
//------------------------------------------------------------------------------
//Gets incoming data and puts it into SerialReceiveBuffer[].
void CFA635::Sync_Read_Buffer(void)
{
  ubyte
    Incoming[4096];
  long
    BytesRead;

  //  COMSTAT status;
  

  if(!handle)
    return;

  //get data
  BytesRead = read(handle, &Incoming, 4096);
  if(0<BytesRead)
  {
  //Read the incoming ubyte, store it.
  for(long i=0; i < BytesRead; i++)
    {
    SerialReceiveBuffer[ReceiveBufferHead] = Incoming[i];

    //Increment the pointer and wrap if needed.
    ReceiveBufferHead++;
    if (RECEIVEBUFFERSIZE <= ReceiveBufferHead)
      ReceiveBufferHead=0;
      }
    }
}
/*---------------------------------------------------------------------------*/
dword CFA635::BytesAvail(void)
{
  //LocalReceiveBufferHead and return_value are a signed variables.
  int
    LocalReceiveBufferHead;
  int
    return_value;

  //Get a register copy of the head pointer, since an interrupt that
  //modifies the head pointer could screw up the value. Convert it to
  //our signed local variable.
  LocalReceiveBufferHead = ReceiveBufferHead;
  if((return_value = (LocalReceiveBufferHead - (int)ReceiveBufferTail)) < 0)
    return_value+=RECEIVEBUFFERSIZE;

  return(return_value);
}

/*---------------------------------------------------------------------------*/
ubyte CFA635::GetByte(void)
  {
  dword
    LocalReceiveBufferTail;
  dword
    LocalReceiveBufferHead;
  ubyte
    return_byte;

  //Get a register copy of the tail pointer for speed and size.
  LocalReceiveBufferTail=ReceiveBufferTail;

  //Get a register copy of the head pointer, since an interrupt that
  //modifies the head pointer could screw up the value.
  LocalReceiveBufferHead=ReceiveBufferHead;


  //See if there are any more bytes available.
  if(LocalReceiveBufferTail!=LocalReceiveBufferHead)
    {
    //There is at least one more ubyte.
    return_byte=SerialReceiveBuffer[LocalReceiveBufferTail];

    //Increment the pointer and wrap if needed.
    LocalReceiveBufferTail++;
    if(RECEIVEBUFFERSIZE<=LocalReceiveBufferTail)
      LocalReceiveBufferTail=0;

    //Update the real ReceiveBufferHead with our register copy. Make sure
    //the ISR does not get serviced during the transfer.
    ReceiveBufferTail=LocalReceiveBufferTail;
    }

  return(return_byte);
  }

	
dword CFA635::PeekBytesAvail(void)
  {
  //LocalReceiveBufferHead and return_value are a signed variables.
  int
    LocalReceiveBufferHead;
  int
    return_value;

  //Get a register copy of the head pointer, since an interrupt that
  //modifies the head pointer could screw up the value. Convert it to
  //our signed local variable.
  LocalReceiveBufferHead=ReceiveBufferHead;
  if((return_value=(LocalReceiveBufferHead-(int)ReceiveBufferTailPeek))<0)
    return_value+=RECEIVEBUFFERSIZE;
  return(return_value);
  }

void CFA635::Sync_Peek_Pointer(void)
{
	ReceiveBufferTailPeek=ReceiveBufferTail;
}

void CFA635::AcceptPeekedData(void)
{
  ReceiveBufferTail=ReceiveBufferTailPeek;
}

ubyte CFA635::PeekByte(void)
{
  int
    LocalReceiveBufferTailPeek;
  int
    LocalReceiveBufferHead;
  ubyte return_byte;

  //Get a register copy of the tail pointer for speed and size.
  LocalReceiveBufferTailPeek=ReceiveBufferTailPeek;

  //Get a register copy of the head pointer, since an interrupt that
  //modifies the head pointer could screw up the value.
  LocalReceiveBufferHead=ReceiveBufferHead;

  //See if there are any more bytes available.
  if(LocalReceiveBufferTailPeek!=LocalReceiveBufferHead)
    {
    //There is at least one more ubyte.
    return_byte=SerialReceiveBuffer[LocalReceiveBufferTailPeek];

    //Increment the pointer and wrap if needed.
    LocalReceiveBufferTailPeek++;
    if(RECEIVEBUFFERSIZE<=LocalReceiveBufferTailPeek)
      LocalReceiveBufferTailPeek=0;

    //Update the real ReceiveBufferHead with our register copy. Make sure
    //the ISR does not get serviced during the transfer.
    ReceiveBufferTailPeek=LocalReceiveBufferTailPeek;
    }
  return(return_byte);
}
