#include "Catstructor.h"

/* 
* Live movie timeline discovery and valid-ID navigation.. 
*/

static int TimelineChildIsNamedHelper(const uint8_t* entry, const void* child);

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
        if (ReadTimelineChildKey(children[index], &candidate) && TimelineChildKeyEquals(&candidate, key))
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
        // The outer part frame may have replaced its named child objects...
        RefreshGraphicsEntryChildren(entry);
        return *(void**)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET);
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
        * extents, so don't enlarge an editor ID range...
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

    if (profile->backgroundFirstFrames)
    {
        HeapFree(GetProcessHeap(), 0, profile->backgroundFirstFrames);
    }

    if (profile->distinctArtFrames)
    {
        HeapFree(GetProcessHeap(), 0, profile->distinctArtFrames);
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
        if (!IsTimelineChildRenderable(children[childIndex]) || !ReadTimelineChildKey(children[childIndex], &childKey) || TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
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

static void RememberRepeatedTimelineChild(TimelineProfile* profile, const TimelineChildKey* key, int repeatLength)
{
    if (!profile || !profile->backgroundChildren || !key || repeatLength < TIMELINE_BACKGROUND_REPEAT_LENGTH || TimelineChildKeyArrayContains(profile->backgroundChildren, profile->backgroundChildTotal, key))
    {
        return;
    }

    profile->backgroundChildren[profile->backgroundChildTotal++] = *key;
}

static TimelineProfile* GetTimelineProfile(uint8_t* entry, void* movieClip, AppearanceField field)
{
    TimelineChildKey* commonChildren;
    TimelineChildKey childKey;
    TimelineChildKey previousSoleKey;
    TimelineChildKey soleKey;
    TimelineProfile* profile;
    void** children;
    void* definition;
    size_t index;
    int backgroundIndex;
    int childIndex;
    int childTotal;
    int commonIndex;
    int commonTotal;
    int currentFrame;
    int frame;
    int frameExtent;
    int hasPreviousSole;
    int learnBackgroundChildren;
    int readable;
    int repeatLength;
    int specificTotal;
    int previousSpecificTotal;
    UINT_PTR childValue;
    UINT_PTR frameSignature;
    UINT_PTR frameSignature2;
    UINT_PTR previousSignature;
    UINT_PTR previousSignature2;

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
            return profile;
        }
    }

    currentFrame = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);
    SetMovieClipFrameForInspection(movieClip, 0);

    if (!GetMovieClipChildren(movieClip, &children, &childTotal))
    {
        SetMovieClipFrameForInspection(movieClip, currentFrame);
        return NULL;
    }

    commonChildren = NULL;
    commonTotal = 0;

    if (childTotal > 0)
    {
        commonChildren = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), 0, (size_t)childTotal * sizeof(*commonChildren));

        if (!commonChildren)
        {
            SetMovieClipFrameForInspection(movieClip, currentFrame);
            return NULL;
        }

        for (childIndex = 0; childIndex < childTotal; ++childIndex)
        {
            if (!ReadTimelineChildKey(children[childIndex], &childKey))
            {
                continue;
            }

            readable = 1;

            for (commonIndex = 0; commonIndex < commonTotal; ++commonIndex)
            {
                if (TimelineChildKeyEquals(&commonChildren[commonIndex], &childKey))
                {
                    readable = 0;
                    break;
                }
            }

            if (readable)
            {
                commonChildren[commonTotal++] = childKey;
            }
        }
    }

    readable = 1;

    for (frame = 1; frame < frameExtent && commonTotal > 0; ++frame)
    {
        SetMovieClipFrameForInspection(movieClip, frame);

        if (!GetMovieClipChildren(movieClip, &children, &childTotal))
        {
            readable = 0;
            break;
        }

        for (commonIndex = commonTotal - 1; commonIndex >= 0; --commonIndex)
        {
            if (!TimelineChildrenContainKey(children, childTotal, &commonChildren[commonIndex]))
            {
                --commonTotal;
                commonChildren[commonIndex] = commonChildren[commonTotal];
            }
        }
    }

    SetMovieClipFrameForInspection(movieClip, currentFrame);

    if (!readable)
    {
        if (commonChildren)
        {
            HeapFree(GetProcessHeap(), 0, commonChildren);
        }

        return NULL;
    }

    if (g_timelineProfileUsed < TIMELINE_PROFILE_CACHE_CAPACITY)
    {
        profile = &g_timelineProfiles[g_timelineProfileUsed++];
    }
    else
    {
        index = ((UINT_PTR)definition >> 4) % TIMELINE_PROFILE_CACHE_CAPACITY;
        profile = &g_timelineProfiles[index];
        ReleaseTimelineProfile(profile);
    }

    profile->definition = definition;
    profile->commonChildren = commonChildren;
    profile->frameExtent = frameExtent;
    profile->commonChildTotal = commonTotal;
    profile->learnsBackgroundChildren = learnBackgroundChildren;

    if (learnBackgroundChildren)
    {
        if ((size_t)frameExtent > (size_t)-1 / sizeof(*profile->backgroundChildren))
        {
            ReleaseTimelineProfile(profile);
            return NULL;
        }

        profile->backgroundChildren = (TimelineChildKey*)HeapAlloc(GetProcessHeap(), 0, (size_t)frameExtent * sizeof(*profile->backgroundChildren));
        profile->backgroundFirstFrames = (unsigned char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->backgroundFirstFrames));
        
        if (!profile->backgroundChildren || !profile->backgroundFirstFrames)
        {
            ReleaseTimelineProfile(profile);
            return NULL;
        }

        hasPreviousSole = 0;
        repeatLength = 0;
        memset(&previousSoleKey, 0, sizeof(previousSoleKey));

        for (frame = 0; frame < frameExtent; ++frame)
        {
            SetMovieClipFrameForInspection(movieClip, frame);

            if (!GetMovieClipChildren(movieClip, &children, &childTotal))
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                ReleaseTimelineProfile(profile);
                return NULL;
            }

            if (FindSoleTimelineSpecificChild(children, childTotal, profile, &soleKey))
            {
                if (hasPreviousSole && TimelineChildKeyEquals(&previousSoleKey, &soleKey))
                {
                    ++repeatLength;
                }
                else
                {
                    if (hasPreviousSole)
                    {
                        RememberRepeatedTimelineChild(profile, &previousSoleKey, repeatLength);
                    }

                    previousSoleKey = soleKey;
                    hasPreviousSole = 1;
                    repeatLength = 1;
                }
            }
            else
            {
                if (hasPreviousSole)
                {
                    RememberRepeatedTimelineChild(profile, &previousSoleKey, repeatLength);
                }

                hasPreviousSole = 0;
                repeatLength = 0;
            }
        }

        if (hasPreviousSole)
        {
            RememberRepeatedTimelineChild(profile, &previousSoleKey, repeatLength);
        }

        // Keep only the first authored occurrence of each learned background symbol...
        for (backgroundIndex = 0; backgroundIndex < profile->backgroundChildTotal; ++backgroundIndex)
        {
            for (frame = 0; frame < frameExtent; ++frame)
            {
                SetMovieClipFrameForInspection(movieClip, frame);

                if (!GetMovieClipChildren(movieClip, &children, &childTotal))
                {
                    SetMovieClipFrameForInspection(movieClip, currentFrame);
                    ReleaseTimelineProfile(profile);
                    return NULL;
                }

                if (FindSoleTimelineSpecificChild(children, childTotal, profile, &soleKey) && TimelineChildKeyEquals(&soleKey, &profile->backgroundChildren[backgroundIndex]))
                {
                    profile->backgroundFirstFrames[frame] = 1;
                    break;
                }
            }
        }

        SetMovieClipFrameForInspection(movieClip, currentFrame);
    }
    else
    {
        profile->distinctArtFrames = (unsigned char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)frameExtent * sizeof(*profile->distinctArtFrames));
        
        if (!profile->distinctArtFrames)
        {
            ReleaseTimelineProfile(profile);
            return NULL;
        }

        previousSignature = 0;
        previousSignature2 = 0;
        previousSpecificTotal = 0;

        for (frame = 0; frame < frameExtent; ++frame)
        {
            SetMovieClipFrameForInspection(movieClip, frame);
            RefreshGraphicsEntryChildren(entry);

            if (!GetMovieClipChildren(movieClip, &children, &childTotal))
            {
                SetMovieClipFrameForInspection(movieClip, currentFrame);
                RefreshGraphicsEntryChildren(entry);
                ReleaseTimelineProfile(profile);
                return NULL;
            }

            frameSignature = (UINT_PTR)1469598103934665603ULL;
            frameSignature2 = 0;
            specificTotal = 0;

            for (childIndex = 0; childIndex < childTotal; ++childIndex)
            {
                if (!IsTimelineChildRenderable(children[childIndex]) || TimelineChildIsNamedHelper(entry, children[childIndex]) || !ReadTimelineChildKey(children[childIndex], &childKey))
                {
                    continue;
                }

                childValue = ((UINT_PTR)childKey.characterID << 16) | (UINT_PTR)childKey.libraryID;
                childValue += (UINT_PTR)0x9E3779B97F4A7C15ULL;
                childValue ^= childValue >> 30;
                childValue *= (UINT_PTR)0xBF58476D1CE4E5B9ULL;
                childValue ^= childValue >> 27;
                childValue *= (UINT_PTR)0x94D049BB133111EBULL;
                childValue ^= childValue >> 31;
                frameSignature += childValue;
                frameSignature2 += childValue * (childValue | 1);
                ++specificTotal;
            }

            if (specificTotal > 0 && (frame == 0 || specificTotal != previousSpecificTotal || frameSignature != previousSignature || frameSignature2 != previousSignature2))
            {
                profile->distinctArtFrames[frame] = 1;
            }

            previousSpecificTotal = specificTotal;
            previousSignature = frameSignature;
            previousSignature2 = frameSignature2;
        }

        SetMovieClipFrameForInspection(movieClip, currentFrame);
        RefreshGraphicsEntryChildren(entry);
    }

    return profile;
}

