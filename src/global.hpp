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

#include "physics.hpp"

using namespace geode::prelude;

inline PhysicsState g_p1State;
inline PhysicsState g_p2State;

inline double g_levelStartTime = 0.0;

inline bool g_firstFrame = true;

inline uint64_t g_tickCount = 0;
inline uint64_t g_pollCount = 0;

inline float g_tps = 240.0f;
inline float g_inputHz = 240.0f;
inline bool g_modActive = false;
inline bool g_velocityUnroundingEnabled = false;
inline bool g_subframesEnabled = false;

inline bool g_debugCountersEnabled = false;
inline PhysicsDebugCounters g_debugCounters;
inline uint64_t g_lastDebugLogPoll = 0;

inline double g_rawDt = 60.0 / 240.0;
inline double g_scaledDt = g_rawDt * 0.9;

void toggleVelocityUnroundingPatches(bool enable);