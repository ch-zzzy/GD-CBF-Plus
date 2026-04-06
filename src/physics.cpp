// there's like a 50/50 chance this works lol
#include "global.hpp"

constexpr uint64_t kDebugLogPollInterval = 240;
float tps;

void checkOrbs(PhysicsState& state, PlayLayer* pl, PlayerObject* player,
	double pollTimestamp);
void handleInput(PhysicsState& state, PlayerButtonCommand& input);
void handleSpiderInput(
	PhysicsState& state, PlayerObject* player, double pollTimestamp);
inline double quantizeYVelocity(double velocity);

inline void refreshDebugCounterToggle() {
	auto* mod = Mod::get();
	g_debugCountersEnabled =
		mod && mod->getSettingValue<bool>("physics-debug-counters");
}

void resetPhysicsDebugCounters() {
	refreshDebugCounterToggle();
	g_debugCounters = {};
	g_lastDebugLogPoll = g_pollCount;
}

void logPhysicsDebugCounters(char const* reason) {
	refreshDebugCounterToggle();
	if (!g_debugCountersEnabled) return;

	log::debug(
		"[independent-physics] {} | inputs={}, jumpTimed={}, flushes={}, "
		"dashStops={}, reconciliations={}",
		reason ? reason : "physics-debug", g_debugCounters.inputsProcessed,
		g_debugCounters.jumpTimedInputs, g_debugCounters.frameEndFlushes,
		g_debugCounters.dashStops, g_debugCounters.reconciliations);
}

inline void maybeLogDebugCounters() {
	if (!g_debugCountersEnabled) return;
	if (g_pollCount < g_lastDebugLogPoll + kDebugLogPollInterval) return;

	logPhysicsDebugCounters("periodic");
	g_lastDebugLogPoll = g_pollCount;
}

inline void reconcileStateWithPlayer(
	PhysicsState& state, PlayerObject* player) {
	if (!player) return;

	CCPoint pos = player->getPosition();
	double playerYVel = player->m_yVelocity;

	constexpr float kMaxPosDrift = 20.0f;
	constexpr float kMaxVelDrift = 25.0f;
	bool outOfSync = std::abs(state.xPos - pos.x) > kMaxPosDrift ||
		std::abs(state.yPos - pos.y) > kMaxPosDrift ||
		std::abs(state.yVel - playerYVel) > kMaxVelDrift;

	if (!outOfSync) return;

	g_debugCounters.reconciliations++;

	state.xPos = pos.x;
	state.yPos = pos.y;
	state.yVel = quantizeYVelocity(playerYVel);
	state.isOnGround = player->m_isOnGround;
	state.isDashing = player->m_isDashing;
	state.isMini = player->m_vehicleSize - 0.6f < 0.01f;
	state.Dir = player->m_isUpsideDown ? -1 : 1;
}

#pragma region helpers

void updateDeltaTime() {
	if (g_subframesEnabled) {
		g_inputHz = g_tps;
		tps = g_tps * 4.0f;
	} else {
		tps = g_tps;
	}
	g_rawDt = 60.0 / tps;
	g_scaledDt = g_rawDt * 0.9;
}

int getSpeedIndex(double playerSpeed) {
	for (int i = 0; i < 5; i++) {
		if (std::abs(SPEED_TABLE[i].speedName - playerSpeed) < 0.001f) {
			return i;
		}
	}
	return 0;
}

void updateShipGravity(PhysicsState& state) {
	float threshold = state.mGravity * 2.0f;
	bool wrongDirection =
		(state.Dir > 0 && state.yVel < 0) || (state.Dir < 0 && state.yVel > 0);
	bool inDeadzone = (state.yVel > -6.4f && state.yVel < 8.0f);
	bool belowThreshold = (state.yVel < threshold);

	if (state.isHolding) {
		if (wrongDirection)
			state.shipGravCoeff = -0.40f;
		else if (belowThreshold)
			state.shipGravCoeff = 0.50f;
		else
			state.shipGravCoeff = 0.40f;
	} else {
		if (inDeadzone)
			state.shipGravCoeff = -0.40f;
		else if (belowThreshold)
			state.shipGravCoeff = 0.40f;
		else
			state.shipGravCoeff = 0.48f;
	}
}

void updateUfoGravity(PhysicsState& state) {
	float threshold = state.mGravity * 2.0f;
	state.ufoGravCoeff = (state.yVel < threshold) ? 0.4f : 0.6f;
}

