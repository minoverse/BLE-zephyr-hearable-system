#ifndef NETWORK_STRESS_H
#define NETWORK_STRESS_H

#include <zephyr/kernel.h>

void network_stress_set_queue(struct k_msgq *q);
void network_stress_run(void);

#endif
