/**************************************************************************/
/*  os_switch.cpp                                                         */
/**************************************************************************/

#include "os_switch.h"

#include "switch_helpers.h"
#include "switch_logger.h"
#include "switch_message.h"
#include "switch_wrapper.h"

#include "display_server_switch.h"
#include "ip_switch.h"

#include "main/main.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/main_loop.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "servers/display/display_server.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

OS_Switch::OS_Switch() {
	char buf[1024];
	start_cwd = SwitchHelpers::sd_cwd(getcwd(buf, sizeof(buf)) ? String::utf8(buf) : String());

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ticks_start = uint64_t(ts.tv_sec) * 1000000ULL + uint64_t(ts.tv_nsec) / 1000ULL;

	AudioDriverManager::add_driver(&audio_driver);
}

OS_Switch::~OS_Switch() {
}

void OS_Switch::set_argv0(const char *p_argv0) {
	argv0 = p_argv0 ? String::utf8(p_argv0) : String();
}

void OS_Switch::run() {
	if (!main_loop) {
		return;
	}

	main_loop->initialize();

	while (true) {
		DisplayServer::get_singleton()->process_events();
		if (Main::iteration()) {
			break;
		}
	}

	main_loop->finalize();
}

// Lifecycle.

void OS_Switch::initialize_core() {
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

	IPSwitch::make_default();

	Vector<Logger *> loggers;
	loggers.push_back(memnew(SwitchLogger));
	_set_logger(memnew(CompositeLogger(loggers)));
}

void OS_Switch::initialize() {
	initialize_core();
	DisplayServerSwitch::register_switch_driver();

	// user:// and its subfolders live beside the NRO on the SD card.
	const String user_dir = _user_dir();
	const char *subdirs[] = { "", "cache", "config", "tmp", "logs" };
	for (const char *sub : subdirs) {
		String dir = *sub ? user_dir.path_join(sub) : user_dir;
		if (DirAccess::make_dir_recursive_absolute(dir) != OK && !DirAccess::dir_exists_absolute(dir)) {
			user_dir_writable = false;
			WARN_PRINT(vformat("Cannot create %s; saves and logs are disabled for this session.", dir));
			break;
		}
	}
}

void OS_Switch::initialize_joypads() {
}

void OS_Switch::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_Switch::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_Switch::finalize() {
	delete_main_loop();
}

void OS_Switch::finalize_core() {
}

MainLoop *OS_Switch::get_main_loop() const {
	return main_loop;
}

// Identity.

bool OS_Switch::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "switch" || p_feature == "arm64" || p_feature == "gles3" || p_feature == "s3tc" || p_feature == "bptc";
}

String OS_Switch::get_name() const {
	return "Switch";
}

String OS_Switch::get_distribution_name() const {
	return "Horizon";
}

String OS_Switch::get_version() const {
	u32 v = hosversionGet();
	return vformat("%d.%d.%d", HOSVER_MAJOR(v), HOSVER_MINOR(v), HOSVER_MICRO(v));
}

Vector<String> OS_Switch::get_video_adapter_driver_info() const {
	Vector<String> info;
	info.push_back("nouveau");
	info.push_back("switch-mesa 20.1");
	return info;
}

int OS_Switch::get_processor_count() const {
	return 3; // hbloader exposes cores 0..2; std::thread::hardware_concurrency() returns 0 here.
}

// Stdin and entropy.

String OS_Switch::get_stdin_string(int64_t p_buffer_size) {
	return String();
}

PackedByteArray OS_Switch::get_stdin_buffer(int64_t p_buffer_size) {
	return PackedByteArray();
}

Error OS_Switch::get_entropy(uint8_t *r_buffer, int p_bytes) {
	randomGet(r_buffer, p_bytes);
	return OK;
}

// Processes: Horizon cannot spawn processes.

Error OS_Switch::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_Switch::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_Switch::kill(const ProcessID &p_pid) {
	return ERR_UNAVAILABLE;
}

bool OS_Switch::is_process_running(const ProcessID &p_pid) const {
	return false;
}

int OS_Switch::get_process_exit_code(const ProcessID &p_pid) const {
	return -1;
}

// Environment.

bool OS_Switch::has_environment(const String &p_var) const {
	return getenv(p_var.utf8().get_data()) != nullptr;
}

String OS_Switch::get_environment(const String &p_var) const {
	const char *val = getenv(p_var.utf8().get_data());
	return val ? String::utf8(val) : String();
}

