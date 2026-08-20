#include "Catstructor.h"

/* 
* Live movie timeline discovery and valid-ID navigation.. 
*/

static int TimelineChildIsNamedHelper(const uint8_t* entry, const void* child);
static int TimelineFrameFingerprintEquals(const TimelineFrameFingerprint* first, const TimelineFrameFingerprint* second);

#define TIMELINE_HELPER_CHILD_CAPACITY 3
#define TIMELINE_SHARED_TEXTURE_BACKGROUND_CHILDREN 2
#define TIMELINE_TRAILING_TEXTURE_GARBAGE_MIN_CHILDREN 4
#define TIMELINE_TEXTURE_KIND_BODY_MASK 0x01U
#define TIMELINE_TEXTURE_KIND_HEAD_MASK 0x02U
#define TIMELINE_TEXTURE_KIND_TAIL_MASK 0x04U
#define TIMELINE_TEXTURE_KIND_LEG_MASK 0x08U
#define TIMELINE_TEXTURE_KIND_EAR_MASK 0x10U
#define TIMELINE_TEXTURE_KIND_ALL_MASK (TIMELINE_TEXTURE_KIND_BODY_MASK | TIMELINE_TEXTURE_KIND_HEAD_MASK | TIMELINE_TEXTURE_KIND_TAIL_MASK | TIMELINE_TEXTURE_KIND_LEG_MASK | TIMELINE_TEXTURE_KIND_EAR_MASK)

static int g_timelineProfileProbeBudget = TIMELINE_PROFILE_PROBE_BUDGET_PER_FRAME;

typedef struct InitialPartTextureIndexState
{
    AppearanceField field;
    int validIndex;
    int completedTotal;
    int targetTotal;
    int targetsReady;
} InitialPartTextureIndexState;

static InitialPartTextureIndexState g_initialPartTextureIndex =
{
    APPEARANCE_FIELD_BODY, 0, 0, 0, 0
};

void BeginTimelineIndexingFrame(void)
{
    g_timelineProfileProbeBudget = TIMELINE_PROFILE_PROBE_BUDGET_PER_FRAME;
}

static int ConsumeTimelineProfileProbeBudget(void)
{
    if (g_timelineProfileProbeBudget <= 0)
    {
        return 0;
    }

    --g_timelineProfileProbeBudget;
    return 1;
}

void SetAllPartTextures(uint8_t* parts, int texture)
{
    static const size_t textureOffsets[] =
    {
        CATPART_BODY_TEXTURE_OFFSET, CATPART_HEAD_TEXTURE_OFFSET, CATPART_TAIL_TEXTURE_OFFSET, CATPART_LEG1_TEXTURE_OFFSET, CATPART_LEG2_TEXTURE_OFFSET,
        CATPART_ARM1_TEXTURE_OFFSET, CATPART_ARM2_TEXTURE_OFFSET, CATPART_LEFTEYE_TEXTURE_OFFSET, CATPART_RIGHTEYE_TEXTURE_OFFSET,
        CATPART_LEFTEYEBROW_TEXTURE_OFFSET, CATPART_RIGHTEYEBROW_TEXTURE_OFFSET, CATPART_LEFTEAR_TEXTURE_OFFSET, CATPART_RIGHTEAR_TEXTURE_OFFSET,
        CATPART_MOUTH_TEXTURE_OFFSET
    };

    size_t index;

    for (index = 0; index < sizeof(textureOffsets) / sizeof(textureOffsets[0]); ++index)
    {
        *(int*)(parts + textureOffsets[index]) = texture;
    }
}

static int GetMovieClipFrameExtent(void* movieClip)
{
    void* definition;
    int frameExtent;

    if (!movieClip || !g_setMovieClipFrame || !IsReadableMemoryRange(movieClip, MOVIECLIP_CURRENT_FRAME_OFFSET + sizeof(int)))
    {
        return 0;
    }

    definition = *(void**)((uint8_t*)movieClip + MOVIECLIP_DEFINITION_OFFSET);

    if (!IsReadableMemoryRange(definition, sizeof(int)))
    {
        return 0;
    }

    frameExtent = *(const int*)definition;
    return frameExtent > 0 ? frameExtent : 0;
}

static int GetMovieClipChildren(void* movieClip, void*** outChildren, int* outChildTotal)
{
    void** children;
    int childCapacity;
    int childTotal;

    if (outChildren)
    {
        *outChildren = NULL;
    }

    if (outChildTotal)
    {
        *outChildTotal = 0;
    }

    if (!movieClip || !outChildren || !outChildTotal || !IsReadableMemoryRange(movieClip, MOVIECLIP_CHILDREN_OFFSET + sizeof(void*)))
    {
        return 0;
    }

    childCapacity = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CHILD_CAPACITY_OFFSET);
    childTotal = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CHILD_TOTAL_OFFSET);

    if (childCapacity < MOVIECLIP_INLINE_CHILD_CAPACITY || childTotal < 0 || childTotal > childCapacity || (size_t)childTotal > (size_t)-1 / sizeof(void*))
    {
        return 0;
    }

    if (childCapacity > MOVIECLIP_INLINE_CHILD_CAPACITY)
    {
        children = *(void***)((uint8_t*)movieClip + MOVIECLIP_CHILDREN_OFFSET);
    }
    else
    {
        children = (void**)((uint8_t*)movieClip + MOVIECLIP_CHILDREN_OFFSET);
    }

    if (childTotal > 0 && (!children || !IsReadableMemoryRange(children, (size_t)childTotal * sizeof(void*))))
    {
        return 0;
    }

    *outChildren = children;
    *outChildTotal = childTotal;
    return 1;
}

static int ReadTimelineChildKey(const void* child, TimelineChildKey* outKey)
{
    if (!child || !outKey || !IsReadableMemoryRange(child, DISPLAY_OBJECT_LIBRARY_ID_OFFSET + sizeof(uint16_t)))
    {
        return 0;
    }

    outKey->characterID = *(const uint16_t*)((const uint8_t*)child + DISPLAY_OBJECT_CHARACTER_ID_OFFSET);
    outKey->libraryID = *(const uint16_t*)((const uint8_t*)child + DISPLAY_OBJECT_LIBRARY_ID_OFFSET);
    return 1;
}

static int ReadTimelineChildKeyFast(const void* child, TimelineChildKey* outKey)
{
    if (!child || !outKey)
    {
        return 0;
    }

    outKey->characterID = *(const uint16_t*)((const uint8_t*)child + DISPLAY_OBJECT_CHARACTER_ID_OFFSET);
    outKey->libraryID = *(const uint16_t*)((const uint8_t*)child + DISPLAY_OBJECT_LIBRARY_ID_OFFSET);
    return 1;
}

static int IsTimelineChildRenderableFast(const void* child)
{
    unsigned char flags;
    int clipDepth;

    if (!child)
    {
        return 0;
    }

    flags = *((const unsigned char*)child + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET);
    clipDepth = *(const int*)((const uint8_t*)child + DISPLAY_OBJECT_CLIP_DEPTH_OFFSET);
    return (flags & DISPLAY_OBJECT_ACTIVE_MASK) == DISPLAY_OBJECT_ACTIVE_MASK && clipDepth <= 0;
}

static int TimelineChildKeyEquals(const TimelineChildKey* first, const TimelineChildKey* second)
{
    return first && second && first->characterID == second->characterID && first->libraryID == second->libraryID;
}

static int TimelineChildrenContainKey(void** children, int childTotal, const TimelineChildKey* key)
{
    TimelineChildKey candidate;
    int index;

    for (index = 0; index < childTotal; ++index)
    {
        if (ReadTimelineChildKeyFast(children[index], &candidate) && TimelineChildKeyEquals(&candidate, key))
        {
            return 1;
        }
    }

    return 0;
}

static void UpdateMovieClipDisplayList(void* movieClip)
{
    void** vtable;
    fn_movieclip_update update;

    if (!movieClip || !IsReadableMemoryRange(movieClip, sizeof(void*)))
    {
        return;
    }

    vtable = *(void***)movieClip;

    if (!IsReadableMemoryRange(vtable, 4 * sizeof(void*)))
    {
        return;
    }

    // CatPartGraphics calls MovieClip's vtable +0x18 after every seek...
    update = (fn_movieclip_update)vtable[3];

    if (update)
    {
        update(movieClip);
    }
}

void SetMovieClipFrameForInspection(void* movieClip, int zeroBasedFrame)
{
    if (!movieClip || !g_setMovieClipFrame)
    {
        return;
    }

    g_setMovieClipFrame(movieClip, zeroBasedFrame);

    if (IsReadableMemoryRange((uint8_t*)movieClip + MOVIECLIP_STATE_FLAGS_OFFSET, sizeof(unsigned char)))
    {
        *((unsigned char*)movieClip + MOVIECLIP_STATE_FLAGS_OFFSET) &= (unsigned char)~MOVIECLIP_PLAYING_FLAG;
    }
    
    UpdateMovieClipDisplayList(movieClip);
}

static void* GetMovieClipNamedChild(void* movieClip, const char* childName)
{
    MsvcString name;

    if (!movieClip || !childName || !g_getMovieClipChild)
    {
        return NULL;
    }

    InitMsvcString(&name, childName);
    return g_getMovieClipChild(movieClip, &name);
}

static void RefreshGraphicsEntryChildren(uint8_t* entry)
{
    void* movieClip;

    if (!entry || !IsReadableMemoryRange(entry, CATPART_GRAPHICS_ENTRY_SIZE))
    {
        return;
    }

    movieClip = *(void**)(entry + CATPART_GRAPHICS_MOVIECLIP_OFFSET);
    *(void**)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET) = GetMovieClipNamedChild(movieClip, "tex");
    *(void**)(entry + CATPART_GRAPHICS_SCARS_CLIP_OFFSET) = GetMovieClipNamedChild(movieClip, "scars");
    *(void**)(entry + CATPART_GRAPHICS_AUX_CLIP_OFFSET) = GetMovieClipNamedChild(movieClip, "aux");
}

static void* RefreshGraphicsEntryTextureChild(uint8_t* entry)
{
    void* movieClip;
    void* textureClip;

    if (!entry || !IsReadableMemoryRange(entry, CATPART_GRAPHICS_ENTRY_SIZE))
    {
        return NULL;
    }

    movieClip = *(void**)(entry + CATPART_GRAPHICS_MOVIECLIP_OFFSET);
    textureClip = GetMovieClipNamedChild(movieClip, "tex");
    *(void**)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET) = textureClip;
    return textureClip;
}

