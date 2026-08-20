#include "Catstructor.h"

/* 
* Editor widgets, appearance controls, scene rendering stuff... 
*/

static void RefreshEditorCatVisual(void* catVisual, uint8_t* parts);

#define INITIAL_INDEXING_VISUAL_PART_TOTAL 14

typedef struct InitialIndexingVisualSnapshot
{
    int partIDs[INITIAL_INDEXING_VISUAL_PART_TOTAL];
    int textures[INITIAL_INDEXING_VISUAL_PART_TOTAL];
    int palette;
    int arm1Claws;
    int arm2Claws;
    int captured;
} InitialIndexingVisualSnapshot;

static const size_t g_initialIndexingPartIDOffsets[INITIAL_INDEXING_VISUAL_PART_TOTAL] =
{
    CATPART_BODY_ID_OFFSET, CATPART_HEAD_ID_OFFSET, CATPART_TAIL_ID_OFFSET, CATPART_LEG1_ID_OFFSET, CATPART_LEG2_ID_OFFSET,
    CATPART_ARM1_ID_OFFSET, CATPART_ARM2_ID_OFFSET, CATPART_LEFTEYE_ID_OFFSET, CATPART_RIGHTEYE_ID_OFFSET,
    CATPART_LEFTEYEBROW_ID_OFFSET, CATPART_RIGHTEYEBROW_ID_OFFSET, CATPART_LEFTEAR_ID_OFFSET, CATPART_RIGHTEAR_ID_OFFSET,
    CATPART_MOUTH_ID_OFFSET
};

static const size_t g_initialIndexingTextureOffsets[INITIAL_INDEXING_VISUAL_PART_TOTAL] =
{
    CATPART_BODY_TEXTURE_OFFSET, CATPART_HEAD_TEXTURE_OFFSET, CATPART_TAIL_TEXTURE_OFFSET, CATPART_LEG1_TEXTURE_OFFSET, CATPART_LEG2_TEXTURE_OFFSET,
    CATPART_ARM1_TEXTURE_OFFSET, CATPART_ARM2_TEXTURE_OFFSET, CATPART_LEFTEYE_TEXTURE_OFFSET, CATPART_RIGHTEYE_TEXTURE_OFFSET,
    CATPART_LEFTEYEBROW_TEXTURE_OFFSET, CATPART_RIGHTEYEBROW_TEXTURE_OFFSET, CATPART_LEFTEAR_TEXTURE_OFFSET, CATPART_RIGHTEAR_TEXTURE_OFFSET,
    CATPART_MOUTH_TEXTURE_OFFSET
};

static InitialIndexingVisualSnapshot g_initialIndexingVisualSnapshot;

static int g_presetSkipBlankValues[APPEARANCE_FIELD_COUNT];
static unsigned char g_presetSkipBlankActive[APPEARANCE_FIELD_COUNT];

static void ClearPresetSkipBlankBypass(void)
{
    memset(g_presetSkipBlankValues, 0, sizeof(g_presetSkipBlankValues));
    memset(g_presetSkipBlankActive, 0, sizeof(g_presetSkipBlankActive));
}

static int PreservePresetLoadedValue(AppearanceField field, int value)
{
    if (field < 0 || field >= APPEARANCE_FIELD_COUNT || !g_presetSkipBlankActive[field])
    {
        return 0;
    }

    /* 
    * Exemption belongs to the exact value supplied by the preset.
    * Any editor/symmetry/randomization change retires it permanently, so a
    * later coincidental return to that numeric ID cannot bypass the user's skip setting...
    */
    if (g_presetSkipBlankValues[field] != value)
    {
        g_presetSkipBlankActive[field] = 0;
        return 0;
    }

    return 1;
}

static void PreservePresetLoadedAppearance(const AppearanceSnapshot* appearance, unsigned int loadedMask)
{
    int values[APPEARANCE_FIELD_COUNT];
    AppearanceField field;

    if (!appearance)
    {
        ClearPresetSkipBlankBypass();
        return;
    }

    values[APPEARANCE_FIELD_TEXTURE] = appearance->texture;
    values[APPEARANCE_FIELD_PALETTE] = appearance->palette;
    values[APPEARANCE_FIELD_BODY] = appearance->body;
    values[APPEARANCE_FIELD_HEAD] = appearance->head;
    values[APPEARANCE_FIELD_TAIL] = appearance->tail;
    values[APPEARANCE_FIELD_LEG1] = appearance->leg1;
    values[APPEARANCE_FIELD_LEG2] = appearance->leg2;
    values[APPEARANCE_FIELD_ARM1] = appearance->arm1;
    values[APPEARANCE_FIELD_ARM2] = appearance->arm2;
    values[APPEARANCE_FIELD_LEFTEYE] = appearance->lefteye;
    values[APPEARANCE_FIELD_RIGHTEYE] = appearance->righteye;
    values[APPEARANCE_FIELD_LEFTEYEBROW] = appearance->lefteyebrow;
    values[APPEARANCE_FIELD_RIGHTEYEBROW] = appearance->righteyebrow;
    values[APPEARANCE_FIELD_LEFTEAR] = appearance->leftear;
    values[APPEARANCE_FIELD_RIGHTEAR] = appearance->rightear;
    values[APPEARANCE_FIELD_MOUTH] = appearance->mouth;

    ClearPresetSkipBlankBypass();

    for (field = APPEARANCE_FIELD_TEXTURE; field < APPEARANCE_FIELD_COUNT; ++field)
    {
        if ((loadedMask & (1U << (unsigned int)field)) == 0)
        {
            continue;
        }

        g_presetSkipBlankValues[field] = values[field];
        g_presetSkipBlankActive[field] = 1;
    }
}

void ResetInitialIndexingAppearanceSnapshot(void)
{
    memset(&g_initialIndexingVisualSnapshot, 0, sizeof(g_initialIndexingVisualSnapshot));
}

int CaptureInitialIndexingAppearance(const uint8_t* parts)
{
    int index;

    if (!parts)
    {
        return 0;
    }

    /* 
    * The first capture is authoritative for the whole initialization pass!
    * Later frames can call this defensively...
    */
    if (g_initialIndexingVisualSnapshot.captured)
    {
        return 1;
    }

    for (index = 0; index < INITIAL_INDEXING_VISUAL_PART_TOTAL; ++index)
    {
        g_initialIndexingVisualSnapshot.partIDs[index] = *(const int*)(parts + g_initialIndexingPartIDOffsets[index]);
        g_initialIndexingVisualSnapshot.textures[index] = *(const int*)(parts + g_initialIndexingTextureOffsets[index]);
    }

    g_initialIndexingVisualSnapshot.palette = *(const int*)(parts + CATPART_PALETTE_OFFSET);
    g_initialIndexingVisualSnapshot.arm1Claws = *(const int*)(parts + CATPART_ARM1_CLAWS_OFFSET);
    g_initialIndexingVisualSnapshot.arm2Claws = *(const int*)(parts + CATPART_ARM2_CLAWS_OFFSET);
    g_initialIndexingVisualSnapshot.captured = 1;
    return 1;
}

static int RestoreInitialIndexingAppearance(uint8_t* parts)
{
    int index;

    if (!parts || !g_initialIndexingVisualSnapshot.captured)
    {
        return 0;
    }

    for (index = 0; index < INITIAL_INDEXING_VISUAL_PART_TOTAL; ++index)
    {
        *(int*)(parts + g_initialIndexingPartIDOffsets[index]) = g_initialIndexingVisualSnapshot.partIDs[index];
        *(int*)(parts + g_initialIndexingTextureOffsets[index]) = g_initialIndexingVisualSnapshot.textures[index];
    }

    *(int*)(parts + CATPART_PALETTE_OFFSET) = g_initialIndexingVisualSnapshot.palette;
    *(int*)(parts + CATPART_ARM1_CLAWS_OFFSET) = g_initialIndexingVisualSnapshot.arm1Claws;
    *(int*)(parts + CATPART_ARM2_CLAWS_OFFSET) = g_initialIndexingVisualSnapshot.arm2Claws;

    return 1;
}

static void SetLockedEditorWindowGeometry(int autoFitHeight)
{
    uint8_t* context;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return;
    }

    /*
    * Keep the controls wholly inside the left half of a 1280x720 stage (FLA/SWF resolution).
    * During initial indexing, keep the normal locked width but pass a zero Y size so ImGui
    * auto-fits only the window height to the status text and progress bar. Once indexing has
    * finished, restore the editor's normal fixed height...
    */
    *(int*)(context + IMGUI_NEXT_WINDOW_FLAGS_OFFSET) |= IMGUI_NEXT_WINDOW_HAS_POS | IMGUI_NEXT_WINDOW_HAS_SIZE;
    *(float*)(context + IMGUI_NEXT_WINDOW_POS_OFFSET) = DEBUG_WINDOW_LOCKED_X;
    *(float*)(context + IMGUI_NEXT_WINDOW_POS_OFFSET + sizeof(float)) = DEBUG_WINDOW_LOCKED_Y;
    *(float*)(context + IMGUI_NEXT_WINDOW_SIZE_OFFSET) = DEBUG_WINDOW_LOCKED_WIDTH;
    *(float*)(context + IMGUI_NEXT_WINDOW_SIZE_OFFSET + sizeof(float)) = autoFitHeight ? 0.0f : DEBUG_WINDOW_LOCKED_HEIGHT;
    *(int*)(context + IMGUI_NEXT_WINDOW_POS_COND_OFFSET) = IMGUI_COND_ALWAYS;
    *(int*)(context + IMGUI_NEXT_WINDOW_SIZE_COND_OFFSET) = IMGUI_COND_ALWAYS;
}

static uint8_t* GetCurrentImGuiWindow(void)
{
    uint8_t* context;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    return context ? *(uint8_t**)(context + IMGUI_CURRENT_WINDOW_OFFSET) : NULL;
}

void EndImGuiCombo(void)
{
    uint8_t* context;
    int* comboDepth;

    if (!g_imguiEndCombo)
    {
        return;
    }

    g_imguiEndCombo();

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (context)
    {
        comboDepth = (int*)(context + IMGUI_COMBO_DEPTH_OFFSET);

        if (*comboDepth > 0)
        {
            --*comboDepth;
        }
    }
}

void SetNextItemWidth(float width)
{
    uint8_t* context;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return;
    }

    *(int*)(context + IMGUI_NEXT_ITEM_DATA_FLAGS_OFFSET) |= IMGUI_NEXT_ITEM_HAS_WIDTH;
    *(float*)(context + IMGUI_NEXT_ITEM_WIDTH_OFFSET) = width;
}

int SelectableKeepPopupOpen(const char* label, bool selected, const float size[2])
{
    uint8_t* context;
    int clicked;
    int* itemFlags;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);
    itemFlags = NULL;

    if (context)
    {
        itemFlags = (int*)(context + IMGUI_NEXT_ITEM_ITEM_FLAGS_OFFSET);
        *itemFlags |= IMGUI_ITEM_SELECTABLE_DONT_CLOSE_POPUP;
    }

    clicked = g_imguiSelectable(label, selected, 0, size);

    if (itemFlags)
    {
        *itemFlags &= ~IMGUI_ITEM_SELECTABLE_DONT_CLOSE_POPUP;
    }

    return clicked;
}

