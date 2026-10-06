#include "wsa/model.hpp"
#include <windows.h>
#include <powrprof.h>
namespace wsa {
void acquire_power(Audit& audit) {
    SYSTEM_POWER_CAPABILITIES caps{};
    if (!GetPwrCapabilities(&caps)) {
        audit.power.source.status = "unavailable";
        audit.required_failure = true;
        audit.warnings.push_back(error_code("GetPwrCapabilities", GetLastError()));
        return;
    }
    auto& p = audit.power;
    p.source.status = "ok";
    p.s1 = caps.SystemS1 != FALSE; p.s2 = caps.SystemS2 != FALSE; p.s3 = caps.SystemS3 != FALSE;
    p.s4 = caps.SystemS4 != FALSE; p.hiber_file = caps.HiberFilePresent != FALSE;
    p.modern_standby = caps.AoAc != FALSE; p.wake_alarm = caps.WakeAlarmPresent != FALSE;
}
}
