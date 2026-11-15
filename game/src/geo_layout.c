/* Stadium Zero resident code. */
/* C ported from the Stadium 1 decomp: src/geo_layout.c. */

#include "geo_layout.h"
#include "matrix.h"
#include "util.h"
#include "StadionPort.h"
#include "zero_lift.h"
#include <stdio.h>
extern GeoLayoutCommandProc GeoLayoutJumpTable[];
extern MemoryBlock* gGraphNodePool;
extern GraphNode* gCurRootGraphNode;
extern uintptr_t gGeoLayoutStack[16]; // PORT: holds return addresses (was u32)
extern GraphNode* gCurGraphNodeList[32];
extern s16 gCurGraphNodeIndex;
extern s16 gGeoLayoutStackIndex;
extern s16 gGeoLayoutReturnIndex;
extern u8* gGeoLayoutCommand;
int gGeoPacked;

// PORT: ROM layouts (model blobs, lifted data) are big-endian with 32-bit link addresses ("packed"); arrays built
// into the executable are in slot form. The form follows from where the command pointer lies.
static void GeoSetCmd(u8* p) {
    gGeoLayoutCommand = p;
    if (p == NULL) {
        gGeoPacked = 0;
    } else if (Port_VIsRaw(p) || ZLift_IsRaw(p)) {
        gGeoPacked = 1;
    } else if (ZLift_IsLifted(p)) {
        gGeoPacked = 2;
    } else {
        gGeoPacked = 0;
    }
}

void GeoLayout_CmdBranchAndLink(void);
void GeoLayout_CmdEnd(void);
void GeoLayout_CmdJump(void);
void GeoLayout_CmdBranch(void);
void GeoLayout_CmdReturn(void);
void GeoLayout_CmdOpenNode(void);
void GeoLayout_CmdCloseNode(void);
void GeoLayout_RegisterGraphNode(GraphNode* arg0);
void GeoLayout_CmdAttachNode(void);
void GeoLayout_CmdSetCallback(void);
void GeoLayout_CmdCreateContainer(void);
void GeoLayout_CmdCreateReference(void);
void GeoLayout_CmdCreateCamera(void);
void GeoLayout_CmdCreateType3(void);
void GeoLayout_CmdCreateType4(void);
void GeoLayout_CmdCreateOrtho(void);
void GeoLayout_CmdCreatePerspective(void);
void GeoLayout_CmdCreateBackground(void);
void GeoLayout_CmdCreateClearDepth(void);
void GeoLayout_CmdNoop(void);
void GeoLayout_CmdCreateFog(void);
void GeoLayout_CmdCreateLight(void);
void GeoLayout_CmdSkip(void);
void GeoLayout_CmdCreateAmbientLight(void);
void GeoLayout_CmdCreateTextureBank(void);
void GeoLayout_CmdCreateShadow(void);
void GeoLayout_CmdCreateCullDistance(void);
void GeoLayout_CmdCreateSwitchCase(void);
void GeoLayout_CmdCreateTranslateRotate(void);
void GeoLayout_CmdCreateTranslate(void);
void GeoLayout_CmdCreateAnimatedPart(void);
void GeoLayout_CmdCreateDisplayListPart(void);
void GeoLayout_CmdCreateModelPart(void);
void GeoLayout_CmdCreateDisplayListMatrix(void);
void GeoLayout_CmdCreateBillboard(void);
void GeoLayout_CmdCreateDisplayList(void);
void GeoLayout_CmdCreateMaterial(void);
void GeoLayout_CmdCreateAnchor(void);
void GeoLayout_CmdSetNodeFlag4(void);
void GeoLayout_CmdCreateGroup(void);
GraphNode* GeoLayout_Process(MemoryBlock* pool, void* segptr);

/*
  0x00: Branch and store return address
   cmd+0x04: void *branchTarget
*/
void GeoLayout_CmdBranchAndLink(void) {
    u8* cmd = (void*)gGeoLayoutCommand;

    cmd += 0;

    gGeoLayoutStack[gGeoLayoutStackIndex++] = (uintptr_t)(cmd + GL_CMDSIZE(8));
    gGeoLayoutStack[gGeoLayoutStackIndex++] = (gCurGraphNodeIndex << 16) + gGeoLayoutReturnIndex;
    gGeoLayoutReturnIndex = gGeoLayoutStackIndex;
    GeoSetCmd(Util_ConvertAddrToVirtAddr((uintptr_t)cur_geo_cmd_ptr(0x04)));
}