void AddHorizontalGap(float amount)
{
    uint8_t* window;

    window = GetCurrentImGuiWindow();

    if (window)
    {
        *(float*)(window + IMGUI_WINDOW_CURSOR_POS_X_OFFSET) += amount;
    }
}

void AddVerticalGap(float amount)
{
    uint8_t* window;

    window = GetCurrentImGuiWindow();

    if (window)
    {
        *(float*)(window + IMGUI_WINDOW_CURSOR_POS_Y_OFFSET) += amount;
    }
}

static void AdjustSectionIndent(float amount)
{
    uint8_t* window;
    float indent;

    window = GetCurrentImGuiWindow();

    if (!window)
    {
        return;
    }

    indent = *(float*)(window + IMGUI_WINDOW_INDENT_X_OFFSET) + amount;
    *(float*)(window + IMGUI_WINDOW_INDENT_X_OFFSET) = indent;
    *(float*)(window + IMGUI_WINDOW_CURSOR_POS_X_OFFSET) = *(float*)(window + IMGUI_WINDOW_POS_X_OFFSET) + indent + *(float*)(window + IMGUI_WINDOW_COLUMNS_X_OFFSET);
}

static void UpdateSliderNavigationHeight(void)
{
    uint8_t* context;
    float minimumY;
    float maximumY;
    float height;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return;
    }

    minimumY = *(float*)(context + IMGUI_LAST_ITEM_RECT_MIN_Y_OFFSET);
    maximumY = *(float*)(context + IMGUI_LAST_ITEM_RECT_MAX_Y_OFFSET);
    height = maximumY - minimumY;

    if (height > 0.0f)
    {
        g_sliderNavigationHeight = height;
    }
}

static float SliderNavigationButtonHeight(void)
{
    uint8_t* context;
    float spacing;
    float height;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return g_sliderNavigationHeight;
    }

    spacing = *(float*)(context + IMGUI_STYLE_ITEM_SPACING_Y_OFFSET);
    height = g_sliderNavigationHeight - spacing;

    return height > 1.0f ? height : 1.0f;
}

static int SliderInt(const char* label, int* value, int minimum, int maximum, const char* format)
{
    float sliderValue;
    float sliderMinimum;
    float sliderMaximum;
    int changed;
    int roundedValue;

    sliderValue = (float)*value;
    sliderMinimum = (float)minimum;
    sliderMaximum = (float)maximum;
    changed = g_imguiSliderScalar(label, IMGUI_DATA_TYPE_FLOAT, &sliderValue, &sliderMinimum, &sliderMaximum, format, 0);
    UpdateSliderNavigationHeight();

    if (!changed)
    {
        return 0;
    }

    roundedValue = (int)(sliderValue + 0.5f);

    if (roundedValue < minimum)
    {
        roundedValue = minimum;
    }
    else if (roundedValue > maximum)
    {
        roundedValue = maximum;
    }

    *value = roundedValue;

    return 1;
}

static int AdvanceTimelineIndexes(void* catVisual, int* outPercent)
{
    TimelineIDMap* map;
    AppearanceField field;
    int completeTotal;
    int maximum;
    int timelineTotal;

    if (outPercent)
    {
        *outPercent = 100;
    }

    /* 
    * A missing visual is handled by the normal fallback controls. Don't
    * turn that transient state into a loading screen that can never finish... 
    */
    if (!catVisual)
    {
        return 1;
    }

    completeTotal = 0;
    timelineTotal = 0;

    /* 
    * Build every blank-art navigation map before the editor controls are
    * exposed. This is independent of section/dropdown visibility, so
    * collapsed sections can't prevent timelines from indexing...
    */
    for (field = APPEARANCE_FIELD_TEXTURE; field < APPEARANCE_FIELD_COUNT; ++field)
    {
        if (!FieldSkipsBlankFrames(field))
        {
            continue;
        }

        maximum = GetFieldTimelineExtent(catVisual, field);

        if (maximum < 1)
        {
            continue;
        }

        ++timelineTotal;
        map = GetTimelineIDMap(catVisual, field, 1, maximum);

        /* 
        * A missing map/signature means this field has no usable live
        * timeline to index. Treat it as unavailable instead of hanging the
        * editor forever at less than 100 percent...
        */
        if (!map || !map->building)
        {
            ++completeTotal;
        }
    }

    if (outPercent)
    {
        if (timelineTotal <= 0)
        {
            *outPercent = 100;
        }
        else
        {
            *outPercent = (completeTotal * 100) / timelineTotal;
        }
    }

    return timelineTotal <= 0 || completeTotal >= timelineTotal;
}

static int AdvanceInitialTimelineIndexes(void* catVisual, int* outPercent)
{
    int mapPercent;
    int texturePercent;
    int mapsReady;
    int texturesReady;

    mapPercent = 0;
    texturePercent = 0;
    mapsReady = AdvanceTimelineIndexes(catVisual, &mapPercent);

    /* 
    * Reserve the first 20% for the ordinary part/texture maps. The remaining
    * 80% or whatever represents the pass across every selectable part frame
    * and its MCPF-synchronized nested texture definition...
    */
    if (!mapsReady || g_timelineVisualNeedsRefresh)
    {
        if (outPercent)
        {
            *outPercent = mapPercent / 5;
        }

        return 0;
    }

    texturesReady = AdvanceAllPartTextureIndexes(catVisual, &texturePercent);

    if (outPercent)
    {
        *outPercent = 20 + (texturePercent * 80) / 100;

        if (*outPercent > 100)
        {
            *outPercent = 100;
        }
    }

    return texturesReady;
}

static void DrawTimelineIndexingState(int percent)
{
    char status[96];
    float automaticSize[2];
    int progressValue;

    if (percent < 0)
    {
        percent = 0;
    }
    else if (percent > 100)
    {
        percent = 100;
    }

    automaticSize[0] = 0.0f;
    automaticSize[1] = 0.0f;
    snprintf(status, sizeof(status), "Loading editor, please wait... %d%%##timeline_index_status", percent);
    g_imguiSelectable(status, false, 0, automaticSize);

    progressValue = percent;
    SetNextItemWidth(DEBUG_WINDOW_LOCKED_WIDTH - 40.0f);
    SliderInt("##timeline_index_progress", &progressValue, 0, 100, "%.0f%%");

    g_imguiSelectable("Indexing appearance timelines, including modded entries!##timeline_index_explanation", false, 0, automaticSize);
}

static int RestoreInitialStrayAfterIndexing(void* catVisual, uint8_t* parts)
{
    if (!catVisual || !parts || !RestoreInitialIndexingAppearance(parts))
    {
        return 0;
    }

    /* 
    * Indexing intentionally walks live part and nested texture
    * timelines. Re-apply the exact backing appearance captured before that
    * walk, then rebuild the visual so the editor opens on the same cat that
    * was present when indexing began rather than generating a replacement...
    */
    if (g_prepareCatPartsVisual)
    {
        g_prepareCatPartsVisual(parts);
    }

    ClearPresetSkipBlankBypass();
    ClearNamedAppearanceIDs();
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    g_activeAppearancePath[0] = '\0';
    g_selectedPresetName[0] = '\0';

    RefreshEditorCatVisual(catVisual, parts);
    g_timelineVisualNeedsRefresh = 0;
    ResetInitialIndexingAppearanceSnapshot();

    AddDebugMessage("Ready!");
    Log("Restored the indexed stray's original parts and textures after exhaustive appearance indexing completed!");
    return 1;
}

static int TimelineMapSlider(const char* label, const char* displayLabel, int* actualValue, const TimelineIDMap* map)
{
    char valueFormat[96];
    int selectedID;
    int sliderIndex;

    if (!actualValue || !map || !map->validIDs || map->validTotal <= 0)
    {
        return 0;
    }

    sliderIndex = FindTimelineMapIndex(map, *actualValue);

    if (sliderIndex < 0)
    {
        return 0;
    }

    snprintf(valueFormat, sizeof(valueFormat), "%s: %d", displayLabel, map->validIDs[sliderIndex]);

    if (!SliderInt(label, &sliderIndex, 0, map->validTotal - 1, valueFormat))
    {
        return 0;
    }

    selectedID = map->validIDs[sliderIndex];

    if (selectedID == *actualValue)
    {
        return 0;
    }

    *actualValue = selectedID;
    return 1;
}

static void SyncIDInput(AppearanceField field, int value)
{
    IDInputState* state;

    state = &g_idInputStates[field];

    if (g_namedAppearanceIDs[field][0])
    {
        snprintf(state->text, sizeof(state->text), "%s", g_namedAppearanceIDs[field]);
    }
    else
    {
        snprintf(state->text, sizeof(state->text), "%d", value);
    }

    state->initialized = 1;
    state->replaceOnType = 0;
}

static void UpdateKeyEdge(int key)
{
    int isDown;
    isDown = (GetAsyncKeyState(key) & 0x8000) != 0;
    g_keyPressed[key] = (unsigned char)(isDown && !g_keyWasDown[key]);
    g_keyWasDown[key] = (unsigned char)isDown;
}

static void UpdateEditorInputState(void)
{
    int letter;
    int digit;

    memset(g_keyPressed, 0, sizeof(g_keyPressed));
    UpdateKeyEdge(KEY_BACKSPACE);
    UpdateKeyEdge(KEY_RETURN);
    UpdateKeyEdge(KEY_ESCAPE);
    UpdateKeyEdge(KEY_DELETE);
    UpdateKeyEdge(KEY_SHIFT);
    UpdateKeyEdge(KEY_DECIMAL);
    UpdateKeyEdge(KEY_SUBTRACT);
    UpdateKeyEdge(KEY_OEM_MINUS);
    UpdateKeyEdge(KEY_OEM_PERIOD);

    for (digit = 0; digit <= 9; ++digit)
    {
        UpdateKeyEdge('0' + digit);
        UpdateKeyEdge(KEY_NUMPAD_ZERO + digit);
    }

    for (letter = 'A'; letter <= 'Z'; ++letter)
    {
        UpdateKeyEdge(letter);
    }
}

static void DeactivateIDInput(AppearanceField field, int value)
{
    if (g_activeIDInput != field)
    {
        return;
    }

    SyncIDInput(field, value);
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
}

static int ParseIDInput(const char* text, int minimum, int maximum, int* outValue)
{
    unsigned int value;
    const unsigned char* cursor;

    if (!text || !text[0] || !outValue)
    {
        return 0;
    }

    value = 0;
    cursor = (const unsigned char*)text;

    while (*cursor)
    {
        if (*cursor < '0' || *cursor > '9')
        {
            return 0;
        }

        value = value * 10U + (unsigned int)(*cursor - '0');

        if (value > (unsigned int)maximum)
        {
            value = (unsigned int)maximum;
            break;
        }

        ++cursor;
    }

    if (value < (unsigned int)minimum)
    {
        value = (unsigned int)minimum;
    }

    *outValue = (int)value;
    return 1;
}

