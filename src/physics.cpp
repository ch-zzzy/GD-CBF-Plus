#include "global.hpp"

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

#pragma region input handler

void handleInput(PlayerButtonCommand& input, PlayerObject* player,
	PlayLayer* playLayer, double& lastEventTimestamp) {
	advancePlayerToTimestamp(player, input.m_timestamp, lastEventTimestamp);

	bool isMini = std::abs(player->m_vehicleSize - 1.0f) > 0.01f;
	float generalSizeScale = isMini ? 0.8f : 1.0f;
	int dir = player->flipMod();

	if (!input.m_isPush) {
		// Release handling
		if (player->m_isShip || player->m_isBird) {
			// Ship/UFO: holding state changes gravity coefficient
			// handleButton already updated m_holdingButtons
		} else if (player->m_isDart) {
			// Wave: reverse direction on release
			double baseVel = player->getCurrentXVelocity();
			double yVel = baseVel * -1.0 * player->flipMod();
			if (isMini) yVel *= 2.0;
			player->m_yVelocity = quantizeYVelocity(yVel);
		}
		lastEventTimestamp = input.m_timestamp;
		return;
	}

	// Click handling
	if (player->m_isShip) {
		// Ship: holding state change, handleButton sets m_holdingButtons
		// Gravity coefficient change handled by the formula

	} else if (player->m_isBird) {
		// UFO: impulse at input time
		if (player->m_isOnGround || player->m_stateRingJump) {
			float impulse = isMini ? 8.0f : 7.0f;
			impulse *= generalSizeScale;
			player->m_yVelocity = quantizeYVelocity(dir * impulse);
			player->m_isOnGround = false;
			player->m_isOnGround2 = false;
			player->m_stateRingJump = false;
			player->m_touchedPad = false;
		}

	} else if (player->m_isDart) {
		// Wave: set velocity direction on press
		double baseVel = player->getCurrentXVelocity();
		double yVel = baseVel * 1.0 * dir;
		if (isMini) yVel *= 2.0;
		player->m_yVelocity = quantizeYVelocity(yVel);

	} else if (player->m_isBall) {
		// Ball: flip gravity + scale velocity
		if (player->m_isOnGround) {
			player->flipGravity(!player->m_isUpsideDown, true);
			player->m_yVelocity = quantizeYVelocity(player->m_yVelocity * 0.6);
			player->m_jumpBuffered = false;
			player->m_isOnGround = false;
		}

	} else if (player->m_isSwing) {
		// Swing: flip gravity + dampen velocity
		if (player->m_isOnGround || player->m_stateRingJump) {
			player->flipGravity(!player->m_isUpsideDown, true);
			player->m_yVelocity = quantizeYVelocity(player->m_yVelocity * 0.8);
			player->m_jumpBuffered = false;
			player->m_stateRingJump = false;
			player->m_isOnGround = false;
		}

	} else if (player->m_isSpider) {
		if (player->m_isOnGround) {
			player->spiderTestJump(player->m_isUpsideDown);
			player->m_jumpBuffered = false;
		}

	} else if (player->m_isRobot) {
		// Robot: initial impulse
		if (player->m_isOnGround) {
			float impulse = (float) player->m_yStart * 0.5f * generalSizeScale;
			player->m_yVelocity = quantizeYVelocity(dir * impulse);
			player->m_isOnGround = false;
			player->m_isOnGround2 = false;
			player->m_stateRingJump = false;
			player->m_touchedPad = false;
			player->m_accelerationOrSpeed = 0.0f;
			player->m_maybeIsBoosted = true;
		}

	} else {
		// Cube: jump impulse
		if (player->m_isOnGround) {
			float impulse = (float) player->m_yStart * generalSizeScale;
			player->m_yVelocity = quantizeYVelocity(dir * impulse);
			player->m_isOnGround = false;
			player->m_isOnGround2 = false;
			player->m_stateRingJump = false;
			player->m_touchedPad = false;
			player->m_accelerationOrSpeed = 0.0f;
			player->m_maybeIsBoosted = true;
		}
	}

	lastEventTimestamp = input.m_timestamp;
}

#pragma endregion

#pragma region continuous formula

