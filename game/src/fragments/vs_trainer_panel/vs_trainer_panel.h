/* Declarations for src/fragments/vs_trainer_panel/ (fragment 032, the VS screen's trainer panels). */
#ifndef VS_TRAINER_PANEL_H
#define VS_TRAINER_PANEL_H

#include "global.h"
#include "fragments/vs_results_board/vs_results_board.h"

// vs_trainer_panel.c
intptr_t VsPanel_CheckBootCode(intptr_t display_list); // PORT: was s32
void VsPanel_SelectTitleRound(s32 event, GeoModelRootNode* node);
Gfx* VsPanel_WritePortraitGfx(Gfx* gfx, void* trainer_file);
void VsPanel_PortraitCallback(s32 event, DemoGeoNode* node);
void VsPanel_TitleTextCallback(s32 event, GeoTextNode* node); // PORT: was DemoGeoNode* (the text node's own type)
void VsPanel_NameTextCallback(s32 event, GeoTextNode* node); // PORT: was DemoGeoNode* (the text node's own type)
Gfx* VsPanel_WriteIconGfx(Gfx* gfx, void* icon_image);
void VsPanel_IconCallback(s32 event, DemoGeoNode *node);
void VsPanel_NicknameCallback(s32 event, GeoTextNode* node); // PORT: was DemoGeoNode* (the text node's own type)

#endif // VS_TRAINER_PANEL_H
