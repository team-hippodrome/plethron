/* Stadium Zero fragment016_252E10. */
/* C ported from the Stadium 1 decomp: src/fragments/lab_pc_list/lab_pc_list_1D7B40.c, src/fragments/lab_pc_list/lab_pc_list_1E46B0.c. */

#include "lab_pokemon_list.h"
#include "lab_item.h"
#include "pokemon_stats.h"
#include "save_data.h"

// Declared here instead of in a header: the callers of these disagree on their types.
void LabPCList_ConsumeUsedItem(TmEntry8910A21C*);
extern Gfx D_8910D028[];

#include "m2c_macros.h"
extern u8 D_8910D7D0[];
typedef struct unk_func_88309614 {
    /* 0x00 */ char unk00[0x8];
    /* 0x08 */ s32 unk_08;
} unk_func_88309614;
typedef struct unk_func_88313894 {
    /* 0x00 */ char unk00[0xC];
    /* 0x0C */ unk_func_8830867C_02C_0CC_000_000* unk_0C;
    /* 0x10 */ char unk10[0xC];
    /* 0x1C */ s32 unk_1C;
    /* 0x20 */ unk_func_88313894_020 unk_20;
    /* 0x40 */ unk_func_88313894_020 unk_40;
} unk_func_88313894;

void LabPCList_ReadDeckCategory(unk_func_8830867C_02C_154* deck);
void LabPCList_WriteDeckCategory(unk_func_8830867C_02C_154* deck);
s32 LabPCList_IsTmHmItemId(s32* itemId);
s32 LabPCList_IsVitaminItemId(s32* itemId);
s32 LabPCList_IsPpUpItemId(s32* itemId);
s32 LabPCList_CompareUsableItems(TmEntry8910A21C* a, TmEntry8910A21C* b);
s32 LabPCList_CanLearnMoveByLevelUp(s32 species, s32 tmIndex);
void LabPCList_ConsumeUsedItem(TmEntry8910A21C* item);

void func_80027804(PointerList* arg0, void** arg1, s32 arg2, s32 arg3); // PORT: matches PointerList_Initialize
/**
 * Sets up the box slots of one source and points each slot at its 0x58-byte entries.
 * Seen in play (2026-11-14) in いちらん: ran three times on entry, for てもち, the GB boxes and the 64 boxes.
 */
void LabPCList_InitSourceBoxes(Entry89100140* src_boxes, LabPCEntry** slot_ptrs, LabPCEntry* entry_buffer, s32 slot_capacity) { // PORT: typed (were N64 strides of 0x20 and 0x58)
    UNUSED s32 pad[2]; // FAKE: stack padding
    LabPCBox* var_s1 = (LabPCBox*)src_boxes->boxes.rows;
    s32 var_t0 = 0;
    s32 i;
    s32 j;

    switch (src_boxes->unk_04) {
        case 0x11:
        case 0x13:
            break;
        case 0x20:
            var_t0 = 1;
            break;
        case 0x21:
            break;
    }

    src_boxes->boxes.count = src_boxes->boxes.capacity;
    for (i = 0; i < src_boxes->boxes.count; i++) {
        var_s1[i].source = src_boxes;
        var_s1[i].index = i;
        var_s1[i].unk_08 = 1;
        var_s1[i].checked = var_t0;
        var_s1[i].changed = 0;
        // The loop has to stay on one line: IDO orders the unrolled stores by source line.
        for (j = 0; j < slot_capacity; j++) { slot_ptrs[j] = &entry_buffer[j]; }
        func_80027804(&var_s1[i].entries, (void**)slot_ptrs, slot_capacity, 0);
        slot_ptrs += slot_capacity;
        entry_buffer += slot_capacity;
    }
}

