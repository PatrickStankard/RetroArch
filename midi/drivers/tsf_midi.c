/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2020 The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libretro.h>
#include <rthreads/rthreads.h>
#include <lists/string_list.h>

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../midi_driver.h"

#define TSF_IMPLEMENTATION
#include <tsf/tsf.h>

typedef struct
{
   tsf *instance;
#ifdef HAVE_THREADS
   slock_t *synth_lock;
#endif
} tsf_midi_t;

#ifdef HAVE_THREADS
#define TSF_MIDI_SYNTH_LOCK() slock_lock(d->synth_lock)
#define TSF_MIDI_SYNTH_UNLOCK() slock_unlock(d->synth_lock)
#else
#define TSF_MIDI_SYNTH_LOCK()
#define TSF_MIDI_SYNTH_UNLOCK()
#endif

/* Midi channel message types */
enum
{
   TSF_MIDI_NOTE_OFF         = 0x80,
   TSF_MIDI_NOTE_ON          = 0x90,
   TSF_MIDI_KEY_PRESSURE     = 0xA0,
   TSF_MIDI_CONTROL_CHANGE   = 0xB0,
   TSF_MIDI_PROGRAM_CHANGE   = 0xC0,
   TSF_MIDI_CHANNEL_PRESSURE = 0xD0,
   TSF_MIDI_PITCH_BEND       = 0xE0,
   TSF_MIDI_SYSEX            = 0xF0,
};

static bool tsf_midi_get_avail_inputs(struct string_list *inputs)
{
   return true;
}

static bool tsf_midi_get_avail_outputs(struct string_list *outputs)
{
   /** The output name is irrelevant for TSF but RetroArch depends on the
    *  user entering a correct string, so allow a few names.
    **/
   union string_list_elem_attr attr = {0};
   string_list_append(outputs, "SF2", attr);
   string_list_append(outputs, "sf2", attr);
   string_list_append(outputs, "GM", attr);
   string_list_append(outputs, "gm", attr);
   return true;
}

static void tsf_synth(void* p, float* buffer, size_t num_frames, float volume)
{
   tsf_midi_t *d = (tsf_midi_t*)p;

   TSF_MIDI_SYNTH_LOCK();
   tsf_render_float(d->instance, buffer, num_frames, 1);
   TSF_MIDI_SYNTH_UNLOCK();
}

static void *tsf_midi_init(const char *input, const char *output)
{
   settings_t *settings = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   char sf2path[PATH_MAX_LENGTH];

   if (!output)
      return NULL;

   fill_pathname_join(sf2path, settings->paths.directory_system, "GM.SF2", sizeof(sf2path));
   tsf* instance = tsf_load_filename(sf2path);
   if (!instance)
      return NULL;

   // Set synth mode and sample rate
   tsf_set_output(instance, TSF_STEREO_INTERLEAVED, (int)av_info->timing.sample_rate, 0);

   tsf_midi_t *d = (tsf_midi_t*)calloc(sizeof(tsf_midi_t), 1);
   d->instance = instance;
#ifdef HAVE_THREADS
   d->synth_lock = slock_new();
#endif

   audio_mixer_play_synth(tsf_synth, d);
   audio_driver_mixer_set_active();

   return d;
}

static void tsf_midi_free(void *p)
{
   tsf_midi_t *d = (tsf_midi_t*)p;

   if (d)
   {
      tsf_close(d->instance);
#ifdef HAVE_THREADS
      slock_free(d->synth_lock);
#endif
      free(d);
   }
}

static bool tsf_midi_set_input(void *p, const char *input)
{
   return false;
}

static bool tsf_midi_set_output(void *p, const char *output)
{
   return true;
}

static bool tsf_midi_read(void *p, midi_event_t *event)
{
   return false;
}

static bool tsf_midi_write(void *p, const midi_event_t *event)
{
   tsf_midi_t *d = (tsf_midi_t*)p;
   uint8_t channel, p1, p2;

   if (event->data_size < 2)
      return false;

   channel = (event->data[0] & 0x0f);
   p1 = (event->data[1] & 0x7f);
   p2 = (event->data_size >= 3 ? (event->data[2] & 0x7f) : 0);

   TSF_MIDI_SYNTH_LOCK();
   switch (event->data[0] & 0xf0)
   {
      case TSF_MIDI_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
         tsf_channel_set_presetnumber(d->instance, channel, p1, (channel == 9));
         break;
      case TSF_MIDI_NOTE_ON: //play a note
         tsf_channel_note_on(d->instance, channel, p1, p2 / 127.0f);
         break;
      case TSF_MIDI_NOTE_OFF: //stop a note
         tsf_channel_note_off(d->instance, channel, p1);
         break;
      case TSF_MIDI_PITCH_BEND: //pitch wheel modification
         tsf_channel_set_pitchwheel(d->instance, channel, (p2 << 7) | p1);
         break;
      case TSF_MIDI_CONTROL_CHANGE: //MIDI controller messages
         tsf_channel_midi_control(d->instance, channel, p1, p2);
         break;
      case TSF_MIDI_SYSEX: //System exclusive messages (handles only midi_driver_set_volume call)
         if (event->data_size == 8 && !memcmp(event->data+1, "\x7F\x7F\x04\x01", 4))
            tsf_set_volume(d->instance, (((event->data[6] & 0x7F) << 7) | (event->data[5] & 0x7F)) / 16383.0f);
         break;
   }
   TSF_MIDI_SYNTH_UNLOCK();
   return true;
}

static bool tsf_midi_flush(void *p)
{
   return true;
}

midi_driver_t midi_tsf = {
   "TinySoundFont",
   tsf_midi_get_avail_inputs,
   tsf_midi_get_avail_outputs,
   tsf_midi_init,
   tsf_midi_free,
   tsf_midi_set_input,
   tsf_midi_set_output,
   tsf_midi_read,
   tsf_midi_write,
   tsf_midi_flush
};
