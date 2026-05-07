#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chizz.continuous-physics-api/include/ContinuousPhysics.hpp>

using namespace geode::prelude;
using namespace continuousphysics::prelude;

static auto g_mod = Mod::get();

class $modify(CCEGLView) {
	void pollEvents() {
		PlayLayer* playLayer = PlayLayer::get();
		CCNode* parent;

		// clang-format off
		if (!GetFocus() || !playLayer
			|| !(parent = playLayer->getParent())
			|| parent->getChildByType<PauseLayer>(0)
			|| playLayer->getChildByType<EndLevelLayer>(0)
			|| playLayer->m_playerDied)
		{
			g_physicsState.firstFrame = true;
		}
		// clang-format on

		CCEGLView::pollEvents();
	}
};

class $modify(PlayerObject) {
	void update(float dt) {
		if (useVanillaTick(this)) {
			PlayerObject::update(dt);
			return;
		}
		processInputsUpToTimestamp(this);
		PlayerObject::update(dt);
		postTick(this);
	}
};

$on_mod(Loaded) {
	// TPS
	float tps = g_mod->getSettingValue<float>("tps");
	updateTPS(tps);
	listenForSettingChanges<float>("tps", +[](float val) { updateTPS(val); });

	// Input Hz
	g_inputHz = g_mod->getSettingValue<float>("input-hz");
	listenForSettingChanges<float>(
		"input-hz", +[](float val) { g_inputHz = val; });

	// Velocity unrounding
	g_velocityUnroundingEnabled =
		g_mod->getSettingValue<bool>("velocity-unrounding");
	toggleVelocityUnroundingPatches(g_velocityUnroundingEnabled);
	listenForSettingChanges<bool>(
		"velocity-unrounding", +[](bool val) {
			g_velocityUnroundingEnabled = val;
			toggleVelocityUnroundingPatches(val);
		});

	// Subframes
	g_subframesEnabled = g_mod->getSettingValue<bool>("subframes-enabled");
	listenForSettingChanges<bool>(
		"subframes-enabled", +[](bool val) {
			g_subframesEnabled = val;
			updateTPS(g_mod->getSettingValue<float>("tps"));
		});

	// Mod active
	g_modActive = !g_mod->getSettingValue<bool>("mod-disabled");
	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) { g_modActive = !val; });
}
