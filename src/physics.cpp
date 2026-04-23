#include "global.hpp"

void updateDeltaTime() {
	if (g_subframesEnabled) {
		g_tps = g_inputHz * 4.0f;
	} else {
		g_tps = Mod::get()->getSettingValue<float>("tps");
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
	if (player->isInBasicMode()) {
		return static_cast<float>(player->m_gravity);
	} else {
		return 0.958199f;
	}
}

float getGravityCoefficient(PlayerObject* player) {
	bool isMini = std::abs(player->m_vehicleSize - 0.6f) < 0.01f;
	float divisor = 1.0f;

	if (isMini) {
		if (player->m_isShip || player->m_isBird || player->m_isSwing) {
			divisor = 0.85f;
		} else if (player->m_isDart) {
			divisor = 1.0f; // useless since wave has no gravity accel
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
		return coeff / divisor;
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

float evalYPosition(PlayerObject* player, double secondsSinceEvent, float tps) {
	float yPos = player->getPositionY();
	double yVel = player->m_yVelocity;
	double t = secondsSinceEvent;

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
	int dir = player->reverseMod();

	return static_cast<float>(xPos + (xSpeed * dir * secondsSinceEvent * 60.0));
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