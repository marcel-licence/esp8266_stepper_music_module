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
 * Main project file of
 * ESP-8266 stepper motor midi music module (ESP32 "synthesizer" Arduino project)
 *
 * youtube: https://youtu.be/NWeLQI_MDSE
 *
 * Selected board: LOLIN(WEMOS) D1 R2 & mini
 *
 * Author: Marcel Licence
 */

// defines pins numbers
const int stepPin1 = 0; /* D3 - connect this to STEP */
const int stepPin2 = 12; /* D6 - connect this to STEP or piezzo speaker driver */
const int dirPin = 4; /* D2 - connect this to DIR */
const int ledPin = 2; /* D4 - connect this to /ENA */

/*
 * connect /RESET to /SLEEP
 * otherwise the A4988 will not work
 */

/*
 * structure containing all data for a single mono voice
 */
struct mono_voice
{
    uint8_t noteList[256]; /*!< storage of played notes before note off to remember all pressed keys */
    uint8_t noteCount = 0; /*!< count of notes in note list */
    float oldNote = 0; /*!< last active midi note */
    uint32_t activeNote = 0; /*!< midi note of the active note */
    float port = 1.0f; /*!< portamento position, controls the sweep; 0.0: old note, 1.0: new note */

    float bendValue = 0.0f; /*!< pitch bending */
    float modulationDepth = 0.0f; /*!< depth controlled by modulation wheel, range 0..1 */
    float modulationSpeed = 8.0f; /*!< modulation speed in Hz */
    float modulationPitch = 1.0f; /*!< modulation pitch, one equals one half note */

    int pin; /*!< pin for output */
};

/*
 * we use two channels here
 */
struct mono_voice MonoVoice;
struct mono_voice MonoVoice2;

/*
 * only channel 1 supports the direction which will be changed when all notes go off
 */
uint8_t dir = 0;


/* daft punk formula here: https://www.youtube.com/watch?v=W1cpim5EAqI */

#define ARP_STEPS 16

uint8_t arpSelected = 0; /*!< selected riff, 0 means no playback */

uint8_t arp[4][ARP_STEPS] =
{
    {0, 12, 24, 0, 12, 24, 0, 12, 24, 0, 12, 24, 0, 12, 24, 36}, /* popcorn bass riff */
    {0, 0, 12, 0, 12, 12, 0, 12, 0, 0, 12, 0, 12, 12, 0, 12},
    {12 + 0, 36 + 3, 24 + 0, 36 - 2, 24 - 4, 12 - 6, 12 - 6, 24 - 6, 36 - 4, 12 - 4, 36 - 2, 36 + 00, 12 + 1, 12 + 0, 12 - 7, 24 + 00}, /* daft punk - da funk */
    {0, 0, 12, 7, 0, 0, 12, 7, 0, 0, 12, 7, 0, 0, 7, 12 }
};

/*
 * this data adds options to the riff playback
 * 0 - short note
 * 1 - long note without slide
 * 2 - long note -> slide
 */
uint8_t arp_sld[4][ARP_STEPS] =
{
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1},
    {0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2},
    {0, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 2, 0, 2, 2, 2},
    {2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 0},
};


float portAdd = 0.01f; /*!< speed of portamento */
uint8_t modulationType = 0; /*! 0: sine, 1: square */

void Voice_Setup()
{
    /* test tone on startup to ensure motor is working */
    digitalWrite(ledPin, LOW);
    tone(stepPin1, 440, 0);
    delay(500);
    tone(stepPin1, 880, 0);
    delay(500);
    noTone(stepPin1);
    digitalWrite(ledPin, HIGH);
    delay(500);

    digitalWrite(ledPin, LOW);
    tone(stepPin2, 440, 0);
    delay(500);
    tone(stepPin2, 880, 0);
    delay(500);
    noTone(stepPin2);
    digitalWrite(ledPin, HIGH);
    delay(500);

    /*
     * assign output pins
     * - do not assign multiple voice to the same pin, that would not work
     */
    MonoVoice.pin = stepPin1;
    MonoVoice2.pin = stepPin2;
}


void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.setTimeout(1);

    // Sets the two pins as Outputs
    pinMode(stepPin1, OUTPUT);
    pinMode(dirPin, OUTPUT);
    pinMode(ledPin, OUTPUT);

    /*
     * little cleanup
     */
    for (int i = 0; i < 256; i++)
    {
        MonoVoice.noteList[i] = 0;
        MonoVoice.noteCount = 0;
    }

    Midi_Setup();

    Voice_Setup();
}