/* Stadium 1's LabPCList_LoadAllBoxes, without its box-count check and NULL deck check. */
void LabPCList_LoadAllBoxes(ByteMatrix* sources) { // PORT: typed (were N64 views of the sources and boxes)
    s32 i;
    s32 j;
    s32 n;
    DeckHandle* handle;
    LabPCBox* deck;
    LabPCEntry** node;
    Entry89100140* box;

    box = (Entry89100140*)sources->rows;
    for (i = 0; i < sources->count; i++, box++) {
        deck = (LabPCBox*)box->boxes.rows;
        for (j = 0; j < box->boxes.count; j++, deck++) {
            node = (LabPCEntry**)deck->entries.items;
            handle = Deck_Open(box->unk_04, box->unk_08, j, 0);
            n = 0;
            while (TRUE) {
                if (Deck_ReadEntries((u8*)&(*node)->mon, 1, handle) == 0) {
                    break;
                }
                (*node)->box = deck;
                (*node)->slot = n;
                node++;
                n++;
            }
            Deck_CloseAndFlush(handle);
            deck->entries.count = n;
        }
    }
}

/** Flattens the list's nested entries into one list. */
void LabPCList_FlattenEntries(PointerList* list, ByteMatrix* sources) { // PORT: typed (were N64 views and s32 pointer copies)
    s32 i;
    s32 j;
    s32 x;
    void** var_v0;
    LabPCBox* var_v1;
    void** var_t0;
    Entry89100140* var_a2;

    var_v0 = list->items;
    var_a2 = (Entry89100140*)sources->rows;

    for (i = 0; i < sources->count; i++, var_a2++) {
        var_v1 = (LabPCBox*)var_a2->boxes.rows;

        for (j = 0; j < var_a2->boxes.count; j++, var_v1++) {
            if (var_v1->checked != 0) {
                var_t0 = var_v1->entries.items;

                for (x = 0; x < var_v1->entries.count; x++, var_t0++, var_v0++) {
                    *var_v0 = *var_t0;
                }
            }
        }
    }

    list->count = var_v0 - list->items;
}

void GbSave_SetSeenOwnedBits(s32 arg0, s32 arg1, s32 arg2);

/* Stadium 1's LabPCList_SaveAllBoxes, with every deck flushed the same way. */
void LabPCList_SaveDirtyBoxes(ByteMatrix* sources) { // PORT: typed (were N64 views of the sources and boxes)
    s32 i;
    s32 j;
    s32 x;
    DeckHandle* handle;
    LabPCBox* deck;
    LabPCEntry** node;
    Entry89100140* box;

    box = (Entry89100140*)sources->rows;
    for (i = 0; i < sources->count; i++, box++) {
        deck = (LabPCBox*)box->boxes.rows;
        for (j = 0; j < box->boxes.count; j++, deck++) {
            if (deck->changed != 0) {
                node = (LabPCEntry**)deck->entries.items;
                handle = Deck_Open(box->unk_04, box->unk_08, j, 1);
                for (x = 0; x < deck->entries.count; x++, node++) {
                    Deck_WriteEntries((u8*)&(*node)->mon, 1, handle);
                    if ((box->unk_04 == 0x20) || (box->unk_04 == 0x21)) {
                        GbSave_SetSeenOwnedBits(box->unk_08, (*node)->mon.species.id, 3);
                    }
                }
                Deck_CloseAndFlush(handle);
            }
            if (1) {} // karodo: also in Stadium 1's version
        }
    }
}

s32 func_80025BD8(void);
void func_80027410(ByteMatrix* arg0, void* arg1, s32 arg2, s32 arg3, s32 arg4); // PORT: matches TableView_Initialize
void func_80027FF8(GbSavePortSummary* arg0, s32 arg1); // PORT: matches GbSave_BuildPortSummary
s32 func_800246A8(s32 arg0, s32 arg1); // PORT: matches Deck_IsSaveEntryPresent
s32 Deck_GetSaveEntryTrainerId(s32, s32);
s32 Deck_GetSaveEntryName(s32 arg0, s32 arg1, u8* arg2);
s32 Deck_GetSaveEntryCount(s32, s32);
size_t func_8000A4E8(char* dest, char* src); // PORT: matches HAL_Strcmp
/**
 * Sets up the three item lists: the bag (deck 0x23, up to 20 rows), the GB cartridge's PC (deck 0x22,
 * up to 50 rows) and the Game Pak item box in the 64 cartridge (deck 0x12, up to 100 rows). The item box
 * is the one of the four 0x12 slots whose trainer ID and name match the GB save; with no match the third
 * list is marked absent (unk_04 = 0).
 */