// 0x01: Terminate geo layout
/**
 * Geo layout command: ends the current layout and returns to the caller's
 * stack frame.
 */
void GeoLayout_CmdEnd(void) {
    gGeoLayoutStackIndex = gGeoLayoutReturnIndex;
    gGeoLayoutReturnIndex = gGeoLayoutStack[--gGeoLayoutStackIndex] & 0xFFFF;
    gCurGraphNodeIndex = gGeoLayoutStack[gGeoLayoutStackIndex] >> 16;
    GeoSetCmd((u8*)gGeoLayoutStack[--gGeoLayoutStackIndex]);
}

/*
  0x02: Jump to address
    cmd+0x04: void *jumpTarget
*/
void GeoLayout_CmdJump(void) {
    u8* cmd = (void*)gGeoLayoutCommand;

    cmd += 0;

    GeoSetCmd(Util_ConvertAddrToVirtAddr((uintptr_t)cur_geo_cmd_ptr(0x04)));
}

/*
  0x03: Branch
    cmd+0x04: void *branchTarget
*/
void GeoLayout_CmdBranch(void) {
    u8* cmd = (void*)gGeoLayoutCommand;

    cmd += 0;

    // unlike in sm64, this argument doesnt seem to be optional.
    gGeoLayoutStack[gGeoLayoutStackIndex++] = (uintptr_t)(cmd + GL_CMDSIZE(8));
    GeoSetCmd(Util_ConvertAddrToVirtAddr((uintptr_t)cur_geo_cmd_ptr(0x04)));
}

// 0x04: Return from branch
/** Geo layout command: returns to the command address saved on the stack. */
void GeoLayout_CmdReturn(void) {
    GeoSetCmd((u8*)gGeoLayoutStack[--gGeoLayoutStackIndex]);
}

// 0x05: Open node
/**
 * Geo layout command: makes the current node the parent of the nodes that
 * follow.
 */
void GeoLayout_CmdOpenNode(void) {
    gCurGraphNodeList[gCurGraphNodeIndex + 1] = gCurGraphNodeList[gCurGraphNodeIndex];
    gCurGraphNodeIndex++;
    gGeoLayoutCommand += GL_CMDSIZE(0x04);
}

// 0x06: Close node
/** Geo layout command: ends the current parent node. */
void GeoLayout_CmdCloseNode(void) {
    gCurGraphNodeIndex--;
    gGeoLayoutCommand += GL_CMDSIZE(0x04);
}

/**
 * Adds a node to the layout being built, as a child of the current parent or
 * as the root.
 */
void GeoLayout_RegisterGraphNode(GraphNode* arg0) {
    if (arg0 != NULL) {
        gCurGraphNodeList[gCurGraphNodeIndex] = arg0;
        if (gCurGraphNodeIndex == 0) {
            if (gCurRootGraphNode == NULL) {
                gCurRootGraphNode = arg0;
            }
        } else {
            GraphNode_AppendChild(gCurGraphNodeList[gCurGraphNodeIndex - 1], arg0);
        }
    }
}

