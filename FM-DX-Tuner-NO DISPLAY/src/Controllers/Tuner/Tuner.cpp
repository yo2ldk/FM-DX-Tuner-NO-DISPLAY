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

#include <Arduino.h>
#include "Tuner.hpp"
#include "../../Utils/Utils.hpp"
#include "../../Protocol.h"

void
Tuner::setup()
{
    this->driver.setup();
}

void
Tuner::hello()
{
    this->driver.hello();
}

void
Tuner::loop()
{
    this->driver.loop();

    if (this->scan.isActive())
    {
        this->scan.process();
        return;
    }

    this->handleRds();

    if (this->driver.getQuality())
    {
        if (timerQuality.process(Timer::Continous))
        {
            this->handleQuality();
        }
    }
    
    if (timerSquelch.process(Timer::Continous))
    {
        this->handleSquelch();
    }
}

const Command*
Tuner::getCommands(uint8_t *len)
{
    static constexpr Command commands[] =
    {
        { FMDX_TUNER_PROTOCOL_CANCEL, &this->cbCancel },
        { FMDX_TUNER_PROTOCOL_MODE, &this->cbMode },
        { FMDX_TUNER_PROTOCOL_TUNE, &this->cbFrequency },
        { FMDX_TUNER_PROTOCOL_DEEMPHASIS, &this->cbDeemphasis },
        { FMDX_TUNER_PROTOCOL_AGC, &this->cbAgc },
        { FMDX_TUNER_PROTOCOL_ALIGNMENT, &this->cbAlignment },
        { FMDX_TUNER_PROTOCOL_BANDWIDTH, &this->cbBandwidth },
        { FMDX_TUNER_PROTOCOL_VOLUME, &this->cbVolume },
        { FMDX_TUNER_PROTOCOL_SQUELCH, &this->cbSquelch },
        { FMDX_TUNER_PROTOCOL_OUTPUT_MODE, &this->cbOutputMode },
        { FMDX_TUNER_PROTOCOL_QUALITY, &this->cbQuality },
        { FMDX_TUNER_PROTOCOL_SCAN, &this->cbScan },
        { FMDX_TUNER_PROTOCOL_CUSTOM, &this->cbCustom }
    };
    
    *len = sizeof(commands) / sizeof(Command);
    return commands;
}

bool
Tuner::start()
{
    if (this->driver.start())
    {
        constexpr Timer::Interval qualityInterval = 66;
        this->timerQuality.set(qualityInterval);

        constexpr Timer::Interval squelchInterval = 50;
        this->timerSquelch.set(squelchInterval);

        return true;
    }

    return false;
}

void
Tuner::shutdown()
{
    this->driver.shutdown();

    this->timerQuality.unset();
    this->timerSquelch.unset();

#ifdef ARDUINO_ARCH_AVR
    /* Release SDA and SCL lines used by hardware I2C */
    TWCR = 0;
#endif
}

void
Tuner::clear()
{
    this->driver.resetQuality();
    this->driver.resetRds();
    this->driver.piBuffer.clear();
    this->driver.rdsBuffer.clear();
}

void
Tuner::handleQuality()
{
    const auto pilot = this->driver.getQualityStereo(this->qualityMode);
    const auto rssi = this->driver.getQualityRssi(this->qualityMode);
    const auto cci = this->driver.getQualityCci(this->qualityMode);
    const auto aci = this->driver.getQualityAci(this->qualityMode);

    Serial.print('S');

    if (this->stereo)
    {
        Serial.print(pilot ? 's' : 'm');
    }
    else
    {
        Serial.print(pilot ? 'S' : 'M');
    }

    Utils::serialDecimal(rssi, 2);

    Serial.print(',');
    Serial.print(cci, DEC);
    Serial.print(',');
    Serial.print(aci, DEC);
    Serial.print('\n');
}

void
Tuner::handleRds()
{
    uint16_t pi;
    RdsPiBuffer::State piState = this->driver.piBuffer.get(pi);
    if (piState != RdsPiBuffer::STATE_INVALID)
    {
        Serial.print('P');
        Utils::serialHexPi(pi, (uint8_t)piState);
        Serial.print('\n');
    }

    uint16_t *rdsData;
    uint8_t rdsStatus;
    if (this->driver.rdsBuffer.get(&rdsData, &rdsStatus))
    {
        Serial.print('R');
        Utils::serialHex16(rdsData[0]);
        Utils::serialHex16(rdsData[1]);
        Utils::serialHex16(rdsData[2]);
        Utils::serialHex16(rdsData[3]);
        Utils::serialHex8(rdsStatus);
        Serial.print('\n');
    }
}

void
Tuner::handleSquelch()
{
    constexpr TunerDriver::QualityMode qualityMode = TunerDriver::QUALITY_DEFAULT;

    int8_t value;
    switch (this->squelch.getMode())
    {
        case SquelchMode::SQUELCH_RSSI:
            value = (int8_t)(this->driver.getQualityRssi(qualityMode) / 100);
            break;

        case SquelchMode::SQUELCH_STEREO:
            value = (int8_t)this->driver.getQualityStereo(qualityMode);
            break;

        case SquelchMode::SQUELCH_CCI:
            value = (int8_t)(this->driver.getQualityCci(qualityMode));
            break;

        default:
            return;
    }

    this->squelch.process(value);
}

bool
Tuner::cbCancel(Controller *instance,
             const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    (void)args;

    /* Cancel any ongoing asynchronous operation */

    /* Currently only scan is asynchronous */
    tuner->scan.stop();

    return true;
}