static int IsNamedIDCharacter(char value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.';
}

static int IsCompleteNamedID(const char* text)
{
    const char* cursor;

    if (!text || text[0] != '@' || !text[1])
    {
        return 0;
    }

    cursor = text + 1;

    while (*cursor)
    {
        if (!IsNamedIDCharacter(*cursor))
        {
            return 0;
        }

        ++cursor;
    }

    return 1;
}

static void AppendIDInputCharacter(IDInputState* state, char value)
{
    size_t length;

    if (!state)
    {
        return;
    }

    if (state->replaceOnType)
    {
        state->text[0] = '\0';
        state->replaceOnType = 0;
    }

    length = strlen(state->text);

    if (length + 1U < sizeof(state->text))
    {
        state->text[length] = value;
        state->text[length + 1U] = '\0';
    }
}

static int TypedIDField(AppearanceField field, const char* label, int* value, int minimum, int maximum)
{
    char inputLabel[256];
    float buttonSize[2];
    IDInputState* state;
    int parsedValue;
    int namedValue;
    int changed;
    int digit;
    int letter;
    int length;
    int clicked;
    int shiftDown;
    int namedSupported;

    if (!value || field < 0 || field >= APPEARANCE_FIELD_COUNT)
    {
        return 0;
    }

    state = &g_idInputStates[field];
    namedSupported = FieldSupportsNamedID(field);

    if (!state->initialized)
    {
        SyncIDInput(field, *value);
    }

    snprintf(inputLabel, sizeof(inputLabel), "[%s%s]##%s_exact_id", g_activeIDInput == field ? "ID:> " : "ID: ", state->text[0] ? state->text : "_", label);
    buttonSize[0] = DEBUG_ID_INPUT_WIDTH;
    buttonSize[1] = 0.0f;
    clicked = g_imguiSelectable(inputLabel, g_activeIDInput == field, 0, buttonSize);

    if (clicked && g_activeIDInput != field)
    {
        g_activeIDInput = field;
        SyncIDInput(field, *value);
        state->replaceOnType = 1;
    }

    if (g_activeIDInput != field)
    {
        return 0;
    }

    if (g_keyPressed[KEY_ESCAPE])
    {
        SyncIDInput(field, *value);
        g_activeIDInput = APPEARANCE_FIELD_COUNT;
        return 0;
    }

    changed = 0;

    if (g_keyPressed[KEY_DELETE])
    {
        state->text[0] = '\0';
        state->replaceOnType = 0;
    }

    if (g_keyPressed[KEY_BACKSPACE])
    {
        length = (int)strlen(state->text);

        if (state->replaceOnType)
        {
            state->text[0] = '\0';
            state->replaceOnType = 0;
        }
        else if (length > 0)
        {
            state->text[length - 1] = '\0';
        }
    }

    shiftDown = (GetAsyncKeyState(KEY_SHIFT) & 0x8000) != 0;

    for (digit = 0; digit <= 9; ++digit)
    {
        if (g_keyPressed[KEY_NUMPAD_ZERO + digit])
        {
            AppendIDInputCharacter(state, (char)('0' + digit));
        }

        if (!g_keyPressed['0' + digit])
        {
            continue;
        }

        if (digit == 2 && shiftDown && namedSupported)
        {
            AppendIDInputCharacter(state, '@');
        }
        else
        {
            AppendIDInputCharacter(state, (char)('0' + digit));
        }
    }

    if (namedSupported)
    {
        for (letter = 'A'; letter <= 'Z'; ++letter)
        {
            if (g_keyPressed[letter])
            {
                if (state->replaceOnType)
                {
                    state->text[0] = '\0';
                    state->replaceOnType = 0;
                }

                if (!state->text[0])
                {
                    AppendIDInputCharacter(state, '@');
                }

                AppendIDInputCharacter(state, (char)('a' + (letter - 'A')));
            }
        }

        if (g_keyPressed[KEY_OEM_PERIOD] || g_keyPressed[KEY_DECIMAL])
        {
            AppendIDInputCharacter(state, '.');
        }

        if (g_keyPressed[KEY_SUBTRACT] || g_keyPressed[KEY_OEM_MINUS])
        {
            AppendIDInputCharacter(state, shiftDown ? '_' : '-');
        }
    }

    if (state->text[0] == '@' && IsCompleteNamedID(state->text) && ResolveNamedAppearanceID(field, state->text, &namedValue) && namedValue >= minimum && namedValue <= maximum)
    {
        if (*value != namedValue || strcmp(g_namedAppearanceIDs[field], state->text) != 0)
        {
            *value = namedValue;
            SetNamedAppearanceID(field, state->text);
            changed = 1;
        }
    }
    else if (ParseIDInput(state->text, minimum, maximum, &parsedValue) && (parsedValue != *value || g_namedAppearanceIDs[field][0]))
    {
        *value = parsedValue;
        SetNamedAppearanceID(field, NULL);
        changed = 1;
    }

    if (g_keyPressed[KEY_RETURN])
    {
        if (state->text[0] == '@' && (!IsCompleteNamedID(state->text) || !ResolveNamedAppearanceID(field, state->text, &namedValue) || namedValue < minimum || namedValue > maximum))
        {
            AddDebugMessage("Unknown, unavailable, or out-of-range named ID: %s", state->text);
        }
        else
        {
            SyncIDInput(field, *value);
            g_activeIDInput = APPEARANCE_FIELD_COUNT;
        }
    }

    return changed;
}

static void AlignCurrentLineToSliderFrame(void)
{
    uint8_t* context;
    uint8_t* window;
    float spacing;
    float upperPadding;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);
    window = GetCurrentImGuiWindow();

    if (!context || !window)
    {
        return;
    }

    spacing = *(float*)(context + IMGUI_STYLE_ITEM_SPACING_Y_OFFSET);
    upperPadding = (float)(int)(spacing * 0.5f);
    *(float*)(window + IMGUI_WINDOW_CURR_LINE_TEXT_BASE_OFFSET) = upperPadding;
}

static int CenteredSelectable(const char* label, bool selected, const float size[2])
{
    uint8_t* context;
    float previousAlignment[2];
    float* alignment;
    int clicked;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return g_imguiSelectable(label, selected, 0, size);
    }

    alignment = (float*)(context + IMGUI_STYLE_SELECTABLE_TEXT_ALIGN_OFFSET);
    previousAlignment[0] = alignment[0];
    previousAlignment[1] = alignment[1];
    alignment[0] = 0.5f;
    alignment[1] = 0.5f;
    clicked = g_imguiSelectable(label, selected, 0, size);
    alignment[0] = previousAlignment[0];
    alignment[1] = previousAlignment[1];

    return clicked;
}

static int ArrowButton(const char* label, int* value, int minimum, int maximum, int direction)
{
    char buttonLabel[96];
    float buttonSize[2];

    buttonSize[0] = DEBUG_ARROW_WIDTH;
    buttonSize[1] = SliderNavigationButtonHeight();
    AlignCurrentLineToSliderFrame();
    snprintf(buttonLabel, sizeof(buttonLabel), "%s##%s_%s", direction < 0 ? "<" : ">", label, direction < 0 ? "previous" : "next");

    if (!CenteredSelectable(buttonLabel, true, buttonSize))
    {
        return 0;
    }

    if (direction < 0 && *value > minimum)
    {
        --*value;
        return 1;
    }

    if (direction > 0 && *value < maximum)
    {
        ++*value;
        return 1;
    }

    return 0;
}

