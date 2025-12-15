#include "feature_config.h"

#if FEATURE_TEAMICONS_OFFSET

#include "util.h"
#include <cmath>
#include "../shared.h"

void drawteamicons_pre_callback(float*& targetPlayerOrigin, char*& playerName, 
                                char*& imageNameTeamIcon, int& redOrBlue) {
	float fovY = *engine_fovY;
	float fovX = *engine_fovX;

	fovfix_x = fovX / tan(degToRad(fovX * 0.5f));
	fovfix_y = fovY / tan(degToRad(fovY * 0.5f));

	float diagonalFov = fovX * 1.4142f;
	teamviewFovAngle = diagonalFov;
}

#endif // FEATURE_TEAMICONS_OFFSET

