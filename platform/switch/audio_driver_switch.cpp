/**************************************************************************/
/*  audio_driver_switch.cpp                                               */
/**************************************************************************/

#include "audio_driver_switch.h"

#include "switch_helpers.h"
#include "switch_wrapper.h"

#include "core/os/os.h"

#include <malloc.h>

AudioDriverSwitch *AudioDriverSwitch::singleton = nullptr;

struct AudioDriverSwitch::Impl {
	// The service identifies buffers by the address of this struct, so they
	// must not move while queued.
	AudioOutBuffer buffers[BUFFER_COUNT] = {};
	void *memory[BUFFER_COUNT] = {};
};

AudioDriverSwitch::AudioDriverSwitch() {
	singleton = this;
}

AudioDriverSwitch::~AudioDriverSwitch() {
	singleton = nullptr;
}

Error AudioDriverSwitch::init() {
	Result rc = audoutInitialize();
	if (R_FAILED(rc)) {
		WARN_PRINT(vformat("audoutInitialize failed (0x%x); audio disabled.", rc));
		return ERR_CANT_OPEN;
	}
	if (audoutGetSampleRate() != (u32)MIX_RATE || audoutGetChannelCount() != (u32)SwitchHelpers::AUDOUT_CHANNELS || audoutGetPcmFormat() != PcmFormat_Int16) {
		WARN_PRINT(vformat("audout device is %d Hz / %d ch / format %d, expected 48000 / 2 / Int16; audio disabled.", audoutGetSampleRate(), audoutGetChannelCount(), (int)audoutGetPcmFormat()));
		audoutExit();
		return ERR_CANT_OPEN;
	}

	impl = memnew(Impl);
	for (int i = 0; i < BUFFER_COUNT; i++) {
		impl->memory[i] = memalign(0x1000, SwitchHelpers::audout_block_bytes());
		if (impl->memory[i] == nullptr) {
			finish();
			return ERR_OUT_OF_MEMORY;
		}
		memset(impl->memory[i], 0, SwitchHelpers::audout_block_bytes());
		impl->buffers[i].buffer = impl->memory[i];
		impl->buffers[i].buffer_size = SwitchHelpers::audout_block_bytes();
		impl->buffers[i].data_size = SwitchHelpers::audout_buffer_bytes(BUFFER_FRAMES);
		impl->buffers[i].data_offset = 0;
	}

	mix_buffer.resize(BUFFER_FRAMES * SwitchHelpers::AUDOUT_CHANNELS);

	rc = audoutStartAudioOut();
	if (R_FAILED(rc)) {
		WARN_PRINT(vformat("audoutStartAudioOut failed (0x%x); audio disabled.", rc));
		finish();
		return ERR_CANT_OPEN;
	}

	initialized = true;
	return OK;
}

void AudioDriverSwitch::start() {
	if (!initialized) {
		return;
	}
	exit_thread.clear();
	thread.start(_thread_func, this);
}

void AudioDriverSwitch::_fill_and_append(int p_index) {
	AudioOutBuffer *buf = &impl->buffers[p_index];

	lock();
	start_counting_ticks();
	audio_server_process(BUFFER_FRAMES, mix_buffer.ptrw());
	stop_counting_ticks();
	unlock();

	SwitchHelpers::mix_to_int16(mix_buffer.ptr(), (int16_t *)buf->buffer, BUFFER_FRAMES * SwitchHelpers::AUDOUT_CHANNELS);
	buf->data_size = SwitchHelpers::audout_buffer_bytes(BUFFER_FRAMES);
	audoutAppendAudioOutBuffer(buf);
}

void AudioDriverSwitch::_thread_func(void *p_udata) {
	AudioDriverSwitch *ad = static_cast<AudioDriverSwitch *>(p_udata);
	svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B);

	for (int i = 0; i < BUFFER_COUNT; i++) {
		ad->_fill_and_append(i);
	}

	while (!ad->exit_thread.is_set()) {
		if (ad->paused.is_set()) {
			OS::get_singleton()->delay_usec(10000);
			continue;
		}

		AudioOutBuffer *released = nullptr;
		u32 count = 0;
		Result rc = audoutWaitPlayFinish(&released, &count, WAIT_TIMEOUT_NS);
		if (R_FAILED(rc) || count == 0 || released == nullptr) {
			continue; // Timeout (device change, sleep): try again, never block under lock().
		}

		int index = int(released - ad->impl->buffers);
		if (index < 0 || index >= BUFFER_COUNT) {
			continue;
		}
		ad->_fill_and_append(index);
	}
}

void AudioDriverSwitch::lock() {
	mutex.lock();
}

void AudioDriverSwitch::unlock() {
	mutex.unlock();
}

void AudioDriverSwitch::set_paused(bool p_paused) {
	if (p_paused) {
		paused.set();
	} else {
		paused.clear();
	}
}

void AudioDriverSwitch::finish() {
	if (thread.is_started()) {
		exit_thread.set();
		thread.wait_to_finish();
	}
	if (initialized) {
		audoutStopAudioOut();
	}
	if (impl) {
		for (int i = 0; i < BUFFER_COUNT; i++) {
			if (impl->memory[i]) {
				free(impl->memory[i]);
			}
		}
		memdelete(impl);
		impl = nullptr;
	}
	if (initialized || audoutGetSampleRate() != 0) {
		audoutExit();
	}
	initialized = false;
}