static int TimelineChildIsNamedHelper(const uint8_t* entry, const void* child)
{
    return entry && child && (child == *(void* const*)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET) || child == *(void* const*)(entry + CATPART_GRAPHICS_SCARS_CLIP_OFFSET) || child == *(void* const*)(entry + CATPART_GRAPHICS_AUX_CLIP_OFFSET));
}

static int TimelineFrameHasSpecificContent(uint8_t* entry, void* movieClip, AppearanceField field, int oneBasedFrame, int* outHasRenderableContent)
{
    TimelineChildKey childKey;
    TimelineProfile* profile;
    void** children;
    int childIndex;
    int childTotal;
    int currentFrame;
    int specific;

    if (outHasRenderableContent)
    {
        *outHasRenderableContent = 0;
    }

    profile = GetTimelineProfile(entry, movieClip, field);

    if (field != APPEARANCE_FIELD_TEXTURE)
    {
        // (Profile construction seeks the outer part through every frame)...
        RefreshGraphicsEntryChildren(entry);
        g_timelineVisualNeedsRefresh = 1;
    }

    if (!profile || oneBasedFrame < 1 || oneBasedFrame > profile->frameExtent)
    {
        return 0;
    }

    if (field != APPEARANCE_FIELD_TEXTURE)
    {
        specific = profile->distinctArtFrames && profile->distinctArtFrames[oneBasedFrame - 1];

        if (outHasRenderableContent)
        {
            *outHasRenderableContent = specific;
        }

        return specific;
    }

    currentFrame = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);
    SetMovieClipFrameForInspection(movieClip, oneBasedFrame - 1);

    if (field != APPEARANCE_FIELD_TEXTURE)
    {
        RefreshGraphicsEntryChildren(entry);
    }

    specific = 0;

    if (GetMovieClipChildren(movieClip, &children, &childTotal))
    {
        for (childIndex = 0; childIndex < childTotal && !specific; ++childIndex)
        {
            if (!IsTimelineChildRenderable(children[childIndex]) || !ReadTimelineChildKey(children[childIndex], &childKey))
            {
                continue;
            }

            if (TimelineChildKeyArrayContains(profile->commonChildren, profile->commonChildTotal, &childKey))
            {
                continue;
            }

            if (outHasRenderableContent)
            {
                *outHasRenderableContent = 1;
            }

            if (TimelineChildKeyArrayContains(profile->backgroundChildren, profile->backgroundChildTotal, &childKey) && (!profile->backgroundFirstFrames || !profile->backgroundFirstFrames[oneBasedFrame - 1]))
            {
                continue;
            }

            specific = 1;
        }
    }

    SetMovieClipFrameForInspection(movieClip, currentFrame);

    if (field != APPEARANCE_FIELD_TEXTURE)
    {
        RefreshGraphicsEntryChildren(entry);
        g_timelineVisualNeedsRefresh = 1;
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
    int hasRenderableContent;
    int specificTimelineTotal;
    int valid;

    containerTotal = GetTimelineContainerOffsets(field, containerOffsets);
    eligibleTimelineTotal = 0;
    specificTimelineTotal = 0;

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

        ++eligibleTimelineTotal;

        if (oneBasedFrame < 1 || oneBasedFrame > frameExtent)
        {
            if (field == APPEARANCE_FIELD_TEXTURE)
            {
                return 0;
            }

            continue;
        }

        hasRenderableContent = 0;
        valid = TimelineFrameHasSpecificContent(entry, movieClip, field, oneBasedFrame, &hasRenderableContent);

        if (field == APPEARANCE_FIELD_TEXTURE)
        {
            if (!hasRenderableContent)
            {
                return 0;
            }

            if (valid)
            {
                ++specificTimelineTotal;
            }

            continue;
        }

        if (valid)
        {
            return 1;
        }
    }

    return field == APPEARANCE_FIELD_TEXTURE && eligibleTimelineTotal > 0 && specificTimelineTotal > 0;
}