static const char* McpfTextureKindForContainer(size_t containerOffset)
{
    switch (containerOffset)
    {
        case CAT_VISUAL_BODY_GRAPHICS_OFFSET:
            return "body";
        case CAT_VISUAL_HEAD_GRAPHICS_OFFSET:
            return "head";
        case CAT_VISUAL_TAIL_GRAPHICS_OFFSET:
            return "tail";
        case CAT_VISUAL_LEG1_GRAPHICS_OFFSET:
        case CAT_VISUAL_LEG2_GRAPHICS_OFFSET:
        case CAT_VISUAL_ARM1_GRAPHICS_OFFSET:
        case CAT_VISUAL_ARM2_GRAPHICS_OFFSET:
            return "leg";
        case CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET:
        case CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET:
            return "ear";
        default:
            return NULL;
    }
}

static unsigned int TextureKindMaskForContainer(size_t containerOffset)
{
    switch (containerOffset)
    {
        case CAT_VISUAL_BODY_GRAPHICS_OFFSET:
            return TIMELINE_TEXTURE_KIND_BODY_MASK;
        case CAT_VISUAL_HEAD_GRAPHICS_OFFSET:
            return TIMELINE_TEXTURE_KIND_HEAD_MASK;
        case CAT_VISUAL_TAIL_GRAPHICS_OFFSET:
            return TIMELINE_TEXTURE_KIND_TAIL_MASK;
        case CAT_VISUAL_LEG1_GRAPHICS_OFFSET:
        case CAT_VISUAL_LEG2_GRAPHICS_OFFSET:
        case CAT_VISUAL_ARM1_GRAPHICS_OFFSET:
        case CAT_VISUAL_ARM2_GRAPHICS_OFFSET:
            return TIMELINE_TEXTURE_KIND_LEG_MASK;
        case CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET:
        case CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET:
            return TIMELINE_TEXTURE_KIND_EAR_MASK;
        default:
            return 0;
    }
}

static void SyncMcpfTextureChildrenForContainer(size_t containerOffset, uint8_t* entries, int entryTotal, int oneBasedTexture)
{
    const char* kind;
    uint8_t* entry;
    void* textureClip;
    int frameExtent;
    int index;

    kind = McpfTextureKindForContainer(containerOffset);

    if (!entries || entryTotal <= 0)
    {
        return;
    }

    entry = entries;

    for (index = 0; index < entryTotal; ++index, entry += CATPART_GRAPHICS_ENTRY_SIZE)
    {
        // Only tex is relevant here...
        textureClip = RefreshGraphicsEntryTextureChild(entry);

        if (!textureClip)
        {
            continue;
        }

        if (kind)
        {
            SyncMcpfTextureClip(kind, textureClip);
        }

        if (oneBasedTexture > 0)
        {
            frameExtent = GetMovieClipFrameExtent(textureClip);

            if (oneBasedTexture <= frameExtent)
            {
                SetMovieClipFrameForInspection(textureClip, oneBasedTexture - 1);
            }
        }
    }
}

int GetGraphicsEntries(void* catVisual, size_t containerOffset, uint8_t** outEntries, int* outEntryTotal)
{
    uint8_t* container;
    uint8_t* entries;
    size_t byteExtent;
    int entryTotal;

    if (outEntries)
    {
        *outEntries = NULL;
    }

    if (outEntryTotal)
    {
        *outEntryTotal = 0;
    }

    if (!catVisual || !outEntries || !outEntryTotal)
    {
        return 0;
    }

    container = (uint8_t*)catVisual + containerOffset;

    if (!IsReadableMemoryRange(container, CATPART_GRAPHICS_ENTRIES_OFFSET + sizeof(void*)))
    {
        return 0;
    }

    entryTotal = *(const int*)(container + CATPART_GRAPHICS_ENTRY_TOTAL_OFFSET);
    entries = *(uint8_t**)(container + CATPART_GRAPHICS_ENTRIES_OFFSET);

    if (entryTotal <= 0 || !entries || (size_t)entryTotal > (size_t) - 1 / CATPART_GRAPHICS_ENTRY_SIZE)
    {
        return 0;
    }

    byteExtent = (size_t)entryTotal * CATPART_GRAPHICS_ENTRY_SIZE;

    if (!IsReadableMemoryRange(entries, byteExtent))
    {
        return 0;
    }

    *outEntries = entries;
    *outEntryTotal = entryTotal;
    return 1;
}

/*
* Synchronize MCPF texture timelines on the live CatVisual and then
* reapply the currently selected texture frame to those clips...
*/
void SyncMcpfVisualTextureState(void* catVisual, int oneBasedTexture)
{
    static const size_t textureContainers[] =
    {
        CAT_VISUAL_BODY_GRAPHICS_OFFSET,
        CAT_VISUAL_HEAD_GRAPHICS_OFFSET,
        CAT_VISUAL_TAIL_GRAPHICS_OFFSET,
        CAT_VISUAL_LEG1_GRAPHICS_OFFSET,
        CAT_VISUAL_LEG2_GRAPHICS_OFFSET,
        CAT_VISUAL_ARM1_GRAPHICS_OFFSET,
        CAT_VISUAL_ARM2_GRAPHICS_OFFSET,
        CAT_VISUAL_LEFTEYE_GRAPHICS_OFFSET,
        CAT_VISUAL_RIGHTEYE_GRAPHICS_OFFSET,
        CAT_VISUAL_LEFTEYEBROW_GRAPHICS_OFFSET,
        CAT_VISUAL_RIGHTEYEBROW_GRAPHICS_OFFSET,
        CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET,
        CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET,
        CAT_VISUAL_MOUTH_GRAPHICS_OFFSET
    };

    size_t containerIndex;
    uint8_t* entries;
    int entryTotal;

    if (!catVisual || oneBasedTexture < 1)
    {
        return;
    }

    /*
    * Synchronization here, immediately after an actual CatVisual
    * refresh/rebuild. GetGraphicsEntries is intentionally a pure accessor,
    * timeline probing calls it a bunch of times while building ID maps...
    */
    for (containerIndex = 0; containerIndex < sizeof(textureContainers) / sizeof(textureContainers[0]); ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, textureContainers[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        SyncMcpfTextureChildrenForContainer(textureContainers[containerIndex], entries, entryTotal, oneBasedTexture);
    }
}

static size_t GetTimelineContainerOffsets(AppearanceField field, size_t offsets[TIMELINE_CONTAINER_CAPACITY])
{
    static const size_t textureContainers[] =
    {
        CAT_VISUAL_BODY_GRAPHICS_OFFSET, CAT_VISUAL_HEAD_GRAPHICS_OFFSET, CAT_VISUAL_TAIL_GRAPHICS_OFFSET, CAT_VISUAL_LEG1_GRAPHICS_OFFSET,
        CAT_VISUAL_LEG2_GRAPHICS_OFFSET, CAT_VISUAL_ARM1_GRAPHICS_OFFSET, CAT_VISUAL_ARM2_GRAPHICS_OFFSET, CAT_VISUAL_LEFTEYE_GRAPHICS_OFFSET,
        CAT_VISUAL_RIGHTEYE_GRAPHICS_OFFSET, CAT_VISUAL_LEFTEYEBROW_GRAPHICS_OFFSET, CAT_VISUAL_RIGHTEYEBROW_GRAPHICS_OFFSET,
        CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET, CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET, CAT_VISUAL_MOUTH_GRAPHICS_OFFSET
    };

    size_t extent;

    if (!offsets)
    {
        return 0;
    }

    switch (field)
    {
        case APPEARANCE_FIELD_TEXTURE:
            extent = sizeof(textureContainers);
            memcpy(offsets, textureContainers, extent);
            return extent / sizeof(textureContainers[0]);
        case APPEARANCE_FIELD_BODY:
            offsets[0] = CAT_VISUAL_BODY_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_HEAD:
            offsets[0] = CAT_VISUAL_HEAD_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_TAIL:
            offsets[0] = CAT_VISUAL_TAIL_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_LEG1:
            offsets[0] = CAT_VISUAL_LEG1_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_LEG2:
            offsets[0] = CAT_VISUAL_LEG2_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_ARM1:
            offsets[0] = CAT_VISUAL_ARM1_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_ARM2:
            offsets[0] = CAT_VISUAL_ARM2_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_LEFTEYE:
            offsets[0] = CAT_VISUAL_LEFTEYE_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_RIGHTEYE:
            offsets[0] = CAT_VISUAL_RIGHTEYE_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_LEFTEYEBROW:
            offsets[0] = CAT_VISUAL_LEFTEYEBROW_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_RIGHTEYEBROW:
            offsets[0] = CAT_VISUAL_RIGHTEYEBROW_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_LEFTEAR:
            offsets[0] = CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_RIGHTEAR:
            offsets[0] = CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET;
            return 1;
        case APPEARANCE_FIELD_MOUTH:
            offsets[0] = CAT_VISUAL_MOUTH_GRAPHICS_OFFSET;
            return 1;
        default:
            return 0;
    }
}

static void* GetTimelineEntryClip(uint8_t* entry, AppearanceField field)
{
    if (!entry || !IsReadableMemoryRange(entry, CATPART_GRAPHICS_ENTRY_SIZE))
    {
        return NULL;
    }

    if (field == APPEARANCE_FIELD_TEXTURE)
    {
        void* textureClip = *(void**)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET);

        /* 
        * Outer part frame changes refresh this cache explicitly. Texture-ID
        * probing itself never changes the outer frame, so don't perform a
        * native name lookup for every candidate...
        */
        if (!textureClip)
        {
            textureClip = RefreshGraphicsEntryTextureChild(entry);
        }

        return textureClip;
    }

    return *(void**)(entry + CATPART_GRAPHICS_MOVIECLIP_OFFSET);
}

int FieldSkipsBlankFrames(AppearanceField field)
{
    return field == APPEARANCE_FIELD_TEXTURE || NamedKindForField(field) != NULL;
}

int GetFieldTimelineExtent(void* catVisual, AppearanceField field)
{
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerIndex;
    size_t containerTotal;
    uint8_t* entries;
    uint8_t* entry;
    void* movieClip;
    int entryTotal;
    int frameExtent;
    int greatestExtent;

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);
    greatestExtent = 0;

    for (containerIndex = 0; containerIndex < containerTotal; ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        /*
        * CatPartGraphics appends the selectable base art first. Later
        * entries are passive/effect overlays and can have unrelated frame
        * extents, so don't enlarge editor ID range...
        */
        entry = entries;
        movieClip = GetTimelineEntryClip(entry, field);
        frameExtent = GetMovieClipFrameExtent(movieClip);

        if (frameExtent > greatestExtent)
        {
            greatestExtent = frameExtent;
        }
    }

    return greatestExtent;
}

