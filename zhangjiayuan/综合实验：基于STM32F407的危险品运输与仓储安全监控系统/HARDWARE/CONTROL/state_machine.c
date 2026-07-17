#include "state_machine.h"
const char *StateMachine_Name(SystemState state)
{
    switch (state) {
    case SYS_SELF_CHECK: return "STATE_INIT";
    case SYS_WAIT_LOGIN: return "STATE_LOGIN";
    case SYS_IDLE: return "STATE_IDLE";
    case SYS_LOADING: return "STATE_LOAD";
    case SYS_TRANSPORTING: return "STATE_TRANSPORT";
    case SYS_WARNING: return "STATE_WARNING";
    case SYS_DANGER: return "STATE_ALARM";
    case SYS_MAINTENANCE: return "STATE_SETTING";
    default: return "STATE_UNKNOWN";
    }
}
