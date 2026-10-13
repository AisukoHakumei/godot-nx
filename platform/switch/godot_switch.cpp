/**************************************************************************/
/*  godot_switch.cpp                                                      */
/**************************************************************************/

#include "os_switch.h"
#include "switch_wrapper.h"

#include "main/main.h"

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

// hbloader's main thread has a fixed 1 MiB stack; the engine runs on its own.
static constexpr size_t ENGINE_THREAD_STACK_BYTES = 8 * 1024 * 1024;

struct EngineArgs {
	OS_Switch *os = nullptr;
	int argc = 0;
	char **argv = nullptr;
	int exit_code = EXIT_SUCCESS;
};

static void *engine_main(void *p_arg) {
	EngineArgs *args = static_cast<EngineArgs *>(p_arg);

	const char *exec_path = args->argc > 0 ? args->argv[0] : "";
	Error err = Main::setup(exec_path, args->argc > 0 ? args->argc - 1 : 0, args->argc > 0 ? &args->argv[1] : nullptr);
	if (err != OK) {
		args->exit_code = (err == ERR_HELP) ? EXIT_SUCCESS : EXIT_FAILURE;
		return nullptr;
	}

	if (Main::start() == EXIT_SUCCESS) {
		args->os->run();
	} else {
		args->exit_code = EXIT_FAILURE;
	}

	Main::cleanup();
	return nullptr;
}

int main(int argc, char **argv) {
#ifdef DEBUG_ENABLED
	bool sockets = R_SUCCEEDED(socketInitializeDefault());
	if (sockets) {
		nxlinkStdio(); // No-op unless launched over nxlink.
		setvbuf(stdout, nullptr, _IOLBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
	}
#endif

	OS_Switch os;
	os.set_argv0(argc > 0 ? argv[0] : nullptr);

	EngineArgs args;
	args.os = &os;
	args.argc = argc;
	args.argv = argv;

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, ENGINE_THREAD_STACK_BYTES);
	pthread_t engine_thread;
	int rc = pthread_create(&engine_thread, &attr, engine_main, &args);
	pthread_attr_destroy(&attr);
	if (rc != 0) {
		fprintf(stderr, "Failed to create the engine thread (%d).\n", rc);
		args.exit_code = EXIT_FAILURE;
	} else {
		pthread_join(engine_thread, nullptr);
	}

#ifdef DEBUG_ENABLED
	if (sockets) {
		socketExit();
	}
#endif
	return args.exit_code;
}
