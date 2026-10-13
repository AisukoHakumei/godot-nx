/**************************************************************************/
/*  platform_thread.cpp                                                   */
/**************************************************************************/

#include "platform_thread.h"

#include "core/error/error_macros.h"
#include "core/object/script_language.h"
#include "core/string/ustring.h"

SafeNumeric<uint64_t> Thread::id_counter(1); // The first value after .increment() is 2, hence by default the main thread ID should be 1.
thread_local Thread::ID Thread::caller_id = Thread::id_counter.increment();

struct ThreadData {
	Thread::Callback callback;
	void *userdata;
	Thread::ID caller_id;
};

void *Thread::thread_callback(void *p_data) {
	ThreadData *thread_data = static_cast<ThreadData *>(p_data);

	// Set the caller ID for this thread
	caller_id = thread_data->caller_id;

	ScriptServer::thread_enter(); // Scripts may need to attach a stack.

	// Call the actual callback
	thread_data->callback(thread_data->userdata);

	ScriptServer::thread_exit();

	// Clean up
	memdelete(thread_data);

	return nullptr;
}

void Thread::make_main_thread() {
	if (caller_id == MAIN_ID) {
		return; // We're already the main thread
	}
	CRASH_COND_MSG(!is_main_thread_assigned.set_if_clear(), "A second thread attempted to become the main thread.");
	caller_id = MAIN_ID;
}

void Thread::release_main_thread() {
	CRASH_COND_MSG(caller_id != MAIN_ID, "Trying to release main thread from a thread that isn't main.");
	CRASH_COND(!is_main_thread_assigned.clear_if_set());
	caller_id = id_counter.increment();
}

Error Thread::set_name(const String &p_name) {
	// libnx has no pthread_setname_np; thread names are not observable on Horizon.
	return OK;
}

Thread::ID Thread::start(Thread::Callback p_callback, void *p_user, const Settings &p_settings) {
	ERR_FAIL_COND_V_MSG(id != UNASSIGNED_ID, UNASSIGNED_ID, "A Thread object has been re-started without wait_to_finish() having been called on it.");
	id = id_counter.increment();

	ThreadData *thread_data = memnew(ThreadData);
	thread_data->callback = p_callback;
	thread_data->userdata = p_user;
	thread_data->caller_id = id;

	// Create the thread. Priority is ignored: libnx creates every pthread with
	// the preemptive priority 0x3B, which is what Godot's workers want.
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, SWITCH_THREAD_STACK_BYTES);

	int err = pthread_create(&pthread, &attr, thread_callback, thread_data);
	pthread_attr_destroy(&attr);

	if (err != 0) {
		memdelete(thread_data);
		id = UNASSIGNED_ID;
		ERR_FAIL_V_MSG(UNASSIGNED_ID, "pthread_create failed.");
	}

	return id;
}

void Thread::wait_to_finish() {
	ERR_FAIL_COND_MSG(id == UNASSIGNED_ID, "Attempt of waiting to finish on a thread that was never started.");
	ERR_FAIL_COND_MSG(id == get_caller_id(), "Threads can't wait to finish on themselves, another thread must wait.");

	int err = pthread_join(pthread, nullptr);
	if (err != 0) {
		ERR_FAIL_MSG("Thread::wait_to_finish() failed to join thread.");
	}
	pthread = pthread_t();
	id = UNASSIGNED_ID;
}

Thread::~Thread() {
	if (id != UNASSIGNED_ID) {
#ifdef DEBUG_ENABLED
		WARN_PRINT(
				"A Thread object is being destroyed without its completion having been realized.\n"
				"Please call wait_to_finish() on it to ensure correct cleanup.");
#endif
		pthread_detach(pthread);
	}
}
