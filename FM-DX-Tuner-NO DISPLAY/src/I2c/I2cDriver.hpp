/*  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  FM-DX Tuner
 *  Copyright (C) 2023  Konrad Kosmatka 
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 3
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef FMDX_TUNER_I2C_DRIVER_H
#define FMDX_TUNER_I2C_DRIVER_H

#ifdef ARDUINO_ARCH_AVR
#include "I2cAvr.hpp"
#define I2cDriver I2cAvr
#else
#include "I2cWire.hpp"
#define I2cDriver I2cWire
#endif

#endif