bool
Tuner::cbMode(Controller *instance,
              const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const Mode value = (Mode)atol(args);

    const uint32_t prevFrequency = tuner->driver.getFrequency();
    const uint32_t prevStep = tuner->driver.getFrequency();
    if (tuner->driver.setMode(value))
    {
        tuner->clear();
        tuner->feedback(FMDX_TUNER_PROTOCOL_MODE, value);

        const uint32_t newFrequency = tuner->driver.getFrequency();
        const uint32_t newStep = tuner->driver.getStep();
        if (prevFrequency != newFrequency ||
            prevStep != newStep)
        {
            tuner->feedback2(FMDX_TUNER_PROTOCOL_TUNE, newFrequency, newStep);
        }

        return true;
    }
    
    return false;
}

bool
Tuner::cbFrequency(Controller *instance,
                   const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    const Mode prevMode = tuner->driver.getMode();
    const uint32_t prevFrequency = tuner->driver.getFrequency();
    if (tuner->driver.setFrequency(value, TunerDriver::TUNE_DEFAULT))
    {
        const uint32_t newFrequency = tuner->driver.getFrequency();
        const uint32_t step = tuner->driver.getStep();
        const int32_t diff = prevFrequency - newFrequency;
        if (abs(diff) > step)
        {
            tuner->clear();
        }

        const uint32_t newMode = tuner->driver.getMode();
        if (prevMode != newMode)
        {
            tuner->feedback(FMDX_TUNER_PROTOCOL_MODE, newMode);
        }

        tuner->feedback2(FMDX_TUNER_PROTOCOL_TUNE, newFrequency, step);
        return true;
    }

    return false;
}

bool
Tuner::cbDeemphasis(Controller *instance,
                    const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    if (tuner->driver.setDeemphasis(value))
    {
        tuner->feedback(FMDX_TUNER_PROTOCOL_DEEMPHASIS, value);
        return true;
    }

    return false;
}

bool
Tuner::cbAgc(Controller *instance,
             const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    if (tuner->driver.setAgc(value))
    {
        tuner->feedback(FMDX_TUNER_PROTOCOL_AGC, value);
        return true;
    }

    return false;
}

bool
Tuner::cbAlignment(Controller *instance,
                   const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    if (tuner->driver.setAlignment(value))
    {
        tuner->feedback(FMDX_TUNER_PROTOCOL_ALIGNMENT, value);
        return true;
    }

    return false;
}

bool
Tuner::cbBandwidth(Controller *instance,
                   const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    if (tuner->driver.setBandwidth(value))
    {
        tuner->feedback(FMDX_TUNER_PROTOCOL_BANDWIDTH, tuner->driver.getBandwidth());
        return true;
    }

    return false;
}

bool
Tuner::cbVolume(Controller *instance,
                const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);
    
    if (value <= 100)
    {
        tuner->volume.setVolume((uint8_t)value);

        tuner->feedback(FMDX_TUNER_PROTOCOL_VOLUME, value);
        return true;
    }
    
    return false;
}

bool
Tuner::cbSquelch(Controller *instance,
                 const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const int32_t value = atol(args);

    if (value < 0)
    {
        tuner->squelch.set(SQUELCH_STEREO, 1);
    }
    else if (value == 0)
    {
        tuner->squelch.set(SQUELCH_NONE, 0);
    }
    else
    {
        tuner->squelch.set(SQUELCH_RSSI, value);
    }

    tuner->feedback(FMDX_TUNER_PROTOCOL_SQUELCH, value);
    return true;
}

bool
Tuner::cbOutputMode(Controller *instance,
                    const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const OutputMode value = (OutputMode)atol(args);

    if (tuner->driver.setOutputMode(value))
    {
        tuner->feedback(FMDX_TUNER_PROTOCOL_OUTPUT_MODE, value);
        tuner->stereo = (value == OUTPUT_MODE_STEREO);
        return true;
    }

    return false;
}

bool
Tuner::cbQuality(Controller *instance,
                 const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);
    const uint32_t value = atol(args);

    if (value <= 1000)
    {
        tuner->timerQuality.set(value);
        tuner->feedback(FMDX_TUNER_PROTOCOL_QUALITY, value);
        return true;
    }

    return false;
}

bool
Tuner::cbScan(Controller *instance,
              const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);

    switch (args[0])
    {
        case 'm':
        case '\0':
            tuner->scan.setRepeat(args[0] == 'm');
            return tuner->scan.start();

        case 'a':
            tuner->scan.setFrom(atol(args+1));
            return true;

        case 'b':
            tuner->scan.setTo(atol(args+1));
            return true;

        case 'c':
            tuner->scan.setStep(atol(args+1));
            return true;

        case 'w':
            tuner->scan.setBandwidth(atol(args+1));
            return true;

        case 'z':
            /* TODO: Antenna */
            break;
    }

    return false;
}

bool
Tuner::cbCustom(Controller *instance,
                const char *args)
{
    Tuner *tuner = (Tuner*)instance;
    tuner->cbCancel(instance, NULL);

    if (tuner->driver.setCustom("custom", args))
    {
        Serial.print(FMDX_TUNER_PROTOCOL_CUSTOM);
        Serial.print(args);
        Serial.print('\n');
        return true;
    }

    return false;
}

void
Tuner::feedback(const char *message,
                uint32_t    value)
{
    Serial.print(message);
    Serial.print(value, DEC);
    Serial.print('\n');
}


void
Tuner::feedback2(const char *message,
                 uint32_t    value,
                 uint32_t    value2)
{
    Serial.print(message);
    Serial.print(value, DEC);
    Serial.print(",");
    Serial.print(value2, DEC);
    Serial.print('\n');
}