void OS_Switch::set_environment(const String &p_var, const String &p_value) const {
	setenv(p_var.utf8().get_data(), p_value.utf8().get_data(), 1);
}

void OS_Switch::unset_environment(const String &p_var) const {
	unsetenv(p_var.utf8().get_data());
}

// Time.

OS::DateTime OS_Switch::get_datetime(bool p_utc) const {
	time_t t = time(nullptr);
	struct tm lt;
	if (p_utc) {
		gmtime_r(&t, &lt);
	} else {
		localtime_r(&t, &lt);
	}
	DateTime ret;
	ret.year = 1900 + lt.tm_year;
	ret.month = (Month)(lt.tm_mon + 1);
	ret.day = lt.tm_mday;
	ret.weekday = (Weekday)lt.tm_wday;
	ret.hour = lt.tm_hour;
	ret.minute = lt.tm_min;
	ret.second = lt.tm_sec;
	ret.dst = lt.tm_isdst;
	return ret;
}

OS::TimeZoneInfo OS_Switch::get_time_zone_info() const {
	time_t t = time(nullptr);
	struct tm lt;
	localtime_r(&t, &lt);
	char name[16];
	strftime(name, 16, "%Z", &lt);
	name[15] = 0;
	TimeZoneInfo ret;
	ret.name = name;

	char bias_buf[16];
	strftime(bias_buf, 16, "%z", &lt);
	int bias = 0;
	bias_buf[15] = 0;
	sscanf(bias_buf, "%d", &bias);
	int hour = bias / 100;
	int minutes = bias % 100;
	ret.bias = bias < 0 ? hour * 60 - minutes : hour * 60 + minutes;
	return ret;
}

void OS_Switch::delay_usec(uint32_t p_usec) const {
	struct timespec requested = { static_cast<time_t>(p_usec / 1000000), (static_cast<long>(p_usec) % 1000000) * 1000 };
	struct timespec remaining;
	while (nanosleep(&requested, &remaining) == -1 && errno == EINTR) {
		requested.tv_sec = remaining.tv_sec;
		requested.tv_nsec = remaining.tv_nsec;
	}
}

uint64_t OS_Switch::get_ticks_usec() const {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint64_t now = uint64_t(ts.tv_sec) * 1000000ULL + uint64_t(ts.tv_nsec) / 1000ULL;
	return now - ticks_start;
}

// Locale and paths.

String OS_Switch::get_locale() const {
	u64 code = 0;
	SetLanguage lang = SetLanguage_ENUS;
	if (R_FAILED(setGetSystemLanguage(&code)) || R_FAILED(setMakeLanguage(code, &lang))) {
		return "en";
	}
	switch (lang) {
		case SetLanguage_JA:
			return "ja";
		case SetLanguage_FR:
		case SetLanguage_FRCA:
			return "fr";
		case SetLanguage_DE:
			return "de";
		case SetLanguage_IT:
			return "it";
		case SetLanguage_ES:
		case SetLanguage_ES419:
			return "es";
		case SetLanguage_ZHCN:
		case SetLanguage_ZHHANS:
			return "zh_CN";
		case SetLanguage_KO:
			return "ko";
		case SetLanguage_NL:
			return "nl";
		case SetLanguage_PT:
		case SetLanguage_PTBR:
			return "pt";
		case SetLanguage_RU:
			return "ru";
		case SetLanguage_ZHTW:
		case SetLanguage_ZHHANT:
			return "zh_TW";
		default:
			return "en";
	}
}

String OS_Switch::get_executable_path() const {
	return SwitchHelpers::executable_path(argv0, start_cwd);
}

String OS_Switch::_user_dir() const {
	return SwitchHelpers::user_dir_for_executable(get_executable_path());
}

String OS_Switch::get_user_data_dir(const String &p_user_dir) const {
	// One game per folder: user:// is always <game folder>/user, whatever the project name.
	return _user_dir();
}

String OS_Switch::get_data_path() const {
	return _user_dir();
}

String OS_Switch::get_config_path() const {
	return _user_dir().path_join("config");
}

String OS_Switch::get_cache_path() const {
	return _user_dir().path_join("cache");
}

String OS_Switch::get_temp_path() const {
	return _user_dir().path_join("tmp");
}

void OS_Switch::alert(const String &p_alert, const String &p_title) {
	ERR_PRINT(vformat("ALERT: %s: %s", p_title, p_alert));
	if (DisplayServer::get_singleton() == nullptr) {
		// Before the renderer owns the window the console can still show it.
		switch_show_blocking_message(p_title + "\n\n" + p_alert);
	}
}