/** Geo layout command: registers an existing node. */
void GeoLayout_CmdAttachNode(void) {

    GeoLayout_RegisterGraphNode(&((GeoFogNode*)Util_ConvertAddrToVirtAddr((uintptr_t)GL_PTR(0x4)))->node);

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: sets a callback on the current node. */
void GeoLayout_CmdSetCallback(void) {

    if (gCurGraphNodeList[gCurGraphNodeIndex] != NULL) {
        GeoNode_SetCallback(gCurGraphNodeList[gCurGraphNodeIndex], ((GraphNodeCallback)GL_PTR(0x4)), ((unk_D_86002F34_000_014*)GL_PTR(0x8)));
    }

    gGeoLayoutCommand += GL_CMDSIZE(0xC);
}

/** Geo layout command: creates a container node. */
void GeoLayout_CmdCreateContainer(void) {
    GeoLayout_RegisterGraphNode(GeoNode_CreateContainer(gGraphNodePool, NULL));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a reference node. */
void GeoLayout_CmdCreateReference(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateWithReference(gGraphNodePool, NULL, ((GraphNode*)Util_ConvertAddrToVirtAddr((uintptr_t)GL_PTR(0x4)))));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a camera node. */
void GeoLayout_CmdCreateCamera(void) {
    CameraNode* temp_v0 =
        GeoNode_CreateCamera(gGraphNodePool, NULL, ((u16)GL_S16(0x4)), ((u16)GL_S16(0x6)), GL_S16(0x8), GL_S16(0xA));

    if (temp_v0 != NULL) {
        temp_v0->perspective.verticalFieldOfView = ((u16)GL_S16(0x2));
        Vec3f_FromVec3s(&temp_v0->view.targetPosition, GL_VEC3S(0xC));
        Camera_ComputeEyeFromAngles(&temp_v0->view.targetPosition, &temp_v0->view.eyePosition, GL_S16(0x16), (GL_S16(0x12) << 0xF) / 180,
                      (GL_S16(0x14) << 0xF) / 180);
    }

    GeoLayout_RegisterGraphNode(&temp_v0->node);

    gGeoLayoutCommand += GL_CMDSIZE(0x18);
}

/** Geo layout command: creates a type3 node. */
void GeoLayout_CmdCreateType3(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateType3(gGraphNodePool, NULL, GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a type4 node. */
void GeoLayout_CmdCreateType4(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateType4(gGraphNodePool, NULL, GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a ortho node. */
void GeoLayout_CmdCreateOrtho(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateOrtho(gGraphNodePool, NULL, GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a perspective node. */
void GeoLayout_CmdCreatePerspective(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreatePerspective(gGraphNodePool, NULL, GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a background node. */
void GeoLayout_CmdCreateBackground(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateBackground(gGraphNodePool, NULL, GL_U8(0x1), GL_U8(0x2), GL_U8(0x3)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a clear depth node. */
void GeoLayout_CmdCreateClearDepth(void) {
    GeoLayout_RegisterGraphNode(GeoNode_CreateClearDepth(gGraphNodePool, NULL));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: does nothing. */
void GeoLayout_CmdNoop(void) {
}

/** Geo layout command: creates a fog node. */
void GeoLayout_CmdCreateFog(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateFog(gGraphNodePool, NULL, GL_S16(0x4), GL_S16(0x6), GL_U8(0x1), GL_U8(0x2), GL_U8(0x3), 0xFF));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a light node. */
void GeoLayout_CmdCreateLight(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateLight(gGraphNodePool, NULL, (GL_S16(0x4) << 0xF) / 180, (GL_S16(0x6) << 0xF) / 180, GL_U8(0x8),
                                GL_U8(0x9), GL_U8(0xA), GL_U8(0xB)));

    gGeoLayoutCommand += GL_CMDSIZE(0xC);
}

/** Geo layout command: skips over an unused command. */
void GeoLayout_CmdSkip(void) {
    gGeoLayoutCommand += GL_CMDSIZE(0xC);
}

/** Geo layout command: creates a ambient light node. */
void GeoLayout_CmdCreateAmbientLight(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateAmbientLight(gGraphNodePool, NULL, GL_U8(0x1), GL_U8(0x2), GL_U8(0x3)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a texture bank node. */
void GeoLayout_CmdCreateTextureBank(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateTextureBank(gGraphNodePool, NULL, GL_S16(0x2), ((GeoTexture*)GL_PTR(0x8)), GL_S16(0x4), ((GeoPalette*)GL_PTR(0xC)), GL_S16(0x6),
                                ((Vtx*)GL_PTR(0x10))));

    gGeoLayoutCommand += GL_CMDSIZE(0x14);
}

/** Geo layout command: creates a shadow node. */
void GeoLayout_CmdCreateShadow(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateShadow(gGraphNodePool, NULL, GL_U8(0x1), GL_S16(0x4), GL_S16(0x6), GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a cull distance node. */
void GeoLayout_CmdCreateCullDistance(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateCullDistance(gGraphNodePool, NULL, GL_S16(0x4), GL_S16(0x6)));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a switch case node. */
void GeoLayout_CmdCreateSwitchCase(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateSwitchCase(gGraphNodePool, NULL, GL_U8(0x1), GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a translate rotate node. */
void GeoLayout_CmdCreateTranslateRotate(void) {
    Vec3s sp30;
    Vec3f sp24;
    UNUSED s32 pad; // FAKE: stack padding

    Vec3f_FromVec3s(&sp24, GL_VEC3S(0xA));
    Vec3s_SetComponents(&sp30, (GL_S16(0x4) << 0xF) / 180, (GL_S16(0x6) << 0xF) / 180, (GL_S16(0x8) << 0xF) / 180);
    GeoLayout_RegisterGraphNode(GeoNode_CreateTranslateRotate(gGraphNodePool, NULL, &sp24, &sp30));

    gGeoLayoutCommand += GL_CMDSIZE(0x10);
}

/** Geo layout command: creates a translate node. */
void GeoLayout_CmdCreateTranslate(void) {
    Vec3f sp24;

    sp24.x = GL_U32(0x4) / 65536.0f;
    sp24.y = GL_U32(0x8) / 65536.0f;
    sp24.z = GL_U32(0xC) / 65536.0f;
    GeoLayout_RegisterGraphNode(GeoNode_CreateTranslate(gGraphNodePool, NULL, &sp24));

    gGeoLayoutCommand += GL_CMDSIZE(0x10);
}

/** Geo layout command: creates a animated part node. */
void GeoLayout_CmdCreateAnimatedPart(void) {
    Vec3f sp3C;
    s16 var_a3 = 1;

    sp3C.x = GL_U32(0x10) / 65536.0f;
    sp3C.y = GL_U32(0x14) / 65536.0f;
    sp3C.z = GL_U32(0x18) / 65536.0f;

    if (GL_U8(0x2) & 1) {
        var_a3 = 0;
    }

    if (GL_U8(0x2) & 2) {
        var_a3 |= 2;
    }

    GeoLayout_RegisterGraphNode(
        GeoNode_CreateAnimatedPart(gGraphNodePool, NULL, GL_U8(0x1), var_a3, GL_U8(0x3), GL_VEC3S(0x4), GL_VEC3S(0xA), &sp3C));

    gGeoLayoutCommand += GL_CMDSIZE(0x1C);
}

/** Geo layout command: creates a display list part node. */
void GeoLayout_CmdCreateDisplayListPart(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateDisplayListPart(gGraphNodePool, NULL, GL_U8(0x1), ((Gfx*)GL_PTR(0x4)), GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a model part node. */
void GeoLayout_CmdCreateModelPart(void) {
    Vec3s sp40;
    Vec3f sp34;
    Vec3f sp28;
    DisplayObject* tmp;

    Vec3f_FromVec3s(&sp34, GL_VEC3S(0xA));

    sp28.x = GL_S16(0x10) / 100.0f;
    sp28.y = GL_S16(0x12) / 100.0f;
    sp28.z = GL_S16(0x14) / 100.0f;

    Vec3s_SetComponents(&sp40, (GL_S16(0x4) << 0xF) / 180, (GL_S16(0x6) << 0xF) / 180, (GL_S16(0x8) << 0xF) / 180);
    tmp = GeoNode_CreateModelPart(gGraphNodePool, NULL, GL_S16(0x2), &sp34, &sp40, &sp28);
    GeoLayout_RegisterGraphNode(tmp);

    gGeoLayoutCommand += GL_CMDSIZE(0x18);
}

/** Geo layout command: creates a display list matrix node. */
void GeoLayout_CmdCreateDisplayListMatrix(void) {
    Vec3f sp34;
    Vec3s sp2C;
    GeoDisplayListMatrixNode* tmp;

    Vec3f_FromVec3s(&sp34, GL_VEC3S(0xA));
    Vec3s_SetComponents(&sp2C, (GL_S16(0x4) << 0xF) / 180, (GL_S16(0x6) << 0xF) / 180, (GL_S16(0x8) << 0xF) / 180);
    tmp = GeoNode_CreateDisplayListMatrixFromTransform(gGraphNodePool, NULL, GL_U8(0x1), ((Gfx*)GL_PTR(0x10)), &sp34, &sp2C);
    GeoLayout_RegisterGraphNode(tmp);

    gGeoLayoutCommand += GL_CMDSIZE(0x14);
}

/** Geo layout command: creates a scale node. */
void GeoLayout_CmdCreateBillboard(void) {
    Vec3f sp2C;
    GeoBillboardNode* tmp;

    Vec3f_FromVec3s(&sp2C, GL_VEC3S(0x2));
    tmp = GeoNode_CreateBillboard(gGraphNodePool, NULL, GL_U8(0x1), ((Gfx*)GL_PTR(0xC)), &sp2C, GL_U32(0x8) / 65536.0f);
    GeoLayout_RegisterGraphNode(tmp);

    gGeoLayoutCommand += GL_CMDSIZE(0x10);
}

/** Geo layout command: creates a display list node. */
void GeoLayout_CmdCreateDisplayList(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateDisplayList(gGraphNodePool, NULL, GL_U8(0x1), ((Gfx*)GL_PTR(0x4))));

    gGeoLayoutCommand += GL_CMDSIZE(0x8);
}

/** Geo layout command: creates a material node. */
void GeoLayout_CmdCreateMaterial(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateMaterial(gGraphNodePool, NULL, GL_U8(0x1), GL_S16(0x2), ((Gfx*)GL_PTR(0x4)), GL_S16(0x8), GL_S16(0xA),
                                GL_U8(0xC), GL_U8(0xD), GL_U8(0xE), GL_U8(0xF)));

    gGeoLayoutCommand += GL_CMDSIZE(0x10);
}

/** Geo layout command: creates a anchor node. */
void GeoLayout_CmdCreateAnchor(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateAnchor(gGraphNodePool, NULL, GL_S16(0x2)));

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: sets flag 4 on the current node. */
void GeoLayout_CmdSetNodeFlag4(void) {

    if (gCurGraphNodeList[gCurGraphNodeIndex] != NULL) {
        gCurGraphNodeList[gCurGraphNodeIndex]->renderFlags |= 4;
    }

    gGeoLayoutCommand += GL_CMDSIZE(0x4);
}

/** Geo layout command: creates a group node. */
void GeoLayout_CmdCreateGroup(void) {

    GeoLayout_RegisterGraphNode(GeoNode_CreateGroup(gGraphNodePool, NULL, GL_U8(0x1), GL_S16(0x4), GL_S16(0x6), GL_S16(0x8), GL_S16(0xA),
                                GL_U8(0x2), ((s8)GL_U8(0x3)), ((s32)GL_U32(0xC)), GL_U8(0x10), GL_U8(0x11), GL_U8(0x12), GL_U8(0x13)));

    gGeoLayoutCommand += GL_CMDSIZE(0x14);
}

/**
 * Builds a graph node tree from a geo layout command stream and returns its
 * root.
 */
GraphNode* GeoLayout_Process(MemoryBlock* pool, void* segptr) {
    // set by register_scene_graph_node when gCurGraphNodeIndex is 0
    // and gCurRootGraphNode is NULL
    gCurRootGraphNode = NULL;

    // gGeoNumViews seems to have been removed in this version of geo_script.c
    gCurGraphNodeIndex = 0;

    gGeoLayoutStackIndex = 2;
    gGeoLayoutReturnIndex = 2; // stack index is often copied here?

    GeoSetCmd((u8*)Util_ConvertAddrToVirtAddr((uintptr_t)segptr));

    gGraphNodePool = pool;

    gCurGraphNodeList[0] = 0;

    gGeoLayoutStack[0] = 0;
    gGeoLayoutStack[1] = 0;

    while (gGeoLayoutCommand != NULL) {
        if (GL_U8(0x00) >= 39) { // PORT: guard against a misread layout instead of jumping through garbage
            fprintf(stderr, "[geo] bad command 0x%02X at %p (form %d)\n", GL_U8(0x00), (void*)gGeoLayoutCommand,
                    gGeoPacked);
            break;
        }
        GeoLayoutJumpTable[GL_U8(0x00)]();
    }

    return gCurRootGraphNode;
}