static void ReleaseTimelineProfile(TimelineProfile* profile)
{
    if (!profile)
    {
        return;
    }

    if (profile->commonChildren)
    {
        HeapFree(GetProcessHeap(), 0, profile->commonChildren);
    }

    if (profile->backgroundChildren)
    {
        HeapFree(GetProcessHeap(), 0, profile->backgroundChildren);
    }

    if (profile->soleSpecificKeys)
    {
        HeapFree(GetProcessHeap(), 0, profile->soleSpecificKeys);
    }

    if (profile->backgroundFirstFrames)
    {
        HeapFree(GetProcessHeap(), 0, profile->backgroundFirstFrames);
    }

    if (profile->distinctArtFrames)
    {
        HeapFree(GetProcessHeap(), 0, profile->distinctArtFrames);
    }

    if (profile->specificChildTotals)
    {
        HeapFree(GetProcessHeap(), 0, profile->specificChildTotals);
    }

    if (profile->renderedChildTotals)
    {
        HeapFree(GetProcessHeap(), 0, profile->renderedChildTotals);
    }

    if (profile->frameFingerprintFirst)
    {
        HeapFree(GetProcessHeap(), 0, profile->frameFingerprintFirst);
    }

    if (profile->frameFingerprintSecond)
    {
        HeapFree(GetProcessHeap(), 0, profile->frameFingerprintSecond);
    }

    if (profile->buildState)
    {
        HeapFree(GetProcessHeap(), 0, profile->buildState);
    }

    memset(profile, 0, sizeof(*profile));
}

void ClearTimelineProfiles(void)
{
    size_t index;

    for (index = 0; index < TIMELINE_PROFILE_CACHE_CAPACITY; ++index)
    {
        ReleaseTimelineProfile(&g_timelineProfiles[index]);
    }

    g_timelineProfileUsed = 0;

    /* 
    * A fresh editor scene must discover the texture definition behind every
    * selectable part frame again. This includes MCPF-added/modded frames...
    */
    memset(&g_initialPartTextureIndex, 0, sizeof(g_initialPartTextureIndex));
    g_initialPartTextureIndex.field = APPEARANCE_FIELD_BODY;
}

static void ReleaseTimelineIdMap(TimelineIDMap* map)
{
    if (!map)
    {
        return;
    }

    if (map->validIDs)
    {
        HeapFree(GetProcessHeap(), 0, map->validIDs);
    }

    memset(map, 0, sizeof(*map));
}

void ClearTimelineIdMaps(void)
{
    size_t index;

    for (index = 0; index < APPEARANCE_FIELD_COUNT; ++index)
    {
        ReleaseTimelineIdMap(&g_timelineIDMaps[index]);
    }

    ReleaseTimelineIdMap(&g_paletteIDMap);
}

static int TimelineChildKeyArrayContains(const TimelineChildKey* keys, int keyTotal, const TimelineChildKey* key)
{
    int index;

    if (!keys || !key)
    {
        return 0;
    }

    for (index = 0; index < keyTotal; ++index)
    {
        if (TimelineChildKeyEquals(&keys[index], key))
        {
            return 1;
        }
    }

    return 0;
}

static void RememberTimelineHelperKey(TimelineProfile* profile, const TimelineChildKey* key)
{
    if (!profile || !profile->commonChildren || !key || profile->commonChildTotal >= TIMELINE_HELPER_CHILD_CAPACITY || TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, key))
    {
        return;
    }

    profile->commonChildren[profile->commonChildTotal++] = *key;
}

static void InitializeTimelineHelperKeys(TimelineProfile* profile, void* movieClip)
{
    static const char* helperNames[TIMELINE_HELPER_CHILD_CAPACITY] = { "tex", "scars", "aux" };
    TimelineChildKey key;
    void* child;
    int index;

    if (!profile || !movieClip)
    {
        return;
    }

    profile->commonChildren = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), 0, TIMELINE_HELPER_CHILD_CAPACITY * sizeof(*profile->commonChildren));
    
    if (!profile->commonChildren)
    {
        return;
    }

    profile->commonChildTotal = 0;

    for (index = 0; index < TIMELINE_HELPER_CHILD_CAPACITY; ++index)
    {
        child = GetMovieClipNamedChild(movieClip, helperNames[index]);

        if (ReadTimelineChildKey(child, &key))
        {
            RememberTimelineHelperKey(profile, &key);
        }
    }
}

static int IsTimelineChildRenderable(const void* child)
{
    unsigned char flags;
    int clipDepth;

    if (!child || !IsReadableMemoryRange(child, DISPLAY_OBJECT_CLIP_DEPTH_OFFSET + sizeof(int)))
    {
        return 0;
    }

    flags = *((const unsigned char*)child + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET);
    clipDepth = *(const int*)((const uint8_t*)child + DISPLAY_OBJECT_CLIP_DEPTH_OFFSET);
    
    return (flags & DISPLAY_OBJECT_ACTIVE_MASK) == DISPLAY_OBJECT_ACTIVE_MASK && clipDepth <= 0;
}

static int FindSoleTimelineSpecificChild(void** children, int childTotal, const TimelineProfile* profile, TimelineChildKey* outKey)
{
    TimelineChildKey childKey;
    TimelineChildKey soleKey;
    int childIndex;
    int found;

    if (!profile || !outKey)
    {
        return 0;
    }

    found = 0;
    memset(&soleKey, 0, sizeof(soleKey));

    for (childIndex = 0; childIndex < childTotal; ++childIndex)
    {
        if (!IsTimelineChildRenderableFast(children[childIndex]) || !ReadTimelineChildKeyFast(children[childIndex], &childKey) || TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
        {
            continue;
        }

        if (!found)
        {
            soleKey = childKey;
            found = 1;
        }
        else if (!TimelineChildKeyEquals(&soleKey, &childKey))
        {
            return 0;
        }
    }

    if (!found)
    {
        return 0;
    }

    *outKey = soleKey;
    return 1;
}

typedef struct TimelineBackgroundCandidate
{
    TimelineChildKey key;
    int firstFrame;
    int maxRepeat;
} TimelineBackgroundCandidate;

static int TimelineChildIsNamedHelperDirect(const void* child)
{
    const MsvcString* name;
    const char* text;

    if (!child)
    {
        return 0;
    }

    /*
    * Child comes directly from MovieClip's validated live child array.
    * Avoid VirtualQuery in this per-child/per-frame loop, the engine's
    * own child-name scanner reads the same DisplayObject string in place...
    */
    name = (const MsvcString*)((const uint8_t*)child + DISPLAY_OBJECT_NAME_OFFSET);

    /* 
    * Use a single validation/read and dispatch by length, instead of constructing
    * three names and invoking MovieClip::GetChild three times for every probed frame...
    */
    if (name->length != 3 && name->length != 5)
    {
        return 0;
    }

    if (name->capacity < name->length)
    {
        return 0;
    }

    if (name->capacity <= 15)
    {
        text = name->storage.small;
    }
    else
    {
        text = name->storage.heap;

        if (!text || !IsReadableMemoryRange(text, name->length))
        {
            return 0;
        }
    }

    if (name->length == 3)
    {
        return memcmp(text, "tex", 3) == 0 || memcmp(text, "aux", 3) == 0;
    }

    return memcmp(text, "scars", 5) == 0;
}

static int FindBackgroundCandidate(TimelineBackgroundCandidate* candidates, int candidateTotal, const TimelineChildKey* key)
{
    int index;

    if (!candidates || !key)
    {
        return -1;
    }

    for (index = 0; index < candidateTotal; ++index)
    {
        if (TimelineChildKeyEquals(&candidates[index].key, key))
        {
            return index;
        }
    }

    return -1;
}

static int EnsureBackgroundCandidate(TimelineBackgroundCandidate* candidates, int* candidateTotal, int candidateCapacity, const TimelineChildKey* key, int frame)
{
    int index;

    if (!candidates || !candidateTotal || !key)
    {
        return -1;
    }

    index = FindBackgroundCandidate(candidates, *candidateTotal, key);

    if (index >= 0)
    {
        return index;
    }

    if (*candidateTotal >= candidateCapacity)
    {
        return -1;
    }

    index = (*candidateTotal)++;
    candidates[index].key = *key;
    candidates[index].firstFrame = frame;
    candidates[index].maxRepeat = 0;
    return index;
}

static void FinishBackgroundRun(TimelineBackgroundCandidate* candidates, int candidateTotal, const TimelineChildKey* key, int repeatLength)
{
    int index;

    if (!candidates || !key || repeatLength <= 0)
    {
        return;
    }

    index = FindBackgroundCandidate(candidates, candidateTotal, key);

    if (index >= 0 && repeatLength > candidates[index].maxRepeat)
    {
        candidates[index].maxRepeat = repeatLength;
    }
}

static UINT_PTR MixProfileChildValue(UINT_PTR value)
{
    value += (UINT_PTR)0x9E3779B97F4A7C15ULL;
    value ^= value >> 30;
    value *= (UINT_PTR)0xBF58476D1CE4E5B9ULL;
    value ^= value >> 27;
    value *= (UINT_PTR)0x94D049BB133111EBULL;
    value ^= value >> 31;
    return value;
}

static TimelineProfile* ReserveTimelineProfile(const void* definition)
{
    TimelineProfile* profile;
    size_t index;

    if (g_timelineProfileUsed < TIMELINE_PROFILE_CACHE_CAPACITY)
    {
        profile = &g_timelineProfiles[g_timelineProfileUsed++];
        memset(profile, 0, sizeof(*profile));
        return profile;
    }

    index = ((UINT_PTR)definition >> 4) % TIMELINE_PROFILE_CACHE_CAPACITY;
    profile = &g_timelineProfiles[index];
    ReleaseTimelineProfile(profile);
    return profile;
}

#define TIMELINE_PROFILE_STAGE_NON_TEXTURE_ART 1
#define TIMELINE_PROFILE_STAGE_TEXTURE_COMMON_INIT 2
#define TIMELINE_PROFILE_STAGE_TEXTURE_COMMON 3
#define TIMELINE_PROFILE_STAGE_TEXTURE_SCAN 4
#define TIMELINE_PROFILE_STAGE_TEXTURE_VALIDITY 5

typedef struct TimelineProfileBuildState
{
    TimelineChildKey previousSoleKey;
    UINT_PTR previousSignature;
    UINT_PTR previousSignature2;
    int previousSpecificTotal;
    int candidateTotal;
    int hasPreviousSole;
    int repeatLength;
} TimelineProfileBuildState;

static TimelineBackgroundCandidate* GetTimelineBuildCandidates(TimelineProfileBuildState* state)
{
    return state ? (TimelineBackgroundCandidate*)(state + 1) : NULL;
}

static int InitializeTextureProfileArrays(TimelineProfile* profile)
{
    int frameExtent;

    if (!profile || profile->frameExtent <= 0)
    {
        return 0;
    }

    frameExtent = profile->frameExtent;
    profile->backgroundChildren = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), 0, (size_t)frameExtent * sizeof(*profile->backgroundChildren));
    profile->soleSpecificKeys = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->soleSpecificKeys));
    profile->backgroundFirstFrames = (unsigned char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->backgroundFirstFrames));
    profile->specificChildTotals = (unsigned short*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->specificChildTotals));
    profile->renderedChildTotals = (unsigned short*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->renderedChildTotals));
    profile->frameFingerprintFirst = (UINT_PTR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->frameFingerprintFirst));
    profile->frameFingerprintSecond = (UINT_PTR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->frameFingerprintSecond));

    return profile->backgroundChildren && profile->soleSpecificKeys && profile->backgroundFirstFrames && profile->specificChildTotals && profile->renderedChildTotals && profile->frameFingerprintFirst && profile->frameFingerprintSecond;
}

