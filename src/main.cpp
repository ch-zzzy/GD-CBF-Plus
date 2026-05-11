#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chizz.continuous-physics-api/include/ContinuousPhysics.hpp>

using namespace geode::prelude;
using namespace continuousphysics::prelude;

auto mod = Mod::get();
auto& config = Config::get();
auto& physicsState = ContinuousPhysicsState::get();

class $modify(PlayerObject) {
	void update(float dt) {
		if (useVanillaTick(this)) {
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
		if (!config.isModActive() || physicsState.m_firstFrame) {
			GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
			return;
		} else {
			double tickEnd = this->m_timestamp + dt;
			processInputs(this->m_player1, tickEnd);
			processInputs(this->m_player2, tickEnd);
		}
	}
};

$on_mod(Loaded) {
	// Input Hz
	config.setInputHz(mod->getSettingValue<float>("input-hz"));
	listenForSettingChanges<float>(
		"input-hz", +[](float val) { config.setInputHz(val); });

	// Velocity unrounding
	config.setVelocityUnroundingEnabled(
		mod->getSettingValue<bool>("velocity-unrounding"));
	continuousphysics::patches::toggleVelocityUnroundingPatches(
		config.isVelocityUnroundingEnabled());
	listenForSettingChanges<bool>(
		"velocity-unrounding", +[](bool val) {
			config.setVelocityUnroundingEnabled(val);
			continuousphysics::patches::toggleVelocityUnroundingPatches(val);
		});

	// Mod active
	config.setModActive(!mod->getSettingValue<bool>("mod-disabled"));
	listenForSettingChanges<bool>(
		"mod-disabled", +[](bool val) { config.setModActive(!val); });
}