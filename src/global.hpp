#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace geode::prelude;

inline double g_levelStartTimestamp = 0.0;

inline double g_p1LastEventTimestamp = 0.0;
inline double g_p2LastEventTimestamp = 0.0;

inline bool g_firstFrame = true;

inline uint64_t g_tickCount = 0;
inline uint64_t g_inputChecksCount = 0;

inline float g_tps = 240.0f;
inline float g_inputHz = 240.0f;
inline bool g_modActive = false;
inline bool g_velocityUnroundingEnabled = false;
inline bool g_subframesEnabled = false;

inline double g_rawDt = 60.0 / 240.0;
inline double g_scaledDt = g_rawDt * 0.9;

inline std::vector<PlayerButtonCommand> g_inputQueue;

void toggleVelocityUnroundingPatches(bool enable);
void updateDeltaTime();
void onPostCollision(PlayerObject* player, PlayLayer* playLayer);
void advancePlayerToTimestamp(
	PlayerObject* player, double timestamp, double& lastEventTimestamp);
void processInputsUpToTimestamp(double tickTimestamp, PlayerObject* player,
	PlayLayer* playLayer, bool isPlayer1);