inline double quantizeYVelocity(double velocity) {
	velocity = std::clamp(velocity, -1000.0, 1000.0);
	if (g_velocityUnroundingEnabled) {
		return velocity;
	}
	double wholePart = static_cast<double>(static_cast<int>(velocity));
	double value = velocity;
	if (value != wholePart) {
		double frac = std::round((value - wholePart) * 1000.0);
		return frac / 1000.0 + wholePart;
	}
	return velocity;
}

void readCollisionResults(PhysicsState& state, PlayerObject* player) {
	CCPoint newPos = player->getPosition();
	state.xPos = newPos.x;
	state.yPos = newPos.y;
	state.yVel = quantizeYVelocity(player->m_yVelocity);
	state.isMini = player->m_vehicleSize - 0.6f < 0.01f;

	if (player->m_isShip) {
		state.gamemode = Gamemode::Ship;
	} else if (player->m_isBird) {
		state.gamemode = Gamemode::Ufo;
	} else if (player->m_isBall) {
		state.gamemode = Gamemode::Ball;
	} else if (player->m_isDart) {
		state.gamemode = Gamemode::Wave;
	} else if (player->m_isRobot) {
		state.gamemode = Gamemode::Robot;
	} else if (player->m_isSpider) {
		state.gamemode = Gamemode::Spider;
	} else if (player->m_isSwing) {
		state.gamemode = Gamemode::Swing;
	} else {
		state.gamemode = Gamemode::Cube;
	}

	if (player->m_isUpsideDown && state.Dir > 0) {
		state.Dir = -1;
	} else if (!player->m_isUpsideDown && state.Dir < 0) {
		state.Dir = 1;
	}

	state.speedIndex = getSpeedIndex(player->m_playerSpeed);
	auto speed = SPEED_TABLE[state.speedIndex];
	state.mGravity = speed.gravity;
	state.xSpeed = speed.speedName * speed.speedMult;

	if (state.isDashing && !player->m_isDashing) {
		state.isDashing = false;
	}

	return;
}

#pragma endregion

#pragma region polling loop

void processPollingBoundary(double pollTimestamp,
	std::vector<PlayerButtonCommand>& inputQueue, int& inputIdx,
	PlayLayer* playLayer, PlayerObject* p1, PlayerObject* p2,
	bool flushAllInputs = false) {
	if (flushAllInputs && inputIdx < inputQueue.size()) {
		g_debugCounters.frameEndFlushes++;
	}

	while (inputIdx < inputQueue.size() &&
		(flushAllInputs || inputQueue[inputIdx].m_timestamp < pollTimestamp)) {
		g_debugCounters.inputsProcessed++;

		PlayerButtonCommand input = inputQueue[inputIdx];
		PhysicsState& state = input.m_isPlayer2 ? g_p2State : g_p1State;
		PlayerObject* player = input.m_isPlayer2 ? p2 : p1;

		playLayer->handleButton(input.m_isPush,
			static_cast<int>(input.m_button), !input.m_isPlayer2);

		double inputTimestamp = pollTimestamp;
		if (playLayer->buttonIsRelevant(input)) {
			g_debugCounters.jumpTimedInputs++;
			inputTimestamp = input.m_timestamp;
		}

		if (!input.m_isPush && player && player->m_isDashing) {
			state.advanceToTimestamp(inputTimestamp, tps);
			player->m_yVelocity = static_cast<double>(state.yVel);
			player->setPosition({state.xPos, state.yPos});

			player->stopDashing();
			state.isDashing = false;
			g_debugCounters.dashStops++;

			state.yVel = quantizeYVelocity(player->m_yVelocity);
			CCPoint newPos = player->getPosition();
			state.xPos = newPos.x;
			state.yPos = newPos.y;
			state.lastEventTimestamp = inputTimestamp;

			if (player->m_isUpsideDown && state.Dir > 0) {
				state.Dir = -1;
			} else if (!player->m_isUpsideDown && state.Dir < 0) {
				state.Dir = 1;
			}
			inputIdx++;
			continue;
		}

		if (state.gamemode == Gamemode::Spider) {
			if (input.m_isPush && player) {
				handleSpiderInput(state, player, inputTimestamp);
			}
		} else {
			input.m_timestamp = inputTimestamp;
			handleInput(state, input);
			state.isHolding = input.m_isPush;
		}

		inputIdx++;
	}

	checkOrbs(g_p1State, playLayer, p1, pollTimestamp);
	if (p2) checkOrbs(g_p2State, playLayer, p2, pollTimestamp);
}

