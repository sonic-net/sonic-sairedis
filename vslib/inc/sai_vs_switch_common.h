#ifndef __SAI_VS_SWITCH_COMMON__
#define __SAI_VS_SWITCH_COMMON__

#include "sai_vs_state.h"
#include <memory>

sai_status_t set_switch_mac_address(std::shared_ptr<SwitchState> ss);

#endif // __SAI_VS_SWITCH_COMMON__
