#ifndef GAINPUTDEVPROTOCOL_H_
#define GAINPUTDEVPROTOCOL_H_

namespace gainput
{

enum DevCmd
{
	DevCmdHello,
	DevCmdDevice,
	DevCmdDeviceButton,
	DevCmdMap,
	DevCmdRemoveMap,
	DevCmdUserButton,
	DevCmdRemoveUserButton,
	DevCmdPing,
	DevCmdUserButtonChanged,
	DevCmdGetAllInfos,
	DevCmdStartDeviceSync,
	DevCmdSetDeviceButton,
};

const static unsigned DevProtocolVersion = 0x3;

}

#endif