void checkOrbs(PhysicsState& state, PlayLayer* playLayer, PlayerObject* player,
	double pollTimestamp) {
	if (!state.isHolding) return;
	if (!player) return;

	CCArray* orbsTouching = player->m_touchingRings;
	if (!orbsTouching || orbsTouching->count() == 0) return;

	state.advanceToTimestamp(pollTimestamp, tps);
	player->m_yVelocity = static_cast<double>(state.yVel);
	player->setPosition({state.xPos, state.yPos});

	RingObject* orb = static_cast<RingObject*>(orbsTouching->objectAtIndex(0));
	player->ringJump(orb, false);

	state.yVel = quantizeYVelocity(player->m_yVelocity);
	state.isOnGround = player->m_isOnGround;

	CCPoint newPos = player->getPosition();
	state.xPos = newPos.x;
	state.yPos = newPos.y;

	if (player->m_isUpsideDown && state.Dir > 0) {
		state.Dir = -1;
	} else if (!player->m_isUpsideDown && state.Dir < 0) {
		state.Dir = 1;
	}

	if (player->m_isDashing) {
		state.isDashing = true;
	}
}

#pragma endregion

#pragma region tick loop

void processTickBoundary(PhysicsState& state, PlayLayer* playLayer,
	PlayerObject* player, double tickTimestamp) {
	if (!player) return;

	player->updateInternalActions(1.0f / tps);

	if (state.isDashing) { // let vanilla handle dashing
		player->update(static_cast<float>(g_rawDt));

		if (!player->m_isDashing) {
			state.isDashing = false;

			CCPoint newPos = player->getPosition();
			state.xPos = newPos.x;
			state.yPos = newPos.y;
			state.yVel = quantizeYVelocity(player->m_yVelocity);
			state.isOnGround = player->m_isOnGround;
			state.lastEventTimestamp = tickTimestamp;

			if (player->m_isUpsideDown && state.Dir > 0) {
				state.Dir = -1;
			} else if (!player->m_isUpsideDown && state.Dir < 0) {
				state.Dir = 1;
			}
		}
		return;
	}

	state.advanceToTimestamp(tickTimestamp, tps);

	if (state.gamemode == Gamemode::Ship) {
		updateShipGravity(state);
	}

	if (state.gamemode == Gamemode::Ufo) {
		updateUfoGravity(state);
	}

	player->m_yVelocity = static_cast<double>(state.yVel);
	player->setPosition({state.xPos, state.yPos});

	float collisionDt = static_cast<float>(g_scaledDt);
	playLayer->checkCollisions(player, collisionDt, false);

	bool wasOnGround = state.isOnGround;
	readCollisionResults(state, player);
	state.isOnGround = player->m_isOnGround;

	// buffered cube jump
	if (state.gamemode == Gamemode::Cube && state.isHolding &&
		state.isOnGround && !wasOnGround) {
		auto speed = SPEED_TABLE[state.speedIndex];
		state.yVel = quantizeYVelocity(speed.yStart * state.Dir);
		state.isOnGround = false;
		player->m_yVelocity = static_cast<double>(state.yVel);
	}

	if (g_tps != 240.0f) {
		playLayer->m_gameState.m_currentProgress =
			static_cast<int>(playLayer->m_gameState.m_levelTime * 240.0);
	}
}

#pragma endregion

#pragma region input handlers

void handleInput(PhysicsState& state, PlayerButtonCommand& input) {
	state.advanceToTimestamp(input.m_timestamp, tps);

	SpeedData const& speed = SPEED_TABLE[state.speedIndex];

	switch (state.gamemode) {
		case Gamemode::Cube: {
			if (input.m_isPush && state.isOnGround) {
				float gravAcceleration = state.getGravityAcceleration(tps);
				float gravPerTick = gravAcceleration / tps;
				state.yVel =
					quantizeYVelocity((speed.yStart - gravPerTick) * state.Dir);
				state.isOnGround = false;
			}
			state.isHolding = input.m_isPush;
			break;
		}

		case Gamemode::Ball: {
			if (input.m_isPush) {
				state.yVel = quantizeYVelocity(speed.yStart * 0.3f * state.Dir);
				state.Dir = -state.Dir;
			}
			break;
		}

		case Gamemode::Robot: {
			if (input.m_isPush) {
				state.yVel = quantizeYVelocity(speed.yStart * 0.5f * state.Dir);
			}
			state.isHolding = input.m_isPush;
			break;
		}

		case Gamemode::Wave: {
			state.isHolding = input.m_isPush;
			float baseVel = speed.speedName * speed.speedMult;
			state.yVel = quantizeYVelocity(
				baseVel * (state.isHolding ? 1.0f : -1.0f) * state.Dir);
			if (state.isMini) {
				state.yVel = quantizeYVelocity(state.yVel * 2.0f);
			}
			break;
		}

		case Gamemode::Ufo: {
			if (input.m_isPush) {
				state.yVel = quantizeYVelocity(7.0f * state.Dir);
			}
			state.isHolding = input.m_isPush;
			break;
		}

		case Gamemode::Swing: {
			if (input.m_isPush) {
				state.yVel = quantizeYVelocity(state.yVel * 0.8f);
				state.Dir = -state.Dir;
			}
			state.isHolding = input.m_isPush;
			break;
		}

		default:
			break;
	}
}

