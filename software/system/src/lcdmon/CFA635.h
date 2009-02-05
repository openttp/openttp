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
		
		void Debug(std::string);
		
	protected:
	
		bool debugOn;
		
		int		Serial_Init(char *devname, int baud_rate);
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

		void CFA635::USendByte(unsigned char datum);
		
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
