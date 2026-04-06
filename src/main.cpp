#include "global.hpp"

$on_mod(Loaded) {
	g_tps = Mod::get()->getSettingValue<float>("tps");
	updateDeltaTime();
	listenForSettingChanges<float>(
		"tps", +[](float val) {
			g_tps = val;
			updateDeltaTime();
		});

	g_inputHz = Mod::get()->getSettingValue<float>("input-hz");
	listenForSettingChanges<float>(
		"input-hz", +[](float val) { g_inputHz = val; });

	g_velocityUnroundingEnabled =
		Mod::get()->getSettingValue<bool>("velocity-unrounding");
	toggleVelocityUnroundingPatches(g_velocityUnroundingEnabled);
	listenForSettingChanges<bool>(
		"velocity-unrounding", +[](bool val) {
			g_velocityUnroundingEnabled = val;
			toggleVelocityUnroundingPatches(val);
		});

	g_subframesEnabled = Mod::get()->getSettingValue<bool>("subframes-enabled");
	listenForSettingChanges<bool>(
		"subframes-enabled", +[](bool val) {
			g_subframesEnabled = val;
			updateDeltaTime();
		});

	g_modActive = !Mod::get()->getSettingValue<bool>("mod-disabled");
	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) { g_modActive = !val; });
}