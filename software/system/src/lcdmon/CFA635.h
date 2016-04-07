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


#ifndef __CFA635_H_
#define __CFA635_H_

#include <string>
#include <sstream>

#define MAX_DATA_LENGTH 22
#define MAX_COMMAND 35
#define RECEIVEBUFFERSIZE 4096

#include "typedefs.h"

typedef struct
{
	ubyte command;
	ubyte data_length;
	ubyte data[MAX_DATA_LENGTH];
	WORD_UNION CRC;
}COMMAND_PACKET;


class CFA635
{
	public:
	
		CFA635();
		virtual ~CFA635();
		
		word crc(ubyte *bufptr,word len,word seed);
		ubyte packetReceived(void);
		void send_packet(void);
		
		void ShowReceivedPacket(void);
		
	protected:
		
		int	Serial_Init(const char *devname, int baud_rate);
		void	Uninit_Serial();
		void	SendByte(unsigned char datum);
		void	Sync_Read_Buffer(void);
		dword	BytesAvail(void);
		ubyte	GetByte(void);
		dword	PeekBytesAvail(void);
		void	Sync_Peek_Pointer(void);
		void	AcceptPeekedData(void);
		ubyte	PeekByte(void);
		void	SendData(unsigned char *data,int length);

		void USendByte(unsigned char datum);
		
		COMMAND_PACKET incoming_command;
		COMMAND_PACKET outgoing_response;
		
	private:
	
		int handle; // serial port fd
		ubyte SerialReceiveBuffer[RECEIVEBUFFERSIZE];
		dword ReceiveBufferHead;
		dword ReceiveBufferTail;
		dword ReceiveBufferTailPeek;
};
#endif