static UINT_PTR GetTimelineIdMapSignature(void* catVisual, AppearanceField field)
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

static int GetTextureTimelineFingerprint(void* catVisual, int oneBasedFrame, TimelineFrameFingerprint* outFingerprint)
{
    TimelineChildKey childKey;
    size_t containerOffsets[TIMELINE_CONTAINER_CAPACITY];
    size_t containerIndex;
    size_t containerTotal;
    uint8_t* entries;
    uint8_t* entry;
    void** children;
    void* movieClip;
    UINT_PTR first;
    UINT_PTR second;
    UINT_PTR value;
    int childIndex;
    int childTotal;
    int currentFrame;
    int entryTotal;
    int frameExtent;
    int renderedChildTotal;
    int timelineTotal;

    if (!catVisual || !outFingerprint || oneBasedFrame < 1)
    {
        return 0;
    }

    first = (UINT_PTR)1469598103934665603ULL;
    second = 0;
    renderedChildTotal = 0;
    timelineTotal = 0;
    containerTotal = GetTimelineContainerOffsets(APPEARANCE_FIELD_TEXTURE, containerOffsets);

    for (containerIndex = 0; containerIndex < containerTotal; ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        entry = entries;
        movieClip = GetTimelineEntryClip(entry, APPEARANCE_FIELD_TEXTURE);
        frameExtent = GetMovieClipFrameExtent(movieClip);

        if (frameExtent <= 0)
        {
            continue;
        }
        if (oneBasedFrame > frameExtent || !IsReadableMemoryRange((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET, sizeof(int)))
        {
            return 0;
        }

        currentFrame = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);
        SetMovieClipFrameForInspection(movieClip, oneBasedFrame - 1);

        if (!GetMovieClipChildren(movieClip, &children, &childTotal))
        {
            SetMovieClipFrameForInspection(movieClip, currentFrame);
            return 0;
        }

        value = MixTimelineFingerprintValue((UINT_PTR)containerOffsets[containerIndex] ^ ((UINT_PTR)timelineTotal << 32));
        first ^= value;
        first *= (UINT_PTR)1099511628211ULL;
        second += value * (value | 1U);
        ++timelineTotal;

        for (childIndex = 0; childIndex < childTotal; ++childIndex)
        {
            if (!IsTimelineChildRenderable(children[childIndex]) || !ReadTimelineChildKey(children[childIndex], &childKey))
            {
                continue;
            }

            value = ((UINT_PTR)childKey.characterID << 16) | (UINT_PTR)childKey.libraryID;
            value ^= (UINT_PTR)(unsigned int)childIndex << 40;
            value ^= (UINT_PTR)(unsigned char)*((const unsigned char*)children[childIndex] + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET) << 8;
            value ^= (UINT_PTR)(unsigned int)*(const int*)((const uint8_t*)children[childIndex] + DISPLAY_OBJECT_CLIP_DEPTH_OFFSET);
            value = MixTimelineFingerprintValue(value ^ ((UINT_PTR)containerIndex << 48));
            first ^= value;
            first *= (UINT_PTR)1099511628211ULL;
            second += value * (value | 1U);
            ++renderedChildTotal;
        }

        SetMovieClipFrameForInspection(movieClip, currentFrame);
    }

    if (timelineTotal <= 0)
    {
        return 0;
    }

    outFingerprint->first = first;
    outFingerprint->second = second;
    outFingerprint->childTotal = renderedChildTotal;
    outFingerprint->timelineTotal = timelineTotal;
    return 1;
}