static int AdvanceTimelineProfile(TimelineProfile* profile, uint8_t* entry, void* movieClip, AppearanceField field)
{
    TimelineProfileBuildState* state;
    TimelineBackgroundCandidate* candidates;
    TimelineChildKey childKey;
    TimelineChildKey soleKey;
    void** children;
    int candidateIndex;
    int childIndex;
    int childTotal;
    int commonIndex;
    int currentFrame;
    int frame;
    int renderedTotal;
    int specificTotal;
    UINT_PTR childValue;
    UINT_PTR frameSignature;
    UINT_PTR frameSignature2;

    if (!profile || !movieClip || profile->buildComplete)
    {
        return profile && profile->buildComplete ? 1 : -1;
    }

    state = (TimelineProfileBuildState*)profile->buildState;

    if (!state || !IsReadableMemoryRange(movieClip, MOVIECLIP_CURRENT_FRAME_OFFSET + sizeof(int)))
    {
        return -1;
    }

    candidates = GetTimelineBuildCandidates(state);
    currentFrame = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);

    while (!profile->buildComplete)
    {
        if (profile->buildStage == TIMELINE_PROFILE_STAGE_NON_TEXTURE_ART)
        {
            while (profile->buildCursor < profile->frameExtent && ConsumeTimelineProfileProbeBudget())
            {
                frame = profile->buildCursor++;
                SetMovieClipFrameForInspection(movieClip, frame);

                if (!GetMovieClipChildren(movieClip, &children, &childTotal))
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    RefreshGraphicsEntryChildren(entry);
                    return -1;
                }

                frameSignature = (UINT_PTR)1469598103934665603ULL;
                frameSignature2 = 0;
                specificTotal = 0;

                for (childIndex = 0; childIndex < childTotal; ++childIndex)
                {
                    if (!IsTimelineChildRenderableFast(children[childIndex]) || !ReadTimelineChildKeyFast(children[childIndex], &childKey))
                    {
                        continue;
                    }

                    if (TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey) || TimelineChildIsNamedHelper(entry, children[childIndex]) || TimelineChildIsNamedHelperDirect(children[childIndex]))
                    {
                        RememberTimelineHelperKey(profile, &childKey);
                        continue;
                    }

                    childValue = ((UINT_PTR)childKey.characterID << 16) | (UINT_PTR)childKey.libraryID;
                    childValue ^= (UINT_PTR)(unsigned int)childIndex << 40;
                    childValue ^= (UINT_PTR)(unsigned char)*((const unsigned char*)children[childIndex] + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET) << 8;
                    childValue ^= (UINT_PTR)(unsigned int)*(const int*)((const uint8_t*)children[childIndex] + DISPLAY_OBJECT_CLIP_DEPTH_OFFSET);
                    childValue = MixProfileChildValue(childValue);
                    frameSignature ^= childValue;
                    frameSignature *= (UINT_PTR)1099511628211ULL;
                    frameSignature2 += childValue * (childValue | 1U);
                    ++specificTotal;
                }

                if (specificTotal > 0 && (frame == 0 || specificTotal != state->previousSpecificTotal || frameSignature != state->previousSignature || frameSignature2 != state->previousSignature2))
                {
                    profile->distinctArtFrames[frame] = 1;
                }

                state->previousSpecificTotal = specificTotal;
                state->previousSignature = frameSignature;
                state->previousSignature2 = frameSignature2;
            }

            SetMovieClipFrameForInspection(movieClip, currentFrame);
            RefreshGraphicsEntryChildren(entry);

            if (profile->buildCursor >= profile->frameExtent)
            {
                profile->buildComplete = 1;
                HeapFree(GetProcessHeap(), 0, profile->buildState);
                profile->buildState = NULL;
                g_timelineVisualNeedsRefresh = 1;
                return 1;
            }

            return 0;
        }

        if (profile->buildStage == TIMELINE_PROFILE_STAGE_TEXTURE_COMMON_INIT)
        {
            if (!ConsumeTimelineProfileProbeBudget())
            {
                return 0;
            }

            SetMovieClipFrameForInspection(movieClip, 0);

            if (!GetMovieClipChildren(movieClip, &children, &childTotal))
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                return -1;
            }

            profile->commonChildTotal = 0;

            if (childTotal > 0)
            {
                profile->commonChildren = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), 0, (size_t)childTotal * sizeof(*profile->commonChildren));
                
                if (!profile->commonChildren)
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    return -1;
                }

                for (childIndex = 0; childIndex < childTotal; ++childIndex)
                {
                    if (!ReadTimelineChildKeyFast(children[childIndex], &childKey) || TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
                    {
                        continue;
                    }

                    profile->commonChildren[profile->commonChildTotal++] = childKey;
                }
            }

            profile->buildCursor = 1;
            profile->buildStage = TIMELINE_PROFILE_STAGE_TEXTURE_COMMON;
        }

        if (profile->buildStage == TIMELINE_PROFILE_STAGE_TEXTURE_COMMON)
        {
            while (profile->buildCursor < profile->frameExtent && profile->commonChildTotal > 0 && ConsumeTimelineProfileProbeBudget())
            {
                frame = profile->buildCursor++;
                SetMovieClipFrameForInspection(movieClip, frame);

                if (!GetMovieClipChildren(movieClip, &children, &childTotal))
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    return -1;
                }

                for (commonIndex = profile->commonChildTotal - 1; commonIndex >= 0; --commonIndex)
                {
                    if (!TimelineChildrenContainKey(children, childTotal, &profile->commonChildren[commonIndex]))
                    {
                        --profile->commonChildTotal;
                        profile->commonChildren[commonIndex] = profile->commonChildren[profile->commonChildTotal];
                    }
                }
            }

            if (profile->commonChildTotal > 0 && profile->buildCursor < profile->frameExtent)
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                return 0;
            }

            if (!InitializeTextureProfileArrays(profile))
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                return -1;
            }

            profile->buildCursor = 0;
            profile->buildStage = TIMELINE_PROFILE_STAGE_TEXTURE_SCAN;
            state->candidateTotal = 0;
            state->hasPreviousSole = 0;
            state->repeatLength = 0;
            memset(&state->previousSoleKey, 0, sizeof(state->previousSoleKey));
        }

        if (profile->buildStage == TIMELINE_PROFILE_STAGE_TEXTURE_SCAN)
        {
            while (profile->buildCursor < profile->frameExtent && ConsumeTimelineProfileProbeBudget())
            {
                frame = profile->buildCursor++;
                SetMovieClipFrameForInspection(movieClip, frame);

                if (!GetMovieClipChildren(movieClip, &children, &childTotal))
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    return -1;
                }

                frameSignature = (UINT_PTR)1469598103934665603ULL;
                frameSignature2 = 0;
                renderedTotal = 0;
                specificTotal = 0;
                memset(&soleKey, 0, sizeof(soleKey));

                for (childIndex = 0; childIndex < childTotal; ++childIndex)
                {
                    if (!IsTimelineChildRenderableFast(children[childIndex]) || !ReadTimelineChildKeyFast(children[childIndex], &childKey))
                    {
                        continue;
                    }

                    ++renderedTotal;

                    if (TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
                    {
                        continue;
                    }

                    /* 
                    * Texture validity is about the art contributed by this
                    * timeline, not persistent/common display-list children...
                    * Keep fingerprint order-independent so same
                    * shared filler symbols can be recognized across body,
                    * head, leg, tail, and ear texture definitions...
                    */
                    childValue = ((UINT_PTR)childKey.characterID << 16) | (UINT_PTR)childKey.libraryID;
                    childValue = MixProfileChildValue(childValue);
                    frameSignature += childValue;
                    frameSignature2 += childValue * (childValue | 1U);
                    soleKey = childKey;
                    ++specificTotal;
                }

                profile->frameFingerprintFirst[frame] = frameSignature;
                profile->frameFingerprintSecond[frame] = frameSignature2;
                profile->renderedChildTotals[frame] = (unsigned short)(renderedTotal > 0xFFFF ? 0xFFFF : renderedTotal);
                profile->specificChildTotals[frame] = (unsigned short)(specificTotal > 0xFFFF ? 0xFFFF : specificTotal);

                if (specificTotal == 1)
                {
                    profile->soleSpecificKeys[frame] = soleKey;
                    candidateIndex = EnsureBackgroundCandidate(candidates, &state->candidateTotal, profile->frameExtent, &soleKey, frame);
                    (void)candidateIndex;

                    if (state->hasPreviousSole && TimelineChildKeyEquals(&state->previousSoleKey, &soleKey))
                    {
                        ++state->repeatLength;
                    }
                    else
                    {
                        if (state->hasPreviousSole)
                        {
                            FinishBackgroundRun(candidates, state->candidateTotal, &state->previousSoleKey, state->repeatLength);
                        }

                        state->previousSoleKey = soleKey;
                        state->hasPreviousSole = 1;
                        state->repeatLength = 1;
                    }
                }
                else
                {
                    if (state->hasPreviousSole)
                    {
                        FinishBackgroundRun(candidates, state->candidateTotal, &state->previousSoleKey, state->repeatLength);
                    }

                    state->hasPreviousSole = 0;
                    state->repeatLength = 0;
                }
            }

            if (profile->buildCursor < profile->frameExtent)
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                return 0;
            }

            if (state->hasPreviousSole)
            {
                FinishBackgroundRun(candidates, state->candidateTotal, &state->previousSoleKey, state->repeatLength);
            }

            for (candidateIndex = 0; candidateIndex < state->candidateTotal; ++candidateIndex)
            {
                if (candidates[candidateIndex].maxRepeat >= TIMELINE_BACKGROUND_REPEAT_LENGTH)
                {
                    profile->backgroundChildren[profile->backgroundChildTotal++] = candidates[candidateIndex].key;
                    
                    if (candidates[candidateIndex].firstFrame >= 0 && candidates[candidateIndex].firstFrame < profile->frameExtent)
                    {
                        profile->backgroundFirstFrames[candidates[candidateIndex].firstFrame] = 1;
                    }
                }
            }

            profile->buildCursor = 0;
            profile->buildStage = TIMELINE_PROFILE_STAGE_TEXTURE_VALIDITY;
        }

        if (profile->buildStage == TIMELINE_PROFILE_STAGE_TEXTURE_VALIDITY)
        {
            while (profile->buildCursor < profile->frameExtent && ConsumeTimelineProfileProbeBudget())
            {
                frame = profile->buildCursor++;
                SetMovieClipFrameForInspection(movieClip, frame);

                if (!GetMovieClipChildren(movieClip, &children, &childTotal))
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    return -1;
                }

                for (childIndex = 0; childIndex < childTotal; ++childIndex)
                {
                    if (!IsTimelineChildRenderableFast(children[childIndex]) || !ReadTimelineChildKeyFast(children[childIndex], &childKey))
                    {
                        continue;
                    }

                    if (TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
                    {
                        continue;
                    }

                    if (TimelineChildKeyArrayContains(profile->backgroundChildren, profile->backgroundChildTotal, &childKey) && !profile->backgroundFirstFrames[frame])
                    {
                        continue;
                    }

                    profile->distinctArtFrames[frame] = 1;
                    break;
                }
            }

            SetMovieClipFrameForInspection(movieClip, currentFrame);

            if (profile->buildCursor < profile->frameExtent)
            {
                return 0;
            }

            profile->buildComplete = 1;
            HeapFree(GetProcessHeap(), 0, profile->buildState);
            profile->buildState = NULL;
            return 1;
        }

        SetMovieClipFrameForInspection(movieClip, currentFrame);
        return -1;
    }

    SetMovieClipFrameForInspection(movieClip, currentFrame);
    return 1;
}

