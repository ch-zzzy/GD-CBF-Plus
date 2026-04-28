#include "global.hpp"

void updateTPS() {
	if (g_subframesEnabled) {
		g_tps = g_inputHz * 4.0f;
	} else {
		g_tps = g_mod->getSettingValue<float>("tps");
	}
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

float evalYPosition(PlayerObject* player, double secondsSinceEvent, float tps) {
	float yPos = player->getPositionY();
	double yVel = player->m_yVelocity;
	double t = secondsSinceEvent;

	if (player->m_isDart) {
		return yPos + static_cast<float>(yVel * t * 60.0);
	} else {
		return yPos + static_cast<float>(yVel * t * 54.0);
	}
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

	float newX, newY;
	if (!player->m_isSideways) {
		newX = evalXPosition(player, secondsSinceLastEvent);
		newY = evalYPosition(player, secondsSinceLastEvent, g_tps);
	} else {
		newX = evalYPosition(player, secondsSinceLastEvent, g_tps);
		newY = evalXPosition(player, secondsSinceLastEvent);
	}

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