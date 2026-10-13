/**************************************************************************/
/*  ip_switch.h                                                           */
/**************************************************************************/

#pragma once

#include "core/io/ip.h"

// Networking is out of scope for the first milestones (spec section 8), but
// the engine requires an IP singleton at startup. This one resolves nothing.
class IPSwitch : public IP {
	GDCLASS(IPSwitch, IP);

	virtual void _resolve_hostname(List<IPAddress> &r_addresses, const String &p_hostname, Type p_type = TYPE_ANY) const override {}
	static IP *_create_switch() { return memnew(IPSwitch); }

public:
	virtual void get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const override {}
	static void make_default() { _create = _create_switch; }
};
