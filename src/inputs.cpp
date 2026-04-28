#include "global.hpp"

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

	int inputIdx = 0;

	while (nextInputCheck <= tickTimestamp) {
		while (inputIdx < static_cast<int>(g_inputQueue.size())) {
			auto& input = g_inputQueue[inputIdx];

			bool inputIsP1 = !input.m_isPlayer2;
			if (inputIsP1 != isPlayer1) {
				inputIdx++;
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
				inputIdx++;
				continue;
			}

			double originalTimestamp = input.m_timestamp;
			input.m_timestamp = nextInputCheck;
			handleInput(input, player, playLayer, lastEventTimestamp);
			input.m_timestamp = originalTimestamp;

			inputIdx++;
		}

		g_inputChecksCount++;
		nextInputCheck += inputCheckInterval;
	}
}

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