void handleSpiderInput(
	PhysicsState& state, PlayerObject* player, double pollTimestamp) {
	state.advanceToTimestamp(pollTimestamp, tps);
	// write state to player so vanilla reads correct position
	player->m_yVelocity = state.yVel;
	player->setPosition({state.xPos, state.yPos});

	// let vanilla handle the raycast + teleport + gravity flip
	player->spiderTestJump(false);

	// read back results
	CCPoint newPos = player->getPosition();
	state.xPos = newPos.x;
	state.yPos = newPos.y;
	state.yVel = quantizeYVelocity(player->m_yVelocity);

	if (player->m_isUpsideDown && state.Dir > 0) {
		state.Dir = -1;
	} else if (!player->m_isUpsideDown && state.Dir < 0) {
		state.Dir = 1;
	}
	state.isOnGround = player->m_isOnGround;
}

#pragma endregion

#pragma region frame loop

void processFrame(PlayLayer* playLayer, PlayerObject* p1, PlayerObject* p2,
	std::vector<PlayerButtonCommand>& inputQueue, double frameStartTimestamp,
	double frameEndTimestamp) {
	refreshDebugCounterToggle();

	p2 = playLayer->m_gameState.m_isDualMode ? playLayer->m_player2 : nullptr;

	std::sort(inputQueue.begin(), inputQueue.end(),
		[](PlayerButtonCommand const& a, PlayerButtonCommand const& b) {
			return a.m_timestamp < b.m_timestamp;
		});

	// advance tick counter to the first tick at or after frameStartTimestamp
	double tickInterval = 1.0 / tps;
	double pollInterval = 1.0 / g_inputHz;
	double nextTickTimestamp = g_levelStartTime + g_tickCount * tickInterval;
	while (nextTickTimestamp < frameStartTimestamp) {
		g_tickCount++;
		nextTickTimestamp += tickInterval;
	}

	// advance poll counter to the first poll at or after frameStartTimestamp
	double nextPoll = g_levelStartTime + g_pollCount * pollInterval;
	while (nextPoll < frameStartTimestamp) {
		g_pollCount++;
		nextPoll += pollInterval;
	}

	int inputIdx = 0;

	while (true) {
		bool tickInFrame = nextTickTimestamp <= frameEndTimestamp;
		bool pollInFrame = nextPoll <= frameEndTimestamp;

		if (!tickInFrame && !pollInFrame) break;

		if (tickInFrame && (!pollInFrame || nextTickTimestamp <= nextPoll)) {
			// process all polling boundaries before or at this tick
			while (nextPoll <= nextTickTimestamp) {
				if (nextPoll > frameEndTimestamp) break;

				processPollingBoundary(
					nextPoll, inputQueue, inputIdx, playLayer, p1, p2);
				g_pollCount++;
				nextPoll += pollInterval;
			}

			// run tick for both players
			processTickBoundary(g_p1State, playLayer, p1, nextTickTimestamp);
			if (p2) {
				processTickBoundary(
					g_p2State, playLayer, p2, nextTickTimestamp);
			}
			g_tickCount++;
			nextTickTimestamp += tickInterval;
		} else {
			processPollingBoundary(
				nextPoll, inputQueue, inputIdx, playLayer, p1, p2);
			g_pollCount++;
			nextPoll += pollInterval;
		}
	}

	if (inputIdx < inputQueue.size()) {
		processPollingBoundary(
			frameEndTimestamp, inputQueue, inputIdx, playLayer, p1, p2, true);
		logPhysicsDebugCounters("frame-end-flush");
	}

	// evaluate render positions at frame end
	if (!g_p1State.isDashing) {
		g_p1State.advanceToTimestamp(frameEndTimestamp, tps);
		p1->setPosition({g_p1State.xPos, g_p1State.yPos});
	}

	if (p2 && !g_p2State.isDashing) {
		g_p2State.advanceToTimestamp(frameEndTimestamp, tps);
		p2->setPosition({g_p2State.xPos, g_p2State.yPos});
	}

	reconcileStateWithPlayer(g_p1State, p1);
	if (p2) {
		reconcileStateWithPlayer(g_p2State, p2);
	}

	maybeLogDebugCounters();
}

#pragma endregion