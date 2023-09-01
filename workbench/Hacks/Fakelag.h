#pragma once
#include <deque>

struct command_fk
{
	int previous_command_number = 0;
	int cmd_number = 0;
	bool is_used = false;
	bool is_outgoing = false;
};

namespace Fakelag
{
	void run(bool& sendPacket) noexcept;
	std::deque <command_fk> commands_fakelag;
}