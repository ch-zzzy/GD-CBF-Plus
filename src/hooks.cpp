#include "global.hpp"

static double s_frameStartTime = 0.0;

class $modify(CCEGLView) {
	// copied from cbf lol
	void pollEvents() {
		PlayLayer* playLayer = PlayLayer::get();
		CCNode* parent;

		s_frameStartTime = geode::utils::getInputTimestamp();

		// clang-format off
		if (!GetFocus() || !playLayer
			|| !(parent = playLayer->getParent())
			|| parent->getChildByType<PauseLayer>(0)
			|| playLayer->getChildByType<EndLevelLayer>(0)
			|| playLayer->m_playerDied)
		{
			g_firstFrame = true;
		}
		// clang-format on

		CCEGLView::pollEvents();
	}
};

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		bool result = PlayLayer::init(level, useReplay, dontCreateObjects);
		if (!result) return false;

		this->m_clickBetweenSteps = false;
		this->m_clickOnSteps = false;

		g_levelStartTimestamp = geode::utils::getInputTimestamp();
		g_tickCount = 0;
		g_inputChecksCount = 0;
		g_firstFrame = true;
		g_inputQueue.clear();

		g_p1LastEventTimestamp = g_levelStartTimestamp;
		g_p2LastEventTimestamp = g_levelStartTimestamp;

		g_modActive = !g_mod->getSettingValue<bool>("mod-disabled");
		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		g_firstFrame = true;
		g_tickCount = 0;
		g_inputChecksCount = 0;
		g_levelStartTimestamp = geode::utils::getInputTimestamp();
		g_inputQueue.clear();

		g_p1LastEventTimestamp = g_levelStartTimestamp;
		g_p2LastEventTimestamp = g_levelStartTimestamp;
	}

	void onQuit() {
		g_modActive = false;
		PlayLayer::onQuit();
	}
};

class $modify(GJBaseGameLayer) {
	int checkCollisions(PlayerObject* object, float dt, bool ignoreDamage) {
		int result = GJBaseGameLayer::checkCollisions(object, dt, ignoreDamage);

		if (g_modActive) {
			PlayLayer* playLayer = PlayLayer::get();
			if (playLayer) {
				onPostCollision(object, playLayer);
			}
		}

		return result;
	}

	void update(float dt) {
		PlayLayer* playLayer = PlayLayer::get();

		if (!g_modActive || !playLayer || !playLayer->m_player1 ||
			this->m_isPlatformer || this->m_useReplay) {
			GJBaseGameLayer::update(dt);
			return;
		}

		if (g_firstFrame) {
			g_firstFrame = false;
			g_levelStartTimestamp = s_frameStartTime;
			g_p1LastEventTimestamp = g_levelStartTimestamp;
			g_p2LastEventTimestamp = g_levelStartTimestamp;
			GJBaseGameLayer::update(dt);
			return;
		}

		if (playLayer->m_playerDied) {
			g_firstFrame = true;
			GJBaseGameLayer::update(dt);
			return;
		}

		g_inputQueue.insert(g_inputQueue.end(),
			playLayer->m_queuedButtons.begin(),
			playLayer->m_queuedButtons.end());
		playLayer->m_queuedButtons.clear();

		std::sort(g_inputQueue.begin(), g_inputQueue.end(),
			[](PlayerButtonCommand const& a, PlayerButtonCommand const& b) {
				return a.m_timestamp < b.m_timestamp;
			});

		GJBaseGameLayer::update(dt);

		double frameEnd = geode::utils::getInputTimestamp();
		PlayerObject* p1 = playLayer->m_player1;
		PlayerObject* p2 = playLayer->m_gameState.m_isDualMode
			? playLayer->m_player2
			: nullptr;

		if (!p1->m_isDashing) {
			advancePlayerToTimestamp(p1, frameEnd, g_p1LastEventTimestamp);
		}
		if (p2 && !p2->m_isDashing) {
			advancePlayerToTimestamp(p2, frameEnd, g_p2LastEventTimestamp);
		}

		g_inputQueue.clear();
	}
};

class $modify(PlayerObject) {
	void update(float dt) {
		auto* playLayer = PlayLayer::get();
		if (!playLayer || !g_modActive || this->m_isDashing) {
			PlayerObject::update(dt);
			return;
		}

		bool isPlayer1 = this->isPlayer1();
		double& lastEventTimestamp =
			isPlayer1 ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;

		double tickTimestamp =
			g_levelStartTimestamp + g_tickCount * (1.0 / g_tps);

		processInputsUpToTimestamp(tickTimestamp, this, playLayer, isPlayer1);

		advancePlayerToTimestamp(this, tickTimestamp, lastEventTimestamp);

		float savedX = this->getPositionX();
		float savedY = this->getPositionY();
		double savedVel = this->m_yVelocity;

		// call vanilla function to handle lots of stuff
		PlayerObject::update(dt);

		// overwrite changes to position and velocity
		this->setPosition({savedX, savedY});
		this->m_yVelocity = savedVel;

		if (isPlayer1) {
			g_tickCount++;
		}
	}

	void setYVelocity(double velocity, int type) {
		// probably redundant since velocity is overwritten and quantizeYVelocity would handle it
		if (g_velocityUnroundingEnabled) {
			this->m_yVelocity = velocity;
			return;
		}
		PlayerObject::setYVelocity(velocity, type);
	}
};