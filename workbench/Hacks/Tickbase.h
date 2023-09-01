#pragma once

struct UserCmd;

namespace Tickbase
{
	bool isPeeking{ false };
	float realTime{ 0.0f };
	int targetTickShift{ 0 };
	int tickShift{ 0 };
	int shiftCommand{ 0 };
	int shiftedTickbase{ 0 };
	int ticksAllowedForProcessing{ 0 };
	int chokedPackets{ 0 };
	int pauseTicks{ 0 };
	bool shifting{ false };
	bool finalTick{ false };
	bool hasHadTickbaseActive{ false };
	int nextShiftAmount = 0;

	int isRechargeTime{ 0 };
	bool isRecharging{ false };

	bool sendPacket{ false };
	int DeftargetTickShift{ 0 };

	void getCmd(UserCmd* cmd);
	void start(UserCmd* cmd) noexcept;
	void end(UserCmd* cmd) noexcept;
	bool shiftTicks(UserCmd* cmd, int shiftAmountt) noexcept;
	void breakLagComp(UserCmd* cmd);
	bool canRun() noexcept;
	bool canShift(int shiftAmount) noexcept;
	int adjust_tick_base(const int old_new_cmds, const int total_new_cmds, const int delta) noexcept;
	int getCorrectTickbase(int commandNumber) noexcept;
	int& pausedTicks() noexcept;	
	int getTargetTickShift() noexcept;
	int getTickshift() noexcept;
	void resetTickshift() noexcept;
	bool& isFinalTick() noexcept;
	bool& isShifting() noexcept;
	void updateInput() noexcept;
	void reset() noexcept;
}