static TimelineProfile* GetTimelineProfile(uint8_t* entry, void* movieClip, AppearanceField field)
{
    TimelineProfileBuildState* state;
    TimelineProfile* profile;
    void* definition;
    size_t buildStateSize;
    size_t index;
    int advanceResult;
    int frameExtent;
    int learnBackgroundChildren;

    learnBackgroundChildren = field == APPEARANCE_FIELD_TEXTURE;
    frameExtent = GetMovieClipFrameExtent(movieClip);

    if (frameExtent <= 0 || !IsReadableMemoryRange(movieClip, MOVIECLIP_CURRENT_FRAME_OFFSET + sizeof(int)))
    {
        return NULL;
    }

    definition = *(void**)((uint8_t*)movieClip + MOVIECLIP_DEFINITION_OFFSET);

    for (index = 0; index < g_timelineProfileUsed; ++index)
    {
        profile = &g_timelineProfiles[index];

        if (profile->definition == definition && profile->frameExtent == frameExtent && profile->learnsBackgroundChildren == learnBackgroundChildren)
        {
            if (!profile->buildComplete)
            {
                advanceResult = AdvanceTimelineProfile(profile, entry, movieClip, field);

                if (advanceResult < 0)
                {
                    ReleaseTimelineProfile(profile);
                    return NULL;
                }
            }

            return profile;
        }
    }

    profile = ReserveTimelineProfile(definition);
    profile->definition = definition;
    profile->frameExtent = frameExtent;
    profile->learnsBackgroundChildren = learnBackgroundChildren;
    profile->distinctArtFrames = (unsigned char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->distinctArtFrames));

    if (!profile->distinctArtFrames)
    {
        ReleaseTimelineProfile(profile);
        return NULL;
    }

    buildStateSize = sizeof(TimelineProfileBuildState);

    if (learnBackgroundChildren)
    {
        if ((size_t)frameExtent > ((size_t)-1 - buildStateSize) / sizeof(TimelineBackgroundCandidate))
        {
            ReleaseTimelineProfile(profile);
            return NULL;
        }

        buildStateSize += (size_t)frameExtent * sizeof(TimelineBackgroundCandidate);
    }

    state = (TimelineProfileBuildState*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buildStateSize);

    if (!state)
    {
        ReleaseTimelineProfile(profile);
        return NULL;
    }

    profile->buildState = state;
    profile->buildCursor = 0;

    if (!learnBackgroundChildren)
    {
        InitializeTimelineHelperKeys(profile, movieClip);
    }

    profile->buildStage = learnBackgroundChildren ? TIMELINE_PROFILE_STAGE_TEXTURE_COMMON_INIT : TIMELINE_PROFILE_STAGE_NON_TEXTURE_ART;
    profile->buildComplete = 0;

    advanceResult = AdvanceTimelineProfile(profile, entry, movieClip, field);

    if (advanceResult < 0)
    {
        ReleaseTimelineProfile(profile);
        return NULL;
    }

    return profile;
}

static int TimelineChildIsNamedHelper(const uint8_t* entry, const void* child)
{
    return entry && child && (child == *(void* const*)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET) || child == *(void* const*)(entry + CATPART_GRAPHICS_SCARS_CLIP_OFFSET) || child == *(void* const*)(entry + CATPART_GRAPHICS_AUX_CLIP_OFFSET));
}

static int FieldHasRenderedTextureFamily(AppearanceField field, size_t* outContainerOffset)
{
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerTotal;

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);

    if (containerTotal != 1 || !McpfTextureKindForContainer(containerOffsets[0]))
    {
        return 0;
    }

    if (outContainerOffset)
    {
        *outContainerOffset = containerOffsets[0];
    }

    return 1;
}

/*
* The ordinary texture map only sees nested tex MovieClips belonging to
* the currently equipped body parts. MCPF can attach a different private
* texture definition to a newly added part frame, so merely indexing the
* randomized starting cat leaves modded definitions undiscovered...
*
* During the initialization screen, walk every valid outer part frame for the
* rendered texture families (body/head/tail/leg/ear), resolve that frame's
* live tex child, let MCPF synchronize it, and fully build immutable texture profile...
*/
int AdvanceAllPartTextureIndexes(void* catVisual, int* outPercent)
{
    TimelineIDMap* partMap;
    TimelineProfile* textureProfile;
    const char* textureKind;
    AppearanceField field;
    size_t containerOffset;
    uint8_t* entries;
    uint8_t* entry;
    void* partMovieClip;
    void* textureMovieClip;
    int candidateBudget;
    int currentFrame;
    int entryTotal;
    int maximum;
    int partID;

    if (outPercent)
    {
        *outPercent = 100;
    }

    if (!catVisual)
    {
        return 1;
    }

    if (!g_initialPartTextureIndex.targetsReady)
    {
        g_initialPartTextureIndex.targetTotal = 0;

        for (field = APPEARANCE_FIELD_BODY; field < APPEARANCE_FIELD_COUNT; ++field)
        {
            if (!FieldHasRenderedTextureFamily(field, NULL))
            {
                continue;
            }

            maximum = GetFieldTimelineExtent(catVisual, field);

            if (maximum < 1)
            {
                continue;
            }

            partMap = GetTimelineIDMap(catVisual, field, 1, maximum);

            if (partMap && partMap->building)
            {
                if (outPercent)
                {
                    *outPercent = 0;
                }
                
                return 0;
            }

            if (partMap && partMap->validTotal > 0)
            {
                g_initialPartTextureIndex.targetTotal += partMap->validTotal;
            }
        }

        g_initialPartTextureIndex.targetsReady = 1;
        g_initialPartTextureIndex.field = APPEARANCE_FIELD_BODY;
        g_initialPartTextureIndex.validIndex = 0;
        g_initialPartTextureIndex.completedTotal = 0;
    }

    if (g_initialPartTextureIndex.targetTotal <= 0)
    {
        return 1;
    }

    candidateBudget = TIMELINE_INITIAL_TEXTURE_DISCOVERY_BUDGET_PER_FRAME;

    while (g_initialPartTextureIndex.field < APPEARANCE_FIELD_COUNT && candidateBudget > 0)
    {
        field = g_initialPartTextureIndex.field;

        if (!FieldHasRenderedTextureFamily(field, &containerOffset))
        {
            ++g_initialPartTextureIndex.field;
            g_initialPartTextureIndex.validIndex = 0;
            continue;
        }

        maximum = GetFieldTimelineExtent(catVisual, field);
        partMap = maximum >= 1 ? GetTimelineIDMap(catVisual, field, 1, maximum) : NULL;

        if (!partMap || partMap->building || partMap->validTotal <= 0)
        {
            ++g_initialPartTextureIndex.field;
            g_initialPartTextureIndex.validIndex = 0;
            continue;
        }

        if (g_initialPartTextureIndex.validIndex >= partMap->validTotal)
        {
            ++g_initialPartTextureIndex.field;
            g_initialPartTextureIndex.validIndex = 0;
            continue;
        }

        if (!GetGraphicsEntries(catVisual, containerOffset, &entries, &entryTotal) || entryTotal <= 0)
        {
            ++g_initialPartTextureIndex.validIndex;
            ++g_initialPartTextureIndex.completedTotal;
            --candidateBudget;
            continue;
        }

        entry = entries;
        partMovieClip = *(void**)(entry + CATPART_GRAPHICS_MOVIECLIP_OFFSET);

        if (!partMovieClip || !IsReadableMemoryRange(partMovieClip, MOVIECLIP_CURRENT_FRAME_OFFSET + sizeof(int)))
        {
            ++g_initialPartTextureIndex.validIndex;
            ++g_initialPartTextureIndex.completedTotal;
            --candidateBudget;
            continue;
        }

        partID = partMap->validIDs[g_initialPartTextureIndex.validIndex];
        currentFrame = *(const int*)((uint8_t*)partMovieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);
        SetMovieClipFrameForInspection(partMovieClip, partID - 1);

        textureMovieClip = GetMovieClipNamedChild(partMovieClip, "tex");
        textureProfile = NULL;

        if (textureMovieClip)
        {
            textureKind = McpfTextureKindForContainer(containerOffset);

            if (textureKind)
            {
                // MCPF appends/synchronizes modded texture frames on demand...
                SyncMcpfTextureClip(textureKind, textureMovieClip);
            }

            textureProfile = GetTimelineProfile(NULL, textureMovieClip, APPEARANCE_FIELD_TEXTURE);
        }

        SetMovieClipFrameForInspection(partMovieClip, currentFrame);
        RefreshGraphicsEntryChildren(entry);

        // Profile probing is time-sliced. Stay on this part frame until its nested texture definition is completely indexed...
        if (textureProfile && !textureProfile->buildComplete)
        {
            if (outPercent)
            {
                *outPercent = (g_initialPartTextureIndex.completedTotal * 100) / g_initialPartTextureIndex.targetTotal;
            }

            return 0;
        }

        ++g_initialPartTextureIndex.validIndex;
        ++g_initialPartTextureIndex.completedTotal;
        --candidateBudget;
    }

    if (outPercent)
    {
        *outPercent = (g_initialPartTextureIndex.completedTotal * 100) / g_initialPartTextureIndex.targetTotal;

        if (*outPercent > 100)
        {
            *outPercent = 100;
        }
    }

    return g_initialPartTextureIndex.field >= APPEARANCE_FIELD_COUNT;
}

