/*
 * OpenOK.h
 *
 * Created on: Oct 15, 2009
 * Author : Jennifer Holt
 * e-mail :
 * Country: USA
 *
 * Modified on: Aug 01, 2012
 * Author : Rik van der Kooij, Taco Walstra
 * e-mail :
 * Country: Holland
 *
 * Modified on: Aug 31, 2012
 * Author : Maximilien de Bayser
 * e-mail :
 * Country: Brazil
 *
 * Modified on: Jun 02, 2014
 * Author : Jorge Francisco B. Melo
 * e-mail : jorgefrancisco.melo@gmail.com
 * Country: Brazil
 *
 * Open Opal Kelly interface library
 *
 * Copyright (c) 2009 Jennifer Holt
 * Copyright (c) 2012 Rik Van der Kooij, Taco Walstra
 * Copyright (c) 2012 Maximilien de Bayser
 * Copyright (c) 2014 Jorge Francisco B. Melo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef OpenOK_H_
#define OpenOK_H_
#endif

#ifndef LIBUSB_H_
#ifdef __linux
#include <libusb-1.0/libusb.h>
#else
#include "libusbx-1.0/libusb.h"
#endif
#define LIBUSB_H_
#endif

#include <fstream>
#include <iostream>
#include <iterator>
#include <math.h>
#include <string>
#include <string.h>
#include <vector>

#ifdef QT_CORE_LIB
#include "Sleep.h"
#else
#include <unistd.h>
#endif

#define WRITE_READ_FAST

#define OPENOK_MAJOR 002
#define OPENOK_MINOR 001
#define OPENOK_MICRO 002
#define OPENOK_NANO  000

#define VENDOR_OPAL_KELLY 0x151F

#define sizeRegistersPLL22150 11

#define maxUSBDevices 127

#define controlReadMode  0xc0
#define controlWriteMode 0x40

#define endpointIN  0x86 // EP 6 IN
#define endpointOUT 0x02 // EP 2 OUT

#define WAIT_RESET_MICROSECONDS 500000

//---------------------------------------------------------------------------------------------------------------------------------

class OpenOK_CPLL22150
{
    public:
        enum ClockSource
        {
            ClkSrc_Ref = 0,
            ClkSrc_Div1ByN = 1,
            ClkSrc_Div1By2 = 2,
            ClkSrc_Div1By3 = 3,
            ClkSrc_Div2ByN = 4,
            ClkSrc_Div2By2 = 5,
            ClkSrc_Div2By4 = 6
        };

        // Inverted in official API
        enum DividerSource
        {
            DivSrc_Ref = 1,
            DivSrc_VCO = 0
        };

        // See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
        enum registersCY22150
        {
            register_09H = 0,
            register_OCH = 1,
            register_12H = 2,
            register_13H = 3,
            register_40H = 4,
            register_41H = 5,
            register_42H = 6,
            register_44H = 7,
            register_45H = 8,
            register_46H = 9,
            register_47H = 10
        };

        unsigned char m_registersPLL[ sizeRegistersPLL22150 ];

        double m_referenceFrequency;

    private:

    public:
        OpenOK_CPLL22150();
        void SetCrystalLoad( double capload );
        void InitFromProgrammingInfo(unsigned char *buf);
        void GetProgrammingInfo(unsigned char *buf);
        void SetReference( double freq, bool extosc );
        void SetDiv1( DividerSource divsrc, int n );
        void SetDiv2( DividerSource divsrc, int n );
        bool SetVCOParameters( int p, int q );
        void SetOutputSource( int output, ClockSource clksrc );
        void SetOutputEnable( int output, bool enable );
        double GetReference();
        int GetVCOP();
        int GetVCOQ();
        double GetVCOFrequency();
        DividerSource GetDiv1Source();
        DividerSource GetDiv2Source();
        int GetDiv1Divider();
        int GetDiv2Divider();
        ClockSource GetOutputSource( int output );
        double GetOutputFrequency( int output );
        bool IsOutputEnabled( int output );
};
//---------------------------------------------------------------------------------------------------------------------------------

struct OpenOK_device
{
        static const unsigned short int SSIZE = 64;

        struct libusb_device *device;

        char deviceID[ SSIZE ];

        char serial[ SSIZE ];

        char manufacturer[ SSIZE ];

        char product[ SSIZE ];

        // USB Vendor ID(VID)
        unsigned short int idVendor;

        // USB Product ID(PID)
        unsigned short int idProduct;

        unsigned short int bcdUSB;

        unsigned short int bNumConfigurations;

        unsigned short int bDeviceClass;

        unsigned short int bNumInterfaces;

        unsigned short int num_altsetting[ SSIZE ];

        unsigned short int bInterval[ SSIZE ];

        unsigned short int bInterfaceNumber[ SSIZE ];

        unsigned short int bNumEndpoints[ SSIZE ];

        unsigned short int bDescriptorType[ SSIZE ];

        unsigned short int bEndpointAddress[ SSIZE ];

        unsigned short int wMaxPacketSize[ SSIZE ];

        unsigned short int maxPacketSize;

        unsigned short int shiftMaxPacketSize;

        OpenOK_device() {
            device = NULL;

            strncpy( deviceID, "Not Available", OpenOK_device::SSIZE );
            strncpy( serial, "Not Available", OpenOK_device::SSIZE );
            strncpy( manufacturer, "Not Available", OpenOK_device::SSIZE );
            strncpy( product, "Not Available", OpenOK_device::SSIZE );

            idVendor = 0;
            idProduct = 0;
            bcdUSB = 0;
            bNumConfigurations = 0;
            bDeviceClass = 0;
            bNumInterfaces = 0;

            memset( wMaxPacketSize, 0,
                    OpenOK_device::SSIZE );

            memset( num_altsetting, 0,
                    OpenOK_device::SSIZE );

            memset( bInterfaceNumber, 0,
                    OpenOK_device::SSIZE );

            memset( bNumEndpoints, 0,
                    OpenOK_device::SSIZE );

            memset( bDescriptorType, 0,
                    OpenOK_device::SSIZE );

            memset( bEndpointAddress, 0,
                    OpenOK_device::SSIZE );

            memset( bInterval, 0,
                    OpenOK_device::SSIZE );

            maxPacketSize = 0;

            shiftMaxPacketSize = 0;
        }
};
//---------------------------------------------------------------------------------------------------------------------------------

class OpenOK
{
    public:

        enum ErrorCode
        {
            // Official

            NoError = 0,
            Failed = -1,
            Timeout = -2,
            DoneNotHigh = -3,
            TransferError = -4,
            CommunicationError = -5,
            InvalidBitstream = -6,
            FileError = -7,
            DeviceNotOpen = -8,
            InvalidEndpoint = -9,
            InvalidBlockSize = -10,
            I2CRestrictedAddress = -11,
            I2CBitError = -12,
            I2CNack = -13,
            I2CUnknownStatus = -14,
            UnsupportedFeature = -15,
            FIFOUnderflow = -16,
            FIFOOverflow = -17,
            DataAlignmentError = -18,

            // Not Official

            SignedArgumentError = -60,
            RangeAddressError = -61,
            NotFoundDeviceStringID = -62,
            ClaimInterfaceError = -63,
            SetConfigurationError = -64,
            NotOpenBitFile = -65,
            NotFoundSyncWord = -66,
            NotConfigureFPGA = -67,
            NotFoundUSBDevices = -68,
            NotFoundDeviceStringSerial = -69,
            NotFoundOpallKellyBoard = -70,
            NotSentBitFile = -71,
            LibusbNotInitialization = -72,
            ReadBitFileError = -73,
            FrontPanelDisabled = -74,
            NotPossibleCheckFrontPanel = -75,
            ControlStatusInvalidResponse = -76,
            OperationNotPermitted = -77,
            GetConfigurationError = -78,
            PointerNULL = -79,
            LibusbError = -80,
            CatchError = -81,
            CheckEnableInvalidResponse = -82,
            CheckDisableInvalidResponse = -83,
            OpenByDeviceIndexListInvalidResponse = -84,
            ControlTransferError = -400, // don't put anything between this line and the next
            BulkTransferError = -500 // don't put anything after this line
        };

        enum BoardModel
        {
            brdUnknown = 0,
            brdXEM3001v1 = 1,
            brdXEM3001v2 = 2,
            brdXEM3010 = 3,
            brdXEM3005 = 4,
            brdXEM3001CL = 5,
            brdXEM3020 = 6,
            brdXEM3050 = 7,
            brdXEM9002 = 8,
            brdXEM3001RB = 9,
            brdXEM5010 = 10,
            brdXEM6110LX45 = 11,
            brdXEM6110LX150 = 15,
            brdXEM6001 = 12,
            brdXEM6010LX45 = 13,
            brdXEM6010LX150 = 14,
            brdXEM6006LX9 = 16,
            brdXEM6006LX16 = 17,
            brdXEM6006LX25 = 18,
            brdXEM5010LX110 = 19,

            brdEnd // don't put anything after this line
        };

    private:
        static const size_t WIREOUTSIZE = 64;
        static const size_t WIREINSIZE = 64;
        static const size_t TRIGGEROUTSIZE = 64;
        static const int m_interfaceDesired = 0;

        static const int m_configurationDesired = 1;

        struct libusb_device_handle *m_deviceHandle;

        struct libusb_context *m_ctx;

        // pointer to pointer of device, used to retrieve a list of devices
        struct libusb_device **m_listUSBDevices;

        short int m_indexOpenedDevice;

        struct OpenOK_device m_listOKDevices[ maxUSBDevices ];

        long m_lastTransferred;

        bool m_enablePrintStdError;

        int m_timeoutUSB;

        unsigned char m_wireOuts[ WIREOUTSIZE ];

        unsigned char m_wireIns[ WIREINSIZE ];

				unsigned char m_triggerOuts[ TRIGGEROUTSIZE ];
				
        unsigned short int m_shiftMaxPacketSize;

        unsigned short int m_maxPacketSize;

        bool m_libusbInitialization;

        double m_crystalReference;

        struct bitstream {
                std::string designName ;
                std::string partName ;
                std::string date ;
                std::string time ;
                unsigned int dataLen ; // Length of Bitstream Data
                unsigned char dummyLen;
                bool addPadWord;
        } m_bitStream;

    public:
        // Not Official

        OpenOK();

        ~OpenOK();

        ErrorCode OpenByDeviceID( std::string str = "" );

        bool InitializationLibUSB();

        bool ResetDeviceUSB( std::string serial );

        void PrintInfoAllDevice();

        std::string GetLibUSBVersion();

        std::string GetOpenOKVersion();

        int GetOpenOKMajorVersion();

        int GetOpenOKMinorVersion();

        int GetOpenOKMicroVersion();

        int GetOpenOKNanoVersion();

        void SetEnablePrintStdError( bool enable );

        void Close( bool force = false );

        // Official

        ErrorCode OpenBySerial( std::string str = "" );

        std::string GetBoardModelString( BoardModel m );

        BoardModel GetBoardModel();

        int GetDeviceCount();

        std::string GetSerialNumber();

        std::string GetDeviceID();

        int GetDeviceMajorVersion();

        int GetDeviceMinorVersion();

        bool IsHighSpeed();

        ErrorCode ConfigureFPGA( const std::string strFilename );

        ErrorCode ConfigureFPGAFromMemory( unsigned char *data, const unsigned long length );

        long WriteToPipeIn( int epAddr, long length, unsigned char *data );

        long ReadFromPipeOut( int epAddr, long length, unsigned char *data );

        void UpdateWireOuts();

        int GetWireOutValue( int epAddr );

        ErrorCode SetWireInValue( int epAddr, unsigned long val, unsigned long mask = 0xffffffff );

        void UpdateWireIns();

        bool IsOpen();

        ErrorCode LoadDefaultPLLConfiguration();

        long GetLastTransferLength();

        std::string GetDeviceListSerial( int num );

        void SetDeviceID( const std::string str );

        ErrorCode ResetFPGA();

        bool IsFrontPanelEnabled();

        BoardModel GetDeviceListModel( int num );

        ErrorCode WriteI2C( const int addr, int length, unsigned char *data );

        ErrorCode ReadI2C( const int addr, int length, unsigned char *data );

        ErrorCode ActivateTriggerIn( int epAddr, int bit );

        void UpdateTriggerOuts();
        bool IsTriggered( int epAddr, unsigned long mask );
				 
        // Not tested
        int OpenOK_set_trigger_in( int ep, unsigned char *data );

       

        bool IsFrontPanel3Supported();

        void SetTimeout( int timeout );

        ErrorCode SetBTPipePollingInterval( int interval );

        int GetHostInterfaceWidth();

        void EnableAsynchronousTransfers( bool enable );

        long WriteToBlockPipeIn( int epAddr, int blockSize, long length, unsigned char *data );

        long ReadFromBlockPipeOut( int epAddr, int blockSize, long length, unsigned char *data );

        ErrorCode SetEepromPLL22150Configuration( OpenOK_CPLL22150 &pll );

        ErrorCode SetPLL22150Configuration( OpenOK_CPLL22150 &pll );

        ErrorCode GetEepromPLL22150Configuration( OpenOK_CPLL22150 &pll );

        ErrorCode GetPLL22150Configuration( OpenOK_CPLL22150 &pll );

    private:
        // Not Official

        int SendDataBitFile( unsigned char endp, unsigned char *bytes, unsigned int size, unsigned int timeout );

        ErrorCode SendPaddedFileToFPGA( unsigned char* buffer, const unsigned int size );

        // As the method is private, has no problem declaring without setting here
        template<class iterator>
        ErrorCode SendFileToFPGA(iterator it, iterator end);

        int GetLanguageID( libusb_device_handle *OpenOK_dev_handle );

        ErrorCode Get_OK_Devices( bool countEnable, int &countDevices );

        ErrorCode OpenByDeviceIndexList( unsigned int indexDeviceList );

        ErrorCode CheckEnable();

        ErrorCode CheckDisable();

        ErrorCode CheckFrontPanelEnabled();

        ErrorCode ControlStatus( unsigned int mode );

        int GetNumberEndpoints( unsigned int numberInterface );

        int GetMaxPacketSizeEndpoint( unsigned int numberEndpoint );

        int GetEndpointAddress( unsigned int numberEndpoint );

        std::string GetStringOpenOKError( int numberError );

        std::string GetManufacturer();

        std::string GetProduct();

        BoardModel GetBoardModelNumber( std::string tmp );

        ErrorCode ReleaseInterface();

        ErrorCode ClearListUSB( libusb_device **device );

        OpenOK::ErrorCode ClearListOpenOK();

        int GetListUSB( libusb_context *ctx,
                        libusb_device ***listDevices );

        ErrorCode OpenDeviceUSB( libusb_device *dev,
                                 libusb_device_handle **handleOpen );

        ErrorCode CloseDeviceUSB( libusb_device_handle *handle );

        ErrorCode GetStringDescriptor( char* str, libusb_device_handle *OpenOK_dev_handle,
                                       int indexStr, int maxSize );

        ErrorCode GetDeviceDescriptor( libusb_device *device,
                                       libusb_device_descriptor *descriptorDevice );

        ErrorCode GetConfigurationDescriptor( libusb_device *device,
                                              libusb_config_descriptor **descriptorConfiguration );

        ErrorCode FreeConfigurationDescriptor( libusb_config_descriptor *descriptorConfiguration );

        std::string GetStringLibUSBError( int errorCodeLibUSB );

        void PrintStdError( std::string nameFunction,
                            std::string str,
                            int errorCodeLibUSB = 0 ,
                            int erroCode = 0 );

        inline long ReadWritePipe( int32_t epAddr, int32_t endpointDir, int32_t endpointLower, int32_t endpointUpper,
                                   int32_t magicNumber1, int32_t magicNumber2, int64_t length, uint8_t *data );

        inline int ControlTransfer( libusb_device_handle *dev_handle,
                                    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                    unsigned char *data, uint16_t wLength, unsigned int timeout );

        inline int BulkTransfer( libusb_device_handle *dev_handle,
                                 unsigned char endpoint, unsigned char *data, int length,
                                 int *actual_length, unsigned int timeout );

        inline int OptionalControlTransfer( libusb_device_handle *dev_handle,
                                            uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                            unsigned char *data, uint16_t wLength, unsigned int timeout );

        inline int OptionalBulkTransfer( libusb_device_handle *dev_handle,
                                         unsigned char endpoint, unsigned char *data, int length,
                                         int *actual_length, unsigned int timeout );
};
//---------------------------------------------------------------------------------------------------------------------------------
