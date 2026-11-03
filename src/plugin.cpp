#include "runtime/BackendRuntime.h"

namespace
{
	using namespace AbsoluteControl::Runtime;

	const char* StateName(State a_state)
	{
		switch (a_state) {
		case State::Unavailable: return "unavailable";
		case State::Starting: return "starting";
		case State::Ready: return "ready";
		case State::Degraded: return "degraded";
		case State::Stopping: return "stopping";
		default: return "unknown";
		}
	}

	void StartInputRuntime()
	{
		static bool attempted = false;
		if (std::exchange(attempted, true)) {
			return;
		}

		// Two DirectInput pollers would fight over the same flight controls.
		// The rewrite replaces AbsoluteHOTAS; it never runs beside it.
		if (GetModuleHandleW(L"AbsoluteHOTAS.dll")) {
			REX::ERROR("Input runtime not started: AbsoluteHOTAS.dll is loaded. Disable AbsoluteHOTAS to use this build.");
			return;
		}
		if (GetModuleHandleW(L"AbsoluteControlPanel.dll")) {
			REX::WARN("AbsoluteControlPanel.dll (the 0.3 menu host) is also loaded; it is not needed by this build.");
		}

		if (!NativeBackend().Start(MakeDirectInputProvider())) {
			REX::ERROR("Input runtime failed to start: {}", NativeBackend().Failure());
			return;
		}
		REX::INFO("Input runtime starting.");
	}

	void LogInventory()
	{
		auto& backend = NativeBackend();
		const auto snapshot = backend.Devices().Snapshot();
		REX::INFO("Input runtime {}; {} device(s).", StateName(backend.Lifecycle()), snapshot.devices.size());
		for (const auto& description : snapshot.devices) {
			REX::INFO("  {} ({} channels)", description.device.displayName, description.channels.size());
		}
		if (const auto failure = backend.Failure(); !failure.empty()) {
			REX::WARN("Input runtime failure: {}", failure);
		}
	}

	void OnSfseMessage(SFSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}
		if (a_message->type == SFSE::MessagingInterface::kPostPostLoad) {
			StartInputRuntime();
		} else if (a_message->type == SFSE::MessagingInterface::kPostDataLoad) {
			LogInventory();
		}
	}
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
	SFSE::Init(a_sfse);

	const auto messaging = SFSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(&OnSfseMessage)) {
		REX::CRITICAL("Could not subscribe to SFSE messaging; input runtime disabled.");
		return false;
	}

	REX::INFO("Absolute Control loaded; input runtime starts at post-post-load.");
	return true;
}
