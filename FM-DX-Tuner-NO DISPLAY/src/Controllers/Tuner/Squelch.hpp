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

#ifndef FMDX_TUNER_SQUELCH_H
#define FMDX_TUNER_SQUELCH_H

#include "../../Protocol.h"
#include "TunerDriver.hpp"
#include "Volume.hpp"

class Squelch
{
public:
    Squelch(Volume &_volume, uint8_t _timeout);

    void process(int8_t value);

    void set(SquelchMode _mode, int8_t _threshold);
    SquelchMode getMode(void);
    int8_t getThreshold(void);

private:
    bool isMuted(void);

    Volume &volume;
    SquelchMode mode;
    int8_t threshold;
    uint8_t countdown;
    uint8_t timeout;
};

#endif