static int TimelineFrameFingerprintEquals(const TimelineFrameFingerprint* first, const TimelineFrameFingerprint* second)
{
    return first && second && first->first == second->first && first->second == second->second && first->childTotal == second->childTotal && first->timelineTotal == second->timelineTotal;
}

TimelineIDMap* GetTimelineIDMap(void* catVisual, AppearanceField field, int minimum, int maximum)
{
    TimelineFrameFingerprint currentFingerprint;
    TimelineFrameFingerprint previousFingerprint;
    TimelineIDMap* map;
    UINT_PTR signature;
    size_t candidateTotal;
    int candidate;
    int hasPreviousTextureFingerprint;
    int timelineValid;

    if (!catVisual || field < 0 || field >= APPEARANCE_FIELD_COUNT || maximum < minimum)
    {
        return NULL;
    }

    signature = GetTimelineIdMapSignature(catVisual, field);
    if (!signature)
    {
        return NULL;
    }

    map = &g_timelineIDMaps[field];

    if (map->catVisual == catVisual && map->signature == signature && map->minimum == minimum && map->maximum == maximum)
    {
        return map;
    }

    ReleaseTimelineIdMap(map);
    map->catVisual = catVisual;
    map->signature = signature;
    map->minimum = minimum;
    map->maximum = maximum;

    candidateTotal = (size_t)maximum - (size_t)minimum + 1;

    if (candidateTotal > (size_t)-1 / sizeof(*map->validIDs))
    {
        return map;
    }

    map->validIDs = (int*)HeapAlloc(GetProcessHeap(), 0, candidateTotal * sizeof(*map->validIDs));

    if (!map->validIDs)
    {
        return map;
    }

    hasPreviousTextureFingerprint = 0;
    memset(&previousFingerprint, 0, sizeof(previousFingerprint));
    candidate = minimum;

    for (;;)
    {
        timelineValid = IsFieldTimelineFrameValid(catVisual, field, candidate);
        if (timelineValid && field == APPEARANCE_FIELD_TEXTURE)
        {
            if (GetTextureTimelineFingerprint(catVisual, candidate, &currentFingerprint))
            {
                // Multi-child held ranges are repeated artwork too..
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

        ++candidate;
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