#include "global.hpp"

struct PatchGroup {
	std::vector<Patch*> patches;

	void init(std::initializer_list<std::pair<ptrdiff_t, size_t>> entries) {
		for (auto& [offset, size] : entries) {
			std::vector<uint8_t> nops(size, 0x90);
			auto result = Mod::get()->patch(
				reinterpret_cast<void*>(base::get() + offset), nops);
			if (result.isOk()) {
				auto* patch = result.unwrap();
				patch->setAutoEnable(false);
				(void) patch->disable();
				patches.push_back(patch);
			}
		}
	}

	void toggle(bool enable) {
		for (auto* patch : patches) {
			(void) patch->toggle(enable);
		}
	}
};

static PatchGroup s_velocityUnroundingPatches;

void toggleVelocityUnroundingPatches(bool enable) {
	if (s_velocityUnroundingPatches.patches.empty()) {
		s_velocityUnroundingPatches.init({
			// PlayerObject patches
			{0x38C329, 0x24}, // updateJump yvel rounding
			{0x213EA2, 0x32}, // checkCollisions yvel rounding
			{0x38DAC7, 0x38}, // postCollision yvel rounding
			{0x39323B, 0x40}, // collidedWithObjectInternal yvel rounding
			{0x39FF18, 0x32}, // boostPlayer yvel rounding
		});
	}

	s_velocityUnroundingPatches.toggle(enable);
}