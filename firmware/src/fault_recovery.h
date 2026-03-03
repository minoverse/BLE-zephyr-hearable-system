#ifndef FAULT_RECOVERY_H
#define FAULT_RECOVERY_H

void fault_recovery_init(void);
void fault_recovery_report_failure(const char *component);
const char* fault_recovery_get_state_string(void);

#endif