void LabPCList_InitDeckCategories(unk_func_8830867C_02C_154* decks, void* bag_buffer, void* pc_buffer, void* cart_buffer) {
    UNUSED s32 pad; // FAKE: stack padding
    s32 temp_s1;
    // PORT: GbSave_BuildPortSummary writes the trainer name after the summary, where IDO put sp54; on the host the two
    // are one object so the name cannot run over the frame (it overwrote the saved screen pointer and List crashed)
    struct {
        GbSavePortSummary summary;
        char name[12];
    } sp48;
#define sp54 (sp48.name)
    char sp38[16];
    s32 i;

    decks->unk_00 = 0x23;
    decks->unk_01 = func_80025BD8();
    decks->unk_02 = 0;
    decks->unk_04 = 1;
    func_80027410(&decks[0].unk_0C, bag_buffer, 2, 0x14, 0);

    decks[1].unk_00 = 0x22;
    decks[1].unk_01 = decks->unk_01;
    decks[1].unk_02 = 0;
    decks[1].unk_04 = 1;
    func_80027410(&decks[1].unk_0C, pc_buffer, 2, 0x32, 0);

    decks[2].unk_00 = 0x12;
    decks[2].unk_01 = 0;
    func_80027410(&decks[2].unk_0C, cart_buffer, 2, 0x64, 0);

    func_80027FF8(&sp48.summary, decks->unk_01);

    for (i = 0; i < 4; i++) {
        if ((func_800246A8(0x12, i) != 0) && (Deck_GetSaveEntryCount(0x12, i) > 0)) {
            temp_s1 = Deck_GetSaveEntryTrainerId(0x12, i);
            Deck_GetSaveEntryName(0x12, i, sp38);
            if ((sp48.summary.trainerId == temp_s1) && (func_8000A4E8(sp54, sp38) == 0)) {
                break;
            }
        }
    }

    if (i < 4) {
        decks[2].unk_02 = i;
        decks[2].unk_04 = 1;
    } else {
        decks[2].unk_04 = 0;
    }
#undef sp54
}

/** Reads a deck category into the list. */
void LabPCList_ReadDeckCategory(unk_func_8830867C_02C_154* deck) {
    DeckHandle* sp24;

    sp24 = Deck_Open(deck->unk_00, deck->unk_01, deck->unk_02, 0);
    deck->unk_14 = Deck_ReadEntries(deck->unk_0C, deck->unk_10, sp24);
    Deck_CloseAndFlush(sp24);
    deck->unk_08 = 0;
}

/** Writes the list back to a deck category. */
void LabPCList_WriteDeckCategory(unk_func_8830867C_02C_154* deck) {
    DeckHandle* sp24 = Deck_Open(deck->unk_00, deck->unk_01, deck->unk_02, 1);

    Deck_WriteEntries(deck->unk_0C, deck->unk_14, sp24);
    Deck_CloseAndFlush(sp24);
    deck->unk_08 = 0;
}

/**
 * Returns whether an item id is a TM or HM and turns it into its usable-item code: TMs (201-250) 100-149, HMs (196-200) 200-204.
 * Seen in play (2026-11-14) in いちらん: わざマシン06 was listed among the usable items.
 */
s32 LabPCList_IsTmHmItemId(s32* item_id) {
    if ((*item_id >= 201) && (*item_id < 251)) {
        *item_id -= 101;
        return 1;
    }
    if ((*item_id >= 196) && (*item_id < 201)) {
        *item_id += 4;
        return 1;
    }
    return 0;
}