static int PaletteControl(const char* label, int* value, int minimum)
{
    char coarseLabel[160];
    char valueFormat[96];
    TimelineIDMap* map;
    int adjustedValue;
    int changed;
    int controlChanged;
    int height;
    int maximum;
    int preserveLoadedValue;
    int previousValue;
    int skipBlankRows;

    if (!value)
    {
        return 0;
    }

    height = GetRuntimePaletteHeight();
    maximum = height - 1;
    preserveLoadedValue = PreservePresetLoadedValue(APPEARANCE_FIELD_PALETTE, *value);
    skipBlankRows = g_skipBlankArt && RuntimePaletteInfoIsReady();
    map = maximum >= minimum ? GetPaletteIDMap(minimum, maximum, skipBlankRows) : NULL;
    snprintf(coarseLabel, sizeof(coarseLabel), "##%s_coarse", label);
    snprintf(valueFormat, sizeof(valueFormat), "%s: %%.0f", label);

    changed = 0;

    if (*value < minimum)
    {
        *value = minimum;
        changed = 1;
    }
    else if (*value > maximum)
    {
        *value = maximum;
        changed = 1;
    }

    if (!preserveLoadedValue && map && map->validTotal > 0)
    {
        adjustedValue = map->validIDs[FindTimelineMapIndex(map, *value)];

        if (adjustedValue != *value)
        {
            *value = adjustedValue;
            changed = 1;
        }
    }

    if (changed)
    {
        SetNamedAppearanceID(APPEARANCE_FIELD_PALETTE, NULL);
        SyncIDInput(APPEARANCE_FIELD_PALETTE, *value);
        DeactivateIDInput(APPEARANCE_FIELD_PALETTE, *value);
    }

    previousValue = *value;
    controlChanged = ArrowButton(label, value, minimum, maximum, -1);

    if (controlChanged && map && map->validTotal > 0)
    {
        int index;

        index = FindTimelineMapIndex(map, previousValue);

        if (index > 0)
        {
            *value = map->validIDs[index - 1];
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(APPEARANCE_FIELD_PALETTE, NULL);
        SyncIDInput(APPEARANCE_FIELD_PALETTE, *value);
        DeactivateIDInput(APPEARANCE_FIELD_PALETTE, *value);
        changed = 1;
    }

    g_imguiSameLine();
    SetNextItemWidth(DEBUG_SLIDER_WIDTH);

    previousValue = *value;

    if (preserveLoadedValue)
    {
        /* 
        * Display the preset's exact palette row even when it is blank or a
        * repeated row. If the user moves the slider, snap that interaction
        * back onto the user's normal skip-filtered palette map...
        */
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
    }
    else if (map && map->validTotal > 0)
    {
        controlChanged = TimelineMapSlider(coarseLabel, label, value, map);
    }
    else
    {
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
    }

    if (controlChanged && preserveLoadedValue && map && map->validTotal > 0)
    {
        int preferredDirection;

        preferredDirection = *value >= previousValue ? 1 : -1;
        adjustedValue = *value;

        if (FindTimelineMapIDForNavigation(map, *value, preferredDirection, &adjustedValue))
        {
            *value = adjustedValue;
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(APPEARANCE_FIELD_PALETTE, NULL);
        SyncIDInput(APPEARANCE_FIELD_PALETTE, *value);
        DeactivateIDInput(APPEARANCE_FIELD_PALETTE, *value);
        changed = 1;
    }

    g_imguiSameLine();
    previousValue = *value;
    controlChanged = ArrowButton(label, value, minimum, maximum, 1);

    if (controlChanged && map && map->validTotal > 0)
    {
        int index;

        index = FindTimelineMapIndex(map, previousValue);

        if (index + 1 < map->validTotal)
        {
            *value = map->validIDs[index + 1];
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(APPEARANCE_FIELD_PALETTE, NULL);
        SyncIDInput(APPEARANCE_FIELD_PALETTE, *value);
        DeactivateIDInput(APPEARANCE_FIELD_PALETTE, *value);
        changed = 1;
    }

    g_imguiSameLine();
    AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
    previousValue = *value;
    controlChanged = TypedIDField(APPEARANCE_FIELD_PALETTE, label, value, minimum, maximum);

    if (controlChanged && map && map->validTotal > 0)
    {
        adjustedValue = map->validIDs[FindTimelineMapIndex(map, *value)];

        if (adjustedValue != *value)
        {
            *value = adjustedValue;
            SetNamedAppearanceID(APPEARANCE_FIELD_PALETTE, NULL);
            SyncIDInput(APPEARANCE_FIELD_PALETTE, *value);
        }
    }

    changed |= controlChanged;

    if (changed)
    {
        g_presetSkipBlankActive[APPEARANCE_FIELD_PALETTE] = 0;
    }

    AddVerticalGap(DEBUG_CONTROL_ROW_GAP);
    return changed;
}

static int AppearanceControl(void* catVisual, AppearanceField field, const char* label, int* value, int minimum)
{
    char coarseLabel[160];
    char valueFormat[96];
    TimelineIDMap* timelineMap;
    int adjustedValue;
    int changed;
    int controlChanged;
    int hasLiveTimeline;
    int maximum;
    int preserveLoadedValue;
    int previousValue;
    int preferredDirection;
    int skipBlankFrames;
    int timelineMapBuilding;

    /*
    * A preset/.catstruct load replaces CatParts immediately, but the live
    * CatVisual/MovieClip graph is not rebuilt until the refresh at the end
    * of this editor frame.  Inspecting that OLD graph here can report the
    * previous texture extent and clamp a freshly loaded custom texture ID
    * before MCPF gets a chance to synchronize the new part timelines.
    *
    * While a full visual refresh is pending, preserve the exact stored ID
    * and defer all live-timeline range/blank-art validation until the next
    * frame, when the refreshed graph is authoritative.
    */
    if (g_timelineVisualNeedsRefresh)
    {
        maximum = APPEARANCE_MAX_ID;
        hasLiveTimeline = 0;
    }
    else
    {
        maximum = GetFieldTimelineExtent(catVisual, field);
        hasLiveTimeline = maximum >= minimum;

        if (!hasLiveTimeline)
        {
            maximum = APPEARANCE_MAX_ID;
        }
    }

    preserveLoadedValue = PreservePresetLoadedValue(field, *value);
    skipBlankFrames = g_skipBlankArt && hasLiveTimeline && FieldSkipsBlankFrames(field);
    timelineMap = NULL;

    snprintf(coarseLabel, sizeof(coarseLabel), "##%s_coarse", label);
    snprintf(valueFormat, sizeof(valueFormat), "%s: %%.0f", label);

    changed = 0;

    if (*value < minimum)
    {
        // All CatParts timeline IDs are one-based in the appearance data..
        *value = minimum;
        SetNamedAppearanceID(field, NULL);
        SyncIDInput(field, *value);
        DeactivateIDInput(field, *value);
        changed = 1;
    }
    else if (hasLiveTimeline && *value > maximum)
    {
        *value = maximum;
        SetNamedAppearanceID(field, NULL);
        SyncIDInput(field, *value);
        DeactivateIDInput(field, *value);
        changed = 1;
    }

    if (skipBlankFrames)
    {
        timelineMap = GetTimelineIDMap(catVisual, field, minimum, maximum);
    }

    timelineMapBuilding = skipBlankFrames && timelineMap && timelineMap->building;

    if (!preserveLoadedValue && skipBlankFrames && !timelineMapBuilding && timelineMap && timelineMap->validTotal > 0)
    {
        adjustedValue = timelineMap->validIDs[FindTimelineMapIndex(timelineMap, *value)];

        if (adjustedValue != *value)
        {
            *value = adjustedValue;
            SetNamedAppearanceID(field, NULL);
            SyncIDInput(field, *value);
            DeactivateIDInput(field, *value);
            changed = 1;
        }
    }
    else if (!preserveLoadedValue && skipBlankFrames && !timelineMapBuilding && (g_timelineValidatedVisual[field] != catVisual || g_timelineValidatedValue[field] != *value))
    {
        adjustedValue = *value;
        
        if (FindClosestTimelineID(catVisual, field, *value, 1, minimum, maximum, &adjustedValue) && adjustedValue != *value)
        {
            *value = adjustedValue;
            SetNamedAppearanceID(field, NULL);
            SyncIDInput(field, *value);
            DeactivateIDInput(field, *value);
            changed = 1;
        }
    }

    previousValue = *value;
    controlChanged = ArrowButton(label, value, minimum, maximum, -1);

    if (controlChanged && skipBlankFrames && !timelineMapBuilding)
    {
        adjustedValue = previousValue;

        if (timelineMap && timelineMap->validTotal > 0)
        {
            if (FindTimelineMapIDInDirection(timelineMap, previousValue, -1, &adjustedValue))
            {
                *value = adjustedValue;
            }
            else
            {
                *value = previousValue;
                controlChanged = 0;
            }
        }
        else if (FindTimelineIDInDirection(catVisual, field, *value, -1, minimum, maximum, &adjustedValue))
        {
            *value = adjustedValue;
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(field, NULL);
        SyncIDInput(field, *value);
        DeactivateIDInput(field, *value);
        changed = 1;
    }

    g_imguiSameLine();
    SetNextItemWidth(DEBUG_SLIDER_WIDTH);
    previousValue = *value;

    if (timelineMapBuilding)
    {
        snprintf(valueFormat, sizeof(valueFormat), "%s: %%.0f (indexing)", label);
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
    }
    else if (preserveLoadedValue)
    {
        /* 
        * Keep the preset's exact loaded frame visible even when that frame
        * is normally filtered. Any slider interaction is still projected
        * onto the active skip map immediately... 
        */
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
    }
    else if (skipBlankFrames && timelineMap && timelineMap->validTotal > 0)
    {
        controlChanged = TimelineMapSlider(coarseLabel, label, value, timelineMap);
    }
    else if (skipBlankFrames && timelineMap && timelineMap->validIDs)
    {
        int emptyTimelineIndex;

        emptyTimelineIndex = 0;
        snprintf(valueFormat, sizeof(valueFormat), "%s: no art", label);
        SliderInt(coarseLabel, &emptyTimelineIndex, 0, 0, valueFormat);
        controlChanged = 0;
    }
    else
    {
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
    }

    if (controlChanged && skipBlankFrames && !timelineMapBuilding && (preserveLoadedValue || !timelineMap || !timelineMap->validIDs))
    {
        preferredDirection = *value >= previousValue ? 1 : -1;
        adjustedValue = *value;

        if (timelineMap && timelineMap->validTotal > 0)
        {
            if (FindTimelineMapIDForNavigation(timelineMap, *value, preferredDirection, &adjustedValue))
            {
                *value = adjustedValue;
            }
            else
            {
                *value = previousValue;
                controlChanged = 0;
            }
        }
        else if (FindTimelineIDForNavigation(catVisual, field, *value, preferredDirection, minimum, maximum, &adjustedValue))
        {
            *value = adjustedValue;
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(field, NULL);
        SyncIDInput(field, *value);
        DeactivateIDInput(field, *value);
        changed = 1;
    }

    g_imguiSameLine();
    previousValue = *value;
    controlChanged = ArrowButton(label, value, minimum, maximum, 1);

    if (controlChanged && skipBlankFrames && !timelineMapBuilding)
    {
        adjustedValue = previousValue;

        if (timelineMap && timelineMap->validTotal > 0)
        {
            if (FindTimelineMapIDInDirection(timelineMap, previousValue, 1, &adjustedValue))
            {
                *value = adjustedValue;
            }
            else
            {
                *value = previousValue;
                controlChanged = 0;
            }
        }
        else if (FindTimelineIDInDirection(catVisual, field, *value, 1, minimum, maximum, &adjustedValue))
        {
            *value = adjustedValue;
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    if (controlChanged)
    {
        SetNamedAppearanceID(field, NULL);
        SyncIDInput(field, *value);
        DeactivateIDInput(field, *value);
        changed = 1;
    }

    g_imguiSameLine();
    AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
    previousValue = *value;

    /*
    * Timeline validation is allowed to
    * reject/adjust the numeric value, but it must NOT canonicalize an exact
    * named match back to its frame number...
    *
    * Preserve the old alias as well, so a rejected resolved ID can restore
    * the complete previous field state instead of leaving a stale new alias
    * attached to the previous numeric value...
    */
    char previousNamedID[NAMED_ID_BUFFER_SIZE];

    snprintf(previousNamedID, sizeof(previousNamedID), "%s", g_namedAppearanceIDs[field]);
    controlChanged = TypedIDField(field, label, value, minimum, maximum);

    if (controlChanged && skipBlankFrames && !timelineMapBuilding)
    {
        int resolvedValue;

        preferredDirection = *value >= previousValue ? 1 : -1;
        resolvedValue = *value;
        adjustedValue = resolvedValue;

        if (timelineMap && timelineMap->validTotal > 0)
        {
            if (FindTimelineMapIDForNavigation(timelineMap, resolvedValue, preferredDirection, &adjustedValue))
            {
                if (adjustedValue != resolvedValue)
                {
                    // Validation actually changed the selection, so the typed alias no longer identifies the selected ID...
                    *value = adjustedValue;
                    SetNamedAppearanceID(field, NULL);
                    SyncIDInput(field, *value);
                }

                // (Exact match: Keep g_namedAppearanceIDs[field] and the user's text untouched)...
            }
            else
            {
                *value = previousValue;
                SetNamedAppearanceID(field, previousNamedID[0] ? previousNamedID : NULL);
                SyncIDInput(field, *value);
                controlChanged = 0;
            }
        }
        else if (FindTimelineIDForNavigation(catVisual, field, resolvedValue, preferredDirection, minimum, maximum, &adjustedValue))
        {
            if (adjustedValue != resolvedValue)
            {
                *value = adjustedValue;
                SetNamedAppearanceID(field, NULL);
                SyncIDInput(field, *value);
            }

            // Exact match: Preserve the typed alias...
        }
        else
        {
            *value = previousValue;
            SetNamedAppearanceID(field, previousNamedID[0] ? previousNamedID : NULL);
            SyncIDInput(field, *value);
            controlChanged = 0;
        }
    }

    changed |= controlChanged;

    if (changed)
    {
        g_presetSkipBlankActive[field] = 0;
    }

    if (skipBlankFrames && !timelineMapBuilding)
    {
        g_timelineValidatedVisual[field] = catVisual;
        g_timelineValidatedValue[field] = *value;
    }

    AddVerticalGap(DEBUG_CONTROL_ROW_GAP);

    return changed;
}

static int MirrorSymmetryField(AppearanceField sourceField, int sourceValue, AppearanceField targetField, int* targetValue)
{
    const char* sourceNamedID;

    if (!targetValue || targetField < 0 || targetField >= APPEARANCE_FIELD_COUNT)
    {
        return 0;
    }

    sourceNamedID = g_namedAppearanceIDs[sourceField][0] ? g_namedAppearanceIDs[sourceField] : NULL;

    if (*targetValue == sourceValue && ((sourceNamedID && strcmp(g_namedAppearanceIDs[targetField], sourceNamedID) == 0) || (!sourceNamedID && g_namedAppearanceIDs[targetField][0] == '\0')))
    {
        return 0;
    }

    *targetValue = sourceValue;
    SetNamedAppearanceID(targetField, sourceNamedID);
    SyncIDInput(targetField, sourceValue);
    DeactivateIDInput(targetField, sourceValue);
    g_timelineValidatedVisual[targetField] = NULL;
    g_timelineValidatedValue[targetField] = 0;
    return 1;
}

static int ApplySymmetryGroups(uint8_t* parts)
{
    int changed;
    int legValue;
    int armValue;
    int eyeValue;
    int browValue;
    int earValue;

    if (!parts || !g_symmetryEnabled)
    {
        return 0;
    }

    changed = 0;

    // Keep left/right members of each limb family paired...
    legValue = *(int*)(parts + CATPART_LEG1_ID_OFFSET);
    changed |= MirrorSymmetryField(APPEARANCE_FIELD_LEG1, legValue, APPEARANCE_FIELD_LEG2, (int*)(parts + CATPART_LEG2_ID_OFFSET));

    armValue = *(int*)(parts + CATPART_ARM1_ID_OFFSET);
    changed |= MirrorSymmetryField(APPEARANCE_FIELD_ARM1, armValue, APPEARANCE_FIELD_ARM2, (int*)(parts + CATPART_ARM2_ID_OFFSET));

    eyeValue = *(int*)(parts + CATPART_LEFTEYE_ID_OFFSET);
    changed |= MirrorSymmetryField(APPEARANCE_FIELD_LEFTEYE, eyeValue, APPEARANCE_FIELD_RIGHTEYE, (int*)(parts + CATPART_RIGHTEYE_ID_OFFSET));

    browValue = *(int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET);
    changed |= MirrorSymmetryField(APPEARANCE_FIELD_LEFTEYEBROW, browValue, APPEARANCE_FIELD_RIGHTEYEBROW, (int*)(parts + CATPART_RIGHTEYEBROW_ID_OFFSET));

    earValue = *(int*)(parts + CATPART_LEFTEAR_ID_OFFSET);
    changed |= MirrorSymmetryField(APPEARANCE_FIELD_LEFTEAR, earValue, APPEARANCE_FIELD_RIGHTEAR, (int*)(parts + CATPART_RIGHTEAR_ID_OFFSET));

    return changed;
}

static int DrawSymmetryCheckbox(uint8_t* parts)
{
    bool enabled;
    int appearanceChanged;

    enabled = g_symmetryEnabled != 0;

    if (!g_imguiCheckbox("Symmetry##appearance_symmetry", &enabled))
    {
        return 0;
    }

    g_symmetryEnabled = enabled ? 1 : 0;
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    appearanceChanged = ApplySymmetryGroups(parts);
    AddDebugMessage("Appearance symmetry %s!", g_symmetryEnabled ? "enabled" : "disabled");
    return appearanceChanged;
}

static int VoiceDropdown(uint8_t* parts)
{
    MsvcString* voice;
    uint8_t* entry;
    uint8_t* end;
    const char* currentVoice;
    const char* voiceName;
    float automaticSize[2];
    int changed;
    int selected;

    if (!parts || !g_imguiBeginCombo)
    {
        return 0;
    }

    voice = (MsvcString*)(parts + CATPART_VOICE_OFFSET);
    currentVoice = GetMsvcStringText(voice);

    if (!currentVoice || !currentVoice[0])
    {
        currentVoice = "none";
    }

    SetNextItemWidth(DEBUG_VOICE_COMBO_WIDTH);

    if (!g_imguiBeginCombo("voice##cat_voice_type", currentVoice, 0))
    {
        return 0;
    }

    changed = 0;
    automaticSize[0] = 0.0f;
    automaticSize[1] = 0.0f;

    if (GetVoiceSetEntries(&entry, &end))
    {
        while (entry < end)
        {
            voiceName = GetMsvcStringText((const MsvcString*)(entry + GON_NODE_KEY_OFFSET));

            if (voiceName && voiceName[0])
            {
                selected = strcmp(currentVoice, voiceName) == 0;

                if (g_imguiSelectable(voiceName, selected != 0, 0, automaticSize) && !selected && SetMsvcString(voice, voiceName))
                {
                    currentVoice = GetMsvcStringText(voice);
                    changed = 1;
                }
            }

            entry += GON_NODE_SIZE;
        }
    }
    else
    {
        g_imguiSelectable("Voice list unavailable##cat_voice_list_unavailable", false, 0, automaticSize);
    }

    EndImGuiCombo();
    return changed;
}

static int ReadGonNumber(uint8_t* object, const char* name, int* outValue)
{
    uint8_t* field;

    if (!outValue)
    {
        return 0;
    }

    field = FindGonObjectChild(object, name);

    if (!field || *(const int*)(field + GON_NODE_TYPE_OFFSET) != GON_NODE_NUMBER_TYPE)
    {
        return 0;
    }

    *outValue = *(const int*)(field + GON_NODE_INT_OFFSET);

    return 1;
}

static int ReadGonDouble(uint8_t* object, const char* name, double* outValue)
{
    uint8_t* field;

    if (!outValue)
    {
        return 0;
    }

    field = FindGonObjectChild(object, name);

    if (!field || *(const int*)(field + GON_NODE_TYPE_OFFSET) != GON_NODE_NUMBER_TYPE)
    {
        return 0;
    }

    *outValue = *(const double*)(field + GON_NODE_FLOAT_OFFSET);
    return 1;
}

static int ReadGonString(uint8_t* object, const char* name, char* outValue, size_t outSize)
{
    uint8_t* field;
    const char* value;
    size_t length;

    if (!outValue || outSize == 0)
    {
        return 0;
    }

    field = FindGonObjectChild(object, name);

    if (!field || *(const int*)(field + GON_NODE_TYPE_OFFSET) != GON_NODE_STRING_TYPE)
    {
        return 0;
    }

    value = GetMsvcStringText((const MsvcString*)(field + GON_NODE_STRING_OFFSET));

    if (!value)
    {
        return 0;
    }

    length = strlen(value);

    if (length == 0 || length >= outSize)
    {
        return 0;
    }

    memcpy(outValue, value, length + 1U);

    return 1;
}

static int ReadGonAppearanceID(uint8_t* object, const char* name, AppearanceField field, int* outValue)
{
    char token[NAMED_ID_BUFFER_SIZE];
    int value;

    if (ReadGonNumber(object, name, &value))
    {
        *outValue = value;
        SetNamedAppearanceID(field, NULL);
        return 1;
    }

    if (!ReadGonString(object, name, token, sizeof(token)) || token[0] != '@' || !ResolveNamedAppearanceID(field, token, &value))
    {
        return 0;
    }

    *outValue = value;
    SetNamedAppearanceID(field, token);

    return 1;
}

static int LoadCustomCatPreset(uint8_t* parts, int* defaultFrame, uint8_t* preset, const char* presetName)
{
    AppearanceSnapshot appearance;
    AppearanceField appearanceField;
    int fieldValue;
    int loadedFields;
    unsigned int loadedAppearanceMask;
    double pitchValue;

    if (!parts || !defaultFrame || !preset || !presetName)
    {
        return 0;
    }

    ReadAppearance(parts, &appearance);
    ClearNamedAppearanceIDs();
    loadedFields = 0;
    loadedAppearanceMask = 0;
    *defaultFrame = APPEARANCE_DEFAULT_FRAME;

    if (ReadGonNumber(preset, "default_frame", &fieldValue))
    {
        fieldValue = ClampAppearanceID(fieldValue, 1);
        appearance.body = fieldValue;
        appearance.head = fieldValue;
        appearance.tail = fieldValue;
        appearance.leg1 = fieldValue;
        appearance.leg2 = fieldValue;
        appearance.arm1 = fieldValue;
        appearance.arm2 = fieldValue;
        appearance.lefteye = fieldValue;
        appearance.righteye = fieldValue;
        appearance.lefteyebrow = fieldValue;
        appearance.righteyebrow = fieldValue;
        appearance.leftear = fieldValue;
        appearance.rightear = fieldValue;
        appearance.mouth = fieldValue;

        for (appearanceField = APPEARANCE_FIELD_BODY; appearanceField < APPEARANCE_FIELD_COUNT; ++appearanceField)
        {
            loadedAppearanceMask |= 1U << (unsigned int)appearanceField;
        }

        ++loadedFields;
    }

    if (ReadGonNumber(preset, "claws", &fieldValue))
    {
        appearance.claws = ClampAppearanceID(fieldValue, CLAWS_ENABLED_VALUE);
        ++loadedFields;
    }

#define LOAD_GON_APPEARANCE_FIELD(name, member, field) \
    do \
    { \
        if (ReadGonAppearanceID(preset, name, field, &fieldValue)) \
        { \
            appearance.member = fieldValue; \
            loadedAppearanceMask |= 1U << (unsigned int)(field); \
            ++loadedFields; \
        } \
    } while (0)

    LOAD_GON_APPEARANCE_FIELD("texture", texture, APPEARANCE_FIELD_TEXTURE);
    LOAD_GON_APPEARANCE_FIELD("palette", palette, APPEARANCE_FIELD_PALETTE);
    LOAD_GON_APPEARANCE_FIELD("body", body, APPEARANCE_FIELD_BODY);
    LOAD_GON_APPEARANCE_FIELD("head", head, APPEARANCE_FIELD_HEAD);
    LOAD_GON_APPEARANCE_FIELD("tail", tail, APPEARANCE_FIELD_TAIL);
    LOAD_GON_APPEARANCE_FIELD("leg1", leg1, APPEARANCE_FIELD_LEG1);
    LOAD_GON_APPEARANCE_FIELD("leg2", leg2, APPEARANCE_FIELD_LEG2);
    LOAD_GON_APPEARANCE_FIELD("arm1", arm1, APPEARANCE_FIELD_ARM1);
    LOAD_GON_APPEARANCE_FIELD("arm2", arm2, APPEARANCE_FIELD_ARM2);
    LOAD_GON_APPEARANCE_FIELD("lefteye", lefteye, APPEARANCE_FIELD_LEFTEYE);
    LOAD_GON_APPEARANCE_FIELD("righteye", righteye, APPEARANCE_FIELD_RIGHTEYE);
    LOAD_GON_APPEARANCE_FIELD("lefteyebrow", lefteyebrow, APPEARANCE_FIELD_LEFTEYEBROW);
    LOAD_GON_APPEARANCE_FIELD("righteyebrow", righteyebrow, APPEARANCE_FIELD_RIGHTEYEBROW);
    LOAD_GON_APPEARANCE_FIELD("leftear", leftear, APPEARANCE_FIELD_LEFTEAR);
    LOAD_GON_APPEARANCE_FIELD("rightear", rightear, APPEARANCE_FIELD_RIGHTEAR);
    LOAD_GON_APPEARANCE_FIELD("mouth", mouth, APPEARANCE_FIELD_MOUTH);
#undef LOAD_GON_APPEARANCE_FIELD

    if (ReadGonString(preset, "voice", appearance.voice, sizeof(appearance.voice)))
    {
        ++loadedFields;
    }

    // Match the game's custom-cat loader when pitch is absent or nonnumeric..
    appearance.voicePitch = (double)VOICE_PITCH_DEFAULT;

    if (ReadGonDouble(preset, "pitch", &pitchValue))
    {
        appearance.voicePitch = ClampVoicePitch(pitchValue);
        ++loadedFields;
    }

    if (loadedFields == 0)
    {
        AddDebugMessage("Custom cat preset load failed: %s has no appearance fields!", presetName);
        return 0;
    }

    ApplyAppearance(parts, &appearance);

    /* 
    * Loading a preset: Preserve every appearance ID that
    * the preset explicitly supplied even if the user's skip filter would
    * normally classify it as blank/repeated. The checkbox itself stays
    * untouched, and any later edit immediately
    * resumes normal skip-filtered navigation...
    */
    PreservePresetLoadedAppearance(&appearance, loadedAppearanceMask);
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    g_activeAppearancePath[0] = '\0';
    snprintf(g_selectedPresetName, sizeof(g_selectedPresetName), "%s", presetName);
    AddDebugMessage("Loaded custom cat preset: %s", presetName);
    Log("Loaded %d fields from custom_cats.gon preset %s!", loadedFields, presetName);

    return 1;
}

static int CustomCatPresetDropdown(uint8_t* parts, int* defaultFrame)
{
    CustomCatPresetEntry* preset;
    uint8_t* presetNode;
    const char* preview;
    char itemLabel[256];
    char pageLabel[96];
    float automaticSize[2];
    float pageButtonSize[2];
    int changed;
    int endIndex;
    int index;
    int pageCount;
    int selected;
    int startIndex;

    if (!parts || !defaultFrame || !g_imguiBeginCombo)
    {
        return 0;
    }

    preview = g_selectedPresetName[0] ? g_selectedPresetName : "choose preset";
    SetNextItemWidth(DEBUG_PRESET_COMBO_WIDTH);

    if (!g_imguiBeginCombo("custom_cats.gon preset##custom_cat_preset", preview, 0))
    {
        return 0;
    }

    changed = 0;
    automaticSize[0] = 0.0f;
    automaticSize[1] = 0.0f;
    pageButtonSize[0] = 100.0f;
    pageButtonSize[1] = 0.0f;

    if (BuildCustomCatPresetCache())
    {
        pageCount = (g_customCatPresetCount + CUSTOM_CAT_PRESET_PAGE_SIZE - 1) / CUSTOM_CAT_PRESET_PAGE_SIZE;

        if (g_customCatPresetPage >= pageCount)
        {
            g_customCatPresetPage = pageCount - 1;
        }
        
        if (g_customCatPresetPage < 0)
        {
            g_customCatPresetPage = 0;
        }

        snprintf(pageLabel, sizeof(pageLabel), "Page %d / %d (%d presets)##custom_cat_preset_page", g_customCatPresetPage + 1, pageCount, g_customCatPresetCount);
        SelectableKeepPopupOpen(pageLabel, false, automaticSize);

        if (g_customCatPresetPage > 0 && SelectableKeepPopupOpen("[< PAGE]##custom_cat_preset_previous_page", false, pageButtonSize))
        {
            --g_customCatPresetPage;
        }

        if (g_customCatPresetPage + 1 < pageCount)
        {
            if (g_customCatPresetPage > 0)
            {
                g_imguiSameLine();
                AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
            }

            if (SelectableKeepPopupOpen("[PAGE >]##custom_cat_preset_next_page", false, pageButtonSize))
            {
                ++g_customCatPresetPage;
            }
        }

        startIndex = g_customCatPresetPage * CUSTOM_CAT_PRESET_PAGE_SIZE;
        endIndex = startIndex + CUSTOM_CAT_PRESET_PAGE_SIZE;

        if (endIndex > g_customCatPresetCount)
        {
            endIndex = g_customCatPresetCount;
        }

        for (index = startIndex; index < endIndex; ++index)
        {
            preset = &g_customCatPresets[index];
            selected = strcmp(g_selectedPresetName, preset->name) == 0;
            snprintf(itemLabel, sizeof(itemLabel), "%s##custom_cat_preset_%d", preset->name, index);

            if (g_imguiSelectable(itemLabel, selected != 0, 0, automaticSize))
            {
                presetNode = FindLoadedCustomCatPreset(preset->name);

                if (presetNode && LoadCustomCatPreset(parts, defaultFrame, presetNode, preset->name))
                {
                    changed = 1;
                }
                else if (!presetNode)
                {
                    AddDebugMessage("Preset unavailable in the internal registry: %s", preset->name);
                }
            }
        }

        for (index = endIndex; index < startIndex + CUSTOM_CAT_PRESET_PAGE_SIZE; ++index)
        {
            snprintf(itemLabel, sizeof(itemLabel), "##custom_cat_preset_padding_%d", index);
            SelectableKeepPopupOpen(itemLabel, false, automaticSize);
        }
    }
    else
    {
        g_imguiSelectable("custom_cats.gon unavailable##custom_cat_presets_unavailable", false, 0, automaticSize);
    }

    EndImGuiCombo();
    return changed;
}

static int VoicePitchControl(uint8_t* parts)
{
    float pitch;
    float minimum;
    float maximum;

    if (!parts)
    {
        return 0;
    }

    pitch = (float)*(double*)(parts + CATPART_VOICE_PITCH_OFFSET);
    minimum = VOICE_PITCH_MINIMUM;
    maximum = VOICE_PITCH_MAXIMUM;
    SetNextItemWidth(DEBUG_SLIDER_WIDTH);

    if (!g_imguiSliderScalar("##cat_voice_pitch", IMGUI_DATA_TYPE_FLOAT, &pitch, &minimum, &maximum, "pitch: %.3f", 0))
    {
        return 0;
    }

    if (pitch < minimum)
    {
        pitch = minimum;
    }
    else if (pitch > maximum)
    {
        pitch = maximum;
    }

    *(double*)(parts + CATPART_VOICE_PITCH_OFFSET) = (double)pitch;
    return 1;
}

static void NormalMeowButton(void* catVisual)
{
    MsvcString emotion;
    float buttonSize[2];

    AddVerticalGap(DEBUG_MEOW_BUTTON_TOP_GAP);
    buttonSize[0] = DEBUG_MEOW_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!CenteredSelectable("MEOW##test_normal_meow", true, buttonSize))
    {
        return;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;

    if (!catVisual || !g_playCatVoice)
    {
        AddDebugMessage("Meow test failed: Cat voice is unavailable!");
        return;
    }

    InitMsvcString(&emotion, "Normal");
    g_playCatVoice(catVisual, &emotion, false, 1.0, 1.0);
    AddDebugMessage("Played a normal meow!");
}

static int SectionHeader(AppearanceSection section, const char* title)
{
    char label[128];
    float automaticSize[2] = { 0.0f, 0.0f };
    bool isOpen;

    if (section < 0 || section >= APPEARANCE_SECTION_COUNT)
    {
        return 0;
    }

    isOpen = g_sectionOpen[section];
    snprintf(label, sizeof(label), "%s %s##appearance_section_%d", isOpen ? "[-]" : "[+]", title, (int)section);

    if (g_imguiSelectable(label, true, 0, automaticSize))
    {
        g_activeIDInput = APPEARANCE_FIELD_COUNT;
        isOpen = !isOpen;
        g_sectionOpen[section] = isOpen;
    }

    if (isOpen)
    {
        AddVerticalGap(DEBUG_SECTION_CONTENT_GAP);
    }

    return isOpen;
}

static int ExportButton(const uint8_t* parts)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_SAVE_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[SAVE]##export_appearance", true, 0, buttonSize))
    {
        return 0;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    return ExportAppearance(parts);
}

static int CopyButton(const uint8_t* parts)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_COPY_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[COPY DATA]##copy_appearance", true, 0, buttonSize))
    {
        return 0;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    return CopyAppearance(parts);
}

static int RandomizeButton(uint8_t* parts)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_RANDOM_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[RANDOMIZE]##randomize_appearance", true, 0, buttonSize))
    {
        return 0;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;

    if (!g_randomizeCatParts)
    {
        AddDebugMessage("Randomize failed: Game generator unavailable!");
        return 0;
    }

    g_randomizeCatParts(parts, 3);
    ClearPresetSkipBlankBypass();
    ClearNamedAppearanceIDs();
    g_selectedPresetName[0] = '\0';
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    AddDebugMessage("Randomized!");
    Log("Randomized the debug cat through CatParts::randomize(3)");
    return 1;
}

static int NewCatButton(uint8_t* parts)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_NEW_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[NEW CAT]##new_cat_appearance", true, 0, buttonSize))
    {
        return 0;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    g_activeAppearancePath[0] = '\0';

    if (!g_randomizeCatParts)
    {
        AddDebugMessage("New Cat failed: Game generator unavailable!");
        return 0;
    }

    g_randomizeCatParts(parts, 3);
    ClearPresetSkipBlankBypass();
    ClearNamedAppearanceIDs();
    g_selectedPresetName[0] = '\0';
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    AddDebugMessage("Created a new randomized cat!");
    Log("Started a new unsaved debug cat through CatParts::randomize(3)");

    return 1;
}

static void ScreenshotButton(void)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_SCREENSHOT_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[SAVE PNG]##capture_cat_png", true, 0, buttonSize))
    {
        return;
    }

    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    BeginNativeAlphaScreenshot();
}

static void ExitButton(void)
{
    float buttonSize[2];

    buttonSize[0] = DEBUG_EXIT_BUTTON_WIDTH;
    buttonSize[1] = 0.0f;

    if (!g_imguiSelectable("[EXIT GAME]##exit_game", true, 0, buttonSize))
    {
        return;
    }

    ExitProcess(0);
}

static void DrawDebugLog(void)
{
    char label[DEBUG_LOG_MESSAGE_LENGTH + 64];
    float automaticSize[2];
    int index;

    automaticSize[0] = 0.0f;
    automaticSize[1] = 0.0f;

    g_imguiSelectable("(DEBUG LOG)##appearance_debug_log_header", true, 0, automaticSize);

    if (g_debugMessageCount == 0)
    {
        g_imguiSelectable("Editor ready!##appearance_debug_log_empty", false, 0, automaticSize);
        return;
    }

    for (index = 0; index < g_debugMessageCount; ++index)
    {
        snprintf(label, sizeof(label), "%.*s##appearance_debug_log_%d", DEBUG_LOG_MESSAGE_LENGTH - 1, g_debugMessages[index], index);
        g_imguiSelectable(label, false, 0, automaticSize);
    }
}

static void DrawClawControl(uint8_t* parts)
{
    bool clawsEnabled;
    int clawsValue;

    if (!parts)
    {
        return;
    }

    clawsValue = *(const int*)(parts + CATPART_ARM1_CLAWS_OFFSET);
    clawsEnabled = clawsValue != CLAWS_DISABLED_VALUE;

    if (g_imguiCheckbox("Enable claws##claws_enabled", &clawsEnabled))
    {
        clawsValue = clawsEnabled ? CLAWS_ENABLED_VALUE : CLAWS_DISABLED_VALUE;
        *(int*)(parts + CATPART_ARM1_CLAWS_OFFSET) = clawsValue;
        *(int*)(parts + CATPART_ARM2_CLAWS_OFFSET) = clawsValue;
        AddDebugMessage("Claws %s (claws %d).", clawsEnabled ? "enabled" : "disabled", clawsValue);
    }
}

static void DrawSkipBlankArtCheckbox(void)
{
    bool skipBlankArt;

    skipBlankArt = g_skipBlankArt != 0;

    if (g_imguiCheckbox("Skip blank/repeated art##skip_blank_art", &skipBlankArt))
    {
        g_skipBlankArt = skipBlankArt ? 1 : 0;
        memset(g_timelineValidatedVisual, 0, sizeof(g_timelineValidatedVisual));
        memset(g_timelineValidatedValue, 0, sizeof(g_timelineValidatedValue));
        AddDebugMessage("Blank/repeated art skipping %s!", g_skipBlankArt ? "enabled" : "disabled");
    }
}

void ReadAppearance(const uint8_t* parts, AppearanceSnapshot* appearance)
{
    const char* voice;
    appearance->texture = *(const int*)(parts + CATPART_BODY_TEXTURE_OFFSET);
    appearance->claws = *(const int*)(parts + CATPART_ARM1_CLAWS_OFFSET);
    appearance->palette = *(const int*)(parts + CATPART_PALETTE_OFFSET);
    appearance->body = *(const int*)(parts + CATPART_BODY_ID_OFFSET);
    appearance->head = *(const int*)(parts + CATPART_HEAD_ID_OFFSET);
    appearance->tail = *(const int*)(parts + CATPART_TAIL_ID_OFFSET);
    appearance->leg1 = *(const int*)(parts + CATPART_LEG1_ID_OFFSET);
    appearance->leg2 = *(const int*)(parts + CATPART_LEG2_ID_OFFSET);
    appearance->arm1 = *(const int*)(parts + CATPART_ARM1_ID_OFFSET);
    appearance->arm2 = *(const int*)(parts + CATPART_ARM2_ID_OFFSET);
    appearance->lefteye = *(const int*)(parts + CATPART_LEFTEYE_ID_OFFSET);
    appearance->righteye = *(const int*)(parts + CATPART_RIGHTEYE_ID_OFFSET);
    appearance->lefteyebrow = *(const int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET);
    appearance->righteyebrow = *(const int*)(parts + CATPART_RIGHTEYEBROW_ID_OFFSET);
    appearance->leftear = *(const int*)(parts + CATPART_LEFTEAR_ID_OFFSET);
    appearance->rightear = *(const int*)(parts + CATPART_RIGHTEAR_ID_OFFSET);
    appearance->mouth = *(const int*)(parts + CATPART_MOUTH_ID_OFFSET);
    voice = GetMsvcStringText((const MsvcString*)(parts + CATPART_VOICE_OFFSET));
    snprintf(appearance->voice, sizeof(appearance->voice), "%s", voice && voice[0] ? voice : "none");
    appearance->voicePitch = *(const double*)(parts + CATPART_VOICE_PITCH_OFFSET);
}

int ClampAppearanceID(int value, int minimum)
{
    if (value < minimum)
    {
        return minimum;
    }

    return value;
}

double ClampVoicePitch(double value)
{
    if (value < (double)VOICE_PITCH_MINIMUM)
    {
        return (double)VOICE_PITCH_MINIMUM;
    }

    if (value > (double)VOICE_PITCH_MAXIMUM)
    {
        return (double)VOICE_PITCH_MAXIMUM;
    }

    return value;
}

void ApplyAppearance(uint8_t* parts, const AppearanceSnapshot* appearance)
{
    int claws;
    int texture;

    if (!parts || !appearance)
    {
        return;
    }

    claws = ClampAppearanceID(appearance->claws, CLAWS_ENABLED_VALUE);

    *(int*)(parts + CATPART_ARM1_CLAWS_OFFSET) = claws;
    *(int*)(parts + CATPART_ARM2_CLAWS_OFFSET) = claws;
    *(int*)(parts + CATPART_PALETTE_OFFSET) = ClampAppearanceID(appearance->palette, 0);
    *(int*)(parts + CATPART_BODY_ID_OFFSET) = ClampAppearanceID(appearance->body, 1);
    *(int*)(parts + CATPART_HEAD_ID_OFFSET) = ClampAppearanceID(appearance->head, 1);
    *(int*)(parts + CATPART_TAIL_ID_OFFSET) = ClampAppearanceID(appearance->tail, 1);
    *(int*)(parts + CATPART_LEG1_ID_OFFSET) = ClampAppearanceID(appearance->leg1, 1);
    *(int*)(parts + CATPART_LEG2_ID_OFFSET) = ClampAppearanceID(appearance->leg2, 1);
    *(int*)(parts + CATPART_ARM1_ID_OFFSET) = ClampAppearanceID(appearance->arm1, 1);
    *(int*)(parts + CATPART_ARM2_ID_OFFSET) = ClampAppearanceID(appearance->arm2, 1);
    *(int*)(parts + CATPART_LEFTEYE_ID_OFFSET) = ClampAppearanceID(appearance->lefteye, 1);
    *(int*)(parts + CATPART_RIGHTEYE_ID_OFFSET) = ClampAppearanceID(appearance->righteye, 1);
    *(int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET) = ClampAppearanceID(appearance->lefteyebrow, 1);
    *(int*)(parts + CATPART_RIGHTEYEBROW_ID_OFFSET) = ClampAppearanceID(appearance->righteyebrow, 1);
    *(int*)(parts + CATPART_LEFTEAR_ID_OFFSET) = ClampAppearanceID(appearance->leftear, 1);
    *(int*)(parts + CATPART_RIGHTEAR_ID_OFFSET) = ClampAppearanceID(appearance->rightear, 1);
    *(int*)(parts + CATPART_MOUTH_ID_OFFSET) = ClampAppearanceID(appearance->mouth, 1);

    /*
    * A full preset/save load replaces part IDs and texture IDs together.
    * CatParts::PrepareVisual is the game's normal post-load path: For each
    * part it seeks the newly selected part frame, resolves its live tex
    * child, then seeks that child to texture - 1. MCPF hooks this tex
    * lookup, so this ordering also guarantees a modded part's private
    * texture timeline is synchronized before the saved custom texture ID is applied...
    *
    * Do this here (full appearance loads only), not for every slider edit!
    */
    texture = ClampAppearanceID(appearance->texture, 1);
    SetAllPartTextures(parts, texture);

    if (appearance->voice[0])
    {
        SetMsvcString((MsvcString*)(parts + CATPART_VOICE_OFFSET), appearance->voice);
    }

    *(double*)(parts + CATPART_VOICE_PITCH_OFFSET) = ClampVoicePitch(appearance->voicePitch);

    if (g_prepareCatPartsVisual)
    {
        g_prepareCatPartsVisual(parts);
    }

    /* 
    * Timeline profiles are keyed by immutable SWF definition + extent, and
    * ID maps self-invalidate from their definition signature. Keep those
    * caches across appearance loads instead of forcing another
    * frame display-list scan after every preset/cat update...
    */
    memset(g_timelineValidatedVisual, 0, sizeof(g_timelineValidatedVisual));
    memset(g_timelineValidatedValue, 0, sizeof(g_timelineValidatedValue));
    g_timelineVisualNeedsRefresh = 1;
}

static void RefreshEditorCatVisual(void* catVisual, uint8_t* parts)
{
    if (!catVisual || !parts)
    {
        return;
    }

    if (!*(void**)((uint8_t*)catVisual + CAT_VISUAL_CAT_DATA_OFFSET))
    {
        g_prepareCatPartsVisual(parts);
    }

    g_refreshCatVisual(catVisual);

    /* 
    * CatVisual::Refresh can replace the live CatPartGraphics/MovieClip
    * instances. Synchronize MCPF textures on those new instances and
    * re-apply the current palette immediately...
    */
    SyncMcpfVisualTextureState(catVisual, *(int*)(parts + CATPART_BODY_TEXTURE_OFFSET));
    ApplyPaletteMaterial(catVisual, parts);
}

void HookSceneDraw(void* scene)
{
    void* customScene;
    void* catVisual;
    uint8_t* parts;
    int changed;
    int indexingReady;
    int indexingPercent;
    int texture;

    customScene = InterlockedCompareExchangePointer((PVOID volatile*)&g_customScene, NULL, NULL);
    
    if (scene != customScene)
    {
        g_originalSceneDraw(scene);
        return;
    }

    UpdateEditorInputState();
    BeginTimelineIndexingFrame();
    g_timelineVisualNeedsRefresh = 0;

    if (g_screenshotState != SCREENSHOT_STATE_IDLE)
    {
        /*
        * Keep the exact pose and mouse position stable for every pass. The
        * background passes disable only the cat clips, everything else is
        * captured again and cancelled mathematically from the final PNG...
        */
        PinScreenshotCatAnimations();
        ParkScreenshotCursor();

        if (g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_BLACK_FRAME || g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_WHITE_FRAME)
        {
            SetScreenshotCatVisible(0);
        }
        else
        {
            SetScreenshotCatVisible(1);
        }
    }

    /*
    * This callback runs after the scene draw and before presentation. Capture
    * the cat over black and white, then repeat both mattes with the cat hidden...
    * Comparing the visible pair to the background pair removes post-process
    * vignette, software cursor, and any other stable full-screen rendering...
    */
    if (g_screenshotState == SCREENSHOT_STATE_WAIT_VISIBLE_BLACK_FRAME)
    {
        int captureStatus;

        ++g_screenshotWaitFrames;

        if (g_screenshotWaitFrames >= SCREENSHOT_PASS_WAIT_FRAMES)
        {
            captureStatus = CaptureScreenshotBlackPass();

            if (captureStatus < 0)
            {
                RestoreScreenshotRenderState();
                return;
            }

            if (captureStatus > 0)
            {
                g_screenshotState = SCREENSHOT_STATE_WAIT_VISIBLE_WHITE_FRAME;
                g_screenshotWaitFrames = 0;

                if (g_screenshotOriginalClearColor)
                {
                    g_screenshotOriginalClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                }

                return;
            }

            if (g_screenshotWaitFrames >= SCREENSHOT_MAX_WAIT_FRAMES)
            {
                AddDebugMessage("Screenshot failed: Timed out waiting for the visible black pass!");
                RestoreScreenshotRenderState();
                return;
            }
        }

        return;
    }

    if (g_screenshotState == SCREENSHOT_STATE_WAIT_VISIBLE_WHITE_FRAME)
    {
        int captureStatus;

        ++g_screenshotWaitFrames;

        if (g_screenshotWaitFrames >= SCREENSHOT_PASS_WAIT_FRAMES)
        {
            captureStatus = CaptureScreenshotWhitePass();

            if (captureStatus < 0)
            {
                RestoreScreenshotRenderState();
                return;
            }

            if (captureStatus > 0)
            {
                SetScreenshotCatVisible(0);
                g_screenshotState = SCREENSHOT_STATE_WAIT_BACKGROUND_BLACK_FRAME;
                g_screenshotWaitFrames = 0;

                if (g_screenshotOriginalClearColor)
                {
                    g_screenshotOriginalClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                }

                return;
            }

            if (g_screenshotWaitFrames >= SCREENSHOT_MAX_WAIT_FRAMES)
            {
                AddDebugMessage("Screenshot failed: Timed out waiting for the visible white pass!");
                RestoreScreenshotRenderState();
                return;
            }

            if (g_screenshotOriginalClearColor)
            {
                g_screenshotOriginalClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        return;
    }

    if (g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_BLACK_FRAME)
    {
        int captureStatus;

        ++g_screenshotWaitFrames;

        if (g_screenshotWaitFrames >= SCREENSHOT_PASS_WAIT_FRAMES)
        {
            captureStatus = CaptureScreenshotBackgroundBlackPass();

            if (captureStatus < 0)
            {
                RestoreScreenshotRenderState();
                return;
            }

            if (captureStatus > 0)
            {
                g_screenshotState = SCREENSHOT_STATE_WAIT_BACKGROUND_WHITE_FRAME;
                g_screenshotWaitFrames = 0;

                if (g_screenshotOriginalClearColor)
                {
                    g_screenshotOriginalClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                }

                return;
            }

            if (g_screenshotWaitFrames >= SCREENSHOT_MAX_WAIT_FRAMES)
            {
                AddDebugMessage("Screenshot failed: Timed out waiting for the background black pass!");
                RestoreScreenshotRenderState();
                return;
            }
        }

        return;
    }

    if (g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_WHITE_FRAME)
    {
        int captureStatus;

        ++g_screenshotWaitFrames;

        if (g_screenshotWaitFrames >= SCREENSHOT_PASS_WAIT_FRAMES)
        {
            captureStatus = CaptureCatScreenshot();

            if (captureStatus != 0)
            {
                RestoreScreenshotRenderState();
                return;
            }

            if (g_screenshotWaitFrames >= SCREENSHOT_MAX_WAIT_FRAMES)
            {
                AddDebugMessage("Screenshot failed: Timed out waiting for the background white pass!");
                RestoreScreenshotRenderState();
                return;
            }

            if (g_screenshotOriginalClearColor)
            {
                g_screenshotOriginalClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        return;
    }

    SetLockedEditorWindowGeometry(!g_timelineInitialIndexingComplete);

    if (!g_editorWindowOpen)
    {
        return;
    }

    if (!g_imguiBegin("Catstructor Editor", &g_editorWindowOpen, IMGUI_WINDOW_FLAGS_NO_RESIZE | IMGUI_WINDOW_FLAGS_NO_MOVE | IMGUI_WINDOW_FLAGS_NO_COLLAPSE | IMGUI_WINDOW_FLAGS_NO_SAVED_SETTINGS))
    {
        g_activeIDInput = APPEARANCE_FIELD_COUNT;
        g_imguiEnd();
        return;
    }

    if (!g_editorWindowOpen)
    {
        g_activeIDInput = APPEARANCE_FIELD_COUNT;
        g_imguiEnd();
        return;
    }

    catVisual = NULL;
    parts = GetCatParts(scene, &catVisual);

    if (parts)
    {
        changed = 0;
        g_defaultFrame = APPEARANCE_DEFAULT_FRAME;

        /* 
        * Reset per-visit editor state and capture the scene's current cat
        * before the exhaustive scanner starts walking live part/texture
        * timelines. That same cat is restored when indexing completes...
        */
        PrepareFreshEditorEntryIfNeeded(catVisual, parts);

        if (!g_timelineInitialIndexingComplete)
        {
            /* 
            * Scene init normally captures this before the first draw. Keep a
            * defensive first-frame capture here as well so our indexing can never
            * start without a stable restoration target...
            */
            if (!catVisual || !CaptureInitialIndexingAppearance(parts))
            {
                DrawTimelineIndexingState(0);
                g_imguiEnd();
                return;
            }

            indexingPercent = 0;
            indexingReady = AdvanceInitialTimelineIndexes(catVisual, &indexingPercent);

            if (!indexingReady || g_timelineVisualNeedsRefresh)
            {
                if (g_timelineVisualNeedsRefresh && catVisual)
                {
                    RefreshEditorCatVisual(catVisual, parts);
                }

                DrawTimelineIndexingState(indexingPercent);
                g_imguiEnd();
                return;
            }

            if (!RestoreInitialStrayAfterIndexing(catVisual, parts))
            {
                AddDebugMessage("Initial stray restore failed: Starting appearance was not captured!");
                Log("Initial appearance indexing completed, but the indexed starting stray could not be restored");
                DrawTimelineIndexingState(100);
                g_imguiEnd();
                return;
            }

            g_timelineInitialIndexingComplete = 1;
            Log("Initial appearance indexing complete, including all discovered modded part texture definitions; automatic scanner stopped");

            DrawTimelineIndexingState(100);
            g_imguiEnd();
            return;
        }

        ExportButton(parts);
        g_imguiSameLine();
        AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
        changed |= NewCatButton(parts);
        g_imguiSameLine();
        AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
        changed |= RandomizeButton(parts);
        g_imguiSameLine();
        AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
        CopyButton(parts);
        AddVerticalGap(DEBUG_CONTROL_ROW_GAP);
        ScreenshotButton();
        g_imguiSameLine();
        AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
        ExitButton();
        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);
        changed |= CustomCatPresetDropdown(parts, &g_defaultFrame);

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);
        {
            int savedCatChanged;

            savedCatChanged = DrawSavedAppearanceFiles(parts, &g_defaultFrame);

            if (savedCatChanged)
            {
                // Saved .catstruct loads honor the user's active skip setting...
                ClearPresetSkipBlankBypass();
                changed = 1;
            }
        }

        /* 
        * Presets, NEW CAT and RANDOMIZE options can replace several CatParts values
        * at once. Reapply the active symmetry constraint before rendering
        * the grouped controls so symmetry remains a live editor invariant...
        */
        changed |= ApplySymmetryGroups(parts);
        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_COAT, "Coat & Color"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);
            texture = *(int*)(parts + CATPART_BODY_TEXTURE_OFFSET);

            if (AppearanceControl(catVisual, APPEARANCE_FIELD_TEXTURE, "texture", &texture, 1))
            {
                SetAllPartTextures(parts, texture);
                changed = 1;
            }

            changed |= PaletteControl("palette", (int*)(parts + CATPART_PALETTE_OFFSET), 0);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_BODY, "Body, Head & Tail"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_BODY, "body", (int*)(parts + CATPART_BODY_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_HEAD, "head", (int*)(parts + CATPART_HEAD_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_TAIL, "tail", (int*)(parts + CATPART_TAIL_ID_OFFSET), 1);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_LIMBS, "Legs & Arms"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);

            if (g_symmetryEnabled)
            {
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEG1, "legs", (int*)(parts + CATPART_LEG1_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_ARM1, "arms", (int*)(parts + CATPART_ARM1_ID_OFFSET), 1);
                changed |= ApplySymmetryGroups(parts);
            }
            else
            {
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEG1, "front leg", (int*)(parts + CATPART_LEG1_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEG2, "back leg", (int*)(parts + CATPART_LEG2_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_ARM1, "front arm", (int*)(parts + CATPART_ARM1_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_ARM2, "back arm", (int*)(parts + CATPART_ARM2_ID_OFFSET), 1);
            }

            DrawClawControl(parts);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_FACE, "Face & Ears"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);

            if (g_symmetryEnabled)
            {
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYE, "eyes", (int*)(parts + CATPART_LEFTEYE_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYEBROW, "brows", (int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEAR, "ears", (int*)(parts + CATPART_LEFTEAR_ID_OFFSET), 1);
                changed |= ApplySymmetryGroups(parts);
            }
            else
            {
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYE, "left eye", (int*)(parts + CATPART_LEFTEYE_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEYE, "right eye", (int*)(parts + CATPART_RIGHTEYE_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYEBROW, "left brow", (int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEYEBROW, "right brow", (int*)(parts + CATPART_RIGHTEYEBROW_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEAR, "left ear", (int*)(parts + CATPART_LEFTEAR_ID_OFFSET), 1);
                changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEAR, "right ear", (int*)(parts + CATPART_RIGHTEAR_ID_OFFSET), 1);
            }

            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_MOUTH, "mouth", (int*)(parts + CATPART_MOUTH_ID_OFFSET), 1);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_VOICE, "Voice & Pitch"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);
            changed |= VoiceDropdown(parts);
            changed |= VoicePitchControl(parts);
            NormalMeowButton(catVisual);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);
        DrawSkipBlankArtCheckbox();
        g_imguiSameLine();
        AddHorizontalGap(DEBUG_HORIZONTAL_GAP);
        changed |= DrawSymmetryCheckbox(parts);
        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if ((changed || g_timelineVisualNeedsRefresh) && catVisual)
        {
            RefreshEditorCatVisual(catVisual, parts);
        }

        if ((GetAsyncKeyState(VK_F6) & 1) != 0)
        {
            ExportAppearance(parts);
        }
    }
    else
    {
        if (InterlockedCompareExchange(&g_missingCatLogged, 1, 0) == 0)
        {
            AddDebugMessage("Cat visual unavailable, controls cannot be applied!");
            Log("CatAppearanceUI loaded, but its first cat visual was not available to the ImGui panel!");
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);
        DrawSkipBlankArtCheckbox();
    }

    AddVerticalGap(DEBUG_LOG_VERTICAL_GAP);
    DrawDebugLog();
    g_imguiEnd();
}