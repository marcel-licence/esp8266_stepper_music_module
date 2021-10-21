/*
 * The GNU GENERAL PUBLIC LICENSE (GNU GPLv3)
 *
 * Copyright (c) 2021 Marcel Licence
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
 * der GNU General Public License, wie von der Free Software Foundation,
 * Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
 * veröffentlichten Version, weiter verteilen und/oder modifizieren.
 *
 * Dieses Programm wird in der Hoffnung bereitgestellt, dass es nützlich sein wird, jedoch
 * OHNE JEDE GEWÄHR,; sogar ohne die implizite
 * Gewähr der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 * Siehe die GNU General Public License für weitere Einzelheiten.
 *
 * Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
 * Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
 */

/*
 * a simple implementation to use midi
 *
 * Author: Marcel Licence
 */


/*
 * look for midi interface using 1N136
 * to convert the MIDI din signal to
 * a uart compatible signal
 */
#define RXD2 16 /* U2RRXD */
#define TXD2 17

/* use define to dump midi data */
//#define DUMP_SERIAL2_TO_SERIAL

/* constant to normalize midi value to 0.0 - 1.0f */
#define NORM127MUL	0.007874f

inline void Midi_NoteOn(uint8_t ch, uint8_t note)
{
    Synth_NoteOn(ch, note);
}

inline void Midi_NoteOff(uint8_t ch, uint8_t note)
{
    Synth_NoteOff(ch, note);
}

/*
 * this function will be called when a control change message has been received
 */
inline void Midi_ControlChange(uint8_t channel, uint8_t data1, uint8_t data2)
{
    if ((data1 == 17) && (channel == 0))
    {
        Synth_PortTime(data2 * NORM127MUL);
    }

    if ((data1 == 17) && (channel == 1))
    {
        Synth_ModulationSpeed(data2 * NORM127MUL);
    }

    if ((data1 == 17) && (channel == 2))
    {
        Synth_ModulationPitch(data2 * NORM127MUL);
    }

    if ((data1 == 17) && (channel == 3))
    {
        Synth_ArpActive(data2);
    }

    if ((data1 == 17) && (channel == 4))
    {
        Synth_ModulationType(data2);
    }

    if (data1 == 1)
    {
        Synth_ModulationWheel(channel, data2 * NORM127MUL);
    }

#if 0
    if (data1 == 17)
    {
        if (channel < 10)
        {
            Synth_SetSlider(channel,  data2 * NORM127MUL);
        }
    }
    if ((data1 == 18) && (channel == 1))
    {
        Synth_SetSlider(8,  data2 * NORM127MUL);
    }

    if ((data1 == 16) && (channel < 9))
    {
        Synth_SetRotary(channel, data2 * NORM127MUL);

    }
    if ((data1 == 18) && (channel == 0))
    {
        Synth_SetRotary(8,  data2 * NORM127MUL);
    }
#endif
}

inline void Midi_PitchBend(uint8_t ch, uint16_t bend)
{
    float value = ((float)bend - 8192.0f) * (1.0f / 8192.0f) - 1.0f;
    Synth_PitchBend(ch, value);
}

/*
 * function will be called when a short message has been received over midi
 */
inline void HandleShortMsg(uint8_t *data)
{
    uint8_t ch = data[0] & 0x0F;

    switch (data[0] & 0xF0)
    {
    /* note on */
    case 0x90:
        if (data[2] > 0)
        {
            Midi_NoteOn(ch, data[1]);
        }
        else
        {
            Midi_NoteOff(ch, data[1]);
        }
        break;
    /* note off */
    case 0x80:
        Midi_NoteOff(ch, data[1]);
        break;
    /* control change */
    case 0xb0:
        Midi_ControlChange(ch, data[1], data[2]);
        break;
    /* pitchbend */
    case 0xe0:
        Midi_PitchBend(ch, ((((uint16_t)data[1]) ) + ((uint16_t)data[2] << 8)));
    }
}

/*
 * stopping serial line
 * and setup for midi
 */
void Midi_Setup()
{
    Serial.end();
    delay(500);
    Serial.begin(31250);
    delay(500);
    Serial.swap();
    delay(500);
}


/*
 * this function should be called continuously to ensure that incoming messages can be processed
 */
void Midi_Process()
{
    /*
     * watchdog to avoid getting stuck by receiving incomplete or wrong data
     */
    static uint32_t inMsgWd = 0;
    static uint8_t inMsg[3];
    static uint8_t inMsgIndex = 0;

    if (Serial.available())
    {
        uint8_t incomingByte = Serial.read();

#ifdef DUMP_SERIAL2_TO_SERIAL
        Serial.printf("%02x", incomingByte);
#endif
        /* ignore live messages */
        if ((incomingByte & 0xF0) == 0xF0)
        {
            return;
        }

        if (inMsgIndex == 0)
        {
            if ((incomingByte & 0x80) != 0x80)
            {
                inMsgIndex = 1;
            }
        }

        inMsg[inMsgIndex] = incomingByte;
        inMsgIndex += 1;

        if (inMsgIndex >= 3)
        {
#ifdef DUMP_SERIAL2_TO_SERIAL
            Serial.printf(">%02x %02x %02x\n", inMsg[0], inMsg[1], inMsg[2]);
#endif
            HandleShortMsg(inMsg);
            inMsgIndex = 0;
        }

        /*
         * reset watchdog to allow new bytes to be received
         */
        inMsgWd = 0;
    }
    else
    {
        if (inMsgIndex > 0)
        {
            inMsgWd++;
            if (inMsgWd == 0xFFFF)
            {
                inMsgIndex = 0;
            }
        }
    }

}

