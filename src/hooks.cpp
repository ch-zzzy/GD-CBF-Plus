#include "global.hpp"

static double g_frameStartTime = 0.0;
static bool g_suppressPlayerUpdate = false;
static bool g_customPhysicsOwnsStep = false;

inline void resetCustomStepFlags() {
	g_customPhysicsOwnsStep = false;
	g_suppressPlayerUpdate = false;
}

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

		this->m_clickBetweenSteps = false;
		this->m_clickOnSteps = false;

		g_levelStartTime = geode::utils::getInputTimestamp();
		g_tickCount = 0;
		g_pollCount = 0;
		g_firstFrame = true;
		resetPhysicsDebugCounters();

		PlayerObject* p1 = this->m_player1;
		CCPoint pos = p1->getPosition();
		int p1SpeedIndex = getSpeedIndex(p1->m_playerSpeed);
		auto p1Speed = SPEED_TABLE[p1SpeedIndex];

		g_p1State = PhysicsState{
			.xPos = pos.x,
			.yPos = pos.y,
			.xSpeed = p1Speed.speedName * p1Speed.speedMult,
			.yVel = p1->m_yVelocity,
			.lastEventTimestamp = g_levelStartTime,
			.Dir = p1->m_isUpsideDown ? -1 : 1,
			.isHolding = false,
			.isOnGround = p1->m_isOnGround,
			.isDashing = false,
			.gamemode = Gamemode::Cube,
			.speedIndex = p1SpeedIndex,
			.mGravity = p1Speed.gravity,
		};

		if (this->m_gameState.m_isDualMode && this->m_player2) {
			PlayerObject* p2 = this->m_player2;
			CCPoint pos2 = p2->getPosition();
			int p2SpeedIndex = getSpeedIndex(p2->m_playerSpeed);
			auto p2Speed = SPEED_TABLE[p2SpeedIndex];

			g_p2State = PhysicsState{
				.xPos = pos2.x,
				.yPos = pos2.y,
				.xSpeed = p2Speed.speedName * p2Speed.speedMult,
				.yVel = p2->m_yVelocity,
				.lastEventTimestamp = g_levelStartTime,
				.Dir = p2->m_isUpsideDown ? -1 : 1,
				.isHolding = false,
				.isOnGround = p2->m_isOnGround,
				.isDashing = false,
				.gamemode = Gamemode::Cube,
				.speedIndex = p2SpeedIndex,
				.mGravity = p2Speed.gravity,
			};
		}

		g_modActive = true;
		return true;
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		g_firstFrame = true;
		g_tickCount = 0;
		g_pollCount = 0;
		g_levelStartTime = geode::utils::getInputTimestamp();
		logPhysicsDebugCounters("reset-level");
		resetPhysicsDebugCounters();

		PlayerObject* p1 = this->m_player1;
		if (p1) {
			CCPoint pos = p1->getPosition();
			g_p1State.xPos = pos.x;
			g_p1State.yPos = pos.y;
			g_p1State.yVel = p1->m_yVelocity;
			g_p1State.speedIndex = getSpeedIndex(p1->m_playerSpeed);
			g_p1State.xSpeed = SPEED_TABLE[g_p1State.speedIndex].speedName *
				SPEED_TABLE[g_p1State.speedIndex].speedMult;
			g_p1State.Dir = p1->m_isUpsideDown ? -1 : 1;
			g_p1State.isHolding = false;
			g_p1State.isOnGround = p1->m_isOnGround;
			g_p1State.isDashing = false;
			g_p1State.lastEventTimestamp = g_levelStartTime;
		}

		PlayerObject* p2 = this->m_player2;
		if (p2) {
			CCPoint pos2 = p2->getPosition();
			g_p2State.xPos = pos2.x;
			g_p2State.yPos = pos2.y;
			g_p2State.yVel = p2->m_yVelocity;
			g_p2State.speedIndex = getSpeedIndex(p2->m_playerSpeed);
			g_p2State.xSpeed = SPEED_TABLE[g_p2State.speedIndex].speedName *
				SPEED_TABLE[g_p2State.speedIndex].speedMult;
			g_p2State.Dir = p2->m_isUpsideDown ? -1 : 1;
			g_p2State.isHolding = false;
			g_p2State.isOnGround = p2->m_isOnGround;
			g_p2State.isDashing = false;
			g_p2State.lastEventTimestamp = g_levelStartTime;
		}
	}

	void onQuit() {
		logPhysicsDebugCounters("quit-level");
		g_modActive = false;
		PlayLayer::onQuit();
	}
};

class $modify(GJBaseGameLayer) {
	int checkCollisions(PlayerObject* object, float dt, bool ignoreDamage) {
		if (g_customPhysicsOwnsStep) {
			return 0;
		}
		return GJBaseGameLayer::checkCollisions(object, dt, ignoreDamage);
	}

	void update(float dt) {
		PlayLayer* playLayer = PlayLayer::get();

		if (!g_modActive || !playLayer) {
			resetCustomStepFlags();
			GJBaseGameLayer::update(dt);
			return;
		}

		if (!playLayer->m_player1) {
			resetCustomStepFlags();
			GJBaseGameLayer::update(dt);
			return;
		}

		if (this->m_isPlatformer) {
			resetCustomStepFlags();
			GJBaseGameLayer::update(dt);
			return;
		}

		if (this->m_useReplay) {
			resetCustomStepFlags();
			GJBaseGameLayer::update(dt);
			return;
		}

		if (g_firstFrame) {
			g_firstFrame = false;
			g_levelStartTime = g_frameStartTime;
			g_p1State.lastEventTimestamp = g_levelStartTime;
			g_p2State.lastEventTimestamp = g_levelStartTime;
			GJBaseGameLayer::update(dt);
			return;
		}

		double frameEnd = geode::utils::getInputTimestamp();

		std::vector<PlayerButtonCommand> inputQueue(
			playLayer->m_queuedButtons.begin(),
			playLayer->m_queuedButtons.end());
		playLayer->m_queuedButtons.clear();

		processFrame(playLayer, playLayer->m_player1, playLayer->m_player2,
			inputQueue, g_frameStartTime, frameEnd);

		g_suppressPlayerUpdate = true;
		g_customPhysicsOwnsStep = true;
		GJBaseGameLayer::update(dt);
		resetCustomStepFlags();

		if (!g_p1State.isDashing) {
			playLayer->m_player1->setPosition({g_p1State.xPos, g_p1State.yPos});
		}

		PlayerObject* p2 = playLayer->m_gameState.m_isDualMode
			? playLayer->m_player2
			: nullptr;
		if (p2 && !g_p2State.isDashing) {
			p2->setPosition({g_p2State.xPos, g_p2State.yPos});
		}
	}
};

class $modify(PlayerObject) {
	void update(float dt) {
		// i hope this doesn't cause any issues
		if (!g_suppressPlayerUpdate) PlayerObject::update(dt);
	}

	void setYVelocity(double velocity, int type) {
		if (g_velocityUnroundingEnabled) {
			this->m_yVelocity = velocity;
			return;
		} else {
			PlayerObject::setYVelocity(velocity, type);
		}
	}
};