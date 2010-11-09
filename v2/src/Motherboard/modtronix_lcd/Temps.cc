
#include "LCDConfigs.hh"
#include "Tool.hh"
#include "Motherboard.hh"
#include "Commands.hh"

namespace temps {

static const uint8_t currentToolIndex = 0;

uint16_t last_head_temp;
uint16_t last_platform_temp;
uint16_t targ_head_temp;
uint16_t targ_platform_temp;
int8_t   targ_extruder_dir;


uint16_t getLastHeadTemp()
{
  return last_head_temp;
}


uint16_t getLastPlatformTemp()
{
  return last_platform_temp;
}


uint16_t getTargetHeadTemp()
{
  return targ_head_temp;
}


uint16_t getTargetPlatformTemp()
{
  return targ_platform_temp;
}


int8_t getExtruderDir()
{
  return targ_extruder_dir;
}


void pollCurrentTemps()
{
  for (;;) {
    if (tool::getLock()) {
      OutPacket& out = tool::getOutPacket();
      InPacket& in = tool::getInPacket();
      out.reset();
      out.append8(currentToolIndex);
      out.append8(SLAVE_CMD_GET_TEMP);
      tool::startTransaction();
      while (!tool::isTransactionDone()) {
	tool::runToolSlice();
      }
      if (!in.hasError()) {
	last_head_temp = in.read16(1);
      }
      tool::releaseLock();
      break;
    }
  }
  for (;;) {
    if (tool::getLock()) {
      OutPacket& out = tool::getOutPacket();
      InPacket& in = tool::getInPacket();
      out.reset();
      out.append8(lcdcfg::HEATED_PLATFORM_TOOL);
      out.append8(SLAVE_CMD_GET_PLATFORM_TEMP);
      tool::startTransaction();
      while (!tool::isTransactionDone()) {
	tool::runToolSlice();
      }
      if (!in.hasError()) {
	last_platform_temp = in.read16(1);
      }
      tool::releaseLock();
      break;
    }
  }
}


void sniff()
{
  OutPacket& out = tool::getOutPacket();
  switch(out.read8(1)) {
    case SLAVE_CMD_SET_TEMP:
	targ_head_temp = out.read16(2);
        break;
    case SLAVE_CMD_SET_PLATFORM_TEMP:
	targ_platform_temp = out.read16(2);
        break;
    case SLAVE_CMD_TOGGLE_MOTOR_1:
	uint8_t flags = out.read8(2);
	if (flags & 0x1) {
	  targ_extruder_dir = (flags & 0x2)? 1 : -1;
	} else {
	  targ_extruder_dir = 0;
	}
        break;
  }
}


}

