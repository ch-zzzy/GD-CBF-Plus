#pragma once

struct SpeedData {
	double yStart;
	double gravity;
	double speedMult;
	float speedName;
};

constexpr SpeedData SPEED_TABLE[] = {
	{10.620032, 0.940199, 5.980002, 0.7f},
	{11.180032f, 0.958199f, 5.770002f, 0.9f},
	{11.420032, 0.957199, 5.870002, 1.1f},
	{11.230032, 0.961199, 6.000002, 1.3f},
	{11.230032, 0.961199, 6.000002, 1.6f},
};

enum class Gamemode { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };

struct PhysicsState {
	float xPos = 0.0f;
	float yPos = 0.0f;
	double xSpeed = 0.0;
	double yVel = 0.0;
	double lastEventTimestamp = 0.0;

	int Dir = 1;
	bool isHolding = false;
	bool isOnGround = false;
	bool isDashing = false;
	bool isMini = false;

	Gamemode gamemode = Gamemode::Cube;
	int speedIndex = 0;
	double mGravity = 0.958199f;

	float shipGravCoeff = 0.4f;
	float ufoGravCoeff = 0.6f;

	void advanceToTimestamp(double timestamp, int64_t tps) {
		if (isDashing) {
			lastEventTimestamp = timestamp;
			return;
		}

		double secondsSinceTimestamp = timestamp - lastEventTimestamp;

		yPos = evalYPosition(secondsSinceTimestamp, static_cast<float>(tps));
		yVel = evalYVelocity(secondsSinceTimestamp, static_cast<double>(tps));
		xPos = evalXPosition(secondsSinceTimestamp);
		lastEventTimestamp = timestamp;
	}

	float getBaseGravity() const {
		switch (gamemode) {
			case Gamemode::Cube:
			case Gamemode::Robot:
				return mGravity;
			default:
				return 0.958199f;
		}
	}

	float getGravityCoefficient() const {
		float divisor = 1.0f;
		if (isMini) {
			switch (gamemode) {
				case Gamemode::Ship:
				case Gamemode::Ufo:
				case Gamemode::Swing:
					divisor = 0.85f;
					break;
				case Gamemode::Wave:
					divisor = 1.0f;
					break;
				default:
					divisor = 0.8f;
					break;
			}
		}

		switch (gamemode) {
			case Gamemode::Cube:
				return 1.0f / divisor;
			case Gamemode::Ship:
				return shipGravCoeff / divisor;
			case Gamemode::Ball:
				return 0.6f / divisor;
			case Gamemode::Ufo:
				return ufoGravCoeff;
			case Gamemode::Wave:
				return 0.0f;
			case Gamemode::Robot:
				return 0.9f / divisor;
			case Gamemode::Spider:
				return 0.6f / divisor;
			case Gamemode::Swing:
				return 0.4f / divisor;
			default:
				return 1.0f;
		}
	}

	float getGravityAcceleration(float tps) const {
		if (gamemode == Gamemode::Wave) {
			return 0.0f;
		}

		float scaledDt = 60.0f / tps * 0.9f;
		float gravityPerTick =
			getBaseGravity() * getGravityCoefficient() * scaledDt;
		gravityPerTick = std::round(gravityPerTick * 1000.0f) / 1000.0f;
		return gravityPerTick * tps;
	}

	float evalYPosition(double secondsSinceEvent, float tps) const {
		if (gamemode == Gamemode::Wave) {
			return yPos + static_cast<float>(yVel * secondsSinceEvent * 60.0);
		}

		double gravAcceleration = getGravityAcceleration(tps);
		return static_cast<float>(yPos + (yVel * secondsSinceEvent) -
			(0.5 * gravAcceleration * secondsSinceEvent *
				(secondsSinceEvent - 1.0 / tps)));
	}

	double evalYVelocity(double secondsSinceEvent, double tps) const {
		if (gamemode == Gamemode::Wave) {
			return yVel;
		}

		float gravAcceleration =
			getGravityAcceleration(static_cast<float>(tps));
		return yVel - (gravAcceleration * secondsSinceEvent);
	}

	float evalXPosition(double secondsSinceEvent) const {
		return static_cast<float>(xPos + (xSpeed * secondsSinceEvent * 60));
	}
};

struct PhysicsDebugCounters {
	uint64_t inputsProcessed = 0;
	uint64_t jumpTimedInputs = 0;
	uint64_t frameEndFlushes = 0;
	uint64_t dashStops = 0;
	uint64_t reconciliations = 0;
};

int getSpeedIndex(double playerSpeed);
void updateDeltaTime();
void resetPhysicsDebugCounters();
void logPhysicsDebugCounters(char const* reason);
void processFrame(PlayLayer* playLayer, PlayerObject* p1, PlayerObject* p2,
	std::vector<PlayerButtonCommand>& inputQueue, double frameStartTimestamp,
	double frameEndTimestamp);
