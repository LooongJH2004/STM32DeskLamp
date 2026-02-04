#ifndef __SENSOR_HUB_H
#define __SENSOR_HUB_H

void SensorHub_Init(void);
void SensorHub_Task(void); // 周期性调用 (建议 2秒一次)

#endif
