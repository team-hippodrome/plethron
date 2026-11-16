// Frame interpolation hooks the game code calls (defined in interp.cpp; also declared in StadionPort.h, which some
// game files cannot include).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// The display list of a new frame starts at dlStart (Gfx_SwapDisplayListBuffer).
void Port_InterpBeginFrame(const void* dlStart);
// The graph node whose handler runs next, so its matrices can be matched across frames.
void Port_InterpNode(const void* node);

#ifdef __cplusplus
}
#endif
