#pragma once

#include "../../../../../../../../Application/Config.h"

struct hid_device_info;


enum UsagePage
{
    upUndef   = 0x0000,

    upDesktop = 0x0001,
};

enum Usage
{
    uUndef               = 0x0000,

    uPointer             = 0x0001,
    uMouse               = 0x0002,
    // ?
    uJoystick            = 0x0004,
    uGamepad             = 0x0005,
    uKeyboard            = 0x0006,
    uKeypad              = 0x0007,
    uMultiAxisController = 0x0008,
};

// Can be found at: https://www.usb.org/sites/default/files/vendor_ids111121_0.pdf
enum Vendor
{
    vAmazon    = 0x1949,
    vAmazonBT  = 0x0171,
    vApple     = 0x05AC,
    vGoogle    = 0x18D1,
    vHyperkin  = 0x2E24,
    vMicrosoft = 0x045E,
    vNintendo  = 0x057E,
    vNvidia    = 0x0955,
    vPDP       = 0x0E6F,
    vPowerA    = 0x24C6,
    vPowerAAlt = 0x20D6,
    vRazer     = 0x1532,
    vShenzhen  = 0x0079,
    vSony      = 0x054C,
    vValve     = 0x28DE,

    // Others
    vAstro        = 0x9886,
    vElecom       = 0x056E,
    vHORI         = 0x0F0D,
    vLogitech     = 0x046D,
    vMadCatz      = 0x0738,
    vNACON        = 0x146B,
    vNumark       = 0x15E4,
    vRazerAlt     = 0x1689,
    vRedOctane    = 0x1430,
    vSaitek       = 0x06A3,
    vSteelSeries  = 0x1038,
    vThrustmaster = 0x044F,
};

// Can be found at: http://www.linux-usb.org/usb.ids
enum ProductID
{
    pidAmazonLuna                     = 0x0419,

    pidGoogleStadia                   = 0x9400,

    pidEvoretroGamecubeAdapter        = 0x1846,
    pidNintendoGamecubeAdapter        = 0x0337,
    pidNintendoSwitchPro              = 0x2009,
    pidNintendoSwitchJoyConLeft       = 0x2006,
    pidNintendoSwitchJoyConRight      = 0x2007,
    pidNintendoSwitchJoyConGrip       = 0x200E,

    pidRazerPanthera                  = 0x0401,
    pidRazerPantheraEvo               = 0x1008,
    pidRazerAtrox                     = 0x0A00,

    pidSonyDS3                        = 0x0268,
    pidSonyDS4                        = 0x05C4,
    pidSonyDS4Dongle                  = 0x0BA0,
    pidSonyDS4Slim                    = 0x09CC,
    pidSonyDS5                        = 0x0CE6,
    
    pidXbox360XUSBController          = 0x02A1,
    pidXbox360WiredController         = 0x028E,
    pidXbox360WirelessReceiver        = 0x0719,
    pidXboxOne                        = 0x02D1,
    pidXboxOne2015                    = 0x02DD,
    pidXboxOneAdaptive                = 0x0B0A,
    pidXboxOneAdaptiveBT              = 0x0B0C,
    pidXboxOneAdaptiveBTLowEnergy     = 0x0B21,
    pidXboxOneEliteSeries1            = 0x02E3,
    pidXboxOneEliteSeries2            = 0x0B00,
    pidXboxOneEliteSeries2BT          = 0x0B02,
    pidXboxOneEliteSeries2BTAlt       = 0x0B05,
    pidXboxOneEliteSeries2BTLowEnergy = 0x0B22,
    pidXboxOneS                       = 0x02EA,
    pidXboxOneSRev1BT                 = 0x02E0,
    pidXboxOneSRev2BT                 = 0x02FD,
    pidXboxOneSRev2BTLowEnergy        = 0x0B20,
    pidXboxOneXboxGIP                 = 0x02FF,
    pidXboxSeriesX                    = 0x0B12,
    pidXboxSeriesXBTLowEnergy         = 0x0B13,
    pidXboxSeriesXVictrixGambit       = 0x02D6,
    pidXboxSeriesXPdpBlue             = 0x02D9,
    pidXboxSeriesXPdpAfterglow        = 0x02DA,
    pidXboxSeriesXPoweraFusionPro2    = 0x4001,
    pidXboxSeriesXPoweraSpectra       = 0x4002,
};

#define CONTROLLER_TYPE_LIST \
    CONVERT(Unknown) \
\
    CONVERT(PS3) \
    CONVERT(PS4) \
    CONVERT(PS5) \
    CONVERT(Stadia) \
    CONVERT(SwitchPro) \
    CONVERT(SwitchJoyConL) \
    CONVERT(SwitchJoyConR) \
    CONVERT(SwitchJoyConPair) \
    CONVERT(Xbox360) \
    CONVERT(XboxOne) \
    CONVERT(Apple) \
    CONVERT(Android) \
    CONVERT(TouchScreen) \

enum ControllerType
{
#define CONVERT(x) ct##x,
    CONTROLLER_TYPE_LIST
#undef CONVERT

    ctNum
};
// if this is exceeded the bitfield HIDDeviceInfo::type needs to be expanded
COMPILE_ASSERT(ctNum - 1 <= 0x1F);

char const* ControllerName(uint32_t type);
bool HIDIsController(hid_device_info *dev);
bool HIDIsSupported(hid_device_info *dev, uint8_t* outType);
bool HIDIsSupported(uint16_t vendor, uint16_t product);


