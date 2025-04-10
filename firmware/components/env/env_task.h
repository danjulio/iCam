/*
 * Environmental monitoring task - Detect and initialize optional AHT20 temp/humidtity
 * and/or VS53L4CX distance sensor and report data to t1c_task for correction.
 *
 * Copyright 2024 Dan Julio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef ENV_TASK_H
#define ENV_TASK_

#include <stdbool.h>
#include <stdint.h>


//
// Constants
//

// Evaluation intervals
#define ENV_TASK_EVAL_MSEC       100
#define ENV_TASK_READ_T_H_MSEC   2000
#define ENV_TASK_READ_DIST_MSEC  500



//
// API
//
void env_task();
void env_sensor_present(bool* temp_hum, bool* dist);

#endif /* ENV_TASK_H */