static int TimelineFrameHasSpecificContent(uint8_t* entry, void* movieClip, AppearanceField field, int oneBasedFrame, int* outHasRenderableContent)
{
    TimelineProfile* profile;
    int frameIndex;
    int specific;

    if (outHasRenderableContent)
    {
        *outHasRenderableContent = 0;
    }

    profile = GetTimelineProfile(entry, movieClip, field);

    if (field != APPEARANCE_FIELD_TEXTURE)
    {
        /* 
        * Profile construction seeks the outer part through its authored
        * frames. The live visual needs one real CatVisual refresh after the
        * profiling pass, but candidate-ID checks themselves are pure array lookups...
        */
        g_timelineVisualNeedsRefresh = 1;
    }

    if (!profile || !profile->buildComplete || oneBasedFrame < 1 || oneBasedFrame > profile->frameExtent || !profile->distinctArtFrames)
    {
        return 0;
    }

    frameIndex = oneBasedFrame - 1;
    specific = profile->distinctArtFrames[frameIndex] != 0;

    if (outHasRenderableContent)
    {
        if (field == APPEARANCE_FIELD_TEXTURE && profile->specificChildTotals)
        {
            *outHasRenderableContent = profile->specificChildTotals[frameIndex] != 0;
        }
        else
        {
            *outHasRenderableContent = specific;
        }
    }

    return specific;
}

static int IsFieldTimelineFrameValid(void* catVisual, AppearanceField field, int oneBasedFrame)
{
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerIndex;
    size_t containerTotal;
    uint8_t* entries;
    uint8_t* entry;
    void* movieClip;
    int entryTotal;
    int eligibleTimelineTotal;
    int frameExtent;
    int textureArtTimelineTotal;
    int valid;

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);
    eligibleTimelineTotal = 0;
    textureArtTimelineTotal = 0;

    for (containerIndex = 0; containerIndex < containerTotal; ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        entry = entries;
        movieClip = GetTimelineEntryClip(entry, field);
        frameExtent = GetMovieClipFrameExtent(movieClip);

        if (frameExtent <= 0)
        {
            continue;
        }

        /* 
        * Eye/brow/mouth containers are part of the generic texture sync
        * list, but MCPF only cares about texture art for the applicable five rendered cat
        * texture families (body, head, tail, leg, and ear)...
        */
        if (field == APPEARANCE_FIELD_TEXTURE && TextureKindMaskForContainer(containerOffsets[containerIndex]) == 0)
        {
            continue;
        }

        ++eligibleTimelineTotal;

        if (oneBasedFrame < 1 || oneBasedFrame > frameExtent)
        {
            if (field == APPEARANCE_FIELD_TEXTURE)
            {
                return 0;
            }

            continue;
        }

        valid = TimelineFrameHasSpecificContent(entry, movieClip, field, oneBasedFrame, NULL);

        if (field == APPEARANCE_FIELD_TEXTURE)
        {
            /* 
            * Neutral/background contribution is
            * not a failure! Several legitimate vanilla textures intentionally
            * leave one or more families on their learned neutral state. Count only genuinely distinct art here and
            * let the "all-family fingerprint logic" decide repeated/filler IDs...
            */
            if (valid)
            {
                ++textureArtTimelineTotal;
            }

            continue;
        }

        if (valid)
        {
            return 1;
        }
    }

    return field == APPEARANCE_FIELD_TEXTURE && eligibleTimelineTotal > 0 && textureArtTimelineTotal > 0;
}

static UINT_PTR GetTimelineIDMapSignature(void* catVisual, AppearanceField field)
{
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerIndex;
    size_t containerTotal;
    uint8_t* entries;
    uint8_t* entry;
    void* definition;
    void* movieClip;
    UINT_PTR signature;
    int entryTotal;
    int foundTimeline;
    int frameExtent;

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);
    signature = (UINT_PTR)1469598103934665603ULL;
    signature ^= (UINT_PTR)field + 1;
    signature *= (UINT_PTR)1099511628211ULL;
    foundTimeline = 0;

    for (containerIndex = 0; containerIndex < containerTotal; ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        entry = entries;
        movieClip = GetTimelineEntryClip(entry, field);
        frameExtent = GetMovieClipFrameExtent(movieClip);

        if (frameExtent <= 0)
        {
            continue;
        }

        definition = *(void**)((uint8_t*)movieClip + MOVIECLIP_DEFINITION_OFFSET);
        signature ^= (UINT_PTR)containerOffsets[containerIndex];
        signature *= (UINT_PTR)1099511628211ULL;
        signature ^= (UINT_PTR)definition;
        signature *= (UINT_PTR)1099511628211ULL;
        signature ^= (UINT_PTR)frameExtent;
        signature *= (UINT_PTR)1099511628211ULL;
        foundTimeline = 1;
    }

    if (!foundTimeline)
    {
        return 0;
    }

    return signature ? signature : 1;
}

static UINT_PTR MixTimelineFingerprintValue(UINT_PTR value)
{
    value += (UINT_PTR)0x9E3779B97F4A7C15ULL;
    value ^= value >> 30;
    value *= (UINT_PTR)0xBF58476D1CE4E5B9ULL;
    value ^= value >> 27;
    value *= (UINT_PTR)0x94D049BB133111EBULL;
    value ^= value >> 31;
    return value;
}

typedef struct TimelineProfileRef
{
    TimelineProfile* profile;
    size_t containerOffset;
} TimelineProfileRef;

static int CollectTimelineProfileRefs(void* catVisual, AppearanceField field, TimelineProfileRef refs[TIMELINE_CONTAINER_CAPACITY], int* outRefTotal, int* outPending)
{
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerIndex;
    size_t containerTotal;
    uint8_t* entries;
    uint8_t* entry;
    void* movieClip;
    TimelineProfile* profile;
    int entryTotal;
    int refTotal;

    if (outRefTotal)
    {
        *outRefTotal = 0;
    }
    if (outPending)
    {
        *outPending = 0;
    }

    if (!catVisual || !refs || !outRefTotal || !outPending)
    {
        return 0;
    }

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);
    refTotal = 0;

    for (containerIndex = 0; containerIndex < containerTotal && refTotal < TIMELINE_CONTAINER_CAPACITY; ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        entry = entries;
        movieClip = GetTimelineEntryClip(entry, field);

        if (!movieClip)
        {
            continue;
        }

        profile = GetTimelineProfile(entry, movieClip, field);

        if (!profile || profile->frameExtent <= 0 || !profile->distinctArtFrames)
        {
            continue;
        }

        if (!profile->buildComplete)
        {
            *outPending = 1;
            continue;
        }

        refs[refTotal].profile = profile;
        refs[refTotal].containerOffset = containerOffsets[containerIndex];

        ++refTotal;
    }

    *outRefTotal = refTotal;
    return refTotal > 0;
}

static int GetTextureFingerprintFromProfiles(const TimelineProfileRef* refs, int refTotal, int oneBasedFrame, TimelineFrameFingerprint* outFingerprint)
{
    const TimelineProfile* profile;
    UINT_PTR first;
    UINT_PTR second;
    UINT_PTR value;
    int frameIndex;
    int includedTimelineTotal;
    int refIndex;
    int specificChildTotal;

    if (!refs || refTotal <= 0 || oneBasedFrame < 1 || !outFingerprint)
    {
        return 0;
    }

    first = (UINT_PTR)1469598103934665603ULL;
    second = 0;
    includedTimelineTotal = 0;
    specificChildTotal = 0;
    frameIndex = oneBasedFrame - 1;

    for (refIndex = 0; refIndex < refTotal; ++refIndex)
    {
        if (TextureKindMaskForContainer(refs[refIndex].containerOffset) == 0)
        {
            continue;
        }

        profile = refs[refIndex].profile;

        /* 
        * Repeated-art comparison follows the same all-body-parts contract as
        * texture validity. A candidate is not fingerprintable if even one
        * active rendered texture clip does not reach it...
        */
        if (!profile || oneBasedFrame > profile->frameExtent || !profile->frameFingerprintFirst || !profile->frameFingerprintSecond || !profile->specificChildTotals)
        {
            return 0;
        }

        value = MixTimelineFingerprintValue((UINT_PTR)refs[refIndex].containerOffset ^ ((UINT_PTR)refIndex << 32));
        first ^= value;
        first *= (UINT_PTR)1099511628211ULL;
        second += value * (value | 1U);

        value = MixTimelineFingerprintValue(profile->frameFingerprintFirst[frameIndex] ^ ((UINT_PTR)refIndex << 48));
        first ^= value;
        first *= (UINT_PTR)1099511628211ULL;
        second += value * (value | 1U);

        value = MixTimelineFingerprintValue(profile->frameFingerprintSecond[frameIndex] ^ ((UINT_PTR)refs[refIndex].containerOffset << 16));
        first ^= value;
        first *= (UINT_PTR)1099511628211ULL;
        second += value * (value | 1U);
        specificChildTotal += profile->specificChildTotals[frameIndex];
        ++includedTimelineTotal;
    }

    if (includedTimelineTotal <= 0)
    {
        return 0;
    }

    outFingerprint->first = first;
    outFingerprint->second = second;
    outFingerprint->childTotal = specificChildTotal;
    outFingerprint->timelineTotal = includedTimelineTotal;
    return 1;
}

