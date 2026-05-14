#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chizz.continuous-physics-api/include/ContinuousPhysics.hpp>

using namespace geode::prelude;
using namespace continuousphysics::prelude;

class $modify(PlayerObject) {
	void update(float dt) {
		if (useVanillaPhysics()) {
			PlayerObject::update(dt);
			return;
		}

		preTick(this);
		PlayerObject::update(dt);
		postTick(this, dt);
	}
};

class $modify(GJBaseGameLayer) {
	void processQueuedButtons(float dt, bool clearInputQueue) {
		if (useVanillaPhysics()) {
			GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
			return;
		}

		double tickEnd = static_cast<double>(dt) + this->m_timestamp;

		processInputs(this->m_player1, tickEnd);
		if (this->m_gameState.m_isDualMode && this->m_player2) {
			processInputs(this->m_player2, tickEnd);
		}

		this->m_queuedButtons.clear();
		GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
	}
};

$on_mod(Loaded) {
	auto mod = Mod::get();
	auto& config = Config::get();
	// Input Hz
	config.setInputHz(mod->getSettingValue<float>("input-hz"));
	listenForSettingChanges<float>(
		"input-hz", +[](float val) { Config::get().setInputHz(val); });

	// Velocity unrounding
	config.setVelocityUnroundingEnabled(
		mod->getSettingValue<bool>("velocity-unrounding"));
	toggleVelocityUnroundingPatches(config.isVelocityUnroundingEnabled());
	listenForSettingChanges<bool>(
		"velocity-unrounding", +[](bool val) {
			Config::get().setVelocityUnroundingEnabled(val);
			toggleVelocityUnroundingPatches(val);
		});

	// Mod active
	config.setModActive(!mod->getSettingValue<bool>("mod-disabled"));
	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) {
			Config::get().setModActive(!val);
			PlayLayer* pl = PlayLayer::get();
			if (pl) {
				if (val) {
					// restore vanilla CBS/COS settings
					auto* gm = GameManager::sharedState();
					pl->m_clickBetweenSteps = gm->getGameVariable("0177");
					pl->m_clickOnSteps = gm->getGameVariable("0176");
				} else {
					pl->m_clickBetweenSteps = false;
					pl->m_clickOnSteps = false;
				}
			}
		});
}