#include "global.hpp"

void checkOrbs(
	PlayLayer* playLayer, PlayerObject* player, double inputCheckTimestamp);
void handleInput(PlayerButtonCommand& input, PlayerObject* player,
	PlayLayer* playLayer, double lastEventTimestamp);
void handleSpiderInput(PlayerObject* player, double lastEventTimestamp);

#pragma region helpers

void updateDeltaTime() {
	if (g_subframesEnabled) {
		g_tps = g_inputHz * 4.0f;
	}
	g_rawDt = 60.0 / g_tps;
	g_scaledDt = g_rawDt * 0.9;
}

double quantizeYVelocity(double velocity) {
	velocity = std::clamp(velocity, -1000.0, 1000.0);

	if (g_velocityUnroundingEnabled) {
		return velocity;
	}

	double wholePart = static_cast<double>(static_cast<int>(velocity));
	if (velocity != wholePart) {
		double frac = std::round((velocity - wholePart) * 1000.0);
		return frac / 1000.0 + wholePart;
	}

	return velocity;
}

float getBaseGravity(PlayerObject* player) {
	if (player->m_isShip || player->m_isBird || player->m_isBall ||
		player->m_isDart || player->m_isSwing || player->m_isSpider) {
		return 0.958199f;
	}
	return static_cast<float>(player->m_gravity);
}

float getGravityCoefficient(PlayerObject* player) {
	bool isMini = std::abs(player->m_vehicleSize - 0.6f) < 0.01f;
	float divisor = 1.0f;

	if (isMini) {
		if (player->m_isShip || player->m_isBird || player->m_isSwing) {
			divisor = 0.85f;
		} else if (player->m_isDart) {
			divisor = 1.0f;
		} else {
			divisor = 0.8f;
		}
	}

	if (player->m_isShip) {
		float threshold = static_cast<float>(player->m_gravity) * 2.0f;
		double yVel = player->m_yVelocity;
		bool upsideDown = player->m_isUpsideDown;
		bool holding = player->m_jumpBuffered;
		bool wrongDir = (!upsideDown && yVel < 0) || (upsideDown && yVel > 0);
		bool inDeadzone = (yVel > -6.4f && yVel < 8.0f);
		bool belowThreshold = (yVel < threshold);

		float coeff;
		if (holding) {
			if (wrongDir)
				coeff = -0.40f;
			else if (belowThreshold)
				coeff = 0.50f;
			else
				coeff = 0.40f;
		} else {
			if (inDeadzone)
				coeff = -0.40f;
			else if (belowThreshold)
				coeff = 0.40f;
			else
				coeff = 0.48f;
		}
		return coeff / divisor;
	}
	if (player->m_isBird) {
		float threshold = static_cast<float>(player->m_gravity) * 2.0f;
		float coeff = (player->m_yVelocity < threshold) ? 0.4f : 0.6f;
		return coeff;
	}
	if (player->m_isBall) return 0.6f / divisor;
	if (player->m_isDart) return 0.0f;
	if (player->m_isRobot) return 0.9f / divisor;
	if (player->m_isSpider) return 0.6f / divisor;
	if (player->m_isSwing) return 0.4f / divisor;

	return 1.0f / divisor;
}

float getGravityAcceleration(PlayerObject* player, float tps) {
	if (player->m_isDart) return 0.0f;

	float scaledDt = 60.0f / tps * 0.9f;
	float gravPerTick =
		getBaseGravity(player) * getGravityCoefficient(player) * scaledDt;
	gravPerTick = std::round(gravPerTick * 1000.0f) / 1000.0f;
	return gravPerTick * tps;
}

#pragma endregion

#pragma region continuous formula

float evalYPosition(PlayerObject* player, double secondsSinceEvent, float tps) {
	float yPos = player->getPositionY();
	double yVel = player->m_yVelocity;

	if (player->m_isDart) {
		return yPos + static_cast<float>(yVel * secondsSinceEvent * 60.0);
	}

	double gravAccel = getGravityAcceleration(player, tps);
	return static_cast<float>(yPos + (yVel * secondsSinceEvent) -
		(0.5 * gravAccel * secondsSinceEvent *
			(secondsSinceEvent - 1.0 / tps)));
}

