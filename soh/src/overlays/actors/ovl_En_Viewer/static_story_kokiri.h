#ifndef STATIC_STORY_KOKIRI_H
#define STATIC_STORY_KOKIRI_H

#include "global.h"

struct EnViewer;

int StaticStoryKokiri_RequestObjects(struct EnViewer* this, PlayState* play);
void StaticStoryKokiri_Init(struct EnViewer* this, PlayState* play);
void StaticStoryKokiri_UpdatePose(struct EnViewer* this, f32 step);
void StaticStoryKokiri_Draw(struct EnViewer* this, PlayState* play);

#endif