#ifdef MODULATION_BY_PITCH /* unused code */
float MultiplikatorFromVoice(struct mono_voice *const voice)
{
    float noteA = voice->oldNote;
    uint8_t noteB = voice->activeNote;
    float port = voice->port;

    float note = (((float)(noteA)) * (1.0f - port) + ((float)(noteB)) * port) - 69.0f + voice->bendValue;
    float f = ((pow(2.0f, note / 12.0f)));
    return f;
}
#endif

/*
 * calculate the modulation pitch value of a voice
 */
float Modulation(struct mono_voice *const voice)
{
#ifdef MODULATION_BY_PITCH
    float modSpeed = voice->modulationSpeed * MultiplikatorFromVoice(voice);
#else
    float modSpeed = voice->modulationSpeed;
#endif
    if (modulationType == 0)
    {
        return voice->modulationDepth * voice->modulationPitch * (sin((modSpeed * ((float)millis()) * 2.0f * PI / 1000.0f)));
    }
    else
    {
        return voice->modulationDepth * voice->modulationPitch * (sin((modSpeed * ((float)millis()) * 2.0f * PI / 1000.0f)) >= 0 ? 0.0f : 1.0f);
    }
}

/*
 * get the actual frequency of a voice including calculation of portamento and modulation
 */
float FrequencyFromVoice(struct mono_voice *const voice)
{
    float noteA = voice->oldNote;
    uint8_t noteB = voice->activeNote;
    float port = voice->port;

    float note = (((float)(noteA)) * (1.0f - port) + ((float)(noteB)) * port) - 69.0f + voice->bendValue + Modulation(voice);
    float f = ((pow(2.0f, note / 12.0f) * 440.0f));
    return f;
}

/*
 * process a voice and update the output
 */
void Voice_Process(struct mono_voice *const voice, uint64_t elapsed_ms)
{
    static uint32_t count = 0;

    count++;

    if (voice->noteCount > 0)
    {
        if (voice->port < 1.0f)
        {
            voice->port += portAdd * ((float)elapsed_ms);
        }
        else
        {
            /* do not overshoot the active note */
            voice->port = 1.0f;
        }

        if (voice->noteCount > 0)
        {
            tone(voice->pin, FrequencyFromVoice(voice));
        }
    }
}

static uint8_t arpNote = 0xFF;
static bool arp_active = false;
static uint64_t arp_cnt = 0;
static uint64_t arp_act = 0;
static uint32_t arp_pos = 0;
static uint32_t arp_key = 0;

/* this define controls the arpeggio playback speed */
#define ARP_MAX 120

void Arp_Process(uint64_t elapsed_ms)
{
    struct mono_voice *const voice = &MonoVoice;

    arp_cnt += elapsed_ms;

    /*
     * stop short notes
     */
    if (arp_cnt > (ARP_MAX / 2))
    {
        if (arpNote != 0xFF)
        {
            if ((voice->noteCount > 0) && (arp_sld[arpSelected][arp_act] == 0))
            {
                Motor_NoteOff(&MonoVoice, voice->activeNote);
            }
        }
    }

    if (arp_cnt > ARP_MAX)
    {
        arp_cnt = 0;
        if (arpNote != 0xFF)
        {
            /*
             * stop note to avoid portamento
             */
            if ((voice->noteCount > 0) && (arp_sld[arpSelected][arp_act] == 1))
            {
                Motor_NoteOff(&MonoVoice, voice->activeNote);
            }

            Motor_NoteOn(&MonoVoice, arpNote + arp[arpSelected][arp_pos]);

            /*
             * note off after note on to get portamento effect
             */
            if ((voice->noteCount > 1) && (arp_sld[arpSelected][arp_act] == 2))
            {
                Motor_NoteOff(&MonoVoice, voice->oldNote);
            }

            arp_act = arp_pos;
            arp_pos ++;
            if (arp_pos >= ARP_STEPS)
            {
                arp_pos = 0;
            }
        }
    }
}

void Arp_NoteOn(uint8_t note)
{
    arpNote = note;
    if (arp_key == 0)
    {
        arp_pos = 0;
        arp_act = sizeof(arp) - 1;
        arp_cnt = ARP_MAX; /* if to high then it will overflow in next arp process call */
    }
    arp_key ++;
}

void Arp_NoteOff(uint8_t note)
{
    if (arp_key == 1)
    {
        Motor_NoteOff(&MonoVoice, MonoVoice.activeNote);
        arpNote = 0xFF;
    }
    arp_key --;
}

void Synth_ArpActive(float value)
{
    uint8_t val8 = (value * 5.0f);
    if (val8 == 0)
    {
        arp_active = false;
    }
    else
    {
        arpSelected = (val8 - 1) % 4;
        arp_active = true;
    }
}

static uint64_t elapsed_ms = 0;
static uint64_t uptime = 0;