static int GetTextureProfileFrameFingerprint(const TimelineProfile* profile, int frameIndex, TimelineFrameFingerprint* outFingerprint)
{
    if (!profile || !outFingerprint || frameIndex < 0 || frameIndex >= profile->frameExtent || !profile->frameFingerprintFirst || !profile->frameFingerprintSecond || !profile->specificChildTotals)
    {
        return 0;
    }

    outFingerprint->first = profile->frameFingerprintFirst[frameIndex];
    outFingerprint->second = profile->frameFingerprintSecond[frameIndex];
    outFingerprint->childTotal = profile->specificChildTotals[frameIndex];
    outFingerprint->timelineTotal = 1;
    return 1;
}

static int TimelineFrameContentFingerprintEquals(const TimelineFrameFingerprint* first, const TimelineFrameFingerprint* second)
{
    return first && second && first->first == second->first && first->second == second->second && first->childTotal == second->childTotal;
}

static int CollectSharedTextureBackgrounds(const TimelineProfileRef* refs, int refTotal, TimelineFrameFingerprint backgrounds[TIMELINE_CONTAINER_CAPACITY], int* outFirstOneBasedFrame)
{
    TimelineFrameFingerprint candidate;
    TimelineFrameFingerprint current;
    unsigned int matchingKindMask;
    int candidateFrame;
    int commonExtent;
    int foundRequiredRef;
    int refIndex;
    int refMatches;
    const TimelineProfile* profile;

    if (outFirstOneBasedFrame)
    {
        *outFirstOneBasedFrame = 0;
    }

    if (!refs || refTotal <= 0 || !backgrounds)
    {
        return 0;
    }

    /* 
    * Search the frame range shared by every rendered texture family. The
    * vanilla invalid fallback is the same tiny two-child display-list state
    * in body/head/tail/leg/ear. Requiring that exact cross-family match is a
    * stronger signal than requiring it to remain the final frame, so a
    * mod may safely append real textures immediately after the fallback...
    */
    commonExtent = 0;
    foundRequiredRef = 0;

    for (refIndex = 0; refIndex < refTotal; ++refIndex)
    {
        if (TextureKindMaskForContainer(refs[refIndex].containerOffset) == 0)
        {
            continue;
        }

        profile = refs[refIndex].profile;

        if (!profile || !profile->buildComplete || profile->frameExtent <= 0)
        {
            return 0;
        }

        if (!foundRequiredRef || profile->frameExtent < commonExtent)
        {
            commonExtent = profile->frameExtent;
        }

        foundRequiredRef = 1;
    }

    if (!foundRequiredRef || commonExtent <= 0)
    {
        return 0;
    }

    for (candidateFrame = commonExtent - 1; candidateFrame >= 0; --candidateFrame)
    {
        int haveCandidate;

        haveCandidate = 0;
        matchingKindMask = 0;
        refMatches = 1;
        memset(&candidate, 0, sizeof(candidate));

        for (refIndex = 0; refIndex < refTotal; ++refIndex)
        {
            unsigned int kindMask;

            kindMask = TextureKindMaskForContainer(refs[refIndex].containerOffset);

            if (kindMask == 0)
            {
                continue;
            }

            profile = refs[refIndex].profile;
            
            if (!GetTextureProfileFrameFingerprint(profile, candidateFrame, &current) || current.childTotal != TIMELINE_SHARED_TEXTURE_BACKGROUND_CHILDREN)
            {
                refMatches = 0;
                break;
            }

            if (!haveCandidate)
            {
                candidate = current;
                haveCandidate = 1;
            }
            else if (!TimelineFrameContentFingerprintEquals(&candidate, &current))
            {
                refMatches = 0;
                break;
            }

            matchingKindMask |= kindMask;
        }

        if (refMatches && haveCandidate && (matchingKindMask & TIMELINE_TEXTURE_KIND_ALL_MASK) == TIMELINE_TEXTURE_KIND_ALL_MASK)
        {
            backgrounds[0] = candidate;

            if (outFirstOneBasedFrame)
            {
                *outFirstOneBasedFrame = candidateFrame + 1;
            }

            return 1;
        }
    }

    return 0;
}

static int FindTrailingTextureGarbageStart(const TimelineProfileRef* refs, int refTotal, int sharedBackgroundOneBasedFrame)
{
    TimelineFrameFingerprint current;
    TimelineFrameFingerprint previous;
    int earliestGarbage;
    int runEnd;
    int runLength;
    int runStart;

    if (!refs || refTotal <= 0 || sharedBackgroundOneBasedFrame <= 1)
    {
        return sharedBackgroundOneBasedFrame;
    }

    /* 
    * The malformed vanilla tail is staged in held multi-child runs directly
    * before the shared fallback (body begins first, then tail/leg/ear join).
    * Treat these contiguous bulk runs as one garbage region. Stop as soon as
    * the preceding run is simple/neutral so long-lived legitimate overlays
    * elsewhere in the texture table are not swallowed...
    */
    earliestGarbage = sharedBackgroundOneBasedFrame;
    runEnd = sharedBackgroundOneBasedFrame - 1;

    while (runEnd >= 1 && GetTextureFingerprintFromProfiles(refs, refTotal, runEnd, &current))
    {
        runStart = runEnd;

        while (runStart > 1 && GetTextureFingerprintFromProfiles(refs, refTotal, runStart - 1, &previous) && TimelineFrameFingerprintEquals(&current, &previous))
        {
            --runStart;
        }

        runLength = runEnd - runStart + 1;

        if (runLength < 2 || current.childTotal < TIMELINE_TRAILING_TEXTURE_GARBAGE_MIN_CHILDREN)
        {
            break;
        }

        earliestGarbage = runStart;
        runEnd = runStart - 1;
    }

    return earliestGarbage;
}

static int TimelineFrameFingerprintEquals(const TimelineFrameFingerprint* first, const TimelineFrameFingerprint* second)
{
    return first && second && first->first == second->first && first->second == second->second && first->childTotal == second->childTotal && first->timelineTotal == second->timelineTotal;
}

TimelineIDMap* GetTimelineIDMap(void* catVisual, AppearanceField field, int minimum, int maximum)
{
    TimelineFrameFingerprint currentFingerprint;
    TimelineFrameFingerprint previousFingerprint;
    TimelineFrameFingerprint sharedTrailingBackgrounds[TIMELINE_CONTAINER_CAPACITY];
    TimelineProfileRef refs[TIMELINE_CONTAINER_CAPACITY];
    TimelineProfile* profile;
    TimelineIDMap* map;
    UINT_PTR signature;
    size_t candidateTotal;
    int candidate;
    int frameIndex;
    int hasPreviousTextureFingerprint;
    int pendingProfiles;
    int refIndex;
    int refTotal;
    int sameMap;
    int sharedTrailingBackgroundFrame;
    int sharedTrailingBackgroundTotal;
    int trailingTextureGarbageStart;
    int timelineValid;

    if (!catVisual || field < 0 || field >= APPEARANCE_FIELD_COUNT || maximum < minimum)
    {
        return NULL;
    }

    signature = GetTimelineIDMapSignature(catVisual, field);

    if (!signature)
    {
        return NULL;
    }

    map = &g_timelineIDMaps[field];
    sameMap = map->catVisual == catVisual && map->signature == signature && map->minimum == minimum && map->maximum == maximum;

    if (sameMap && !map->building)
    {
        return map;
    }

    if (!sameMap)
    {
        ReleaseTimelineIdMap(map);
        map->catVisual = catVisual;
        map->signature = signature;
        map->minimum = minimum;
        map->maximum = maximum;
        map->building = 1;

        candidateTotal = (size_t)maximum - (size_t)minimum + 1;

        if (candidateTotal > (size_t)-1 / sizeof(*map->validIDs))
        {
            map->building = 0;
            return map;
        }

        map->validIDs = (int*)HeapAlloc(GetProcessHeap(), 0, candidateTotal * sizeof(*map->validIDs));

        if (!map->validIDs)
        {
            map->building = 0;
            return map;
        }
    }

    /*
    * Profile construction is intentionally time-sliced. Each editor frame
    * receives a fixed number of MovieClip probes shared by the centralized
    * pre-render indexing pass. Until the relevant immutable definitions have finished 
    * indexing, return a map marked building instead of blocking the render thread...
    */
    pendingProfiles = 0;
    refTotal = 0;
    CollectTimelineProfileRefs(catVisual, field, refs, &refTotal, &pendingProfiles);

    if (pendingProfiles)
    {
        map->building = 1;
        return map;
    }

    map->validTotal = 0;
    map->building = 0;

    if (refTotal <= 0)
    {
        return map;
    }

    /* 
    * Once the profiles are complete, candidate-map construction is entirely
    * memory-only! There are no MovieClip seeks, native child-name lookups, or
    * VirtualQuery calls...
    */
    hasPreviousTextureFingerprint = 0;
    memset(&previousFingerprint, 0, sizeof(previousFingerprint));
    sharedTrailingBackgroundFrame = 0;
    sharedTrailingBackgroundTotal = field == APPEARANCE_FIELD_TEXTURE ? CollectSharedTextureBackgrounds(refs, refTotal, sharedTrailingBackgrounds, &sharedTrailingBackgroundFrame) : 0;
    trailingTextureGarbageStart = (field == APPEARANCE_FIELD_TEXTURE && sharedTrailingBackgroundTotal > 0) ? FindTrailingTextureGarbageStart(refs, refTotal, sharedTrailingBackgroundFrame) : 0;

    for (candidate = minimum;; ++candidate)
    {
        frameIndex = candidate - 1;
        timelineValid = 0;

        if (field == APPEARANCE_FIELD_TEXTURE)
        {
            int requiredTextureTimelineTotal;
            int textureArtTimelineTotal;

            requiredTextureTimelineTotal = 0;
            textureArtTimelineTotal = 0;
            timelineValid = 1;

            for (refIndex = 0; refIndex < refTotal; ++refIndex)
            {
                if (TextureKindMaskForContainer(refs[refIndex].containerOffset) == 0)
                {
                    continue;
                }

                ++requiredTextureTimelineTotal;
                profile = refs[refIndex].profile;

                /* 
                * Every rendered body-part texture timeline must contribute an
                * authored texture state at this ID. That state may be a
                * reused/neutral marker, so do not require a distinct
                * frame change here. What is invalid is a genuinely empty frame!
                */
                if (!profile || candidate < 1 || candidate > profile->frameExtent || !profile->distinctArtFrames || !profile->specificChildTotals || profile->specificChildTotals[frameIndex] == 0)
                {
                    timelineValid = 0;
                    break;
                }

                ++textureArtTimelineTotal;
            }

            if (requiredTextureTimelineTotal <= 0 || textureArtTimelineTotal <= 0)
            {
                timelineValid = 0;
            }

            /* 
            * Reject the learned malformed trailing cluster, but do not apply per-family repeat
            * vetoes. A valid texture may deliberately reuse one family's art while other families change...
            */
            if (timelineValid && trailingTextureGarbageStart > 0 && candidate >= trailingTextureGarbageStart && candidate <= sharedTrailingBackgroundFrame)
            {
                timelineValid = 0;
            }
        }
        else
        {
            for (refIndex = 0; refIndex < refTotal; ++refIndex)
            {
                profile = refs[refIndex].profile;

                if (!profile || candidate < 1 || candidate > profile->frameExtent || !profile->distinctArtFrames)
                {
                    continue;
                }

                if (profile->distinctArtFrames[frameIndex])
                {
                    timelineValid = 1;
                    break;
                }
            }
        }

        if (timelineValid && field == APPEARANCE_FIELD_TEXTURE)
        {
            if (GetTextureFingerprintFromProfiles(refs, refTotal, candidate, &currentFingerprint))
            {
                if (hasPreviousTextureFingerprint && TimelineFrameFingerprintEquals(&currentFingerprint, &previousFingerprint))
                {
                    timelineValid = 0;
                }

                previousFingerprint = currentFingerprint;
                hasPreviousTextureFingerprint = 1;
            }
            else
            {
                hasPreviousTextureFingerprint = 0;
            }
        }
        else if (field == APPEARANCE_FIELD_TEXTURE)
        {
            hasPreviousTextureFingerprint = 0;
        }

        if (timelineValid)
        {
            map->validIDs[map->validTotal++] = candidate;
        }

        if (candidate == maximum)
        {
            break;
        }
    }

    return map;
}

