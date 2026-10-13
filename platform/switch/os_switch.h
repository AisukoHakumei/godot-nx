/**************************************************************************/
/*  os_switch.h                                                           */
/**************************************************************************/

#pragma once

#include "core/os/os.h"

class MainLoop;

// Horizon OS layer. Derives from OS directly: OS_Unix pulls in dynamic
// loading, pipes, fork and signals that do not exist on libnx.
class OS_Switch : public OS {
	MainLoop *main_loop = nullptr;
	String argv0;
	String start_cwd;
	bool user_dir_writable = true;
	uint64_t ticks_start = 0;

	String _user_dir() const;

public:
	OS_Switch();
	virtual ~OS_Switch();

	void set_argv0(const char *p_argv0);
	String get_argv0() const { return argv0; }
	void set_user_dir_writable(bool p_writable) { user_dir_writable = p_writable; }
	bool is_user_dir_writable() const { return user_dir_writable; }

	void run();

	void initialize_core();
	virtual void initialize() override;
	virtual void initialize_joypads() override;
	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual void delete_main_loop() override;
	virtual void finalize() override;
	virtual void finalize_core() override;
	virtual MainLoop *get_main_loop() const override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;

	virtual String get_name() const override;
	virtual String get_distribution_name() const override;
	virtual String get_version() const override;
	virtual Vector<String> get_video_adapter_driver_info() const override;
	virtual int get_processor_count() const override;

	virtual String get_stdin_string(int64_t p_buffer_size = 1024) override;
	virtual PackedByteArray get_stdin_buffer(int64_t p_buffer_size = 1024) override;
	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override;

	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	virtual Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override;
	virtual Error kill(const ProcessID &p_pid) override;
	virtual bool is_process_running(const ProcessID &p_pid) const override;
	virtual int get_process_exit_code(const ProcessID &p_pid) const override;

	virtual bool has_environment(const String &p_var) const override;
	virtual String get_environment(const String &p_var) const override;
	virtual void set_environment(const String &p_var, const String &p_value) const override;
	virtual void unset_environment(const String &p_var) const override;

	virtual DateTime get_datetime(bool p_utc = false) const override;
	virtual TimeZoneInfo get_time_zone_info() const override;
	virtual void delay_usec(uint32_t p_usec) const override;
	virtual uint64_t get_ticks_usec() const override;

	virtual String get_locale() const override;
	virtual String get_executable_path() const override;
	virtual String get_user_data_dir(const String &p_user_dir) const override;
	virtual String get_data_path() const override;
	virtual String get_config_path() const override;
	virtual String get_cache_path() const override;
	virtual String get_temp_path() const override;

	virtual void alert(const String &p_alert, const String &p_title = "ALERT!") override;
};