void loop()
{
    uint64_t now = millis();
    elapsed_ms = now - elapsed_ms;
    uptime += elapsed_ms;

    Midi_Process();

    if (arp_active)
    {
        Arp_Process(elapsed_ms);
    }

    Voice_Process(&MonoVoice, elapsed_ms);
    Voice_Process(&MonoVoice2, elapsed_ms);

    elapsed_ms = now; /* remove this line then sound gets crazy with arp */
}


void AllNotesOff(struct mono_voice *const voice)
{
    noTone(voice->pin);
    if (dir == 0)
    {
        digitalWrite(dirPin, HIGH);
    }
    else
    {
        digitalWrite(dirPin, LOW);
    }
    dir = ~dir;

    /*
     * check if all notes are off
     */
    if ((MonoVoice.noteCount == 0) && (MonoVoice2.noteCount == 0))
    {
        digitalWrite(ledPin, HIGH);
    }
}

struct mono_voice *const voiceFromCh(uint8_t ch)
{
    if (ch == 0)
    {
        return &MonoVoice;
    }
    else
    {
        return &MonoVoice2;
    }
}

void Synth_ModulationWheel(uint8_t ch, float value)
{
    struct mono_voice *const voice = voiceFromCh(ch);

    voice->modulationDepth = value;
}

void Synth_ModulationSpeed(float value)
{
    struct mono_voice *const voice = &MonoVoice;

    float min = 0.01f; /* 0.1 Hz */
    float max = 100.0f; /* 100 Hz */

    voice->modulationSpeed = (pow(2.0f, value) - 1.0f) * (max - min) + min;
}

void Synth_ModulationPitch(float value)
{
    struct mono_voice *const voice = &MonoVoice;

    float min = 0.1f; /* 5 cent */
    float max = 24.0f; /* two octaves */

    voice->modulationPitch = (pow(2.0f, value) - 1.0f) * (max - min) + min;
}

void Synth_ModulationType(float value)
{
    modulationType = value * 2;
}

void Synth_PitchBend(uint8_t ch, float value)
{
    struct mono_voice *const voice = voiceFromCh(ch);

    voice->bendValue = value;
}

void Synth_PortTime(float value)
{
    float min = 0.02f; /* 1/(0.02 * 1000) -> 0.05s */
    float max = 0.0002f; /* 1/(0.0002 * 1000) -> 5s */

    portAdd = (pow(2.0f, value) - 1.0f) * (max - min) + min;
}

void Motor_NoteOn(struct mono_voice *const voice, uint8_t note)
{
    if (voice->noteCount > 0)
    {
        voice->oldNote = voice->port * ((float)voice->activeNote) + (1.0f - voice->port) * voice->oldNote;
        voice->port = 0.0f;
    }
    else
    {
        voice->port = 1.0f;
    }

    if (voice->noteCount < 255) /* one less otherwise we have an overflow */
    {
        voice->noteList[voice->noteCount] = note;
        voice->activeNote = note;
        voice->noteCount ++;
    }

    digitalWrite(ledPin, LOW);
    voice->activeNote = note;
}

void Motor_NoteOff(struct mono_voice *const voice, uint8_t note)
{
    uint8_t *entryToRemove = NULL;

    for (int i = 0; i < 256; i++)
    {
        if (voice->noteList[i] == note)
        {
            /*
             * shift all entries and remove only the one we do not need anymore
             */
            for (int j = i; j < 255; j++)
            {
                voice->noteList[j] = voice->noteList[j + 1];
            }
            break;
        }
    }

    if (voice->noteCount > 0)
    {
        voice->noteCount --;

        if (voice->noteCount > 0)
        {
            voice->oldNote = voice->port * ((float)voice->activeNote) + (1.0f - voice->port) * voice->oldNote;
            voice->activeNote = voice->noteList[voice->noteCount - 1];
            voice->port = 0;

            //digitalWrite(ledPin, LOW);
        }
        else
        {
            /*
             * no active notes left
             */

            AllNotesOff(voice);
        }
    }
}

void Synth_NoteOn(uint8_t ch, uint8_t note)
{
    if (ch == 0)
    {
        if (arp_active)
        {
            Arp_NoteOn(note);
        }
        else
        {
            Motor_NoteOn(&MonoVoice, note);
        }
    }
    else
    {
        Motor_NoteOn(&MonoVoice2, note);
    }
}

void Synth_NoteOff(uint8_t ch, uint8_t note)
{
    if (ch == 0)
    {
        if (arp_active)
        {
            Arp_NoteOff(note);
        }
        else
        {
            Motor_NoteOff(&MonoVoice, note);
        }
    }
    else
    {
        Motor_NoteOff(&MonoVoice2, note);
    }
}