int GetRuntimePaletteHeight(void)
{
    int height;

    if (InterlockedCompareExchange(&g_paletteInfoReady, 1, 1) != 1)
    {
        return BASE_PALETTE_TEXTURE_ROWS;
    }

    height = g_paletteHeight;

    if (height <= 0 || height > APPEARANCE_MAX_ID + 1)
    {
        return BASE_PALETTE_TEXTURE_ROWS;
    }

    return height;
}

int RuntimePaletteInfoIsReady(void)
{
    return InterlockedCompareExchange(&g_paletteInfoReady, 1, 1) == 1;
}

static int RuntimePaletteRowIsBlank(int row)
{
    /*
    * (Palette slot zero is a real game palette)... 
    * Keep it selectable even when its source row happens to look like one of the blank-row sentinels...
    */
    if (row == 0)
    {
        return 0;
    }

    if (!RuntimePaletteInfoIsReady() || row < 0 || row >= g_paletteHeight)
    {
        return 0;
    }

    return g_paletteBlankRows[row] != 0;
}

TimelineIDMap* GetPaletteIDMap(int minimum, int maximum, int skipBlankRows)
{
    TimelineIDMap* map;
    size_t candidateTotal;
    UINT_PTR signature;
    int candidate;
    int paletteInfoReady;
    LONG paletteInfoGeneration;

    if (maximum < minimum)
    {
        return NULL;
    }

    paletteInfoReady = skipBlankRows && RuntimePaletteInfoIsReady();
    paletteInfoGeneration = InterlockedCompareExchange(&g_paletteInfoGeneration, 0, 0);
    signature = ((UINT_PTR)(unsigned int)maximum << 2) | (skipBlankRows ? 2U : 0U) | (paletteInfoReady ? 1U : 0U);
    signature ^= (UINT_PTR)(unsigned int)paletteInfoGeneration << 32;
    map = &g_paletteIDMap;

    if (map->signature == signature && map->minimum == minimum && map->maximum == maximum)
    {
        return map;
    }

    ReleaseTimelineIdMap(map);
    map->signature = signature;
    map->minimum = minimum;
    map->maximum = maximum;
    candidateTotal = (size_t)maximum - (size_t)minimum + 1U;

    if (candidateTotal > (size_t)-1 / sizeof(*map->validIDs))
    {
        return map;
    }

    map->validIDs = (int*)HeapAlloc(GetProcessHeap(), 0, candidateTotal * sizeof(*map->validIDs));

    if (!map->validIDs)
    {
        return map;
    }

    for (candidate = minimum; candidate <= maximum; ++candidate)
    {
        if (!skipBlankRows || !RuntimePaletteRowIsBlank(candidate))
        {
            map->validIDs[map->validTotal++] = candidate;
        }
    }

    return map;
}

int FindTimelineMapIndex(const TimelineIDMap* map, int actualID)
{
    int after;
    int before;
    int high;
    int low;
    int middle;

    if (!map || !map->validIDs || map->validTotal <= 0)
    {
        return -1;
    }

    low = 0;
    high = map->validTotal;

    while (low < high)
    {
        middle = low + (high - low) / 2;

        if (map->validIDs[middle] < actualID)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    if (low < map->validTotal && map->validIDs[low] == actualID)
    {
        return low;
    }

    if (low <= 0)
    {
        return 0;
    }

    if (low >= map->validTotal)
    {
        return map->validTotal - 1;
    }

    before = map->validIDs[low - 1];
    after = map->validIDs[low];
    return actualID - before < after - actualID ? low - 1 : low;
}

int FindTimelineMapIDInDirection(const TimelineIDMap* map, int current, int direction, int* outValue)
{
    int high;
    int low;
    int middle;

    if (!map || !map->validIDs || map->validTotal <= 0 || !outValue || direction == 0)
    {
        return 0;
    }

    // Find the first valid ID that is not less than the current value..
    low = 0;
    high = map->validTotal;

    while (low < high)
    {
        middle = low + (high - low) / 2;

        if (map->validIDs[middle] < current)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    if (direction < 0)
    {
        --low;

        if (low < 0)
        {
            return 0;
        }
        
        *outValue = map->validIDs[low];

        return 1;
    }

    if (low < map->validTotal && map->validIDs[low] == current)
    {
        ++low;
    }

    if (low >= map->validTotal)
    {
        return 0;
    }

    *outValue = map->validIDs[low];

    return 1;
}

int FindTimelineMapIDForNavigation(const TimelineIDMap* map, int requested, int preferredDirection, int* outValue)
{
    int high;
    int low;
    int middle;

    if (!map || !map->validIDs || map->validTotal <= 0 || !outValue)
    {
        return 0;
    }

    if (preferredDirection == 0)
    {
        preferredDirection = 1;
    }

    low = 0;
    high = map->validTotal;

    while (low < high)
    {
        middle = low + (high - low) / 2;

        if (map->validIDs[middle] < requested)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }

    if (low < map->validTotal && map->validIDs[low] == requested)
    {
        *outValue = requested;
        return 1;
    }

    if (preferredDirection > 0 && low < map->validTotal)
    {
        *outValue = map->validIDs[low];
        return 1;
    }

    if (preferredDirection < 0 && low > 0)
    {
        *outValue = map->validIDs[low - 1];
        return 1;
    }

    // The preferred side has no valid entry, use the nearest one on the other side..
    if (low > 0)
    {
        *outValue = map->validIDs[low - 1];
        return 1;
    }

    if (low < map->validTotal)
    {
        *outValue = map->validIDs[low];
        return 1;
    }

    return 0;
}

int FindTimelineIDInDirection(void* catVisual, AppearanceField field, int start, int direction, int minimum, int maximum, int* outValue)
{
    int candidate;

    if (!outValue || direction == 0)
    {
        return 0;
    }

    candidate = start;

    if (candidate < minimum)
    {
        candidate = minimum;
    }

    else if (candidate > maximum)
    {
        candidate = maximum;
    }

    while (candidate >= minimum && candidate <= maximum)
    {
        if (IsFieldTimelineFrameValid(catVisual, field, candidate))
        {
            *outValue = candidate;
            return 1;
        }

        if ((direction < 0 && candidate == minimum) || (direction > 0 && candidate == maximum))
        {
            break;
        }

        candidate += direction;
    }

    return 0;
}

int FindTimelineIDForNavigation(void* catVisual, AppearanceField field, int requested, int preferredDirection, int minimum, int maximum, int* outValue)
{
    if (preferredDirection == 0)
    {
        preferredDirection = 1;
    }

    if (FindTimelineIDInDirection(catVisual, field, requested, preferredDirection, minimum, maximum, outValue))
    {
        return 1;
    }

    return FindTimelineIDInDirection(catVisual, field, requested, -preferredDirection, minimum, maximum, outValue);
}

int FindClosestTimelineID(void* catVisual, AppearanceField field, int requested, int preferredDirection, int minimum, int maximum, int* outValue)
{
    int distance;
    int first;
    int second;

    if (!outValue || maximum < minimum)
    {
        return 0;
    }

    if (requested < minimum)
    {
        requested = minimum;
    }
    else if (requested > maximum)
    {
        requested = maximum;
    }

    if (IsFieldTimelineFrameValid(catVisual, field, requested))
    {
        *outValue = requested;
        return 1;
    }

    for (distance = 1; requested - distance >= minimum || requested + distance <= maximum; ++distance)
    {
        first = requested + preferredDirection * distance;
        second = requested - preferredDirection * distance;

        if (first >= minimum && first <= maximum && IsFieldTimelineFrameValid(catVisual, field, first))
        {
            *outValue = first;
            return 1;
        }

        if (second >= minimum && second <= maximum && IsFieldTimelineFrameValid(catVisual, field, second))
        {
            *outValue = second;
            return 1;
        }
    }

    return 0;
}

void ApplyPaletteMaterial(void* catVisual, const uint8_t* parts)
{
    void* paletteMaterial;
    MsvcString parameterName;

    if (!catVisual || !parts || !g_setMaterialInt)
    {
        return;
    }

    paletteMaterial = *(void**)((uint8_t*)catVisual + CAT_VISUAL_PALETTE_MATERIAL_OFFSET);
    if (!paletteMaterial)
    {
        return;
    }

    InitMsvcString(&parameterName, "palette");
    g_setMaterialInt(paletteMaterial, &parameterName, *(const int*)(parts + CATPART_PALETTE_OFFSET));
}