/**
 * Returns whether an item id is HP Up, Protein, Iron, Carbos, Calcium or Rare Candy (35-40) and turns it into 0-5.
 * Seen in play (2026-11-14) in いちらん: マックスアップ, タウリン, ブロムヘキシン, インドメタシン, リゾチウム and ふしぎなアメ were listed.
 */
s32 LabPCList_IsVitaminItemId(s32* item_id) {
    static u8 D_8831A250[] = { 35, 36, 37, 38, 39, 40 };

    s32 i;

    for (i = 0; i < 6; i++) {
        if (*item_id == D_8831A250[i]) {
            *item_id = i;
            return 1;
        }
    }

    return 0;
}

/**
 * Returns whether an item id is PP Up (0x4F) and turns it into the usable-item code 50.
 * Seen in play (2026-11-14) in いちらん: ポイントアップ was listed.
 */
s32 LabPCList_IsPpUpItemId(s32* item_id) {
    static u8 D_8831A258[] = { 0x4F };

    s32 i;

    for (i = 0; i < 1; i++) {
        if (*item_id == D_8831A258[i]) {
            *item_id = i + 0x32;
            return 1;
        }
    }

    return 0;
}

/**
 * Usable-item list comparison: by usable-item code, then by bag position.
 * Seen in play (2026-11-14) in いちらん: the list came out vitamins, ふしぎなアメ, ポイントアップ, わざマシン06.
 */
s32 LabPCList_CompareUsableItems(TmEntry8910A21C* a, TmEntry8910A21C* b) { // PORT: typed (read the fields at 8 and 0xC as PointerLists)
    return (a->unk_08 - b->unk_08 != 0) ? a->unk_08 - b->unk_08 : a->unk_0C - b->unk_0C;
}

/**
 * Collects the usable items (vitamins, PP Up, TMs, HMs) of every item deck into one sorted list.
 * Seen in play (2026-11-14) in いちらん: built the list of 「フシギダネがつかえるどうぐ」.
 */
void LabPCList_BuildUsableItemList(PointerList* list, TmEntry8910A21C* entries, unk_func_8830867C_02C_154* decks, s32 deck_count) {
    void** var_s6;
    unk_func_8830867C_02C_154* var_s4;
    s32 i;
    TmEntry8910A21C* var_s1;
    s32 j;
    s32 sp50;
    s32 var_s0;
    s32 var_v0;
    u8* var_s2;

    var_s6 = list->items;
    var_s4 = decks;
    var_s1 = entries;

    for (i = 0; i < deck_count; i++, var_s4++) {
        if (var_s4->unk_04 != 0) {
            var_s2 = var_s4->unk_0C;

            for (j = 0; j < var_s4->unk_14; j++, var_s2 += 2) {
                var_s0 = 0;
                sp50 = *var_s2;
                if (LabPCList_IsTmHmItemId(&sp50) != 0) {
                    var_s0 = 1;
                } else if (LabPCList_IsVitaminItemId(&sp50) != 0) {
                    var_s0 = 1;
                } else if (LabPCList_IsPpUpItemId(&sp50) != 0) {
                    var_s0 = 1;
                }

                if (var_s0 != 0) {
                    var_s1->unk_00 = var_s2;
                    var_s1->unk_04 = var_s4;
                    var_s1->unk_08 = sp50;

                    if (var_s4->unk_00 == 0x23) {
                        var_s1->unk_0C = 0;
                    } else {
                        if (var_s4->unk_00 == 0x22) {
                            var_v0 = 1;
                        } else {
                            var_v0 = 2;
                        }
                        var_s1->unk_0C = var_v0;
                    }
                    *var_s6 = var_s1;

                    var_s1++;
                    var_s6++;
                }
            }
        }
    }

    list->count = var_s1 - entries;
    PointerList_Sort(list, LabPCList_CompareUsableItems);
}