float evalYPosition(PlayerObject* player, double secondsSinceEvent, float tps) {
	float yPos = player->getPositionY();
	double yVel = player->m_yVelocity;
	double t = secondsSinceEvent; // cleaned up for readability

	if (player->m_isDart) {
		return yPos + static_cast<float>(yVel * t * 60.0);
	}

	double g = getGravityAcceleration(player, tps);
	return yPos +
		static_cast<float>(
			((yVel * t) - (0.5 * g * t * (t - 1.0 / tps))) * 54.0);
}

double evalYVelocity(
	PlayerObject* player, double secondsSinceEvent, float tps) {
	if (player->m_isDart) return player->m_yVelocity;

	float g = getGravityAcceleration(player, tps);
	return player->m_yVelocity - (g * secondsSinceEvent);
}

float evalXPosition(PlayerObject* player, double secondsSinceEvent) {
	float xPos = player->getPositionX();
	double xSpeed = player->getCurrentXVelocity();
	double dir = player->reverseMod();

	return static_cast<float>(xPos + (xSpeed * dir * secondsSinceEvent * 60.0));
}

void processInputsUpToTimestamp(double tickTimestamp, PlayerObject* player,
	PlayLayer* playLayer, bool isPlayer1) {
	double& lastEventTimestamp =
		isPlayer1 ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;

	double inputCheckInterval = 1.0 / g_inputHz;
	double nextInputCheck =
		g_levelStartTimestamp + g_inputChecksCount * inputCheckInterval;

	// Catch up to current time
	while (nextInputCheck < lastEventTimestamp) {
		g_inputChecksCount++;
		nextInputCheck += inputCheckInterval;
	}

	// Use local index so both players can scan the full queue independently
	int localIdx = 0;

	while (nextInputCheck <= tickTimestamp) {
		while (localIdx < static_cast<int>(g_inputQueue.size())) {
			auto& input = g_inputQueue[localIdx];

			bool inputIsP1 = !input.m_isPlayer2;
			if (inputIsP1 != isPlayer1) {
				localIdx++;
				continue;
			}

			if (input.m_timestamp >= nextInputCheck) break;

			playLayer->handleButton(
				input.m_isPush, static_cast<int>(input.m_button), isPlayer1);

			if (!input.m_isPush && player->m_isDashing) {
				advancePlayerToTimestamp(
					player, nextInputCheck, lastEventTimestamp);
				player->stopDashing();
				lastEventTimestamp = nextInputCheck;
				localIdx++;
				continue;
			}

			// Snap input timestamp to polling boundary for handleInput
			double originalTimestamp = input.m_timestamp;
			input.m_timestamp = nextInputCheck;
			handleInput(input, player, playLayer, lastEventTimestamp);
			input.m_timestamp = originalTimestamp;

			localIdx++;
		}

		g_inputChecksCount++;
		nextInputCheck += inputCheckInterval;
	}
}

void advancePlayerToTimestamp(
	PlayerObject* player, double timestamp, double& lastEventTimestamp) {
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

void onPostCollision(PlayerObject* player, PlayLayer* playLayer) {
	if (!player || !playLayer) return;
	double& lastEventTimestamp =
		player->isPlayer1() ? g_p1LastEventTimestamp : g_p2LastEventTimestamp;

	lastEventTimestamp =
		g_levelStartTimestamp + (g_tickCount - 1) * (1.0 / g_tps);

	player->m_yVelocity = quantizeYVelocity(player->m_yVelocity);

	// Buffered cube jump
	if (player->isInNormalMode() && player->m_jumpBuffered &&
		player->m_isOnGround) {
		bool isMini = std::abs(player->m_vehicleSize - 1.0f) > 0.01f;
		float generalSizeScale = isMini ? 0.8f : 1.0f;
		player->m_yVelocity = quantizeYVelocity(
			player->m_yStart * player->flipMod() * generalSizeScale);
		player->m_isOnGround = false;
		player->m_isOnGround2 = false;
		player->m_stateRingJump = false;
		player->m_touchedPad = false;
		player->m_accelerationOrSpeed = 0.0f;
		player->m_maybeIsBoosted = true;
	}
}

#pragma endregion