double evalYVelocity(
	PlayerObject* player, double secondsSinceEvent, float tps) {
	if (player->m_isDart) return player->m_yVelocity;

	float gravAccel = getGravityAcceleration(player, tps);
	return player->m_yVelocity - (gravAccel * secondsSinceEvent);
}

float evalXPosition(PlayerObject* player, double secondsSinceEvent) {
	float xPos = player->getPositionX();
	double xSpeed =
		static_cast<double>(player->m_playerSpeed) * player->m_speedMultiplier;
	double dir = player->m_isGoingLeft ? -1.0 : 1.0;

	return static_cast<float>(xPos + (xSpeed * dir * secondsSinceEvent * 60.0));
}

void processInputsUpToTimestamp(double timestamp, PlayerObject* player,
	PlayLayer* playLayer, bool isPlayer1) {
	double inputCheckInterval = 1.0 / g_inputHz;
	double nextInputCheck =
		g_levelStartTimestamp + g_inputChecksCount * inputCheckInterval;

	double lastEventTimestamp =
		isPlayer1 ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;

	while (nextInputCheck < lastEventTimestamp) {
		g_inputChecksCount++;
		nextInputCheck += inputCheckInterval;
	}

	while (nextInputCheck <= timestamp) {
		while (g_inputIdx < g_inputQueue.size()) {
			auto& input = g_inputQueue[g_inputIdx];

			bool inputIsP1 = !input.m_isPlayer2;
			if (inputIsP1 != isPlayer1) {
				g_inputIdx++;
				continue;
			}

			if (input.m_timestamp >= nextInputCheck) break;

			playLayer->handleButton(
				input.m_isPush, static_cast<int>(input.m_button), isPlayer1);

			double inputTimestamp = nextInputCheck;
			if (playLayer->buttonIsRelevant(input)) {
				inputTimestamp = input.m_timestamp;
			}

			if (!input.m_isPush && player->m_isDashing) {
				advancePlayerToTimestamp(
					player, inputTimestamp, lastEventTimestamp);

				player->stopDashing();

				lastEventTimestamp = inputTimestamp;
				g_inputIdx++;
				continue;
			}

			if (player->m_isSpider) {
				if (input.m_isPush) {
					handleSpiderInput(player, lastEventTimestamp);
				}
			} else {
				input.m_timestamp = inputTimestamp;
				handleInput(input, player, playLayer, lastEventTimestamp);
			}

			g_inputIdx++;
		}

		checkOrbs(playLayer, player, nextInputCheck);

		g_inputChecksCount++;
		nextInputCheck += inputCheckInterval;
	}
}

void checkOrbs(
	PlayLayer* playLayer, PlayerObject* player, double inputCheckTimestamp) {
	bool isPlayer1 = (player == playLayer->m_player1);
	double lastEventTimestamp =
		isPlayer1 ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;
	if (!player->m_jumpBuffered) return;

	CCArray* orbsTouching = player->m_touchingRings;
	if (!orbsTouching || orbsTouching->count() == 0) return;

	advancePlayerToTimestamp(player, inputCheckTimestamp, lastEventTimestamp);

	auto orb = static_cast<RingObject*>(orbsTouching->objectAtIndex(0));
	player->ringJump(orb, false);

	lastEventTimestamp = inputCheckTimestamp;
}

void advancePlayerToTimestamp(
	PlayerObject* player, double timestamp, double lastEventTimestamp) {
	if (player->m_isDashing) {
		lastEventTimestamp = timestamp;
		return;
	}

	double secondsSinceLastEvent = timestamp - lastEventTimestamp;
	if (secondsSinceLastEvent <= 0.0) return;

	double newYVel = evalYVelocity(player, secondsSinceLastEvent, g_tps);

	float newX, newY;
	if (!player->m_isSideways) {
		newX = evalXPosition(player, secondsSinceLastEvent);
		newY = evalYPosition(player, secondsSinceLastEvent, g_tps);
	} else {
		newX = evalYPosition(player, secondsSinceLastEvent, g_tps);
		newY = evalXPosition(player, secondsSinceLastEvent);
	}

	player->m_yVelocity = newYVel;
	player->setPosition({newX, newY});
	lastEventTimestamp = timestamp;
}

#pragma endregion

#pragma region tick handling

