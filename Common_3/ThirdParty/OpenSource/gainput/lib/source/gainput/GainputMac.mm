
#include "../../include/gainput/gainput.h"

#ifdef GAINPUT_PLATFORM_MAC

#import <AppKit/AppKit.h>

namespace gainput
{

bool MacIsApplicationKey()
{
    return [[NSApplication sharedApplication] keyWindow ] != nil;
}

}

#endif
