#include "global.hpp"

static double g_frameStartTime = 0.0;

class $modify(CCEGLView) {
	void pollEvents() {
		PlayLayer* playLayer = PlayLayer::get();

		if (playLayer) {
			CCNode* parent = playLayer->getParent();
			bool shouldReset = !parent ||
				parent->getChildByType<PauseLayer>(0) ||
				playLayer->getChildByType<EndLevelLayer>(0) || !GetFocus() ||
				playLayer->m_playerDied;

			if (shouldReset) {
				g_firstFrame = true;
			}
		}

		g_frameStartTime = geode::utils::getInputTimestamp();
		CCEGLView::pollEvents();
	}
};

class $modify(PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		bool result = PlayLayer::init(level, useReplay, dontCreateObjects);
		if (!result) return false;

		g_percentageToggleModEnabled =
			Loader::get()->isModLoaded("zsa.percentage-toggle");

		this->m_clickBetweenSteps = false;
		this->m_clickOnSteps = false;

		g_levelStartTimestamp = geode::utils::getInputTimestamp();
		g_tickCount = 0;
		g_inputChecksCount = 0;
		g_firstFrame = true;
		g_inputQueue.clear();
		g_inputIdx = 0;

		g_p1LastEventTimestamp = g_levelStartTimestamp;
		g_p2LastEventTimestamp = g_levelStartTimestamp;

		g_modActive = true;
		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		g_firstFrame = true;
		g_tickCount = 0;
		g_inputChecksCount = 0;
		g_levelStartTimestamp = geode::utils::getInputTimestamp();
		g_inputQueue.clear();
		g_inputIdx = 0;

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
				if (object == playLayer->m_player1) {
					onPostCollision(object, playLayer);
				} else if (object == playLayer->m_player2) {
					onPostCollision(object, playLayer);
				}
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
			g_levelStartTimestamp = g_frameStartTime;
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

		// Capture and sort inputs for this frame
		g_inputQueue.insert(g_inputQueue.end(),
			playLayer->m_queuedButtons.begin(),
			playLayer->m_queuedButtons.end());
		playLayer->m_queuedButtons.clear();

		std::sort(g_inputQueue.begin(), g_inputQueue.end(),
			[](PlayerButtonCommand const& a, PlayerButtonCommand const& b) {
				return a.m_timestamp < b.m_timestamp;
			});

		g_inputIdx = 0;

		// Let vanilla run its entire step loop
		GJBaseGameLayer::update(dt);

		// Render positions at frame end for smooth sub-tick visuals
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
		g_inputIdx = 0;
	}
};

class $modify(PlayerObject) {
	void update(float dt) {
		PlayLayer* pl = PlayLayer::get();

		if (!g_modActive || !pl) {
			PlayerObject::update(dt);
			return;
		}

		bool useVanilla = true;
		if (this == pl->m_player1) {
			useVanilla = onPlayerTick(this, pl, g_p1LastEventTimestamp);
		} else if (this == pl->m_player2) {
			useVanilla = onPlayerTick(this, pl, g_p2LastEventTimestamp);
		}

		if (useVanilla) {
			PlayerObject::update(dt);
		}
	}

	void setYVelocity(double velocity, int type) {
		if (g_velocityUnroundingEnabled) {
			this->m_yVelocity = velocity;
			return;
		}
		PlayerObject::setYVelocity(velocity, type);
	}
};