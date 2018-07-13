
#ifndef GAINPUTTOUCHINFO_H_
#define GAINPUTTOUCHINFO_H_

namespace gainput
{

namespace
{
struct DeviceButtonInfo
{
	ButtonType type;
	const char* name;
};

DeviceButtonInfo const deviceButtonInfos[] =
{
	{ BT_BOOL, "touch_0_down" },
	{ BT_FLOAT, "touch_0_x" },
	{ BT_FLOAT, "touch_0_y" },
	{ BT_FLOAT, "touch_0_pressure" },
	{ BT_BOOL, "touch_1_down" },
	{ BT_FLOAT, "touch_1_x" },
	{ BT_FLOAT, "touch_1_y" },
	{ BT_FLOAT, "touch_1_pressure" },
	{ BT_BOOL, "touch_2_down" },
	{ BT_FLOAT, "touch_2_x" },
	{ BT_FLOAT, "touch_2_y" },
	{ BT_FLOAT, "touch_2_pressure" },
	{ BT_BOOL, "touch_3_down" },
	{ BT_FLOAT, "touch_3_x" },
	{ BT_FLOAT, "touch_3_y" },
	{ BT_FLOAT, "touch_3_pressure" },
	{ BT_BOOL, "touch_4_down" },
	{ BT_FLOAT, "touch_4_x" },
	{ BT_FLOAT, "touch_4_y" },
	{ BT_FLOAT, "touch_4_pressure" },
	{ BT_BOOL, "touch_5_down" },
	{ BT_FLOAT, "touch_5_x" },
	{ BT_FLOAT, "touch_5_y" },
	{ BT_FLOAT, "touch_5_pressure" },
	{ BT_BOOL, "touch_6_down" },
	{ BT_FLOAT, "touch_6_x" },
	{ BT_FLOAT, "touch_6_y" },
	{ BT_FLOAT, "touch_6_pressure" },
	{ BT_BOOL, "touch_7_down" },
	{ BT_FLOAT, "touch_7_x" },
	{ BT_FLOAT, "touch_7_y" },
	{ BT_FLOAT, "touch_7_pressure" }
};
}

const unsigned TouchPointCount = 8;
const unsigned TouchDataElems = 4;

}

#endif

