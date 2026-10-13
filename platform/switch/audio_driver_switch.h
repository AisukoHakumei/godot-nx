/**************************************************************************/
/*  audio_driver_switch.h                                                 */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "core/templates/vector.h"
#include "servers/audio/audio_server.h"

// libnx audout: one 48 kHz stereo int16 stream fed by a dedicated thread.
// Three 480-frame buffers (about 20 to 30 ms queued). Every wait has a
// timeout so a device change can never hang the mixer.
class AudioDriverSwitch : public AudioDriver {
public:
	static constexpr int MIX_RATE = 48000;
	static constexpr int BUFFER_FRAMES = 480;
	static constexpr int BUFFER_COUNT = 3;
	static constexpr uint64_t WAIT_TIMEOUT_NS = 100ULL * 1000ULL * 1000ULL;

private:
	static AudioDriverSwitch *singleton;

	struct Impl;
	Impl *impl = nullptr;

	Mutex mutex;
	Thread thread;
	SafeFlag exit_thread;
	SafeFlag paused;
	bool initialized = false;

	Vector<int32_t> mix_buffer;

	void _fill_and_append(int p_index);
	static void _thread_func(void *p_udata);

public:
	static AudioDriverSwitch *get_singleton() { return singleton; }

	virtual const char *get_name() const override { return "audout"; }

	virtual Error init() override;
	virtual void start() override;
	virtual int get_mix_rate() const override { return MIX_RATE; }
	virtual SpeakerMode get_speaker_mode() const override { return SPEAKER_MODE_STEREO; }
	virtual float get_latency() override { return 0.03f; }
	virtual void lock() override;
	virtual void unlock() override;
	virtual void finish() override;

	void set_paused(bool p_paused);

	AudioDriverSwitch();
	~AudioDriverSwitch();
};
