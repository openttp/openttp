/*
 * OpenOK.cpp
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

#include "OpenOK.h"

//---------------------------------------------------------------------------------------------------------------------------------

/*
Constructor Class
*/

OpenOK_CPLL22150::OpenOK_CPLL22150()
{
    memset( m_registersPLL, 0, sizeRegistersPLL22150 );
    m_referenceFrequency = 0;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Returns true is the specified output is enabled.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

bool OpenOK_CPLL22150::IsOutputEnabled( int output )
{
    return ( ( m_registersPLL[ register_09H ] >> output ) & 1 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the current reference crystal frequency.
*/

double OpenOK_CPLL22150::GetReference()
{
    return m_referenceFrequency;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the VCO P(total) value.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

int OpenOK_CPLL22150::GetVCOP()
{
    unsigned char PO;
    unsigned short int PB;

    int VCOPTotal;

    PO = ( ( m_registersPLL[ register_42H ] >> 7 ) & 1 );
    PB = ( ( m_registersPLL[ register_40H ] & 3 ) << 8 ) + m_registersPLL[ register_41H ];

    VCOPTotal = 2 * ( PB + 4 ) + PO;

    return VCOPTotal;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the VCO Q(total) value.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

int OpenOK_CPLL22150::GetVCOQ()
{
    int VCOQTotal;

    VCOQTotal = ( m_registersPLL[ register_42H ] & 127 ) + 2;

    return VCOQTotal;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the VCO output frequency.
*/

double OpenOK_CPLL22150::GetVCOFrequency()
{
    double VCOFrequency = 0;

    if ( GetVCOQ() ) {
        VCOFrequency = GetReference() * GetVCOP() / GetVCOQ();
    }
    return VCOFrequency;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the source for divider #1.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

OpenOK_CPLL22150::DividerSource OpenOK_CPLL22150::GetDiv1Source()
{
    if ( ( m_registersPLL[ register_OCH ] >> 7 ) & 1 ) {
        return DivSrc_Ref;
    }
    return DivSrc_VCO;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the source for divider #2.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

OpenOK_CPLL22150::DividerSource OpenOK_CPLL22150::GetDiv2Source()
{
    if ( ( m_registersPLL[ register_47H ] >> 7 ) & 1 ) {
        return DivSrc_Ref;
    }
    return DivSrc_VCO;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the divider value for divider #1.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/
int OpenOK_CPLL22150::GetDiv1Divider()
{
    return ( m_registersPLL[ register_OCH ] & 127 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get the divider value for divider #2.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

int OpenOK_CPLL22150::GetDiv2Divider()
{
    return ( m_registersPLL[ register_47H ] & 127 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Gets the current output clock source for a particular output.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

OpenOK_CPLL22150::ClockSource OpenOK_CPLL22150::GetOutputSource( int output )
{
    unsigned char clkSrc = 0;

    switch( output ) {
        case 0 : // LCLK1
            clkSrc = ( m_registersPLL[ register_44H ] >> 5 ) & 7;
            break;

        case 1 : // LCLK2
            clkSrc = ( m_registersPLL[ register_44H ] >> 2 ) & 7;
            break;

        case 2 : // LCLK3
            clkSrc = ( ( m_registersPLL[ register_44H ] & 3 ) << 1 ) +
                    ( m_registersPLL[ register_45H ] >> 7 );
            break;

        case 3 : // LCLK4
            clkSrc = ( m_registersPLL[ register_45H ] >> 4 ) & 7;
            break;

        case 4 : // CLK5
            clkSrc = ( m_registersPLL[ register_45H ] >> 1 ) & 7;
            break;

        case 5 : // CLK6
            clkSrc = ( ( m_registersPLL[ register_45H ] & 1 ) << 2 ) +
                    ( m_registersPLL[ register_46H ] >> 6 );
            break;

        default :
            return ClkSrc_Ref;
            break;
    }

    switch( clkSrc ) {

        case 0 :
            return ClkSrc_Ref;
            break;

        case 1 :
            return ClkSrc_Div1ByN;
            break;

        case 2 :
            return ClkSrc_Div1By2;
            break;

        case 3 :
            return ClkSrc_Div1By3;
            break;

        case 4 :
            return ClkSrc_Div2ByN;
            break;

        case 5 :
            return ClkSrc_Div2By2;
            break;

        case 6 :
            return ClkSrc_Div2By4;
            break;

        default:
            return ClkSrc_Ref;
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Gets the current output frequency for a particular output.
*/

double OpenOK_CPLL22150::GetOutputFrequency( int output )
{
    ClockSource clkSrc;
    double CLK;

    clkSrc = GetOutputSource( output );

    //CLK = ((REF * P)/Q)/Post Divider
    //CLK = REF/Post Divider
    //CLK = REF

    switch( clkSrc ) {
        case ClkSrc_Ref :
            CLK = GetReference();
            break;

        case ClkSrc_Div1ByN :
            if ( GetDiv1Source() ) {
                CLK = GetReference() / GetDiv1Divider();
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / GetDiv1Divider();
                } else {
                    CLK = 0;
                }
            }
            break;

        case ClkSrc_Div1By2 :
            if ( GetDiv1Source() ) {
                CLK = GetReference() / 2.0;
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / 2.0;
                } else {
                    CLK = 0;
                }
            }
            break;

        case ClkSrc_Div1By3 :
            if ( GetDiv1Source() ) {
                CLK = GetReference() / 3.0;
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / 3.0;
                } else {
                    CLK = 0;
                }
            }
            break;

        case ClkSrc_Div2ByN :
            if ( GetDiv2Source() ) {
                CLK = GetReference() / GetDiv2Divider();
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / GetDiv2Divider();
                } else {
                    CLK = 0;
                }
            }
            break;

        case ClkSrc_Div2By2 :
            if ( GetDiv2Source() ) {
                CLK = GetReference() / 2.0;
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / 2.0;
                } else {
                    CLK = 0;
                }
            }
            break;

        case ClkSrc_Div2By4 :
            if ( GetDiv2Source() ) {
                CLK = GetReference() / 4.0;
            } else {
                if ( GetVCOQ() ) {
                    CLK = ( ( GetReference() * GetVCOP() ) / GetVCOQ() ) / 4.0;
                } else {
                    CLK = 0;
                }
            }
            break;

        default :
            CLK = 0;
            break;
    }
    return CLK;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Sets the reference crystal frequency for output frequency computations.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

void OpenOK_CPLL22150::SetReference( double freq, bool extosc )
{
    // Using an External Clock as the Reference Input
    // The CY22150 also accepts an external clock as reference, with
    // speeds up to 133 MHz. With an external clock, the XDRV
    // (register 12H) bits must be set according to Table 5 on page 6.

    if ( extosc ) {
        if ( freq >= 1 && freq < 25 ) {
            m_registersPLL[ register_12H ] = 32;
        } else if ( freq >= 25 && freq < 50 ) {
            m_registersPLL[ register_12H ] = 40;
        } else if ( freq >= 50 && freq < 90 ) {
            m_registersPLL[ register_12H ] = 48;
        } else if ( freq >= 90 && freq <= 133 ) {
            m_registersPLL[ register_12H ] = 56;
        }

        m_registersPLL[ register_13H ] = 149;

        m_referenceFrequency = freq;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the VCO P(total) and Q(total) values.

See Datasheet CY22150 - Table 2. Summary Table - CY22150 Programmable Registers
*/

bool OpenOK_CPLL22150::SetVCOParameters( int p, int q )
{
    double VCOFreq = 0;

    unsigned char pump;

    unsigned char PO;
    unsigned short int PB;

    // Ptotal = (2(PB + 4) + PO)
    // The minimum value of Ptotal is 8. The maximum value of Ptotal is 2055. To achieve the minimum value of Ptotal,
    // PB and PO should both be programmed to 0. To achieve the maximum value of Ptotal, PB should be programmed to
    // 1023, and PO should be
    // programmed to 1.
    // Qtotal= Q + 2
    // The minimum value of Qtotal is 2. The maximum value of Q total is 129.
    // Stable operation of the CY22150 cannot be guaranteed if REF/Qtotal falls below 250 kHz.
    if ( q >= 2 && q <= 129 && p >= 8 && p <= 2055 ) {
        VCOFreq = ( GetReference() * p ) / q;
    } else {
        return false;
    }

    // Stable operation of the CY22150 cannot be guaranteed if the value of (Ptotal*(REF/Qtotal)) is above 400 MHz or below
    // 100 MHz.
    if ( VCOFreq > 400 || VCOFreq < 100 ) {
        return false;
    }

    m_registersPLL[ register_42H ] &= 128;
    m_registersPLL[ register_42H ] |= ( ( q - 2 ) & 127 ) ;

    PO = p % 2;
    PB = ( ( ( p - PO ) / 2 ) - 4 );
    m_registersPLL[ register_42H ] &= 127;
    m_registersPLL[ register_42H ] |= ( ( PO & 1 ) << 7 );
    m_registersPLL[ register_40H ] &= 252;
    m_registersPLL[ register_40H ] |= ( ( PB >> 8 ) & 3 );
    m_registersPLL[ register_41H ] = ( PB & 255 );

    // PLL stability cannot be guaranteed for values below 16 and above 1023. If values above 1023 are needed, use CyClocksRT to
    // determine the best charge pump setting.
    if ( PB >= 16 && PB <= 44 ) {
        pump = 0;
    } else if ( PB >= 45 && PB <= 479 ) {
        pump = 1;
    } else if ( PB >= 480 && PB <= 639 ) {
        pump = 2;
    } else if ( PB >= 640 && PB <= 799 ) {
        pump = 3;
    } else if ( PB >= 800 && PB <= 1023 ) {
        pump = 4;
    } else {
        pump = 1;
    }

    m_registersPLL[ register_40H ] &= 195;
    m_registersPLL[ register_40H ] = ( ( ( pump & 7 ) << 2 ) | 192 );

    return true;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the values for divider #1.
*/

void OpenOK_CPLL22150::SetDiv1( DividerSource divsrc, int n )
{
    if ( n >= 4 && n <= 127 ) {
        m_registersPLL[ register_OCH ] = ( n & 127 ) + ( ( divsrc & 1 ) << 7 ) ;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the values for divider #2.
*/

void OpenOK_CPLL22150::SetDiv2( DividerSource divsrc, int n )
{
    if ( n >= 4 && n <= 127 ) {
        m_registersPLL[ register_47H ] = ( n & 127 ) + ( ( divsrc & 1 ) << 7 ) ;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the source for a particular output.
*/

void OpenOK_CPLL22150::SetOutputSource( int output, OpenOK_CPLL22150::ClockSource clksrc )
{
    switch( output ) {

        case 0 : // LCLK1
            m_registersPLL[ register_44H ] &= 31;
            m_registersPLL[ register_44H ] |= ( ( clksrc & 7 ) << 5 );
            break;

        case 1 : // LCLK2
            m_registersPLL[ register_44H ] &= 227;
            m_registersPLL[ register_44H ] |= ( ( clksrc & 7 ) << 2 );
            break;

        case 2 : // LCLK3
            m_registersPLL[ register_44H ] &= 252;
            m_registersPLL[ register_44H ] |= ( ( clksrc >> 1 ) & 3 );
            m_registersPLL[ register_45H ] &= 127;
            m_registersPLL[ register_45H ] |= ( ( clksrc & 1 ) << 7 );
            break;

        case 3 : // LCLK4
            m_registersPLL[ register_45H ] &= 143;
            m_registersPLL[ register_45H ] |= ( ( clksrc & 7 ) << 4 );
            break;

        case 4 : // CLK5
            m_registersPLL[ register_45H ] &= 241;
            m_registersPLL[ register_45H ] |= ( ( clksrc & 7 ) << 1 );
            break;

        case 5 : // CLK6
            m_registersPLL[ register_45H ] &= 254;
            m_registersPLL[ register_45H ] |= ( ( clksrc >> 2 ) & 1 );
            m_registersPLL[ register_46H ] = ( ( ( clksrc & 3 ) << 6 ) | 63 );
            break;

        default :
            break;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the output enable for a particular output.
*/

void OpenOK_CPLL22150::SetOutputEnable( int output, bool enable )
{
    if ( enable ) {
        m_registersPLL[ register_09H ] |= ( 1 << output );
    } else {
        m_registersPLL[ register_09H ] &= ~( 1 << output );
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Constructor Class
*/

OpenOK::OpenOK()
    : m_deviceHandle( NULL )
    , m_ctx( NULL )
    , m_listUSBDevices( NULL )
    , m_indexOpenedDevice( -1 )
    , m_lastTransferred( 0 )
    , m_enablePrintStdError( true )
    , m_timeoutUSB( 3000 )
{
    try {
        int responseLibusb = LIBUSB_SUCCESS;

        memset( m_wireOuts, 0, OpenOK::WIREOUTSIZE );
        memset( m_wireIns, 0, OpenOK::WIREINSIZE );

        // libusb_init return : 0 on success, or a LIBUSB_ERROR code on failure
        if ( ( responseLibusb = libusb_init( &m_ctx ) ) == LIBUSB_SUCCESS ) {
            // set verbosity level to LIBUSB_LOG_LEVEL_INFO, as suggested in the documentation :
            // LIBUSB_LOG_LEVEL_NONE (0) : no messages ever printed by the library (default)
            // LIBUSB_LOG_LEVEL_ERROR (1) : error messages are printed to stderr
            // LIBUSB_LOG_LEVEL_WARNING (2) : warning and error messages are printed to stderr
            // LIBUSB_LOG_LEVEL_INFO (3) : informational messages are printed to stdout, warning and
            // error messages are printed to stderr
            // LIBUSB_LOG_LEVEL_DEBUG (4) : debug and informational messages are printed to stdout, warnings and errors to stderr
            // enable log debug compiling with "--enable-debug-log"

            libusb_set_debug( m_ctx, 3/*LIBUSB_LOG_LEVEL_INFO*/ );

            m_libusbInitialization = true;

            //            PrintInfoAllDevice();
            //            PrintStdError( "Constructor OpenOK()",
            //                           GetLibUSBVersion() );
            //            PrintStdError( "Constructor OpenOK()",
            //                           GetOpenOKVersion() );
        } else {
            m_libusbInitialization = false;

            PrintStdError( "Constructor OpenOK()",
                           "libusb_init() failed",
                           responseLibusb );
        }
    } catch(...) {
        m_libusbInitialization = false;

        PrintStdError( "Constructor OpenOK()",
                       "catch() - Closing communication" );

        Close( true );
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Destructor Class
*/

OpenOK::~OpenOK()
{
    try {
        Close();

        if ( m_libusbInitialization ) {
            ErrorCode responseClearListUSB = ClearListUSB( m_listUSBDevices );

            if ( responseClearListUSB == NoError ) {
                libusb_exit( m_ctx );
            } else {
                PrintStdError( "Destructor OpenOK()",
                               "ClearListUSB() failed",
                               0,
                               responseClearListUSB );
            }
        }
    } catch(...) {
        PrintStdError( "Destructor OpenOK()",
                       "catch() - Closing communication" );

        Close( true );
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get openOK major version.
*/

int OpenOK::GetOpenOKMajorVersion()
{
    return OPENOK_MAJOR;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get openOK minor version.
*/

int OpenOK::GetOpenOKMinorVersion()
{
    return OPENOK_MINOR;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get openOK micro version.
*/

int OpenOK::GetOpenOKMicroVersion()
{
    return OPENOK_MICRO;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get openOK nano version.
*/

int OpenOK::GetOpenOKNanoVersion()
{
    return OPENOK_NANO;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get openOK version.
*/

std::string OpenOK::GetOpenOKVersion()
{
    char version_str[ 256 ];

    sprintf( version_str, "OpenOK version = %d.%d.%d.%d",
             OPENOK_MAJOR,
             OPENOK_MINOR,
             OPENOK_MICRO,
             OPENOK_NANO );

    return version_str;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get libusb version.
*/

std::string OpenOK::GetLibUSBVersion()
{
#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000100)
    try {
        const libusb_version *version = libusb_get_version();

        if ( version != NULL ) {
            char version_str[ 256 ];

            sprintf( version_str, "LibUSB version = %d.%d.%d.%d",
                     version->major,
                     version->minor,
                     version->micro,
                     version->nano );

            return version_str;
        } else {
            PrintStdError( "GetLibUSBVersion()",
                           "pointer 'version' is NULL" );
        }
    } catch(...) {
        PrintStdError( "GetLibUSBVersion()",
                       "catch() - Closing communication" );

        Close( true );
    }
#endif
    return "LibUSB version = Not Available";
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Check libusb initialization.
*/

bool OpenOK::InitializationLibUSB()
{
    return m_libusbInitialization;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method return string index descriptor.

Parameters:
[out]   string that will receive the descriptor
[in] 	libusb_device_handle
[in]	indexStr - index of string descriptor
[in]	maxSize - max size of string

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::GetStringDescriptor( char *str, libusb_device_handle *handle,
                                               int indexStr, int maxSize )
{
    try {
        if ( ( handle != NULL ) &&
             ( str != NULL ) ) {
            memset( str, 0, maxSize );

            // libusb_get_string_descriptor_ascii return : number of bytes returned in data, or LIBUSB_ERROR code on failure
            const int responseLibusb = libusb_get_string_descriptor_ascii( handle,
                                                                           indexStr,
                                                                           reinterpret_cast< unsigned char* >( str ),
                                                                           maxSize - 1 );

            if ( responseLibusb < 0 ) {
                if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                    Close();
                }

                PrintStdError( "GetStringDescriptor()",
                               "libusb_get_string_descriptor_ascii() failed",
                               responseLibusb );

                return LibusbError;
            }
        } else {
            if ( handle == NULL ) {
                PrintStdError( "GetStringDescriptor()",
                               "pointer 'handle' is NULL" );

                return PointerNULL;
            }

            if ( str == NULL ) {
                PrintStdError( "GetStringDescriptor()",
                               "pointer 'str' is NULL" );

                return PointerNULL;
            }
        }
    } catch(...) {
        PrintStdError( "GetStringDescriptor()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Clear list of USB devices

Parameters:
[in] 	libusb_device_handle

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::ClearListUSB( libusb_device **device )
{
    try {
        // Clear the list of USB devices.
        if ( ( device != NULL ) &&
             ( *device != NULL ) ) {
            libusb_free_device_list( device, 1 );
            device = NULL;
        }
    } catch(...) {
        PrintStdError( "ClearListUSB()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Clear list of OpenOK devices

Parameters:
[in] 	libusb_device_handle

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::ClearListOpenOK()
{
    for ( int idxClear = 0; idxClear < maxUSBDevices; ++idxClear ) {
        m_listOKDevices[ idxClear ] = OpenOK_device();
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Given list of USB devices

Parameters:
[in] 	libusb_context
[out]   list of USB devices

Return:
Number of USB devices
*/

int OpenOK::GetListUSB( libusb_context *ctx,
                        libusb_device ***listDevices )
{
    ssize_t responseLibusb = 0;

    try {
        // libusb_get_device_list return : the number of devices in the outputted list,
        // or any libusb_error according to errors encountered by the backend.
        responseLibusb = libusb_get_device_list( ctx,
                                                 listDevices );

        if ( responseLibusb < 0 ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                Close();
            }

            PrintStdError( "GetListUSB()",
                           "libusb_get_device_list() failed",
                           responseLibusb );

            return LibusbError;
        }

        if ( ( listDevices == NULL ) ||
             ( *listDevices == NULL ) ||
             ( **listDevices == NULL ) ) {
            PrintStdError( "GetListUSB()",
                           "pointer 'listDevices' is NULL" );

            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "GetListUSB()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return responseLibusb;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Given the descriptor device of USB device

Parameters:
[in] 	libusb_device
[out]   descriptor device

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::GetDeviceDescriptor( libusb_device *device,
                                               libusb_device_descriptor *descriptorDevice )
{
    try {
        if ( device != NULL ) {
            // libusb_get_device_descriptor return : 0 on success or a LIBUSB_ERROR code on failure
            const int responseLibusb = libusb_get_device_descriptor( device,
                                                                     descriptorDevice );

            if ( responseLibusb != LIBUSB_SUCCESS ) {
                if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                    Close();
                }

                PrintStdError( "GetDeviceDescriptorListUSB()",
                               "libusb_get_device_descriptor() failed",
                               responseLibusb );

                return LibusbError;
            }

            if ( descriptorDevice == NULL ) {
                PrintStdError( "GetDeviceDescriptorListUSB()",
                               "pointer 'descriptorDevice' is NULL" );

                return PointerNULL;
            }
        } else {
            PrintStdError( "GetDeviceDescriptorListUSB()",
                           "pointer 'device' is NULL" );

            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "GetDeviceDescriptorListUSB()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Open device USB

Parameters:
[in] 	libusb_device
[out]   libusb_device_handle

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::OpenDeviceUSB( libusb_device *deviceOpen,
                                         libusb_device_handle **handleOpen )
{
    try {
        if ( ( deviceOpen != NULL ) ) {
            // libusb_open return :
            // 0 on success
            // LIBUSB_ERROR_NO_MEM on memory allocation failure
            // LIBUSB_ERROR_ACCESS if the user has insufficient permissions
            // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
            // another LIBUSB_ERROR code on other failure
            const int responseLibusb = libusb_open( deviceOpen,
                                                    handleOpen );

            if ( responseLibusb != LIBUSB_SUCCESS ) {
                if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                    Close();
                }

                PrintStdError( "OpenDeviceUSB()",
                               "libusb_open() failed",
                               responseLibusb );

                return LibusbError;
            }

            if ( ( handleOpen == NULL ) ||
                 ( *handleOpen == NULL ) ) {
                PrintStdError( "OpenDeviceUSB()",
                               "pointer 'handleOpen' is NULL",
                               responseLibusb );

                return PointerNULL;
            }
        } else {
            PrintStdError( "OpenDeviceUSB()",
                           "pointer 'deviceOpen' is NULL" );

            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "OpenDeviceUSB()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Close device USB

Parameters:
[in]   libusb_device_handle

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::CloseDeviceUSB( libusb_device_handle *handle )
{
    try {
        if ( handle != NULL ) {
            libusb_close( handle );

            return NoError;
        } else {
            PrintStdError( "CloseDeviceUSB()",
                           "pointer 'handle' is NULL" );

            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "CloseDeviceUSB()",
                       "catch()" );

        return CatchError;
    }
    return Failed;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Given the descriptor configuration of USB device

Parameters:
[in] 	libusb_device
[out]   descriptor configuration

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::GetConfigurationDescriptor( libusb_device *device,
                                                      libusb_config_descriptor **descriptorConfiguration )
{
    try {
        if ( device != NULL ) {
            // libusb_get_config_descriptor return :
            // 0 on success
            // LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
            // another LIBUSB_ERROR code on error
            const int responseLibusb = libusb_get_config_descriptor( device,
                                                                     0,
                                                                     descriptorConfiguration );

            if ( responseLibusb != LIBUSB_SUCCESS ) {
                if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                    Close();
                }

                PrintStdError( "GetConfigurationDescriptorListUSB()",
                               "libusb_get_config_descriptor() failed",
                               responseLibusb );

                return LibusbError;
            }

            if ( ( descriptorConfiguration == NULL ) ||
                 ( *descriptorConfiguration == NULL ) ) {
                PrintStdError( "GetConfigurationDescriptorListUSB()",
                               "pointer 'descriptorConfiguration' is NULL",
                               responseLibusb );

                return PointerNULL;
            }
        } else {
            PrintStdError( "GetConfigurationDescriptorListUSB()",
                           "pointer 'device' is NULL" );
            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "GetConfigurationDescriptorListUSB()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Free configuration descriptor

Parameters:
[in] 	libusb_config_descriptor

Return:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::FreeConfigurationDescriptor( libusb_config_descriptor *descriptorConfiguration )
{
    try {
        if ( descriptorConfiguration != NULL ) {
            libusb_free_config_descriptor(descriptorConfiguration);

            return NoError;
        } else {
            PrintStdError( "FreeConfigurationDescriptor()",
                           "pointer 'descriptorConfiguration' is NULL" );

            return PointerNULL;
        }
    } catch(...) {
        PrintStdError( "FreeConfigurationDescriptor()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }
    return Failed;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method get OK devices in list.

Parameters:
[in] 	countEnable	used for GetCountDevices
[out]	countDevices	number of OK devices

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::Get_OK_Devices( bool countEnable, int &deviceCount )
{
    unsigned char countOK_Devices = 0;

    if ( &deviceCount != NULL ) {
        deviceCount = 0;
    } else {
        PrintStdError( "Get_OK_Devices()",
                       "pointer 'deviceCount' is NULL" );

        return PointerNULL;
    }

    if ( !m_libusbInitialization ) {
        return LibusbNotInitialization;
    }

    // Not is permitted get info devices while the device is opened.
    if ( IsOpen() ) {
        PrintStdError( "Get_OK_Devices()",
                       "Not is permitted get info devices while the device is opened",
                       0,
                       OperationNotPermitted );
        return OperationNotPermitted;
    }

    ErrorCode responseClearListUSB = ClearListUSB( m_listUSBDevices );

    if ( responseClearListUSB != NoError ) {
        PrintStdError( "Get_OK_Devices()",
                       "ClearListUSB failed",
                       0,
                       responseClearListUSB );
    }

    ErrorCode responseClearListOpenOK = ClearListOpenOK();

    if ( responseClearListOpenOK != NoError ) {
        PrintStdError( "Get_OK_Devices()",
                       "ClearListOpenOK failed",
                       0,
                       responseClearListOpenOK );
    }

    // Get the list of USB devices.
    const int numUSBDevices = GetListUSB( m_ctx,
                                          &m_listUSBDevices );

    // Not found USB devices.
    if ( numUSBDevices <= 0 ) {
        if ( numUSBDevices < 0 ) {
            PrintStdError( "Get_OK_Devices()",
                           "GetListUSB failed",
                           0,
                           numUSBDevices );
        }
        return NotFoundUSBDevices;
    }

    for ( unsigned char idxUSBDevice = 0; idxUSBDevice < numUSBDevices; ++idxUSBDevice ) {

        libusb_device_descriptor descriptorDevice;

        // Get descriptor USB device.
        const ErrorCode result = GetDeviceDescriptor( m_listUSBDevices[ idxUSBDevice ],
                                                      &descriptorDevice );

        if ( result >= 0 ) {
            if ( descriptorDevice.idVendor == VENDOR_OPAL_KELLY ) {
                if ( !countEnable ) {
                    OpenOK_device &currentOpalKellyDevice = m_listOKDevices[ countOK_Devices ];

                    currentOpalKellyDevice.device = m_listUSBDevices[ idxUSBDevice ] ;

                    libusb_device_handle *currentHandleDevice;

                    // Open USB device
                    const ErrorCode responseOpen = OpenDeviceUSB( currentOpalKellyDevice.device,
                                                                  &currentHandleDevice );

                    if ( ( responseOpen == NoError ) &&
                         ( currentHandleDevice != NULL ) ) {

                        char strDeviceID[ OpenOK_device::SSIZE ];

                        // Get Device ID
                        const int responseControl = ControlTransfer( currentHandleDevice,
                                                                     controlReadMode,
                                                                     0xb0,
                                                                     0x1fd0,
                                                                     0x0000,
                                                                     reinterpret_cast< unsigned char* >(strDeviceID),
                                                                     32,
                                                                     m_timeoutUSB );

                        if ( responseControl >= 0 ) {
                            strncpy( currentOpalKellyDevice.deviceID, strDeviceID, OpenOK_device::SSIZE );

                            // Serial
                            GetStringDescriptor( currentOpalKellyDevice.serial, currentHandleDevice, descriptorDevice.iSerialNumber,
                                                 OpenOK_device::SSIZE );

                            // Manufacturer
                            GetStringDescriptor( currentOpalKellyDevice.manufacturer, currentHandleDevice, descriptorDevice.iManufacturer,
                                                 OpenOK_device::SSIZE );

                            // Product
                            GetStringDescriptor( currentOpalKellyDevice.product, currentHandleDevice, descriptorDevice.iProduct,
                                                 OpenOK_device::SSIZE );

                            currentOpalKellyDevice.idVendor           = descriptorDevice.idVendor;
                            currentOpalKellyDevice.idProduct          = descriptorDevice.idProduct;
                            currentOpalKellyDevice.bcdUSB             = descriptorDevice.bcdDevice;
                            currentOpalKellyDevice.bNumConfigurations = descriptorDevice.bNumConfigurations;
                            currentOpalKellyDevice.bDeviceClass       = descriptorDevice.bDeviceClass;

                            libusb_config_descriptor* descriptorConfiguration = NULL;

                            const ErrorCode responseConfigDesc = GetConfigurationDescriptor( m_listUSBDevices[ idxUSBDevice ],
                                                                                             &descriptorConfiguration );

                            if ( responseConfigDesc != NoError ) {
                                PrintStdError( "Get_OK_Devices()",
                                               "GetConfigurationDescriptorListUSB() failed",
                                               0,
                                               responseConfigDesc );
                            }

                            if ( descriptorConfiguration != NULL ) {
                                currentOpalKellyDevice.bNumInterfaces = descriptorConfiguration->bNumInterfaces;

                                for ( unsigned char i = 0; i < descriptorConfiguration->bNumInterfaces ; ++i ) {
                                    const libusb_interface *interface = &descriptorConfiguration->interface[i];

                                    currentOpalKellyDevice.num_altsetting[ i ] = interface->num_altsetting;

                                    for ( int j = 0; j < interface->num_altsetting ; ++j ) {
                                        const libusb_interface_descriptor *descriptorInterface = &interface->altsetting[ j ];

                                        currentOpalKellyDevice.bInterfaceNumber[ j ] = descriptorInterface->bInterfaceNumber;
                                        currentOpalKellyDevice.bNumEndpoints[ j ] = descriptorInterface->bNumEndpoints;

                                        for ( unsigned char k = 0; k < descriptorInterface->bNumEndpoints; ++k ) {
                                            const libusb_endpoint_descriptor* epdesc = &descriptorInterface->endpoint[ k ];

                                            currentOpalKellyDevice.bDescriptorType[ k ] = epdesc->bDescriptorType;
                                            currentOpalKellyDevice.bEndpointAddress[ k ] = epdesc->bEndpointAddress;
                                            currentOpalKellyDevice.bInterval[ k ] = epdesc->bInterval;
                                            currentOpalKellyDevice.wMaxPacketSize[ k ] = epdesc->wMaxPacketSize;
                                        }
                                    }
                                }

                                // High Speed = 512 bytes
                                // Full Speed = 8, 16, 32 or 64 bytes
                                currentOpalKellyDevice.maxPacketSize = currentOpalKellyDevice.wMaxPacketSize[ 0 ];

                                // To calculate multiple of MaxPacketSize.
                                currentOpalKellyDevice.shiftMaxPacketSize = log2( currentOpalKellyDevice.maxPacketSize << 1 );

                                if ( FreeConfigurationDescriptor( descriptorConfiguration ) != NoError ) {
                                    PrintStdError( "Get_OK_Devices()",
                                                   "FreeConfigurationDescriptor() failed" );
                                }
                            } else {
                                currentOpalKellyDevice.bNumInterfaces = 0;
                            }
                        } else {
                            PrintStdError( "Get_OK_Devices()",
                                           "ControlTransfer() failed",
                                           0,
                                           responseControl );
                        }

                        if ( CloseDeviceUSB( currentHandleDevice ) != NoError ) {
                            PrintStdError( "Get_OK_Devices()",
                                           "CloseDeviceUSB() failed" );
                        }
                        currentHandleDevice = NULL;
                    } else {
                        if ( currentHandleDevice == NULL ) {
                            PrintStdError( "Get_OK_Devices()",
                                           "pointer 'currentHandleDevice' is NULL" );
                        } else {
                            currentOpalKellyDevice = OpenOK_device();
                        }

                        if ( responseOpen != NoError ) {
                            PrintStdError( "Get_OK_Devices()",
                                           "OpenDeviceUSB() failed",
                                           0,
                                           responseOpen );
                        }
                    }
                }
                ++countOK_Devices;
            }
        }
    }

    deviceCount = countOK_Devices;

    if ( countOK_Devices <= 0 ) {
        return NotFoundOpallKellyBoard;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Check is Open
*/

bool OpenOK::IsOpen()
{
    return ( m_deviceHandle != NULL );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Performs a RESET of the FPGA internals. This requires that FrontPanel support be present in the FPGA design because the RESET signal
actually comes from the FrontPanel Host Interface.

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::ResetFPGA()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    static unsigned char dataControl[ 3 ] = { 0x00, 0x01, 0x00 };

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb3,
                                                 0x0000,
                                                 0x0000,
                                                 dataControl,
                                                 3,
                                                 m_timeoutUSB );

    if ( responseControl < 0 ) {
        PrintStdError( "ResetFPGA()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );
        return ControlTransferError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
ResetDeviceUSB resets the specified device by sending a RESET down the port it is connected to.

OBS :Causes re-enumeration: After calling ResetDeviceUSB, the device will need to re-enumerate and thusly,
requires you to find the new device and open a new handle. The handle used to call libusb_reset_device will no longer work.
*/

bool OpenOK::ResetDeviceUSB( std::string serial )
{
    char stringWait[ 256 ];

    sprintf( stringWait, "Waiting for %02d milliseconds to open the device", WAIT_RESET_MICROSECONDS / 1000 );

    PrintStdError( "ResetDeviceUSB()",
                   stringWait );

#ifdef QT_CORE_LIB
    SleepUtil::usleep( WAIT_RESET_MICROSECONDS );
#else
    usleep( WAIT_RESET_MICROSECONDS );
#endif

    ErrorCode responseOpen = OpenBySerial( serial );

    if ( responseOpen == NoError ) {
        try {
            if ( m_deviceHandle != NULL ) {
                // Perform a USB port reset to reinitialize a device.
                //
                // The system will attempt to restore the previous configuration and
                // alternate settings after the reset has completed.
                //
                // If the reset fails, the descriptors change,
                // or the previous state cannot be restored,
                // the device will appear to be disconnected and reconnected.
                // This means that the device handle is no longer valid (you should close it) and
                // rediscover the device. A return code of LIBUSB_ERROR_NOT_FOUND indicates when
                // this is the case.
                //
                // This is a blocking function which usually incurs a noticeable delay.
                //
                // libusb_reset_device return :
                // 0 on success
                // LIBUSB_ERROR_NOT_FOUND if re-enumeration is required, or if the device has been disconnected
                // another LIBUSB_ERROR code on other failure
                const int responseLibusb = libusb_reset_device( m_deviceHandle );

                if ( responseLibusb != LIBUSB_SUCCESS ) {
                    if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                        Close();
                    }

                    PrintStdError( "ResetDeviceUSB()",
                                   "libusb_reset_device() failed",
                                   responseLibusb );

                    if ( responseLibusb == LIBUSB_ERROR_NOT_FOUND ) {
                        PrintStdError( "ResetDeviceUSB()",
                                       "re-enumeration is required - Closing communication" );
                        Close();
                    }
                } else {
                    PrintStdError( "ResetDeviceUSB()",
                                   stringWait );

                    // wait re-enumeration
#ifdef QT_CORE_LIB
                    SleepUtil::usleep( WAIT_RESET_MICROSECONDS );
#else
                    usleep( WAIT_RESET_MICROSECONDS );
#endif
                    responseOpen = OpenBySerial( serial );

                    if ( responseOpen == NoError ) {
                        PrintStdError( "ResetDeviceUSB()",
                                       "reset successfully!" );

                        return true;
                    } else {
                        PrintStdError( "ResetDeviceUSB()",
                                       "OpenBySerial() failed",
                                       0,
                                       responseOpen );
                    }
                }
            } else {
                PrintStdError( "ResetDeviceUSB()",
                               "pointer 'm_deviceHandle' is NULL" );
            }
        } catch(...) {
            PrintStdError( "ResetDeviceUSB()",
                           "catch() - Closing communication" );

            Close( true );
        }
    } else {
        PrintStdError( "ResetDeviceUSB()",
                       "OpenBySerial() failed",
                       0,
                       responseOpen );
    }
    return false;
}
//---------------------------------------------------------------------------------------------------------------------------------

inline int OpenOK::OptionalControlTransfer( libusb_device_handle *dev_handle,
                                            uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                            unsigned char *data, uint16_t wLength, unsigned int timeout )
{
    int responseLibusb = 0;

    if ( dev_handle == NULL ) {
        PrintStdError( "OptionalControlTransfer()",
                       "pointer 'dev_handle' is NULL" );

        return PointerNULL;
    }

    if ( data == NULL ) {
        PrintStdError( "OptionalControlTransfer()",
                       "pointer 'data' is NULL" );

        return PointerNULL;
    }

    try {
        // libusb_control_transfer return :
        // on success, the number of bytes actually transferred
        // LIBUSB_ERROR_TIMEOUT if the transfer timed out
        // LIBUSB_ERROR_PIPE if the control request was not supported by the device
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // another LIBUSB_ERROR code on other failures
        responseLibusb = libusb_control_transfer( dev_handle,
                                                  request_type, bRequest, wValue, wIndex,
                                                  data, wLength, timeout );


        if ( responseLibusb < 0 ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                PrintStdError( "OptionalControlTransfer()",
                               "libusb_control_transfer() failed - Closing communication",
                               responseLibusb );

                Close();
            } else {
                if ( responseLibusb == LIBUSB_ERROR_PIPE ) {
                    PrintStdError( "OptionalControlTransfer()",
                                   "libusb_control_transfer() failed - control request was not supported by the device",
                                   responseLibusb );
                } else {
                    PrintStdError( "OptionalControlTransfer()",
                                   "libusb_control_transfer() failed",
                                   responseLibusb );
                }
            }
            return ( ControlTransferError + responseLibusb );
        } else if ( data == NULL ) {
            PrintStdError( "OptionalControlTransfer()",
                           "libusb_control_transfer failed - pointer 'data' is NULL" );

            return PointerNULL;
        } else if ( responseLibusb < wLength ) {
            PrintStdError( "OptionalControlTransfer()",
                           "libusb_control_transfer() failed - lost data" );

            return ControlTransferError;
        } else if ( responseLibusb > wLength ) {
            PrintStdError( "OptionalControlTransfer()",
                           "libusb_control_transfer() failed - overflow data" );

            return ControlTransferError;
        }
    } catch(...) {
        PrintStdError( "OptionalControlTransfer()",
                       "catch() - Closing communication!" );

        Close( true );

        return CatchError;
    }

    if ( responseLibusb < 0 ) {
        return LibusbError;
    }
    return responseLibusb;
}
//---------------------------------------------------------------------------------------------------------------------------------

inline int OpenOK::OptionalBulkTransfer( libusb_device_handle *dev_handle,
                                         unsigned char endpoint, unsigned char *data, int length,
                                         int *actual_length, unsigned int timeout )
{
    int responseLibusb = 0;

    if ( dev_handle == NULL ) {
        PrintStdError( "OptionalBulkTransfer()",
                       "pointer 'dev_handle' is NULL" );

        return PointerNULL;
    }

    if ( data == NULL ) {
        PrintStdError( "OptionalBulkTransfer()",
                       "pointer 'data' is NULL" );

        return PointerNULL;
    }

    if ( actual_length == NULL ) {
        PrintStdError( "OptionalBulkTransfer()",
                       "pointer 'actual_length' is NULL" );

        return PointerNULL;
    }

    try {
        // The direction of the transfer is inferred from the direction bits of the endpoint address.
        //
        // For bulk reads, the length field indicates the maximum length of data you are expecting to receive.
        // If less data arrives than expected, this function will return that data,
        // so be sure to check the transferred output parameter.
        //
        // You should also check the transferred parameter for bulk writes. Not all of the data may have been written.
        //
        // Also check transferred when dealing with a timeout error code.
        // libusb may have to split your transfer into a number of chunks to satisfy underlying O/S requirements,
        // meaning that the timeout may expire after the first few chunks have completed.
        // libusb is careful not to lose any data that may have been transferred;
        // do not assume that timeout conditions indicate a complete lack of I/O.
        //
        // Bulk/interrupt transfer overflows
        //
        // When requesting data on a bulk endpoint, libusb requires you to supply a buffer and the maximum number of
        // bytes of data that libusb can put in that buffer. However, the size of the buffer is not communicated to
        // the device - the device is just asked to send any amount of data.
        //
        // There is no problem if the device sends an amount of data that is less than or equal to the buffer size.
        // libusb reports this condition to you through the libusb_transfer.actual_length field.
        //
        // Problems may occur if the device attempts to send more data than can fit in the buffer.
        // libusb reports LIBUSB_TRANSFER_OVERFLOW for this condition but other behaviour is largely undefined:
        // actual_length may or may not be accurate, the chunk of data that can fit in the buffer (before overflow)
        // may or may not have been transferred.
        //
        // Overflows are nasty, but can be avoided. Even though you were told to ignore packets above,
        // think about the lower level details: each transfer is split into packets (typically small,
        // with a maximum size of 512 bytes). Overflows can only happen if the final packet in an incoming data transfer is
        // smaller than the actual packet that the device wants to transfer. Therefore, you will never see an overflow if
        // your transfer buffer size is a multiple of the endpoint's packet size: the final packet will either fill up
        // completely or will be only partially filled.
        //
        // libusb_bulk_transfer return :
        // 0 on success (and populates transferred)
        // LIBUSB_ERROR_TIMEOUT if the transfer timed out (and populates transferred)
        // LIBUSB_ERROR_PIPE if the endpoint halted
        // LIBUSB_ERROR_OVERFLOW if the device offered more data, see Packets and overflows
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // another LIBUSB_ERROR code on other failures
        responseLibusb = libusb_bulk_transfer( dev_handle,
                                               endpoint, data, length,
                                               actual_length, timeout );

        if ( responseLibusb != LIBUSB_SUCCESS ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                PrintStdError( "OptionalBulkTransfer()",
                               "libusb_bulk_transfer() failed - Closing communication",
                               responseLibusb );

                Close();
            } else {
                if ( responseLibusb == LIBUSB_ERROR_TIMEOUT ) {
                    PrintStdError( "OptionalBulkTransfer()",
                                   "libusb_bulk_transfer() failed",
                                   responseLibusb );

                    if ( data == NULL ) {
                        PrintStdError( "OptionalBulkTransfer()",
                                       "pointer 'data' is NULL" );

                        return PointerNULL;
                    } else if ( *actual_length == length ) {
                        return NoError;
                    }
                } else if ( responseLibusb == LIBUSB_ERROR_PIPE ) {
                    PrintStdError( "OptionalBulkTransfer()",
                                   "libusb_bulk_transfer() failed - Cleaning the endpoint halted",
                                   responseLibusb );

                    // Clear the halt/stall condition for an endpoint.
                    //
                    // Endpoints with halt status are unable to receive or transmit data until the halt condition is stalled.
                    //
                    // You should cancel all pending transfers before attempting to clear the halt condition.
                    //
                    // This is a blocking function.
                    //
                    // libusb_clear_halt return :
                    // 0 on success
                    // LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
                    // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
                    // another LIBUSB_ERROR code on other failure
                    const int responseClearHalt = libusb_clear_halt( dev_handle, endpoint );

                    if ( responseClearHalt != LIBUSB_SUCCESS ) {
                        if ( responseClearHalt == LIBUSB_ERROR_NO_DEVICE ) {
                            PrintStdError( "OptionalBulkTransfer()",
                                           "libusb_clear_halt() failed - Closing communication",
                                           responseClearHalt );

                            Close();
                        } else {
                            PrintStdError( "OptionalBulkTransfer()",
                                           "libusb_clear_halt() failed",
                                           responseClearHalt );
                        }
                    }
                } else {
                    PrintStdError( "OptionalBulkTransfer()",
                                   "libusb_bulk_transfer() failed",
                                   responseLibusb );
                }
            }
            return ( BulkTransferError + responseLibusb );
        } else if ( data == NULL ) {
            PrintStdError( "OptionalBulkTransfer()",
                           "pointer 'data' is NULL" );

            return PointerNULL;
        } else if ( actual_length == NULL ) {
            PrintStdError( "OptionalBulkTransfer()",
                           "pointer 'actual_length' is NULL" );

            return PointerNULL;
        } else if ( *actual_length < length ) {
            PrintStdError( "OptionalBulkTransfer()",
                           "libusb_bulk_transfer() failed - lost data" );

            return BulkTransferError;
        } else if ( *actual_length > length ) {
            PrintStdError( "OptionalBulkTransfer()",
                           "libusb_bulk_transfer() failed - overflow data" );

            return BulkTransferError;
        }
    } catch(...) {
        PrintStdError( "OptionalBulkTransfer()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }

    if ( responseLibusb < 0 ) {
        return LibusbError;
    }
    return responseLibusb;
}
//---------------------------------------------------------------------------------------------------------------------------------

inline int OpenOK::ControlTransfer( libusb_device_handle *dev_handle,
                                    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                    unsigned char *data, uint16_t wLength, unsigned int timeout )
{
    int responseLibusb = 0;

    if ( dev_handle == NULL ) {
        PrintStdError( "ControlTransfer()",
                       "pointer 'dev_handle' is NULL" );

        return PointerNULL;
    }

    if ( data == NULL ) {
        PrintStdError( "ControlTransfer()",
                       "pointer 'data' is NULL" );

        return PointerNULL;
    }

    try {
        // libusb_control_transfer return :
        // on success, the number of bytes actually transferred
        // LIBUSB_ERROR_TIMEOUT if the transfer timed out
        // LIBUSB_ERROR_PIPE if the control request was not supported by the device
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // another LIBUSB_ERROR code on other failures
        responseLibusb = libusb_control_transfer( dev_handle,
                                                  request_type, bRequest, wValue, wIndex,
                                                  data, wLength, timeout );

        if ( responseLibusb < 0 ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                PrintStdError( "ControlTransfer()",
                               "libusb_control_transfer() failed - Closing communication",
                               responseLibusb );

                Close();
            } else {
                if ( responseLibusb == LIBUSB_ERROR_PIPE ) {
                    PrintStdError( "ControlTransfer()",
                                   "libusb_control_transfer() failed - control request was not supported by the device",
                                   responseLibusb );
                } else {
                    PrintStdError( "ControlTransfer()",
                                   "libusb_control_transfer() failed",
                                   responseLibusb );
                }

                ErrorCode error = CheckFrontPanelEnabled();

                if ( error != NoError ) {
                    if ( error == FrontPanelDisabled ) {
                        PrintStdError( "ControlTransfer()",
                                       "FrontPanel disabled - Closing communication",
                                       0,
                                       error );

                        Close();
                    } else if ( error == NotPossibleCheckFrontPanel ) {
                        PrintStdError( "ControlTransfer()",
                                       "Is not possible check if the FrontPanel is enabled",
                                       0,
                                       error );
                    } else {
                        PrintStdError( "ControlTransfer()",
                                       "CheckFrontPanelEnabled() failed",
                                       0,
                                       error );
                    }
                }
            }
            return ( ControlTransferError + responseLibusb );
        } else if ( data == NULL ) {
            PrintStdError( "ControlTransfer()",
                           "libusb_control_transfer failed - pointer 'data' is NULL" );

            return PointerNULL;
        } else if ( responseLibusb < wLength ) {
            PrintStdError( "ControlTransfer()",
                           "libusb_control_transfer() failed - lost data" );

            return ControlTransferError;
        } else if ( responseLibusb > wLength ) {
            PrintStdError( "ControlTransfer()",
                           "libusb_control_transfer() failed - overflow data" );

            return ControlTransferError;
        }
    } catch(...) {
        PrintStdError( "ControlTransfer()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }

    if ( responseLibusb < 0 ) {
        return LibusbError;
    }
    return responseLibusb;
}
//---------------------------------------------------------------------------------------------------------------------------------

inline int OpenOK::BulkTransfer( libusb_device_handle *dev_handle,
                                 unsigned char endpoint, unsigned char *data, int length,
                                 int *actual_length, unsigned int timeout )
{
    int responseLibusb = 0;

    if ( dev_handle == NULL ) {
        PrintStdError( "BulkTransfer()",
                       "pointer 'dev_handle' is NULL" );

        return PointerNULL;
    }

    if ( data == NULL ) {
        PrintStdError( "BulkTransfer()",
                       "pointer 'data' is NULL" );

        return PointerNULL;
    }

    if ( actual_length == NULL ) {
        PrintStdError( "BulkTransfer()",
                       "pointer 'actual_length' is NULL" );

        return PointerNULL;
    }

    try {
        // The direction of the transfer is inferred from the direction bits of the endpoint address.
        //
        // For bulk reads, the length field indicates the maximum length of data you are expecting to receive.
        // If less data arrives than expected, this function will return that data,
        // so be sure to check the transferred output parameter.
        //
        // You should also check the transferred parameter for bulk writes. Not all of the data may have been written.
        //
        // Also check transferred when dealing with a timeout error code.
        // libusb may have to split your transfer into a number of chunks to satisfy underlying O/S requirements,
        // meaning that the timeout may expire after the first few chunks have completed.
        // libusb is careful not to lose any data that may have been transferred;
        // do not assume that timeout conditions indicate a complete lack of I/O.
        //
        // Bulk/interrupt transfer overflows
        //
        // When requesting data on a bulk endpoint, libusb requires you to supply a buffer and the maximum number of
        // bytes of data that libusb can put in that buffer. However, the size of the buffer is not communicated to
        // the device - the device is just asked to send any amount of data.
        //
        // There is no problem if the device sends an amount of data that is less than or equal to the buffer size.
        // libusb reports this condition to you through the libusb_transfer.actual_length field.
        //
        // Problems may occur if the device attempts to send more data than can fit in the buffer.
        // libusb reports LIBUSB_TRANSFER_OVERFLOW for this condition but other behaviour is largely undefined:
        // actual_length may or may not be accurate, the chunk of data that can fit in the buffer (before overflow)
        // may or may not have been transferred.
        //
        // Overflows are nasty, but can be avoided. Even though you were told to ignore packets above,
        // think about the lower level details: each transfer is split into packets (typically small,
        // with a maximum size of 512 bytes). Overflows can only happen if the final packet in an incoming data transfer is
        // smaller than the actual packet that the device wants to transfer. Therefore, you will never see an overflow if
        // your transfer buffer size is a multiple of the endpoint's packet size: the final packet will either fill up
        // completely or will be only partially filled.
        //
        // libusb_bulk_transfer return :
        // 0 on success (and populates transferred)
        // LIBUSB_ERROR_TIMEOUT if the transfer timed out (and populates transferred)
        // LIBUSB_ERROR_PIPE if the endpoint halted
        // LIBUSB_ERROR_OVERFLOW if the device offered more data, see Packets and overflows
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // another LIBUSB_ERROR code on other failures
        responseLibusb = libusb_bulk_transfer( dev_handle,
                                               endpoint, data, length,
                                               actual_length, timeout );

        if ( responseLibusb != LIBUSB_SUCCESS ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                PrintStdError( "BulkTransfer()",
                               "libusb_bulk_transfer() failed - Closing communication",
                               responseLibusb );

                Close();
            } else {
                if ( responseLibusb == LIBUSB_ERROR_TIMEOUT ) {
                    PrintStdError( "BulkTransfer()",
                                   "libusb_bulk_transfer() failed",
                                   responseLibusb );

                    if ( data == NULL ) {
                        PrintStdError( "BulkTransfer()",
                                       "pointer 'data' is NULL" );

                        return PointerNULL;
                    } else if ( *actual_length == length ) {
                        return NoError;
                    }
                } else if ( responseLibusb == LIBUSB_ERROR_PIPE ) {
                    PrintStdError( "BulkTransfer()",
                                   "libusb_bulk_transfer() failed - Cleaning the endpoint halted",
                                   responseLibusb );

                    // Clear the halt/stall condition for an endpoint.
                    //
                    // Endpoints with halt status are unable to receive or transmit data until the halt condition is stalled.
                    //
                    // You should cancel all pending transfers before attempting to clear the halt condition.
                    //
                    // This is a blocking function.
                    //
                    // libusb_clear_halt return :
                    // 0 on success
                    // LIBUSB_ERROR_NOT_FOUND if the endpoint does not exist
                    // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
                    // another LIBUSB_ERROR code on other failure
                    const int responseClearHalt = libusb_clear_halt( dev_handle, endpoint );

                    if ( responseClearHalt != LIBUSB_SUCCESS ) {
                        if ( responseClearHalt == LIBUSB_ERROR_NO_DEVICE ) {
                            PrintStdError( "BulkTransfer()",
                                           "libusb_clear_halt() failed - Closing communication",
                                           responseClearHalt );

                            Close();
                        } else {
                            PrintStdError( "BulkTransfer()",
                                           "libusb_clear_halt() failed",
                                           responseClearHalt );
                        }
                    }
                } else {
                    PrintStdError( "BulkTransfer()",
                                   "libusb_bulk_transfer() failed",
                                   responseLibusb );
                }

                ErrorCode error = CheckFrontPanelEnabled();

                if ( error == FrontPanelDisabled ) {
                    PrintStdError( "BulkTransfer()",
                                   "FrontPanel disabled! Closing communication",
                                   0,
                                   error );

                    Close();
                } else if ( error == NotPossibleCheckFrontPanel ) {
                    PrintStdError( "BulkTransfer()",
                                   "Is not possible check if the FrontPanel is enabled",
                                   0,
                                   error );
                } else {
                    PrintStdError( "BulkTransfer()",
                                   "CheckFrontPanelEnabled() failed",
                                   0,
                                   error );
                }
            }
            return ( BulkTransferError + responseLibusb );
        } else if ( data == NULL ) {
            PrintStdError( "BulkTransfer()",
                           "pointer 'data' is NULL" );

            return PointerNULL;
        } else if ( actual_length == NULL ) {
            PrintStdError( "BulkTransfer()",
                           "pointer 'actual_length' is NULL" );

            return PointerNULL;
        } else if ( *actual_length < length ) {
            PrintStdError( "BulkTransfer()",
                           "libusb_bulk_transfer() failed - lost data",
                           responseLibusb );

            return BulkTransferError;
        } else if ( *actual_length > length ) {
            PrintStdError( "BulkTransfer()",
                           "libusb_bulk_transfer() failed - overflow data",
                           responseLibusb );

            return BulkTransferError;
        }
    } catch(...) {
        PrintStdError( "BulkTransfer()",
                       "catch() - Closing communication" );

        Close( true );

        return CatchError;
    }

    if ( responseLibusb < 0 ) {
        return LibusbError;
    }
    return responseLibusb;
}
//---------------------------------------------------------------------------------------------------------------------------------

// Don't use ControlTransfer here, only OptionalControlTransfer.
OpenOK::ErrorCode OpenOK::ControlStatus( unsigned int mode )
{
    unsigned char dataControl[ 1 ] = { 0x00 };

    int responseControl;

    if ( mode == controlReadMode ) {
        responseControl = OptionalControlTransfer( m_deviceHandle,
                                                   mode,
                                                   0xb2,
                                                   0x0000,
                                                   0x0000,
                                                   dataControl,
                                                   1,
                                                   m_timeoutUSB );

        if ( responseControl == 1 ) {
            // check for successful status
            if ( dataControl[ 0 ] == 0x01 ) {
                return NoError;
            } else {
                PrintStdError( "ControlStatus()",
                               "controlReadMode - invalid response" );

                return ControlStatusInvalidResponse;
            }
        } else if ( responseControl < 0 ) {
            PrintStdError( "ControlStatus()",
                           "controlReadMode - OptionalControlTransfer() failed",
                           0,
                           responseControl );
        } else if ( responseControl < 1 ) {
            PrintStdError( "ControlStatus()",
                           "controlReadMode - OptionalControlTransfer() failed - lost data" );
        } else if ( responseControl > 1 ) {
            PrintStdError( "ControlStatus()",
                           "controlReadMode - OptionalControlTransfer() failed - overflow data" );
        }
    } else if ( mode == controlWriteMode ) {
        responseControl = OptionalControlTransfer( m_deviceHandle,
                                                   mode,
                                                   0xb2,
                                                   0x0000,
                                                   0x0000,
                                                   dataControl,
                                                   0,
                                                   m_timeoutUSB );

        if ( responseControl == 0 ) {
            return NoError;
        } else if ( responseControl < 0 ) {
            PrintStdError( "ControlStatus()",
                           "controlWriteMode - OptionalControlTransfer() failed",
                           0,
                           responseControl );
        }/* else if ( responseControl < 0 ) {
            PrintStdError( "ControlStatus()",
                           "controlWriteMode - OptionalControlTransfer() failed - lost data" );
        } */else if ( responseControl > 0 ) {
            PrintStdError( "ControlStatus()",
                           "controlWriteMode - OptionalControlTransfer() failed - overflow data" );
        }
    }
    return CommunicationError;
}
//---------------------------------------------------------------------------------------------------------------------------------

// Don't use ControlTransfer here, only OptionalControlTransfer.
OpenOK::ErrorCode OpenOK::CheckEnable()
{
    unsigned char dataControl[ 2 ] = { 0x00, 0x00 };

    const int responseControl = OptionalControlTransfer( m_deviceHandle,
                                                         controlReadMode,
                                                         0xb3,
                                                         0x0000,
                                                         0x0000,
                                                         dataControl,
                                                         2,
                                                         m_timeoutUSB );

    if ( responseControl == 2 ) {
        // check for successful status
        if ( ( dataControl[ 0 ] == 0xd7 ) &&
             ( dataControl[ 1 ] == 0xa5 ) ) {
            return NoError;
        } else {
            PrintStdError( "CheckEnable()",
                           "invalid response" );

            return CheckEnableInvalidResponse;
        }
    } else if ( responseControl < 0 ) {
        PrintStdError( "CheckEnable()",
                       "OptionalControlTransfer() failed",
                       0,
                       responseControl );
    } else if ( responseControl > 2 ) {
        PrintStdError( "CheckEnable()",
                       "OptionalControlTransfer() failed - overflow data" );
    } else if ( responseControl < 2 ) {
        PrintStdError( "CheckEnable()",
                       "OptionalControlTransfer() failed - lost data" );
    }
    return CommunicationError;
}
//---------------------------------------------------------------------------------------------------------------------------------

// Don't use ControlTransfer here, only OptionalControlTransfer.
OpenOK::ErrorCode OpenOK::CheckDisable()
{
    unsigned char dataControl[ 1 ] = { 0x00 };

    const int responseControl = OptionalControlTransfer( m_deviceHandle,
                                                         controlReadMode,
                                                         0xb8,
                                                         0x0000,
                                                         0x0000,
                                                         dataControl,
                                                         1,
                                                         m_timeoutUSB );

    if ( responseControl == 1 ) {
        // check for successful status
        if ( dataControl[ 0 ] == 0x00 ) {
            return NoError;
        } else {
            PrintStdError( "CheckDisable()",
                           "invalid response" );

            return CheckDisableInvalidResponse;
        }
    } else if ( responseControl < 0 ) {
        PrintStdError( "CheckDisable()",
                       "OptionalControlTransfer() failed",
                       0 ,
                       responseControl );
    } else if ( responseControl < 1 ) {
        PrintStdError( "CheckDisable()",
                       "OptionalControlTransfer() failed - lost data" );
    } else if ( responseControl > 1 ) {
        PrintStdError( "CheckDisable()",
                       "OptionalControlTransfer() failed - overflow data" );
    }
    return CommunicationError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method checks to see if the FrontPanel Host Interface has been instantiated in the FPGA design.
If it is detected, FrontPanel support is enabled and endpoint functionality is available.

Returns:
True is FrontPanel support is present, false otherwise.
*/

bool OpenOK::IsFrontPanelEnabled()
{
    return ( CheckFrontPanelEnabled() == NoError );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Check if the FrontPanel is enabled.

Return:
ErroCode
*/

OpenOK::ErrorCode OpenOK::CheckFrontPanelEnabled()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    ErrorCode error = ControlStatus( controlReadMode );

    if ( error == NoError ) {
        error = CheckEnable();

        if ( error != NoError ) {
            if ( error == CheckEnableInvalidResponse ) {
                return FrontPanelDisabled;
            } else if ( error == CommunicationError ) {
                return NotPossibleCheckFrontPanel;
            } else {
                PrintStdError( "CheckFrontPanelEnabled()",
                               "CheckEnable() failed - Unknown error",
                               0,
                               error );
            }
        }
    } else if ( error == CommunicationError ) {
        return NotPossibleCheckFrontPanel;
    } else if ( error == ControlStatusInvalidResponse ) {
        return FrontPanelDisabled;
    } else {
        PrintStdError( "CheckFrontPanelEnabled()",
                       "ControlStatus() failed - Unknown error",
                       0,
                       error );
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Release an interface previously claimed with libusb_claim_interface().

Return:
ErroCode
*/

OpenOK::ErrorCode OpenOK::ReleaseInterface()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    try {
        int responseLibusb = LIBUSB_SUCCESS;

        // Release an interface previously claimed with libusb_claim_interface().
        // You should release all claimed interfaces before closing a device handle.
        // libusb_release_interface return LIBUSB_ERROR_NOT_FOUND if the interface was not claimed
        //
        // libusb_release_interface return :
        // 0 on success
        // LIBUSB_ERROR_NOT_FOUND if the interface was not claimed
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // another LIBUSB_ERROR code on other failure
        responseLibusb = libusb_release_interface( m_deviceHandle, m_interfaceDesired );

        if ( responseLibusb != LIBUSB_SUCCESS ) {
            // if the interface was not claimed
            if ( responseLibusb != LIBUSB_ERROR_NOT_FOUND ) {
                PrintStdError( "ReleaseInterface()",
                               "libusb_release_interface() failed",
                               responseLibusb );
            }
        }
#ifdef  __linux
        // libusb_kernel_driver_active return :
        // 0 if no kernel driver is active
        // 1 if a kernel driver is active
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality is not available
        // another LIBUSB_ERROR code on other failure
        if ( ( responseLibusb = libusb_kernel_driver_active( m_deviceHandle, m_interfaceDesired ) ) == 1 ) {
            // Detach a kernel driver from an interface.
            // If successful, you will then be able to claim the interface and perform I/O.
            // This functionality is not available on Darwin or Windows.
            // Note that libusb itself also talks to the device through a special kernel driver,
            // if this driver is already attached to the device, this call will not detach it and return LIBUSB_ERROR_NOT_FOUND.
            // Detach a kernel driver from interface 0

            // libusb_detach_kernel_driver return :
            // 0 on success
            // LIBUSB_ERROR_NOT_FOUND if no kernel driver was active
            // LIBUSB_ERROR_INVALID_PARAM if the interface does not exist
            // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
            // LIBUSB_ERROR_NOT_SUPPORTED on platforms where the functionality is not available
            // another LIBUSB_ERROR code on other failure
            if ( ( responseLibusb = libusb_detach_kernel_driver( m_deviceHandle, m_interfaceDesired ) ) != LIBUSB_SUCCESS ) {
                PrintStdError( "ReleaseInterface()",
                               "libusb_detach_kernel_driver() failed",
                               responseLibusb );
            }
        } else {
            if ( responseLibusb < 0 ) {
                PrintStdError( "ReleaseInterface()",
                               "libusb_kernel_driver_active() failed",
                               responseLibusb );
            }
        }
#endif

        if ( responseLibusb < 0 ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                return DeviceNotOpen;
            }
            return LibusbError;
        }
    } catch(...) {
        PrintStdError( "ReleaseInterface()",
                       "catch()" );

        return CatchError;
    }
    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Before any communication with the device can proceed, the device must be opened using this method.
A Close() method is not provided and is not necessary.
A device is opened matching the given device ID string
If no device ID (or an empty string) is provided, then the first appropriate device is opened.

Python and Java Note: To open the first available device, you must provide an empty string.

Parameters:
[in] 	str 	The device ID of the device to open.

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::OpenByDeviceID( std::string str )
{
    Close();

    bool device_found = false;

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( false, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "OpenByDeviceID()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return responseGetOKDevices;
    }

    if ( numDevices > 0 ) {
        if ( str == "" ) {
            m_indexOpenedDevice = 0;
            device_found = true;
        } else {
            for ( m_indexOpenedDevice = 0; m_indexOpenedDevice < numDevices; ++m_indexOpenedDevice ) {
                if ( str == m_listOKDevices[ m_indexOpenedDevice ].deviceID ) {
                    device_found = true;
                    break;
                }
            }
        }
    }

    if ( !device_found ) {
        m_indexOpenedDevice = 0;
        return NotFoundDeviceStringID;
    }
    return OpenByDeviceIndexList( m_indexOpenedDevice );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Before any communication with the device can proceed, the device must be opened using this method.
A Close() method is not provided and is not necessary.
A device is opened matching the given serial number string.
If no serial number (or an empty string) is provided, then the first appropriate device is opened.

Python and Java Note: To open the first available device, you must provide an empty string.

Parameters:
[in] 	str 	The serial number of the device to open.

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::OpenBySerial( std::string str )
{
    Close();

    bool device_found = false;

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( false, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "OpenBySerial()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return responseGetOKDevices;
    }

    if ( numDevices > 0 ) {
        if ( str == "" ) {
            m_indexOpenedDevice = 0;
            device_found = true;
        } else {
            for ( m_indexOpenedDevice = 0; m_indexOpenedDevice < numDevices; ++m_indexOpenedDevice ) {
                if ( str == m_listOKDevices[ m_indexOpenedDevice ].serial ) {
                    device_found = true;
                    break;
                }
            }
        }
    }

    if ( !device_found ) {
        m_indexOpenedDevice = 0;
        return NotFoundDeviceStringSerial;
    }
    return OpenByDeviceIndexList( m_indexOpenedDevice );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Before any communication with the device can proceed, the device must be opened using this method.
A Close() method is not provided and is not necessary.

Parameters:
[in] 	indexDeviceList 	The position of list devices to open.

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::OpenByDeviceIndexList( unsigned int indexDeviceList )
{
    if ( !m_libusbInitialization ) {
        return LibusbNotInitialization;
    }

    BoardModel boardModel;

    std::string boardName;

    int responseLibusb = LIBUSB_SUCCESS;
    int responseLibusbSetConfiguration = LIBUSB_SUCCESS;
    int responseLibusbGetConfiguration = LIBUSB_SUCCESS;

    Close();

    ErrorCode responseOpen = OpenDeviceUSB( m_listOKDevices[ indexDeviceList ].device,
                                            &m_deviceHandle );

    if ( responseOpen != NoError ) {
        PrintStdError( "OpenByDeviceIndexList()",
                       "OpenDeviceUSB() failed",
                       0,
                       responseOpen );

        return DeviceNotOpen;
    }

    OpenOK::ErrorCode error = NoError;

    // need to send setup packet on endpoint 0x00
    // packet is 0x00 09 01 00 00 00 00 00
    // libusb provides a convenient function usb_set_configuration for this

    int configurationRead = 0;

    ReleaseInterface();

    // libusb_get_configuration retun :
    // 0 on success
    // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
    // another LIBUSB_ERROR code on other failure
    responseLibusbGetConfiguration = libusb_get_configuration( m_deviceHandle, &configurationRead );

    if ( responseLibusbGetConfiguration == LIBUSB_SUCCESS ) {
        // When libusb presents a device handle to an application, there is a chance that the corresponding
        // device may be in unconfigured state. For devices with multiple configurations, there is also a chance
        // that the configuration currently selected is not the one that the application wants to use.
        //
        // The obvious solution is to add a call to libusb_set_configuration() early on during your device
        // initialization routines, but there are caveats to be aware of:
        //
        // 1) If the device is already in the desired configuration, calling libusb_set_configuration()
        // using the same configuration value will cause a lightweight device reset.
        // This may not be desirable behaviour.
        // 2) libusb will be unable to change configuration if the device is in another configuration
        // and other programs or drivers have claimed interfaces under that configuration.
        // 3) In the case where the desired configuration is already active, libusb may not even be able to
        // perform a lightweight device reset. For example, take my USB keyboard with fingerprint reader:
        // I'm interested in driving the fingerprint reader interface through libusb,
        // but the kernel's USB-HID driver will almost always have claimed the keyboard interface.
        // Because the kernel has claimed an interface, it is not even possible to perform the lightweight device reset,
        // so libusb_set_configuration() will fail. (Luckily the device in question only has a single configuration.)
        if ( configurationRead != m_configurationDesired ) {
            // libusb_set_configuration return :
            // 0 on success
            // LIBUSB_ERROR_NOT_FOUND if the requested configuration does not exist
            // LIBUSB_ERROR_BUSY if interfaces are currently claimed
            // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
            // another LIBUSB_ERROR code on other failure
            responseLibusbSetConfiguration = libusb_set_configuration( m_deviceHandle, m_configurationDesired );

            // TODO : remove BUSY test
            if ( responseLibusbSetConfiguration == LIBUSB_ERROR_BUSY ) {
                responseLibusbSetConfiguration = LIBUSB_SUCCESS;
            }
        }
    }

    if ( ( responseLibusbSetConfiguration == LIBUSB_SUCCESS ) &&
         ( responseLibusbGetConfiguration == LIBUSB_SUCCESS ) ) {

        // claim interface 0
        //
        // Claiming of interfaces is a purely logical operation; it does not cause any requests to be sent over the bus.
        // Interface claiming is used to instruct the underlying operating system that your application wishes to
        // take ownership of the interface.
        //
        // Claim = One possible way to lock your device into a specific configuration,
        //
        // You cannot change/reset configuration if other applications or drivers have claimed interfaces.
        //
        // libusb_claim_interface return :
        // 0 on success
        // LIBUSB_ERROR_NOT_FOUND if the requested interface does not exist
        // LIBUSB_ERROR_BUSY if another program or driver has claimed the interface
        // LIBUSB_ERROR_NO_DEVICE if the device has been disconnected
        // a LIBUSB_ERROR code on other failure
        responseLibusb = libusb_claim_interface(m_deviceHandle, m_interfaceDesired );

        // TODO : remove BUSY test
        if ( responseLibusb == LIBUSB_ERROR_BUSY ) {
            responseLibusb = LIBUSB_SUCCESS;
        }

        if ( responseLibusb == LIBUSB_SUCCESS ) {

            // need to send USB_CONTROL packet, 0xc0:b9 00 00 00 00 01 00
            // this is copied from a capture of traffic when using the official interface library
            // don't know what this request is, but it returns a byte of 0x80 when called by real library
            // request_type = 0xc0 = USB_DIR_HOST & USB_TYPE_VENDOR

            unsigned char dataControl[ 1 ] = { 0x00 };

            // should read 1 byte
            const int responseControl = ControlTransfer( m_deviceHandle,
                                                         controlReadMode,
                                                         0xb9,
                                                         0x0000,
                                                         0x0000,
                                                         dataControl,
                                                         1,
                                                         m_timeoutUSB );

            if ( responseControl >= 0 ) {
                if ( responseControl == 1 ) {
                    if ( ( dataControl[ 0 ] == 0x80 ) ||
                         ( dataControl[ 0 ] == 0x00 ) ) {
                        error = NoError;
                    } else {
                        //device did not return 0x00 or 0x80

                        PrintStdError( "OpenByDeviceIndexList()",
                                       "invalid response" );

                        error = OpenByDeviceIndexListInvalidResponse;
                    }
                } else {
                    if ( responseControl < 1 ) {
                        PrintStdError( "OptionalControlTransfer()",
                                       "libusb_control_transfer() failed - lost data" );
                    } else if ( responseControl > 1 ) {
                        PrintStdError( "OptionalControlTransfer()",
                                       "libusb_control_transfer() failed - overflow data" );
                    }
                    error = ControlTransferError;
                }
            } else {
                PrintStdError( "OpenByDeviceIndexList()",
                               "ControlTransfer() failed",
                               0,
                               responseControl );

                error = ControlTransferError;
            }
        } else {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                Close();
            }

            PrintStdError( "OpenByDeviceIndexList()",
                           "libusb_claim_interface() failed",
                           responseLibusb );

            error = ClaimInterfaceError;
        }
    } else {
        if ( responseLibusbGetConfiguration != LIBUSB_SUCCESS ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                Close();
            }

            PrintStdError( "OpenByDeviceIndexList()",
                           "libusb_get_configuration() failed",
                           responseLibusbGetConfiguration );

            error = GetConfigurationError;
        }
        if ( responseLibusbSetConfiguration != LIBUSB_SUCCESS ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                Close();
            }

            PrintStdError( "OpenByDeviceIndexList()",
                           "libusb_set_configuration() failed",
                           responseLibusbSetConfiguration );

            error = SetConfigurationError;
        }
    }

    if ( error == NoError ) {
        boardModel = GetBoardModel();

        boardName = GetBoardModelString( boardModel );

        if ( boardName == "XEM3005" ) {
            m_crystalReference = 48.0;//Mhz
        }

        m_shiftMaxPacketSize = m_listOKDevices[ indexDeviceList ].shiftMaxPacketSize;

        m_maxPacketSize = m_listOKDevices[ indexDeviceList ].maxPacketSize;
    }

    if ( error != NoError ) {
        Close();
    }
    return error;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Close device opened
*/

void OpenOK::Close( bool force )
{
    if ( !IsOpen() ) {
        return;
    }

    memset( m_wireIns, 0, OpenOK::WIREINSIZE );
    memset( m_wireOuts, 0, OpenOK::WIREOUTSIZE );

    if ( force ) {
        PrintStdError( "Close()",
                       "Forcing close communication!" );
    } else {
        const ErrorCode responseRelease = ReleaseInterface();

        if ( responseRelease != NoError ) {
            PrintStdError( "Close() - ReleaseInterface()",
                           "",
                           0,
                           responseRelease );
        }

        const ErrorCode responseCloseDevice = CloseDeviceUSB( m_deviceHandle );

        if ( responseCloseDevice != NoError ) {
            PrintStdError( "Close() - CloseDeviceUSB()",
                           "",
                           0,
                           responseCloseDevice );
        }
    }
    m_deviceHandle = NULL;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Configures the on-board PLL using the default configuration setup in the EEPROM. If the specific device does not support this,
the method returns error code UnsupportedFeature.

Returns:
ErrorCode
*/

OpenOK::ErrorCode OpenOK::LoadDefaultPLLConfiguration()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    OpenOK_CPLL22150 *pllDefaultCofiguration = new OpenOK_CPLL22150;

    ErrorCode result = NoError;

    result = GetEepromPLL22150Configuration(*pllDefaultCofiguration);

    if ( result == NoError ) {
        result = SetPLL22150Configuration(*pllDefaultCofiguration);
    }

    delete pllDefaultCofiguration;

    return result;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Queries the USB to determine how many Opal Kelly devices are attached. This method also builds a list of serial numbers and
board models which can subsequently be queried using the methods GetDeviceListSerial() and GetDeviceListModel(), respectively.

Returns:
The number of devices attached.
*/

int OpenOK::GetDeviceCount()
{
    Close();

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( true, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "GetDeviceCount()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return responseGetOKDevices;
    }
    return numDevices;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Returns:
Descriptor language of device
*/

int OpenOK::GetLanguageID( libusb_device_handle *handle )
{
    unsigned char strLangID[ 255 ];

    if ( handle == NULL ) {
        PrintStdError( "GetLanguageID()",
                       "pointer 'handle' is NULL" );
        return PointerNULL;
    }

    try {
        // Read Language ID
        //
        // libusb_get_string_descriptor return :number of bytes returned in data, or LIBUSB_ERROR code on failure
        int responseLibusb = libusb_get_string_descriptor( handle,
                                                           0x00,
                                                           0x00,
                                                           strLangID,
                                                           sizeof( strLangID ) );

        if ( responseLibusb < 0 ) {
            if ( responseLibusb == LIBUSB_ERROR_NO_DEVICE ) {
                Close();
            }

            PrintStdError( "GetLanguageID()",
                           "libusb_get_string_descriptor() failed",
                           responseLibusb );

            return LibusbError;
        }
    } catch(...) {
        PrintStdError( "GetLanguageID()",
                       "catch()" );

        return CatchError;
    }
    return ( strLangID[ 3 ] << 8 ) + strLangID[ 2 ];
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method returns the serial number of a particular attached device. The device count must have already been queried using
a call to GetDeviceCount(). If num is outside the range of attached devices, an empty string will be returned.
The serial number may then be used to open the device by calling OpenBySerial().

Parameters:
[in] 	num 	Device number to query (0 to count-1)

Returns:
The serial number string of the specified device.
*/

std::string OpenOK::GetDeviceListSerial( int num )
{
    Close();

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( false, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "GetDeviceListSerial()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return "Not Available";
    }

    if ( num < numDevices ) {
        return m_listOKDevices[num].serial;
    }
    return "Not Available";
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get board model

Parameters:
[in] string name

Returns:
BoardModel
*/

OpenOK::BoardModel OpenOK::GetBoardModelNumber( std::string name )
{
    for ( unsigned char i = brdUnknown; i < brdEnd; ++i ) {
        if ( name == OpenOK::GetBoardModelString( static_cast< BoardModel >( i ) ) ) {
            return static_cast< BoardModel >( i );
        }
    }
    return brdUnknown;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method returns the board model of a particular attached device. The device count must have already been queried using
a call to GetDeviceCount(). If a board is unknown or num is outside the range of attached devices,
okCUsbFrontPanel::brdUnknown will be returned.

Parameters:
[in] 	num 	Device number to query (0 to count-1)

Returns:
The board model (enumerated type) of the specified device.
*/

OpenOK::BoardModel OpenOK::GetDeviceListModel( int num )
{
    Close();

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( false, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "GetDeviceListModel()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return brdUnknown;
    }

    if ( num < numDevices ) {
        char* tmp = m_listOKDevices[ num ].product;

        // "Opal Kelly " = 11 characters
        return GetBoardModelNumber( tmp + 11 );
    }
    return brdUnknown;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the Product string from the device.

Returns:
A string containing the Product. If a device is not open, the string will be empty.
*/

std::string OpenOK::GetProduct()
{
    if ( !IsOpen() ) {
        return "Not Available";
    }

    std::string tmp = m_listOKDevices[ m_indexOpenedDevice ].product;

    return tmp;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Returns the specific board model.
*/

OpenOK::BoardModel OpenOK::GetBoardModel()
{
    std::string tmp = GetProduct();

    // "Opal Kelly " = 11 characters
    if ( ( tmp.length() > 11 ) &&
         ( tmp != "Not Available" ) ) {
        tmp = tmp.substr( 11,tmp.length() );

        return GetBoardModelNumber( tmp );
    }
    return brdUnknown;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get board model string
*/

std::string OpenOK::GetBoardModelString( BoardModel m )
{
    if ( m == brdXEM3001v1 ) {
        return "XEM3001v1";
    } else if ( m == brdXEM3001v2 ) {
        return "XEM3001v2";
    } else if ( m == brdXEM3010 ) {
        return "XEM3010";
    } else if ( m == brdXEM3005 ) {
        return "XEM3005";
    } else if ( m == brdXEM3001CL ) {
        return "XEM3001CL";
    } else if ( m == brdXEM3020 ) {
        return "XEM3020";
    } else if ( m == brdXEM3050 ) {
        return "XEM3050";
    } else if ( m == brdXEM9002 ) {
        return "XEM9002";
    } else if ( m == brdXEM3001RB ) {
        return "XEM3001RB";
    } else if ( m == brdXEM5010 ) {
        return "XEM5010";
    } else if ( m == brdXEM6110LX45 ) {
        return "XEM6110LX45";
    } else if ( m == brdXEM6110LX150 ) {
        return "XEM6110LX150";
    } else if ( m == brdXEM6001 ) {
        return "XEM6001";
    } else if ( m == brdXEM6010LX45 ) {
        return "XEM6010LX45";
    } else if ( m == brdXEM6010LX150 ) {
        return "XEM6010LX150";
    } else if ( m == brdXEM6006LX9 ) {
        return "XEM6006LX9";
    } else if ( m == brdXEM6006LX16 ) {
        return "XEM6006LX16";
    } else if ( m == brdXEM6006LX25 ) {
        return "XEM6006LX25";
    } else if ( m == brdXEM5010LX110 ) {
        return "XEM5010LX110";
    } else {
        return "Unknown";
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get serial number

Returns:
string serial
*/

std::string OpenOK::GetSerialNumber()
{
    if ( !IsOpen() ) {
        return "Not Available";
    }
    return m_listOKDevices[ m_indexOpenedDevice ].serial;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the current XEM Device ID string from the device.

Returns:
A string containing the Device ID. If a device is not open, the string will be empty.
*/

std::string OpenOK::GetDeviceID()
{
    if ( !IsOpen() ) {
        return "Not Available";
    }
    return m_listOKDevices[ m_indexOpenedDevice ].deviceID;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Returns:
The firmware's major version number or DeviceNotOpen if device not open.
*/

int OpenOK::GetDeviceMajorVersion()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return ( ( m_listOKDevices[ m_indexOpenedDevice ].bcdUSB >> 8 ) & 255 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Returns:
The firmware's minor version number or -1 if the query failed
*/

int OpenOK::GetDeviceMinorVersion()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return ( m_listOKDevices[ m_indexOpenedDevice ].bcdUSB & 255 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the Manufacturer string from the device.

Returns:
A string containing the Manufacturer. If a device is not open, the string will be empty.
*/

std::string OpenOK::GetManufacturer()
{
    if ( !IsOpen() ) {
        return "Not Available";
    }
    return m_listOKDevices[ m_indexOpenedDevice ].manufacturer;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the Number Endpoints from the device.

Returns:
Number of Endpoints. If a device is not open, return DeviceNotOpen.
*/

int OpenOK::GetNumberEndpoints( unsigned int numberInterface )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return m_listOKDevices[ m_indexOpenedDevice ].bNumEndpoints[ numberInterface ];
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the maximum packet size of endpoint from the device.

Returns:
The maximum packet size of endpoint. If a device is not open, return DeviceNotOpen.
*/

int OpenOK::GetMaxPacketSizeEndpoint( unsigned int numberEndpoint )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return m_listOKDevices[ m_indexOpenedDevice ].wMaxPacketSize[ numberEndpoint ];
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the endpoint address from the device.

Returns:
The endpoint address. If a device is not open, return DeviceNotOpen.
*/

int OpenOK::GetEndpointAddress( unsigned int numberEndpoint )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return m_listOKDevices[ m_indexOpenedDevice ].bEndpointAddress[ numberEndpoint ];
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method retrieves the last length transferred bytes from the device or to pc.

Returns:
The last length transferred bytes. If a device is not open, return DeviceNotOpen.
*/

long OpenOK::GetLastTransferLength()
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return m_lastTransferred;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Parameters:
[out] 	pll 	A reference to an okCPLL22150 object.
Returns:
ErrorCode
This method retrieves the current PLL configuration from the on-board XEM EEPROM.
The pll object is then initialized with this configuration.

ErrorCodes: NoError, DeviceNotOpen
*/

OpenOK::ErrorCode OpenOK::GetEepromPLL22150Configuration( OpenOK_CPLL22150 &pll )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    unsigned char dataControl[ sizeRegistersPLL22150 ];

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlReadMode,
                                                 0xb0,
                                                 0x1ff0,
                                                 0x0000,
                                                 dataControl,
                                                 sizeRegistersPLL22150,
                                                 m_timeoutUSB );

    // Read Eeprom PLL
    if ( responseControl < 0 ) {
        PrintStdError( "GetEepromPLL22150Configuration()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );
        return ControlTransferError;
    }

    for ( unsigned char n = 0; n < sizeRegistersPLL22150; ++n ) {
        pll.m_registersPLL[ n ] = dataControl[ n ];
    }

    pll.m_referenceFrequency = m_crystalReference;

    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Parameters:
[in] 	pll 	A reference to an okCPLL22150 object.
Returns:
ErrorCode
Reads the current PLL configuration over the I2C bus and modifies the pll argument to reflect that configuration.

ErrorCodes: NoError, DeviceNotOpen
*/

OpenOK::ErrorCode OpenOK::GetPLL22150Configuration( OpenOK_CPLL22150 &pll )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    unsigned char dataControl[ sizeRegistersPLL22150 ];

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlReadMode,
                                                 0xb1,
                                                 0x0000,
                                                 0x0000,
                                                 dataControl,
                                                 sizeRegistersPLL22150,
                                                 m_timeoutUSB );

    // Read Cypress PLL
    if ( responseControl < 0 ) {
        PrintStdError( "GetPLL22150Configuration()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        return ControlTransferError;
    }

    for ( unsigned char n = 0; n < sizeRegistersPLL22150; ++n ) {
        pll.m_registersPLL[ n ] = dataControl[ n ];
    }

    pll.m_referenceFrequency = m_crystalReference;

    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Parameters:
[in] 	pll 	A reference to an okCPLL22150 object.
Returns:
ErrorCode
Configures the on-board PLL via the I2C bus using the configuration information in the pll object.

ErrorCodes: NoError, DeviceNotOpen
*/

OpenOK::ErrorCode OpenOK::SetEepromPLL22150Configuration( OpenOK_CPLL22150 &pll )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    unsigned char dataControl[ sizeRegistersPLL22150 ];

    for ( unsigned char n = 0; n < sizeRegistersPLL22150; ++n ) {
        dataControl[ n ] = pll.m_registersPLL[ n ];
    }

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb0,
                                                 0x1ff0,
                                                 0x0000,
                                                 dataControl,
                                                 sizeRegistersPLL22150,
                                                 m_timeoutUSB );
    // Write Eeprom PLL
    if ( responseControl < 0 ) {
        PrintStdError( "SetEepromPLL22150Configuration()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        return ControlTransferError;
    }

    pll.m_referenceFrequency = m_crystalReference;

    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Parameters:
[in] 	pll 	A reference to an okCPLL22150 object.
Returns:
ErrorCode
Configures the on-board PLL via the I2C bus using the configuration information in the pll object.

ErrorCodes: NoError, DeviceNotOpen
*/

OpenOK::ErrorCode OpenOK::SetPLL22150Configuration( OpenOK_CPLL22150 &pll )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    unsigned char dataControl[ sizeRegistersPLL22150 ];

    for ( unsigned char n = 0; n < sizeRegistersPLL22150; ++n ) {
        dataControl[ n ] = pll.m_registersPLL[ n ];
    }

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb1,
                                                 0x0000,
                                                 0x0000,
                                                 dataControl,
                                                 sizeRegistersPLL22150,
                                                 m_timeoutUSB );

    // Write Cypress PLL
    if ( responseControl < 0 ) {
        PrintStdError( "SetPLL22150Configuration()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );
        return ControlTransferError;
    }

    pll.m_referenceFrequency = m_crystalReference;

    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method modifies the XEM Device ID string with the new string.
The Device ID string is a user-programmable string of up to 32 characters
that can be used to uniquely identify a particular XEM. The string will be truncated if it exceeds 32 characters.

Parameters:
[in] 	str 	A string containing the new Device ID.
*/

void OpenOK::SetDeviceID( const std::string str )
{
    if ( !IsOpen() ) {
        return;
    }

    unsigned char* s = reinterpret_cast< unsigned char* >( const_cast< char* >( str.c_str() ) );

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb0,
                                                 0x1fd0,
                                                 0x0000,
                                                 s,
                                                 32,
                                                 m_timeoutUSB );

    if ( responseControl < 0 ) {
        PrintStdError( "SetDeviceID()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        return;
    }
    strncpy( m_listOKDevices [m_indexOpenedDevice ].deviceID, str.c_str(), OpenOK_device::SSIZE );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method downloads a configuration file to the FPGA. The filename should be that of a valid Xilinx bitfile (generated from bitgen).

Returns:
Total
*/

int OpenOK::SendDataBitFile( unsigned char endp, unsigned char *bytes, unsigned int size, unsigned int timeout )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    if ( size <= 0 ) {
        return SignedArgumentError;
    }

    unsigned int totalSend = 0;

    const unsigned int num1 = size >> 14; // Multiple of 16384 (Maximum of bytes that transfers Bulk Package)
    const unsigned int num2 = size - ( num1 << 14 );// Remainder to give.

    if ( num1 ) {
        for ( unsigned int j = 0; j < num1; ++j ) {
            int transferred = 0;
            int responseBulk = 0;

            responseBulk = BulkTransfer( m_deviceHandle,
                                         endp,
                                         bytes + ( j << 14 ),
                                         16384,
                                         &transferred,
                                         timeout );

            if ( responseBulk < 0 ) {
                PrintStdError( "SendDataBitFile()",
                               "BulkTransfer() failed - Step 01",
                               0,
                               responseBulk );

                return responseBulk;
            }
            totalSend += transferred;
        }
    }

    if ( num2 ) {
        int transferred = 0;
        int responseBulk = 0;

        responseBulk = BulkTransfer( m_deviceHandle,
                                     endp,
                                     bytes + ( ( num1 << 14 ) ),
                                     num2,
                                     &transferred,
                                     timeout );

        if ( responseBulk < 0 ) {
            PrintStdError( "SendDataBitFile()",
                           "BulkTransfer() failed - Step 02",
                           0,
                           responseBulk );

            return responseBulk;
        }
        totalSend += transferred;
    }

    if ( totalSend != size ) {
        return NotSentBitFile;
    }
    return totalSend;
}
//---------------------------------------------------------------------------------------------------------------------------------

OpenOK::ErrorCode OpenOK::SendPaddedFileToFPGA( unsigned char* buffer, const unsigned int size )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    if ( ControlStatus( controlWriteMode ) == NoError ) {
        // do a bulk transfer of the file to endpoint 0x02, starting with the header of the xilinx file
        // ff ff ff ff aa 99 55 66. however every byte is sent as an int16 with the actual byte in the upper bits, ie 0xff -> 0xff00
        // this was taken care of in the loading and parsing of the file

#ifdef QT_CORE_LIB
        SleepUtil::usleep( 3200 );
#else
        usleep(3200);
#endif

        const int sent = SendDataBitFile( endpointOUT, buffer, size, 2000 );

        // can not compare to zero, the return can be NoError
        if ( sent < 0 ) {
            return static_cast< OpenOK::ErrorCode > ( sent );
        }

#ifdef QT_CORE_LIB
        SleepUtil::usleep( 1000 );
#else
        usleep( 1000 );
#endif

        if ( static_cast< unsigned int > ( sent ) == size ) {
            return ControlStatus( controlReadMode );
        }
    }
    return NotConfigureFPGA;
}
//---------------------------------------------------------------------------------------------------------------------------------

template< class iterator >
OpenOK::ErrorCode OpenOK::SendFileToFPGA( iterator it, iterator end )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    ErrorCode error = NoError;

    unsigned int length = 0;
    unsigned char lengthDummy = 0;

    std::vector<unsigned char> buffer;

    std::string str;

    // Number of header fields + data field
    static const unsigned char numberFields = 7;

    for( unsigned char j = 0 ; j < numberFields ; ++j ) {

        str = "";

        if ( j < ( numberFields - 1 ) ) { // Read Header
            length = ( *it & 255 ) << 8;
            length += ( *(++it) & 255 );

            for ( unsigned int n = 0 ; n < length ; ++n ) {
                str += ( *(++it) & 255 );
            }

            ++it;

            if ( ( *it & 255 ) == 'a' || ( *it & 255 ) == 'b' || ( *it & 255 ) == 'c' || ( *it & 255 ) == 'd' ||
                 ( *it & 255 ) == 'e' ) {
                ++it;
            }
        } else if ( j == ( numberFields - 1 ) ) { // Read Data
            length = ( *it & 255 ) << 24;
            length += ( *(++it) & 255 ) << 16;
            length += ( *(++it) & 255 ) << 8;
            length += ( *(++it) & 255 );

            for ( unsigned int n = 0 ; n < length ; ++n ) {
                ++it;

                if ( it != end ) {
                    if ( ( *it & 255 ) == 0xFF ) {
                        lengthDummy++;
                    } else if ( ( *it & 255 ) == 0xAA ) {
                        break;
                    }
                } else {
                    return NotFoundSyncWord;
                }
            }
        }

        switch( j ) {
            case 0 :
                break;

            case 1 :
                break;

            case 2 :
                m_bitStream.designName = str;
                break;

            case 3 :
                m_bitStream.partName = str;
                break;

            case 4 :
                m_bitStream.date = str;
                break;

            case 5 :
                m_bitStream.time = str;
                break;

            case 6 :
                m_bitStream.dataLen = length;
                m_bitStream.dummyLen = lengthDummy;

                if ( lengthDummy <= 4 ) {
                    m_bitStream.addPadWord = true;
                } else {
                    m_bitStream.addPadWord = false;
                }

                for ( unsigned int n = 0 ; n < m_bitStream.dataLen ; ++n ) {
                    if ( n < m_bitStream.dummyLen ) {
                        buffer.push_back( 0xFF );

                        if ( m_bitStream.addPadWord ) {
                            buffer.push_back(0x00);
                        }
                    } else {
                        buffer.push_back( ( *it & 255 ) );

                        if ( m_bitStream.addPadWord ) {
                            buffer.push_back(0x00);
                        }

                        ++it;
                    }
                }
                break;

            default :
                error = ReadBitFileError;
                break;
        }
    }
    error = SendPaddedFileToFPGA( &buffer[ 0 ], buffer.size() );

    return error;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method downloads a configuration file to the FPGA. The filename should be that of a valid Xilinx bitfile (generated from bitgen).

Parameters:
[in] 	strFilename 	A string containing the filename of the configuration file.

Returns:
0 if successful, error code otherwise.
NO_ERROR - Configuration has occurred successfully.
DONE_NOT_HIGH - FPGA DONE signal did not assert.
TRANSFER_ERROR - USB error has occurred during download.
COMMUNICATION_ERROR - Communication error with the firmware.
INVALID_BITSTREAM - The bitstream is not properly formatted.
FILE_ERROR - File error occurred during open or read.
DEVICE_NOT_OPEN - Communication with a device is not established.
*/

OpenOK::ErrorCode OpenOK::ConfigureFPGA( const std::string strFilename )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    // FPGA configuration is sent as a bulk transfer, with a control transfer preceding to
    // tell the device to expect configuration data.
    // the control transfer has no data bytes, and has setup packet 0x40 b2 00 00 00 00 00 00
    // the bulk transfer is the bitstream from a Xilinx .bit file, starting with the dummy bytes ( FF FF FF FF ) +
    // sync word ( AA 99 55 66 )
    // however the bitstream is sent with each byte padded to 16bits on the right, so 0xFF becomes 0xFF00 in the
    // data actually sent over USB.
    // in the real library, a configure call is ended with another control transfer, this appears to be a check to
    // see if configuration was successful
    // the transfer has 1 data byte and the setup packet is 0xc0 b2 00 00 00 00 01 00. a byte of 0x01 is returned on success,
    // and I have seen a byte of
    // 0x02 returned when it didn't work, but I don't know what the 0x02 means,
    // or if there are other return values indicating other errors

    // configures the fpga on an xem with the bitstream in Filename

    // first get and parse the bitstream from the file
    std::basic_ifstream< char > file( strFilename.c_str(), std::ios_base::in | std::ios_base::binary );

    if ( file ) {
        std::istreambuf_iterator< char > it( file );
        std::istreambuf_iterator< char > eos;

        return SendFileToFPGA( it, eos );
    }
    return NotOpenBitFile;
}
//---------------------------------------------------------------------------------------------------------------------------------

OpenOK::ErrorCode OpenOK::ConfigureFPGAFromMemory( unsigned char *data, const unsigned long length )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }
    return SendFileToFPGA( data, data + length );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method Read/Write data from/to Pipe Out/In.

Parameters:
[in] 	epAddr 	The address of the destination Pipe In/Out.
[in] 	length 	The length of the transfer.
[in] 	data 	A pointer to the transfer data buffer.

Returns:
The number of bytes written/read or ErrorCode if the write failed.
*/

inline long OpenOK::ReadWritePipe( int32_t epAddr, int32_t endpointDir, int32_t endpointLower, int32_t endpointUpper,
                                   int32_t magicNumber1, int32_t magicNumber2, int64_t length, uint8_t *data )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    int32_t totalTranferred = 0;

    const uint8_t sizeDataControl = 6;
    uint8_t dataControl[ sizeDataControl ];

    uint8_t lengthDataControl = 2;

    // If address not valid.
    if ( ( epAddr < endpointLower ) || ( epAddr > endpointUpper ) ) {
        return RangeAddressError;
    }

    if ( length <= 0 ) {
        return SignedArgumentError;
    }

    // Odd transfers will be shortened by one byte.
    if ( ( length % 2 ) != 0 ) {
        PrintStdError( "ReadWritePipe()",
                       "Odd transfers will be shortened by one byte" );

        length -= 1;
    }

    // First step : transferred multiples of 16384.
    const uint32_t multipleMaxBulkPackage = length >> 14;// Multiple of 16384 (Maximum of bytes that transfers Bulk Package).
    const uint32_t lenghtFirstPackage = ( multipleMaxBulkPackage << 14 );// Maximum multiple of 16384.

    // Second step : transferred multiples of MaxPacketSize.
    const uint32_t multipleMaxPacketSize = ( length - lenghtFirstPackage ) >> m_shiftMaxPacketSize;// Multiple of USB MaxPacketSize.
    const uint32_t lenghtSecondPackage = ( multipleMaxPacketSize << m_shiftMaxPacketSize );// Maximum multiple of USB MaxPacketSize.

    // Third step : transferred remaining.
    const uint32_t remaining = length - lenghtFirstPackage - lenghtSecondPackage;
    const uint32_t lenghtRemaining = remaining % 2 ? (remaining - 1) : remaining;// Remaining to give.
    const uint32_t halfLenghtRemaining = lenghtRemaining >> 1; // Half of the remaining to give.

#if !defined(WRITE_READ_FAST)
    ErrorCode error = CheckEnable();

    if ( error != NoError ) {
        return error;
    }
#endif

    if ( lenghtFirstPackage || lenghtSecondPackage ) {
        dataControl[ 0 ] = epAddr;
        dataControl[ 1 ] = magicNumber1;

        if ( endpointDir == endpointIN ) {
            dataControl[ 2 ] = ( lenghtFirstPackage + lenghtSecondPackage ) & 255;
            dataControl[ 3 ] = ( ( lenghtFirstPackage + lenghtSecondPackage ) >> 8 ) & 255;
            dataControl[ 4 ] = ( ( lenghtFirstPackage + lenghtSecondPackage ) >> 16 ) & 255;
            dataControl[ 5 ] = ( ( lenghtFirstPackage + lenghtSecondPackage ) >> 24 ) & 255;

            lengthDataControl = sizeDataControl;
        }

        const int responseControl = ControlTransfer( m_deviceHandle,
                                                     controlWriteMode,
                                                     0xb7,
                                                     m_maxPacketSize,
                                                     0x0000,
                                                     dataControl,
                                                     lengthDataControl,
                                                     m_timeoutUSB );
        if ( responseControl < 0 ) {
            PrintStdError( "ReadWritePipe()",
                           "ControlTransfer() failed - Step 01 or Step 02",
                           0,
                           responseControl );

            return responseControl;
        }

        // Write/Read Multiple of 16384.
        if ( lenghtFirstPackage ) {
            int32_t transferred = 0;

            int32_t responseBulk = BulkTransfer( m_deviceHandle,
                                                 endpointDir,
                                                 data,
                                                 lenghtFirstPackage,
                                                 &transferred,
                                                 m_timeoutUSB );

            if ( responseBulk < 0 ) {
                PrintStdError( "ReadWritePipe()",
                               "BulkTransfer() failed - Step 01",
                               0,
                               responseBulk );

                return responseBulk;
            }
            totalTranferred += transferred;
        }

        // Write/Read Multiple of USB MaxPacketSize.
        if ( lenghtSecondPackage ) {
            int32_t transferred = 0;

            int32_t responseBulk = BulkTransfer( m_deviceHandle,
                                                 endpointDir,
                                                 data + lenghtFirstPackage,
                                                 lenghtSecondPackage,
                                                 &transferred,
                                                 m_timeoutUSB );

            if ( responseBulk < 0 ) {
                PrintStdError( "ReadWritePipe()",
                               "BulkTransfer() failed - Step 02",
                               0,
                               responseBulk );

                return responseBulk;
            }
            totalTranferred += transferred;
        }

        m_lastTransferred = totalTranferred;

#if !defined(WRITE_READ_FAST)
        ErrorCode error = CheckDisable();

        if ( error != NoError ) {
            return error;
        }
#endif
    }

    // Write/Read remaining.
    if ( lenghtRemaining ) {
        dataControl[ 0 ] = epAddr;
        dataControl[ 1 ] = magicNumber2;

        if ( endpointDir == endpointIN ) {
            dataControl[ 2 ] = ( lenghtRemaining ) & 255;
            dataControl[ 3 ] = ( ( lenghtRemaining ) >> 8 ) & 255;
            dataControl[ 4 ] = ( ( lenghtRemaining ) >> 16 ) & 255;
            dataControl[ 5 ] = ( ( lenghtRemaining ) >> 24 ) & 255;

            lengthDataControl = sizeDataControl;
        }

        const int responseControl = ControlTransfer( m_deviceHandle,
                                                     controlWriteMode,
                                                     0xb7,
                                                     halfLenghtRemaining,
                                                     0x0000,
                                                     dataControl,
                                                     lengthDataControl,
                                                     m_timeoutUSB );
        if ( responseControl < 0 ) {
            PrintStdError( "ReadWritePipe()",
                           "ControlTransfer() failed - Step 03",
                           0,
                           responseControl );

            return responseControl;
        }

        int32_t transferred = 0;

        int32_t responseBulk = BulkTransfer( m_deviceHandle,
                                             endpointDir,
                                             data + ( lenghtFirstPackage + lenghtSecondPackage ),
                                             lenghtRemaining,
                                             &transferred,
                                             m_timeoutUSB );

        if ( responseBulk < 0 ) {
            PrintStdError( "ReadWritePipe()",
                           "BulkTransfer() failed - Step 03",
                           0,
                           responseBulk );

            return responseBulk;
        }
        totalTranferred += transferred;

        m_lastTransferred = totalTranferred;

#if !defined(WRITE_READ_FAST)
        ErrorCode error = CheckDisable();

        if ( error != NoError ) {
            PrintStdError( "ReadWritePipe()",
                           "CheckDisable() failed - Step 03" );

            return error;
        }
#endif
    }

    if ( totalTranferred != length ) {
        return TransferError;
    }

    m_lastTransferred = totalTranferred;

    return totalTranferred;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method transfers a specified buffer of data to the given Pipe In endpoint.
Note: For 16-bit boards (all but the XEM3001v1), the transfer length must be even. Odd transfers will be shortened by one byte.

Python Note: Within Python, the 'length' parameter is not used and the 'data' parameter is a string buffer. For example,

 buf = "\x00"*1076
 xem.WriteToPipeIn(0x80, buf)

Parameters:
[in] 	epAddr 	The address of the destination Pipe In.
[in] 	length 	The length of the transfer.
[in] 	data 	A pointer to the transfer data buffer.

Returns:
The number of bytes written or ErrorCode if the write failed.
*/

long OpenOK::WriteToPipeIn( int epAddr, long length, unsigned char *data )
{
    // I don't know what the 0x04 means.
    // For 0x80 and 0x9F address see FrontPanel-UM.pdf, Endpoint Types, pg 40.
    const long response = ReadWritePipe( epAddr, endpointOUT, 0x80, 0x9F, 0x04, 0x04, length, data );

    if ( response < 0 ) {
        PrintStdError( "WriteToPipeIn()",
                       "ReadWritePipe() failed",
                       0,
                       response );
    }
    return response;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method transfers data from a specified Pipe Out endpoint to the given buffer. The maximum length of a transfer (per call to
this method) is 16,777,215 bytes.
Note: For 16-bit boards (all but the XEM3001v1), the transfer length must be even. Odd transfers will be shortened by one byte.

Python Note: Within Python, the 'length' parameter is not used and the 'data' parameter is a string buffer. For example,

 buf = "\x00"*1076
 xem.ReadFromPipeOut(0xa0, buf)

Parameters:
[in] 	epAddr 	The address of the source Pipe Out.
[in] 	length 	The length of the transfer.
[in] 	data 	A pointer to the transfer data buffer.

Returns:
The number of bytes read or ErrorCode if the read failed.
*/

long OpenOK::ReadFromPipeOut( int epAddr, long length, unsigned char *data )
{
    // I don't know what the 0x05 and 0x06 means.
    // For 0xA0 and 0xBF address see FrontPanel-UM.pdf, Endpoint Types, pg 40.
    const long response = ReadWritePipe( epAddr, endpointIN, 0xA0, 0xBF, 0x06, 0x05, length, data );

    if ( response < 0 ) {
        PrintStdError( "ReadFromPipeOut()",
                       "ReadWritePipe() failed",
                       0,
                       response );
    }
    return response;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method is called to request the current state of all Wire Out values from the XEM. All wire outs are captured and
read at the same time.
If any Wire Out event handlers have been installed (see AddEventHandler()),
then the corresponding event handler methods are called with the new Wire Out values.
*/

void OpenOK::UpdateWireOuts()
{
    // wireout's are sent as a control transfer. setup packet is 0xc0 b5 20 00 00 00 40 00
    // the 0x0020 value in wValue was somewhat unexpected, since there is no corresponding one in the wirein call.
    // there are 64 data bytes representing the values on each of the 32 possible wireout's
    // wireout endpoint 0x20 is sent as the first pair of bytes LSB:MSB, and the rest follow in order
    // until the last possible wireout endpoint of 0x3F, which takes the last two bytes in the data

    if ( !IsOpen() ) {
        return;
    }

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlReadMode,
                                                 0xb5,
                                                 0x0020,
                                                 0x0000,
                                                 m_wireOuts,
                                                 OpenOK::WIREOUTSIZE,
                                                 m_timeoutUSB );
    if ( responseControl < 0 ) {
        PrintStdError( "UpdateWireOuts()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        memset( m_wireOuts, 0, OpenOK::WIREOUTSIZE );
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method provides a way to get a particular wire out value without going through an event handler. For example,

 // First, query all XEM Wire Outs.
 xem->UpdateWireOuts();
 // Now, get values from endpoints 0x21 and 0x27.
 a = xem->GetWireOutValue(0x21);
 b = xem->GetWireOutValue(0x27);

Parameters:
[in] 	epAddr 	The Wire Out address to query.

Returns:
The value of the Wire Out.
*/

int OpenOK::GetWireOutValue( int epAddr )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    // function allows you to specify a particular endpoint you want data for.
    // endpoint 0x20 is the first two bytes LSB:MSB and the rest follow in order until endpoint 0x3F which is the last two bytes.
    if ( ( epAddr < 0x20 ) || ( epAddr > 0x3F ) ) {
        return RangeAddressError;
    }

    unsigned char idx = ( epAddr - 0x20 ) << 1;
    unsigned char low = m_wireOuts[ idx ];
    unsigned char high = m_wireOuts[ idx + 1 ];

    const int result = ( high << 8 ) + low;

    return result;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Wire In endpoint values are stored internally and updated when necessary by calling UpdateWireIns(). The values are updated on a
per-endpoint basis by calling this method. In addition, specific bits may be updated independent of other bits within an endpoint by
using the optional mask.

 // Update Wire In 0x03 with the value 0x35.
 xem->SetWireInValue(0x03, 0x35);
 // Update only bit 3 of Wire In 0x07 with a 1.
 xem->SetWireInValue(0x07, 0x04, 0x04);
 // Commit the updates to the XEM.
 xem->UpdateWireIns();

Parameters:
[in] 	ep 	The address of the Wire In endpoint to update.
[in] 	val 	The new value of the Wire In.
[in] 	mask 	A mask to apply to the new value.

Returns:
ErrorCode.
*/

OpenOK::ErrorCode OpenOK::SetWireInValue( int epAddr, unsigned long val, unsigned long mask )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    if ( ( epAddr < 0x00 ) || ( epAddr > 0x1F ) ) {
        return RangeAddressError;
    }
    //                          clear bits being modified    OR   bits to be set                  
    m_wireIns[ 2 * epAddr] = (m_wireIns[ 2 * epAddr] & ~mask) | (val & mask); 

    return NoError;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
This method is called after all Wire In values have been updated using SetWireInValue(). The latter call merely updates the values
held within a data structure inside the class. This method actually commits the changes to the XEM simultaneously so that all wires
will be updated at the same time.
*/

void OpenOK::UpdateWireIns()
{
    // wirein's are sent as a control transfer. setup packet is 0x40 b5 00 00 00 00 40 00
    // there are 64 data bytes representing the values on each of the 32 possible wirein's
    // wirein endpoint 0x00 is sent as the first pair of bytes LSB:MSB, and the rest follow in order
    // until the last possible wirein endpoint of 0x1F, which takes the last two bytes in the data
    if ( !IsOpen() ) {
        return;
    }

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb5,
                                                 0x0000,
                                                 0x0000,
                                                 m_wireIns,
                                                 OpenOK::WIREINSIZE,
                                                 m_timeoutUSB );

    if ( responseControl < 0 ) {
        PrintStdError( "UpdateWireIns()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        memset( m_wireIns, 0, OpenOK::WIREINSIZE );
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Not tested
*/

int OpenOK::OpenOK_set_trigger_in( int /*ep*/, unsigned char *data )
{
    if ( !IsOpen() ) {
        return DeviceNotOpen;
    }

    // trigger ins are sent as a control transfer. the format appears is 0x40 0xb5 0x[endpoint] 0x00 0x01 0x00 0x02 0x00
    // which corresponds to libusb_control_transfer(OpenOK_dev, 0x40, 0xb5, endpoint, 1, data, 2, TIMEOUT)
    // the 2 data bytes represent the 16 possible triggers associated with the endpoint. The original library only allows
    // for setting 1 bit at a time, so for each transaction, the data bytes would have only 1 bit set.
    // I don't know what the result of setting more than on bit would be.
    // A reasonable guess would be that multiple triggers would be set.
    // so for compatible operation, only have one set bit in the 2 bytes pointed to by data

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlWriteMode,
                                                 0xb5,
                                                 0x0001,
                                                 0x0001,
                                                 data,
                                                 2,
                                                 m_timeoutUSB );

    return responseControl;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Not tested

trigger outs are sent exactly the same as wire outs, but with a different setup byte: 0xc0 0xb5 0x60 0x00 0x10 0x00 0x40 0x00
*/

void OpenOK::UpdateTriggerOuts(  )
{

    const int responseControl = ControlTransfer( m_deviceHandle,
                                                 controlReadMode,
                                                 0xb5,
                                                 0x0060,
                                                 0x0001,
                                                 m_triggerOuts,
                                                 OpenOK::TRIGGEROUTSIZE,
                                                 m_timeoutUSB );

   if ( responseControl < 0 ) {
        PrintStdError( "UpdateWireOuts()",
                       "ControlTransfer() failed",
                       0,
                       responseControl );

        memset( m_triggerOuts, 0, OpenOK::TRIGGEROUTSIZE );
    }
}

bool OpenOK::IsTriggered( int epAddr, unsigned long mask )
{
	// Function allows you to specify a particular endpoint you want data for.
	// Endpoint 0x60 is the first two bytes LSB:MSB and the rest follow in order until endpoint 0x7F which is the last two bytes.
	// For USB, only the lower 16 bits are used
	

	if ( ( epAddr < 0x60 ) || ( epAddr > 0x7F ) ) {
		return RangeAddressError;
	}

	unsigned char idx = ( epAddr - 0x60 ) << 1;
	unsigned char low = m_triggerOuts[ idx ];
	unsigned char high = m_triggerOuts[ idx + 1 ];

	const unsigned long trigs = ( high << 8 ) + low;
	
	return (trigs & mask);
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Check if device is high speed USB 2.0( 480Mbps )
*/

bool OpenOK::IsHighSpeed()
{
    if ( !IsOpen() ) {
        return false;
    }
    return ( m_listOKDevices[ m_indexOpenedDevice ].maxPacketSize == 512 );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Set the USB timeout duration (in milliseconds).
*/
void OpenOK::SetTimeout( int timeout )
{
    m_timeoutUSB = timeout;
}
//---------------------------------------------------------------------------------------------------------------------------------

void OpenOK::PrintInfoAllDevice()
{
    Close();

    int numDevices = 0;

    ErrorCode responseGetOKDevices = Get_OK_Devices( false, numDevices );

    if ( responseGetOKDevices != NoError ) {
        if ( responseGetOKDevices != NotFoundOpallKellyBoard ) {
            PrintStdError( "PrintInfoAllDevice()",
                           "Get_OK_Devices() failed",
                           0,
                           responseGetOKDevices );
        }
        return;
    }

    PrintStdError( "",
                   "======================================" );
    if ( numDevices > 0 ) {

        char str[ 100 ];

        for ( int idxDevices = 0; idxDevices < numDevices; ++idxDevices ) {
            sprintf( str, "deviceID = %s", m_listOKDevices[ idxDevices ].deviceID );
            PrintStdError( "",
                           str );

            sprintf( str, "serial = %s", m_listOKDevices[ idxDevices ].serial );
            PrintStdError( "",
                           str );

            sprintf( str, "manufacturer = %s", m_listOKDevices[ idxDevices ].manufacturer );
            PrintStdError( "",
                           str );

            sprintf( str, "product = %s", m_listOKDevices[ idxDevices ].product );
            PrintStdError( "",
                           str );

            sprintf( str, "idVendor = %02d", m_listOKDevices[ idxDevices ].idVendor );
            PrintStdError( "",
                           str );

            sprintf( str, "idProduct = %02d", m_listOKDevices[ idxDevices ].idProduct );
            PrintStdError( "",
                           str );

            sprintf( str, "bcdUSB = %02d", m_listOKDevices[ idxDevices ].bcdUSB);
            PrintStdError( "",
                           str );

            sprintf( str, "bNumConfigurations = %02d", m_listOKDevices[ idxDevices ].bNumConfigurations);
            PrintStdError( "",
                           str );

            sprintf( str, "bDeviceClass = %02d", m_listOKDevices[ idxDevices ].bDeviceClass);
            PrintStdError( "",
                           str );

            unsigned char bNumInterfaces = m_listOKDevices[ idxDevices ].bNumInterfaces;
            sprintf( str, "bNumInterfaces = %02d", bNumInterfaces);
            PrintStdError( "",
                           str );

            for ( unsigned char i = 0; i < bNumInterfaces; ++i ) {
                unsigned char nAltSetting = m_listOKDevices[ idxDevices ].num_altsetting[ i ];
                sprintf( str, "-nAltSetting = %02d", nAltSetting );
                PrintStdError( "",
                               str );

                for ( int j = 0; j < nAltSetting; ++j ) {
                    sprintf( str, "--bInterfaceNumber = %02d", m_listOKDevices[ idxDevices ].bInterfaceNumber[ j ] );
                    PrintStdError( "",
                                   str );

                    unsigned char bNumEndpoints = m_listOKDevices[ idxDevices ].bNumEndpoints[ j ];
                    sprintf( str, "--bNumEndpoints = %02d", bNumEndpoints );
                    PrintStdError( "",
                                   str );

                    for ( unsigned char k = 0; k < bNumEndpoints; ++k ) {
                        sprintf( str, "---bDescriptorType = %02d", m_listOKDevices[ idxDevices ].bDescriptorType[ k ] );
                        PrintStdError( "",
                                       str );

                        sprintf( str, "---bEndpointAddress = %02d", m_listOKDevices[ idxDevices ].bEndpointAddress[ k ] );
                        PrintStdError( "",
                                       str );

                        sprintf( str, "---bInterval = %02d", m_listOKDevices[ idxDevices ].bInterval[ k ] );
                        PrintStdError( "",
                                       str );

                        sprintf( str, "---wMaxPacketSize = %02d", m_listOKDevices[ idxDevices ].wMaxPacketSize[ k ] );
                        PrintStdError( "",
                                       str );
                    }
                }
            }
        }
    }

    PrintStdError( "",
                   "======================================" );
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get string openok error
*/

std::string OpenOK::GetStringOpenOKError( int numberError )
{
    // Official

    if ( numberError == NoError ) {
        return "ERROR_NO_ERROR";
    } else if ( numberError == Failed ) {
        return "ERROR_FAILED";
    } else if ( numberError == Timeout ) {
        return "ERROR_TIMEOUT";
    } else if ( numberError == DoneNotHigh ) {
        return "ERROR_DONE_NOT_HIGH";
    } else if ( numberError == TransferError ) {
        return "ERROR_TRANSFER_ERROR";
    } else if ( numberError == CommunicationError ) {
        return "ERROR_COMMUNICATION_ERROR";
    } else if (numberError == InvalidBitstream) {
        return "ERROR_INVALID_BITSTREAM";
    } else if (numberError == FileError) {
        return "ERROR_FILE_ERROR";
    } else if (numberError == DeviceNotOpen ) {
        return "ERROR_DEVICE_NOT_OPEN";
    } else if (numberError == InvalidEndpoint) {
        return "ERROR_INVALID_ENDPOINT";
    } else if ( numberError == InvalidBlockSize ) {
        return "ERROR_INVALID_BLOCK_SIZE";
    } else if ( numberError == I2CRestrictedAddress ) {
        return "ERROR_I2C_RESTRICTED_ADDRESS";
    } else if ( numberError == I2CBitError ) {
        return "ERROR_I2C_BIT_ERROR";
    } else if ( numberError == I2CNack ) {
        return "ERROR_I2C_NACK";
    } else if ( numberError == I2CUnknownStatus ) {
        return "ERROR_I2C_UNKNOWN_STATUS";
    } else if ( numberError == UnsupportedFeature ) {
        return "ERROR_UNSUPPORTED_FEATURE";
    } else if ( numberError == FIFOUnderflow ) {
        return "ERROR_FIFO_UNDERFLOW";
    } else if ( numberError == FIFOOverflow ) {
        return "ERROR_FIFO_OVERFLOW";
    } else if ( numberError == DataAlignmentError ) {
        return "ERROR_DATA_ALIGNMENT_ERROR";
    }

    // Not Official

    else if ( numberError == SignedArgumentError ) {
        return "ERROR_SIGNED_ARGUMENT_ERROR";
    } else if ( numberError == RangeAddressError ) {
        return "ERROR_RANGE_ADDRESS_ERROR";
    } else if ( numberError == NotFoundOpallKellyBoard ) {
        return "ERROR_NOT_FOUND_OPALKELLY_BOARD";
    } else if ( numberError == ClaimInterfaceError ) {
        return "ERROR_CLAIM_INTERFACE_ERROR";
    } else if ( numberError == SetConfigurationError ) {
        return "ERROR_SET_CONFIGURATION_ERROR";
    } else if ( numberError == NotFoundSyncWord ) {
        return "ERROR_NOT_FOUND_SYNC_WORD";
    } else if ( numberError == NotConfigureFPGA ) {
        return "ERROR_NOT_CONFIGURE_FPGA";
    } else if ( numberError == NotFoundDeviceStringID ) {
        return "ERROR_NOT_FOUND_DEVICE_WITH_STRING_ID";
    } else if ( numberError == NotOpenBitFile ) {
        return "ERROR_NOT_OPEN_BITFILE";
    } else if ( numberError == NotFoundUSBDevices ) {
        return "ERROR_NOT_FOUND_USB_DEVICES";
    } else if ( numberError == NotFoundDeviceStringSerial ) {
        return "ERROR_NOT_FOUND_DEVICE_WITH_STRING_SERIAL";
    } else if ( numberError == NotSentBitFile ) {
        return "ERROR_NOT_SENT_BITFILE";
    } else if ( numberError == ControlTransferError ) {
        return "ERROR_CONTROL_TRANSFER_ERROR";
    } else if ( numberError == BulkTransferError ) {
        return "ERROR_BULK_TRANSFER_ERROR";
    } else if ( numberError == LibusbNotInitialization ){
        return "ERROR_LIBUSB_NOT_INITIALIZATION";
    } else if ( numberError == ReadBitFileError ) {
        return "ERROR_READ_BIT_FILE_ERROR";
    } else if ( numberError == FrontPanelDisabled ) {
        return "ERROR_FRONTPANEL_DISABLED";
    } else if ( numberError == ControlStatusInvalidResponse ) {
        return "ERROR_CONTROL_STATUS_INVALID_RESPONSE";
    } else if ( numberError == NotPossibleCheckFrontPanel ) {
        return "ERROR_NOT_POSSIBLE_CHECK_FRONTPANEL";
    } else if ( numberError == OperationNotPermitted ) {
        return "ERROR_OPERATION_NOT_PERMITTED";
    } else if ( numberError == GetConfigurationError ) {
        return "ERROR_GET_CONFIGURATION_ERROR";
    } else if ( numberError == PointerNULL ) {
        return "ERROR_POINTER_NULL";
    } else if ( numberError == LibusbError ) {
        return "ERROR_LIBUSB_ERROR";
    } else if ( numberError == CatchError ) {
        return "ERROR_CATCH_ERROR";
    } else if ( numberError == CheckEnableInvalidResponse ) {
        return "ERROR_CHECK_ENABLE_INVALID_RESPONSE";
    } else if ( numberError == CheckDisableInvalidResponse ) {
        return "ERROR_CHECK_DISABLE_INVALID_RESPONSE";
    } else if ( numberError == OpenByDeviceIndexListInvalidResponse ) {
        return "ERROR_OPENBYDEVICELIST_INVALID_RESPONSE";
    } else {
        std::string str = "UNKNOWN_ERROR";

        if ( numberError < BulkTransferError ) {
            str = "ERROR_BULK_TRANSFER_ERROR + " +
                    GetStringLibUSBError( numberError - BulkTransferError );
        } else if ( numberError < ControlTransferError &&
                    numberError > BulkTransferError ) {
            str = "ERROR_CONTROL_TRANSFER_ERROR + " +
                    GetStringLibUSBError( numberError - ControlTransferError );
        }
        return str;
    }
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Get string libusb error
*/

std::string OpenOK::GetStringLibUSBError( int errorCodeLibUSB )
{
    std::string errorLibUSBStr = "";

    if ( errorCodeLibUSB < 0 ) {
        switch ( errorCodeLibUSB ) {
            case 0 :
                errorLibUSBStr = "LIBUSB_SUCCESS";
                break;
            case -1:
                errorLibUSBStr = "LIBUSB_ERROR_IO";
                break;
            case -2:
                errorLibUSBStr = "LIBUSB_ERROR_INVALID_PARAM";
                break;
            case -3:
                errorLibUSBStr = "LIBUSB_ERROR_ACCESS";
                break;
            case -4:
                errorLibUSBStr = "LIBUSB_ERROR_NO_DEVICE";
                break;
            case -5:
                errorLibUSBStr = "LIBUSB_ERROR_NOT_FOUND";
                break;
            case -6:
                errorLibUSBStr = "LIBUSB_ERROR_BUSY";
                break;
            case -7:
                errorLibUSBStr = "LIBUSB_ERROR_TIMEOUT";
                break;
            case -8:
                errorLibUSBStr = "LIBUSB_ERROR_OVERFLOW";
                break;
            case -9:
                errorLibUSBStr = "LIBUSB_ERROR_PIPE";
                break;
            case -10:
                errorLibUSBStr = "LIBUSB_ERROR_INTERRUPTED";
                break;
            case -11:
                errorLibUSBStr = "LIBUSB_ERROR_NO_MEM";
                break;
            case -12:
                errorLibUSBStr = "LIBUSB_ERROR_NOT_SUPPORTED";
                break;
            default:
                errorLibUSBStr = "LIBUSB_ERROR_OTHER";
                break;
        }
    }
    return errorLibUSBStr;
}
//---------------------------------------------------------------------------------------------------------------------------------

void OpenOK::SetEnablePrintStdError( bool enable )
{
    m_enablePrintStdError = enable;
}
//---------------------------------------------------------------------------------------------------------------------------------

/*
Print message in stderror
*/

void OpenOK::PrintStdError( std::string nameFunction,
                            std::string str,
                            int errorCodeLibUSB,
                            int erroCodeOpenOK )
{
    if ( m_enablePrintStdError ) {
        // get time now
        time_t t = time( 0 );
        struct tm * now = localtime( &t );
        char time[ 50 ];

        std::string deviceID = GetDeviceID();

        sprintf( time, "%02d:%02d:%02d - ", now->tm_hour, now->tm_min, now->tm_sec );

        if ( erroCodeOpenOK > 0 || errorCodeLibUSB > 0 ) {
            if ( erroCodeOpenOK > 0 && errorCodeLibUSB > 0 ) {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " - " << str << erroCodeOpenOK << " and " << errorCodeLibUSB << std::endl;
            } else if ( erroCodeOpenOK > 0 ) {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " - " << str << erroCodeOpenOK << std::endl;
            } else if ( errorCodeLibUSB > 0 ) {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " - " << str << errorCodeLibUSB << std::endl;
            }
        } else if ( erroCodeOpenOK == 0 && errorCodeLibUSB == 0 ) {
            std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                         nameFunction << " - " << str << std::endl;
        } else if ( erroCodeOpenOK < 0 ) {
            if ( str != "" ) {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " failed - " << str << " - Error openok: code = " << erroCodeOpenOK
                          << ", name = " << GetStringOpenOKError( erroCodeOpenOK ) << std::endl;
            } else {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " failed - " << "Error openok: code = " << erroCodeOpenOK
                          << ", name = " << GetStringOpenOKError( erroCodeOpenOK ) << std::endl;
            }
        } else {
            if ( str != "" ) {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " failed - " << str << " - Error libusb: code = " << errorCodeLibUSB
                          << ", name = " << GetStringLibUSBError( errorCodeLibUSB ) << std::endl;
            } else {
                std::cerr << time << "OpenOK : Device ID = " << deviceID << " - " <<
                             nameFunction << " failed - " << "Error libusb: code = " << errorCodeLibUSB
                          << ", name = " << GetStringLibUSBError( errorCodeLibUSB ) << std::endl;
            }
        }
    }
}
//---------------------------------------------------------------------------------------------------------------------------------
