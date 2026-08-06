#include "Catstructor.h"

/* 
* Editor widgets, appearance controls, scene rendering stuff... 
*/

static void SetLockedEditorWindowGeometry(void)
{
    uint8_t* context;

    context = *(uint8_t**)(g_gameBase + RVA_IMGUI_CONTEXT_POINTER);

    if (!context)
    {
        return;
    }

    /* 
    * Keep the controls wholly inside the left half of a 1280x720 stage (FLA/SWF resolution)...
    */
    *(int*)(context + IMGUI_NEXT_WINDOW_FLAGS_OFFSET) |= IMGUI_NEXT_WINDOW_HAS_POS | IMGUI_NEXT_WINDOW_HAS_SIZE;
    *(float*)(context + IMGUI_NEXT_WINDOW_POS_OFFSET) = DEBUG_WINDOW_LOCKED_X;
    *(float*)(context + IMGUI_NEXT_WINDOW_POS_OFFSET + sizeof(float)) = DEBUG_WINDOW_LOCKED_Y;
    *(float*)(context + IMGUI_NEXT_WINDOW_SIZE_OFFSET) = DEBUG_WINDOW_LOCKED_WIDTH;
    *(float*)(context + IMGUI_NEXT_WINDOW_SIZE_OFFSET + sizeof(float)) = DEBUG_WINDOW_LOCKED_HEIGHT;
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
    int previousValue;
    int skipBlankRows;

    if (!value)
    {
        return 0;
    }

    height = GetRuntimePaletteHeight();
    maximum = height - 1;
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

    if (map && map->validTotal > 0)
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

    if (map && map->validTotal > 0)
    {
        controlChanged = TimelineMapSlider(coarseLabel, label, value, map);
    }
    else
    {
        controlChanged = SliderInt(coarseLabel, value, minimum, maximum, valueFormat);
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
    int previousValue;
    int preferredDirection;
    int skipBlankFrames;

    maximum = GetFieldTimelineExtent(catVisual, field);
    hasLiveTimeline = maximum >= minimum;

    if (!hasLiveTimeline)
    {
        maximum = APPEARANCE_MAX_ID;
    }

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

    if (skipBlankFrames && timelineMap && timelineMap->validTotal > 0)
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
    else if (skipBlankFrames && (g_timelineValidatedVisual[field] != catVisual || g_timelineValidatedValue[field] != *value))
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

    if (controlChanged && skipBlankFrames)
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

    if (skipBlankFrames && timelineMap && timelineMap->validTotal > 0)
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

    if (controlChanged && skipBlankFrames && (!timelineMap || !timelineMap->validIDs))
    {
        preferredDirection = *value >= previousValue ? 1 : -1;
        adjustedValue = *value;

        if (FindTimelineIDForNavigation(catVisual, field, *value, preferredDirection, minimum, maximum, &adjustedValue))
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

    if (controlChanged && skipBlankFrames)
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
    controlChanged = TypedIDField(field, label, value, minimum, maximum);

    if (controlChanged && skipBlankFrames)
    {
        preferredDirection = *value >= previousValue ? 1 : -1;
        adjustedValue = *value;

        if (timelineMap && timelineMap->validTotal > 0)
        {
            if (FindTimelineMapIDForNavigation(timelineMap, *value, preferredDirection, &adjustedValue))
            {
                *value = adjustedValue;
                SetNamedAppearanceID(field, NULL);
                SyncIDInput(field, *value);
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
            SetNamedAppearanceID(field, NULL);
            SyncIDInput(field, *value);
        }
        else
        {
            *value = previousValue;
            controlChanged = 0;
        }
    }

    changed |= controlChanged;

    if (skipBlankFrames)
    {
        g_timelineValidatedVisual[field] = catVisual;
        g_timelineValidatedValue[field] = *value;
    }

    AddVerticalGap(DEBUG_CONTROL_ROW_GAP);

    return changed;
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
    int fieldValue;
    int loadedFields;
    double pitchValue;

    if (!parts || !defaultFrame || !preset || !presetName)
    {
        return 0;
    }

    ReadAppearance(parts, &appearance);
    g_skipBlankArt = 0;
    ClearNamedAppearanceIDs();
    loadedFields = 0;
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

    texture = ClampAppearanceID(appearance->texture, 1);
    SetAllPartTextures(parts, texture);
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
    
    if (appearance->voice[0])
    {
        SetMsvcString((MsvcString*)(parts + CATPART_VOICE_OFFSET), appearance->voice);
    }
    
    *(double*)(parts + CATPART_VOICE_PITCH_OFFSET) = ClampVoicePitch(appearance->voicePitch);
}

void HookSceneDraw(void* scene)
{
    void* customScene;
    void* catVisual;
    uint8_t* parts;
    int changed;
    int texture;

    customScene = InterlockedCompareExchangePointer((PVOID volatile*)&g_customScene, NULL, NULL);
    
    if (scene != customScene)
    {
        g_originalSceneDraw(scene);
        return;
    }

    UpdateEditorInputState();
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

    SetLockedEditorWindowGeometry();

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
        changed |= DrawSavedAppearanceFiles(parts, &g_defaultFrame);
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
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEG1, "front leg", (int*)(parts + CATPART_LEG1_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEG2, "back leg", (int*)(parts + CATPART_LEG2_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_ARM1, "front arm", (int*)(parts + CATPART_ARM1_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_ARM2, "back arm", (int*)(parts + CATPART_ARM2_ID_OFFSET), 1);
            DrawClawControl(parts);
            AdjustSectionIndent(-DEBUG_SECTION_INDENT);
        }

        AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);

        if (SectionHeader(APPEARANCE_SECTION_FACE, "Face & Ears"))
        {
            AdjustSectionIndent(DEBUG_SECTION_INDENT);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYE, "left eye", (int*)(parts + CATPART_LEFTEYE_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEYE, "right eye", (int*)(parts + CATPART_RIGHTEYE_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEYEBROW, "left brow", (int*)(parts + CATPART_LEFTEYEBROW_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEYEBROW, "right brow", (int*)(parts + CATPART_RIGHTEYEBROW_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_LEFTEAR, "left ear", (int*)(parts + CATPART_LEFTEAR_ID_OFFSET), 1);
            changed |= AppearanceControl(catVisual, APPEARANCE_FIELD_RIGHTEAR, "right ear", (int*)(parts + CATPART_RIGHTEAR_ID_OFFSET), 1);
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

        if ((changed || g_timelineVisualNeedsRefresh) && catVisual)
        {
            if (!*(void**)((uint8_t*)catVisual + CAT_VISUAL_CAT_DATA_OFFSET))
            {
                g_prepareCatPartsVisual(parts);
            }

            g_refreshCatVisual(catVisual);
            ApplyPaletteMaterial(catVisual, parts);
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
    }

    AddVerticalGap(DEBUG_SECTION_VERTICAL_GAP);
    DrawSkipBlankArtCheckbox();
    AddVerticalGap(DEBUG_LOG_VERTICAL_GAP);
    DrawDebugLog();
    g_imguiEnd();
}