/** Returns whether a species learns a move by level up. */
s32 LabPCList_CanLearnMoveByLevelUp(s32 species, s32 tm_index) {
    static SpeciesData D_8831757C[] = {
        { 0x06, 0x4E, 0x54, 0x4E, 0x64, 0x55, 0x14, 0x02, 0x2D, 0xD1, {0x0A, 0x2D, 0x34, 0x2B}, 0x03, {0xB5, 0x43, 0x4F, 0xCE, 0xE3, 0x08, 0x2E}, 0x00 },
        { 0x0C, 0x3C, 0x2D, 0x32, 0x46, 0x50, 0x07, 0x02, 0x2D, 0xA0, {0x5D, 0x00, 0x00, 0x00}, 0x00, {0x2A, 0x43, 0x38, 0xF0, 0x43, 0x28, 0x42}, 0x00 },
        { 0x30, 0x3C, 0x37, 0x32, 0x2D, 0x28, 0x07, 0x03, 0xBE, 0x4B, {0x21, 0x32, 0x00, 0x00}, 0x00, {0x20, 0x03, 0x38, 0xD0, 0x03, 0x28, 0x42}, 0x00 },
        { 0x31, 0x46, 0x41, 0x3C, 0x5A, 0x5A, 0x07, 0x03, 0x4B, 0x8A, {0x21, 0x32, 0x4D, 0x8D}, 0x00, {0x2A, 0x43, 0x38, 0xF0, 0x43, 0x28, 0x42}, 0x00 },
        { 0x32, 0x0A, 0x37, 0x19, 0x5F, 0x2D, 0x04, 0x04, 0xFF, 0x51, {0x0A, 0x00, 0x00, 0x00}, 0x00, {0xA0, 0x03, 0x08, 0xCE, 0x02, 0x88, 0x06}, 0x00 },
        { 0x33, 0x23, 0x50, 0x32, 0x78, 0x46, 0x04, 0x04, 0x32, 0x99, {0x0A, 0x2D, 0x5B, 0x00}, 0x00, {0xA0, 0x43, 0x08, 0xCE, 0x02, 0x88, 0x06}, 0x00 },
        { 0x8D, 0x3C, 0x73, 0x69, 0x50, 0x46, 0x05, 0x15, 0x2D, 0xC9, {0x0A, 0x6A, 0x47, 0x00}, 0x00, {0xB6, 0x7F, 0x0D, 0xC0, 0x83, 0x08, 0x16}, 0x00 },
        { 0x96, 0x6A, 0x6E, 0x5A, 0x82, 0x9A, 0x18, 0x18, 0x03, 0xDC, {0x5D, 0x32, 0x81, 0x5E}, 0x05, {0xB1, 0x7F, 0xAF, 0xF1, 0xAF, 0x38, 0x63}, 0x00 },
        { 0x97, 0x64, 0x64, 0x64, 0x64, 0x64, 0x18, 0x18, 0x2D, 0x40, {0x01, 0x00, 0x00, 0x00}, 0x03, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F}, 0x00 },
    };

    SpeciesData* var_v1;
    s32 i;

    var_v1 = &gSpeciesData[species - 1];

    if (GbSave_GetPortGame(GbSave_GetActivePort()) == 7) {
        for (i = 0; i < 9; i++) {
            if (species == D_8831757C[i].speciesId) {
                var_v1 = &D_8831757C[i];
            }
        }
    }
    return ((var_v1->tmHmLearnset[tm_index / 8] >> (tm_index % 8)) & 1) != 0;
}

u8 func_80020974(s32 tm_hm_index); // PORT: matches Move_GetIdFromTableIndex
s32 BattleMon_CountMoves(u8* arg0);
s32 BattleMon_GetMoveIndex(u8* arg0, s32 arg1); // PORT: matches the definition

/**
 * Sets each usable item's flags for a Pokémon: 1 vitamin stat exp at the cap, 2 already level 100, 4 no move can take a PP Up, 8 already knows the TM/HM move, 0x10 can't learn it.
 * Seen in play (2026-11-14) in いちらん: ran when どうぐをつかう opened; わざマシン06 was usable on フシギダネ and ゼニガメ.
 */
