
#ifndef GAINPUTMOUSEINFO_H_
#define GAINPUTMOUSEINFO_H_

namespace gainput
{

namespace
{
struct DeviceButtonInfo
{
	ButtonType type;
	const char* name;
};

DeviceButtonInfo deviceButtonInfos[] =
{
		{ BT_BOOL, "mouse_left" },
		{ BT_BOOL, "mouse_middle" },
		{ BT_BOOL, "mouse_right" },
		{ BT_BOOL, "mouse_3" },
		{ BT_BOOL, "mouse_4" },
		{ BT_BOOL, "mouse_5" },
		{ BT_BOOL, "mouse_6" },
		{ BT_BOOL, "mouse_7" },
		{ BT_BOOL, "mouse_8" },
		{ BT_BOOL, "mouse_9" },
		{ BT_BOOL, "mouse_10" },
		{ BT_BOOL, "mouse_11" },
		{ BT_BOOL, "mouse_12" },
		{ BT_BOOL, "mouse_13" },
		{ BT_BOOL, "mouse_14" },
		{ BT_BOOL, "mouse_15" },
		{ BT_BOOL, "mouse_16" },
		{ BT_BOOL, "mouse_17" },
		{ BT_BOOL, "mouse_18" },
		{ BT_BOOL, "mouse_19" },
		{ BT_BOOL, "mouse_20" },
		{ BT_FLOAT, "mouse_x" },
		{ BT_FLOAT, "mouse_y" }
};
}


}

#endif

