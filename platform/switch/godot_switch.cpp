/**************************************************************************/
/*  godot_switch.cpp                                                      */
/**************************************************************************/

#include "os_switch.h"
#include "switch_helpers.h"
#include "switch_message.h"
#include "switch_wrapper.h"

#include "main/main.h"

#include <cstdio>
#include <cstdlib>
#include <pthread.h>

// hbloader's main thread has a fixed 1 MiB stack; the engine runs on its own.
static constexpr size_t ENGINE_THREAD_STACK_BYTES = 8 * 1024 * 1024;

struct EngineArgs {
	OS_Switch *os = nullptr;
	const char *exec_path = "";
	int argc = 0; // Arguments after the executable path.
	char **argv = nullptr;
	int exit_code = EXIT_SUCCESS;
};

static void *engine_main(void *p_arg) {
	EngineArgs *args = static_cast<EngineArgs *>(p_arg);

	Error err = Main::setup(args->exec_path, args->argc, args->argv);
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

	// A 3D game cannot live in the applet memory pool; stop before the renderer tries.
	if (!switch_is_application_mode()) {
		switch_show_blocking_message(SWITCH_APPLET_MODE_MESSAGE);
		return EXIT_SUCCESS;
	}

	// Find the project data here, where a console message is still possible,
	// and hand it to the engine as --main-pack so this search is the only one.
	// Plain stdio: Godot's FileAccess is not set up until Main::setup().
	const int user_argc = argc > 1 ? argc - 1 : 0;
	const char **user_argv = argc > 1 ? (const char **)&argv[1] : nullptr;
	const String explicit_pack = SwitchHelpers::main_pack_from_args(user_argc, user_argv);

	CharString pck_path;
	if (!explicit_pack.is_empty()) {
		CharString c = explicit_pack.utf8();
		FILE *f = fopen(c.get_data(), "rb");
		if (f) {
			fclose(f);
			pck_path = c;
		}
		if (pck_path.length() == 0) {
			String msg = "No game data found. --main-pack names a missing file:\n" + explicit_pack;
			fprintf(stderr, "%s\n", msg.utf8().get_data());
			switch_show_blocking_message(msg);
			return EXIT_FAILURE;
		}
	} else {
		for (const String &candidate : SwitchHelpers::pck_candidates(os.get_executable_path(), os.get_start_cwd())) {
			CharString c = candidate.utf8();
			FILE *f = fopen(c.get_data(), "rb");
			if (f) {
				fclose(f);
				pck_path = c;
				break;
			}
		}
		if (pck_path.length() == 0) {
			String msg = SwitchHelpers::missing_pck_message(os.get_executable_path(), os.get_start_cwd());
			fprintf(stderr, "%s\n", msg.utf8().get_data());
			switch_show_blocking_message(msg);
			return EXIT_FAILURE;
		}
	}

	// argv for Main::setup: the user's arguments, with --main-pack <pck> prepended
	// unless the user already supplied one.
	CharString exec_path_utf8 = os.get_executable_path().utf8();
	static char main_pack_flag[] = "--main-pack";
	Vector<char *> engine_argv;
	if (explicit_pack.is_empty()) {
		engine_argv.push_back(main_pack_flag);
		engine_argv.push_back(pck_path.ptrw());
	}
	for (int i = 1; i < argc; i++) {
		engine_argv.push_back(argv[i]);
	}

	EngineArgs args;
	args.os = &os;
	args.exec_path = exec_path_utf8.get_data();
	args.argc = engine_argv.size();
	args.argv = engine_argv.ptrw();

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