void LabPCList_UpdateItemUsability(PointerList* items, u8* mon) { // PORT: items typed (was s8* and N64 offsets), mon is BattleMon bytes
    s32 i;
    s32 j;
    s32 n;
    s32 n2;
    s32 k;
    s32 move;
    TmEntry8910A21C** base;
    TmEntry8910A21C* e;
    u8* p;

    base = (TmEntry8910A21C**)items->items;
    for (i = 0; i < items->count; i++) {
        base[i]->unk_10 = 0;
        e = base[i];
        if (e->unk_08 >= 100) {
            if (*e->unk_00 >= 201) {
                move = *e->unk_00 - 201;
            } else {
                move = *e->unk_00 - 146;
            }
            n = BattleMon_CountMoves(mon);
            for (j = 0; j < n; j++) {
                if (func_80020974(move) == mon[j + 9]) {
                    break;
                }
            }
            if (j < n) {
                base[i]->unk_10 |= 8;
            }
            if (LabPCList_CanLearnMoveByLevelUp(mon[0], move) == 0) {
                base[i]->unk_10 |= 0x10;
            }
        } else if (e->unk_08 >= 50) {
            n2 = BattleMon_CountMoves(mon);
            for (j = 0; j < n2; j++) {
                if ((BattleMon_GetMoveIndex(mon, j) != -2) && ((mon + j)[0x20] < 0xC0)) {
                    break;
                }
            }
            if (j >= n2) {
                base[i]->unk_10 |= 4;
            }
        } else {
            switch (*e->unk_00) {
                case 35:
                    if (M2C_FIELD(mon, u16*, 0x14) >= 25600) {
                        e->unk_10 |= 1;
                    }
                    break;
                case 36:
                    if (M2C_FIELD(mon, u16*, 0x16) >= 25600) {
                        e->unk_10 |= 1;
                    }
                    break;
                case 37:
                    if (M2C_FIELD(mon, u16*, 0x18) >= 25600) {
                        e->unk_10 |= 1;
                    }
                    break;
                case 38:
                    if (M2C_FIELD(mon, u16*, 0x1A) >= 25600) {
                        e->unk_10 |= 1;
                    }
                    break;
                case 39:
                    if (M2C_FIELD(mon, u16*, 0x1C) >= 25600) {
                        e->unk_10 |= 1;
                    }
                    break;
                case 40:
                    if (mon[0x24] >= 100) {
                        e->unk_10 |= 2;
                    }
                    break;
            }
        }
    }
}

/**
 * Takes one of the used item out of its deck (removing the entry at quantity 1) and marks the deck changed.
 * Seen in play (2026-11-14) in いちらん: ran after マックスアップ, ポイントアップ and わざマシン06; the TM06 count went from 1 to none.
 */
_Static_assert(__builtin_offsetof(unk_func_8830867C_02C_154, unk_14) - __builtin_offsetof(unk_func_8830867C_02C_154, unk_0C) ==
                   __builtin_offsetof(unk_func_8830867C_04C_030_02C_000_000_00C, count),
               "PORT: the deck's word list must match WordList"); // PORT

void LabPCList_ConsumeUsedItem(TmEntry8910A21C* item) { // PORT: typed (was an N64 view of the deck)
    unk_func_8830867C_02C_154* deck = item->unk_04;

    if ((LabItem_IsQuantitylessItemForOperation(item->unk_00[0]) != 0) || (item->unk_00[1] < 2)) {
        // PORT: the deck's unk_0C/unk_10/unk_14 are the word list (rows, capacity, count)
        WordList_Remove((unk_func_8830867C_04C_030_02C_000_000_00C*)&deck->unk_0C, (item->unk_00 - deck->unk_0C) >> 1);
    } else {
        item->unk_00[1]--;
    }
    deck->unk_08 = 1;
}