bool onPlayerTick(PlayerObject* player, PlayLayer* playLayer, float dt,
	double lastEventTimestamp) {
	if (!player) return false;
	static_cast<void>(dt);

	if (player->m_isDashing) {
		return true;
	}

	double tickTimestamp = g_levelStartTimestamp + g_tickCount * (1.0 / g_tps);
	bool isPlayer1 = (player == playLayer->m_player1);

	// Process all inputs up to this tick boundary
	processInputsUpToTimestamp(tickTimestamp, player, playLayer, isPlayer1);

	// Advance our continuous formula to this tick
	advancePlayerToTimestamp(player, tickTimestamp, lastEventTimestamp);

	if (isPlayer1) {
		g_tickCount++;
	}

	return false;
}

void onPostCollision(PlayerObject* player, PlayLayer* playLayer) {
	if (!player || !playLayer) return;
	bool isPlayer1 = (player == playLayer->m_player1);
	double lastEventTimestamp =
		isPlayer1 ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;

	lastEventTimestamp =
		g_levelStartTimestamp + (g_tickCount - 1) * (1.0 / g_tps);

	player->m_yVelocity = quantizeYVelocity(player->m_yVelocity);

	// Buffered cube jump
	bool isCube = !player->m_isShip && !player->m_isBird && !player->m_isBall &&
		!player->m_isDart && !player->m_isRobot && !player->m_isSpider &&
		!player->m_isSwing;

	if (isCube && player->m_jumpBuffered && player->m_isOnGround) {
		player->m_yVelocity = quantizeYVelocity(
			player->m_yStart * (player->m_isUpsideDown ? -1.0 : 1.0));
		player->m_isOnGround = false;
	}

	if (g_tps != 240.0f) {
		playLayer->m_gameState.m_currentProgress =
			static_cast<int>(playLayer->m_gameState.m_levelTime * 240.0);
	}
}

#pragma endregion

#pragma region input handlers

void handleInput(PlayerButtonCommand& input, PlayerObject* player,
	PlayLayer* playLayer, double lastEventTimestamp) {
	advancePlayerToTimestamp(player, input.m_timestamp, lastEventTimestamp);

	int dir = player->m_isUpsideDown ? -1 : 1;

	if (player->m_isShip) {
		// Ship: holding state changes gravity coefficient, no impulse
		// (gravity handled by continuous formula)

	} else if (player->m_isBird) {
		// UFO
		if (input.m_isPush) {
			player->m_yVelocity = quantizeYVelocity(7.0 * dir);
		}

	} else if (player->m_isBall) {
		// Ball: flip gravity, scale velocity
		if (input.m_isPush) {
			player->m_isUpsideDown = !player->m_isUpsideDown;
			player->m_yVelocity = quantizeYVelocity(player->m_yVelocity * 0.3);
		}

	} else if (player->m_isDart) {
		// Wave
		double baseVel = static_cast<double>(player->m_playerSpeed) *
			player->m_speedMultiplier;
		double yVel = baseVel * (input.m_isPush ? 1.0 : -1.0) * dir;
		bool isMini = std::abs(player->m_vehicleSize - 0.6f) < 0.01f;
		if (isMini) yVel *= 2.0;
		player->m_yVelocity = quantizeYVelocity(yVel);

	} else if (player->m_isRobot) {
		// Robot
		if (input.m_isPush) {
			player->m_yVelocity =
				quantizeYVelocity(player->m_yStart * 0.5 * dir);
		}

	} else if (player->m_isSpider) {
		// Should not reach here — handled separately
		if (input.m_isPush) {
			handleSpiderInput(player, lastEventTimestamp);
		}

	} else if (player->m_isSwing) {
		// Swing: dampen velocity, flip gravity
		if (input.m_isPush) {
			player->m_yVelocity = quantizeYVelocity(player->m_yVelocity * 0.8);
			player->m_isUpsideDown = !player->m_isUpsideDown;
		}

	} else {
		// Cube
		if (input.m_isPush && player->m_isOnGround) {
			float gravAccel = getGravityAcceleration(player, g_tps);
			float gravPerTick = gravAccel / g_tps;
			player->m_yVelocity =
				quantizeYVelocity((player->m_yStart - gravPerTick) * dir);
			player->m_isOnGround = false;
		}
	}

	lastEventTimestamp = input.m_timestamp;
}

void handleSpiderInput(PlayerObject* player, double lastEventTimestamp) {
	player->spiderTestJump(false);
	lastEventTimestamp = lastEventTimestamp;
}

#pragma endregion