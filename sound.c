/*
 * Copyright (C) 2026 M. Glargaard, aka graybox
 *
 * This software is provided "as-is", without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would
 *    be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

/*
    sound.c
    Minimal OSS procedural audio.

    Features:
    - procedural ambient drone
    - exit hum increases near exit
    - tiny implementation
    - no external assets

    Usage:

        #include "sound.c"

        sound_init();

        inside main loop:
            sound_update(&game);

        on shutdown:
            sound_shutdown();

*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AUDIO_RATE     44100
#define AUDIO_SAMPLES  1024

#define AUDIO_DT ((double)AUDIO_SAMPLES / AUDIO_RATE)

typedef struct {
    double phase;
    double volume;
    double timer;
    double frequency;
} SoundVoice;

/*     Internal sound state */
typedef struct {
    int fd;

    int sampleRate;

    double phase1;
    double phase2;
    double phase3;

    double humVolume;
	
	double damageTimer;
	double playerspotTimer;
	
	double pingTimer;
	double pingPhase;
	
	SoundVoice ambient;
    SoundVoice ping;
    SoundVoice damage;
    SoundVoice candle;
    SoundVoice playerspot;
	
} SoundState;

/* 	  Global sound state */
static SoundState snd = { .fd = -1 };

/*    Clamp helper */
static double clamp(double v, double min, double max) {
    if(v < min) return min;
    if(v > max) return max;
    return v;
}

int sound_init(void) {
    snd.fd = open(
        "/dev/dsp",
        O_WRONLY | O_NONBLOCK
    );

    if(snd.fd < 0) {
        perror("open /dev/dsp");
        return 0;
    }

    int format = AFMT_S16_LE;

    ioctl(
        snd.fd,
        SNDCTL_DSP_SETFMT,
        &format
    );

    int channels = 1;

    ioctl(
        snd.fd,
        SNDCTL_DSP_CHANNELS,
        &channels
    );

    int rate = AUDIO_RATE;

    ioctl(
        snd.fd,
        SNDCTL_DSP_SPEED,
        &rate
    );

    snd.sampleRate = rate;

    /* ambient hum */
    snd.ambient.volume = 0.15;
    snd.ambient.frequency = 70.0;

    /* enemy ping */
    snd.ping.volume = 0.05;
    snd.ping.frequency = 900.0;

    /* damage pulse */
    snd.damage.volume = 0.01;
    snd.damage.frequency = 120.0;

    printf(
        "audio initialized (%d hz)\n",
        snd.sampleRate
    );

    return 1;
}

/* Generate one audio frame.*/

void sound_update(Game *g) {
    if(snd.fd < 0)
        return;

    int16_t buffer[AUDIO_SAMPLES];

    // EXIT HUM
    double dx = g->exitX - g->player.x;
    double dy = g->exitY - g->player.y;
    double dist = sqrt(dx * dx + dy * dy);
    double targetHum = 0.15 + 1.0 / (1.0 + dist * dist * 0.03);

    targetHum = clamp(targetHum, 0.0, 0.45);

    snd.ambient.volume += (targetHum - snd.ambient.volume) * 0.05;
    //snd.ambient.frequency = 55.0 + targetHum * 40.0;	//not good for low quality soundcard/speakers
    snd.ambient.frequency = 130.0 + targetHum * 60.0;

    // AUDIO GENERATION
    for(int i = 0; i < AUDIO_SAMPLES; i++) {
        double sample = 0.0;

        // AMBIENT DRONE
        double f1 = snd.ambient.frequency;
        double s1 = sin(snd.ambient.phase);
        double s2 = sin(snd.ambient.phase * 2.0);
        double s3 = sin(snd.ambient.phase * 3.0);
        double ambient = s1 * 0.55 + s2 * 0.30 + s3 * 0.15;

        sample += ambient * snd.ambient.volume;

        snd.ambient.phase += 2.0 * M_PI * f1 / snd.sampleRate;

        if(snd.ambient.phase > 2.0 * M_PI)
            snd.ambient.phase -= 2.0 * M_PI;

        // DAMAGE NOISE
        if(snd.damage.timer > 0.0) {
            snd.damage.phase = snd.damage.phase * 0.85 + (((rand() & 255) - 128) * 0.15);

            sample += snd.damage.phase * snd.damage.volume * snd.damage.timer;
        }

        // PLAYER SPOTTED SOUND
        if(snd.playerspot.timer > 0.0) {
            double t = snd.playerspot.timer / 0.5;
            double freq = 700.0 + sin(snd.playerspot.phase * 0.2) * 40.0;
            double ping = sin(snd.playerspot.phase);

            sample += ping * 0.35 * t;

            snd.playerspot.phase += 2.0 * M_PI * freq / snd.sampleRate;
            if(snd.playerspot.phase > 2.0 * M_PI)
                snd.playerspot.phase -= 2.0 * M_PI;
        }

        // SONAR PING
        if(snd.ping.timer > 0.0) {
            double t = snd.ping.timer / 0.4;
            double freq = 900.0 - (1.0 - t) * 500.0;

            sample += sin(snd.ping.phase) * snd.ping.volume * t;

            snd.ping.phase += 2.0 * M_PI * freq / snd.sampleRate;
            if(snd.ping.phase > 2.0 * M_PI)
                snd.ping.phase -= 2.0 * M_PI;
        }

        // SOFT LIMITER
        sample = clamp(sample, -1.0, 1.0);
        buffer[i] = (int16_t)(sample * 24000);
    }

    // TIMERS
    if(snd.damage.timer > 0.0)
        snd.damage.timer -= AUDIO_DT;

    if(snd.playerspot.timer > 0.0)
        snd.playerspot.timer -= AUDIO_DT;

    if(snd.ping.timer > 0.0)
        snd.ping.timer -= AUDIO_DT;

    // WRITE TO OSS
    write(snd.fd, buffer, sizeof(buffer));
}

/*    Shutdown audio */
void sound_shutdown(void) {
    if(snd.fd >= 0) {
        close(snd.fd);
        snd.fd = -1;
    }
}
