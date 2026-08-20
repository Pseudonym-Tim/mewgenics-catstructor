#include "Catstructor.h"

/*
* Runtime initialization, state, hooks, core stuff...
*/

static int FreezeScreenshotCatAnimations(void* scene);
static void RestoreScreenshotCatAnimations(void);

static MewjectorAPI g_mj;
UINT_PTR g_gameBase;
HMODULE g_module;

static fn_bind_singing_button g_originalBindSingingButton;
static fn_button_callback g_originalButtonCallback;
static fn_scene_init g_originalSceneInit;
static fn_scene_update g_originalSceneUpdate;
fn_scene_draw g_originalSceneDraw;

static fn_image_decode_from_memory g_originalImageDecodeFromMemory;
static fn_get_movieclip_child g_originalGetMovieClipChild;
fn_movieclip_get_child g_getMovieClipChild;
fn_movieclip_set_frame g_setMovieClipFrame;
fn_cat_visual_refresh g_refreshCatVisual;
fn_cat_parts_randomize g_randomizeCatParts;
fn_cat_parts_prepare_visual g_prepareCatPartsVisual;
fn_cat_visual_play_voice g_playCatVoice;
fn_material_set_int g_setMaterialInt;
static fn_gon_get_file g_getGonFile;
static fn_msvc_string_assign g_assignMsvcString;

fn_imgui_begin g_imguiBegin;
fn_imgui_end g_imguiEnd;
fn_imgui_same_line g_imguiSameLine;
fn_imgui_checkbox g_imguiCheckbox;
fn_imgui_slider_scalar g_imguiSliderScalar;
fn_imgui_selectable g_imguiSelectable;
fn_imgui_begin_combo g_imguiBeginCombo;
fn_imgui_end_combo g_imguiEndCombo;
static fn_resolve_palette_id g_resolvePaletteID;
static fn_resolve_cat_part_id g_resolveCatPartID;
static fn_sync_cat_texture_clip g_syncCatTextureClip;

static void* volatile g_customButtonCallback;
static void* volatile g_activeDemoMenu;
void* volatile g_customScene;
static volatile LONG g_customLaunchDepth;
static volatile LONG g_editorNeedsFreshCat;
volatile LONG g_missingCatLogged;
static volatile LONG g_installStarted;
static volatile LONG g_installed;
bool g_sectionOpen[APPEARANCE_SECTION_COUNT] = { true, false, false, false, false };
IDInputState g_idInputStates[APPEARANCE_FIELD_COUNT];
AppearanceField g_activeIDInput = APPEARANCE_FIELD_COUNT;
unsigned char g_keyWasDown[256];
unsigned char g_keyPressed[256];
char g_namedAppearanceIDs[APPEARANCE_FIELD_COUNT][NAMED_ID_BUFFER_SIZE];
char g_activeAppearancePath[800];
char g_selectedPresetName[NAMED_ID_BUFFER_SIZE];
CustomCatPresetEntry g_customCatPresets[CUSTOM_CAT_PRESET_MAX_COUNT];
int g_customCatPresetCount;
static int g_customCatPresetCacheState;
int g_customCatPresetPage;
TimelineProfile g_timelineProfiles[TIMELINE_PROFILE_CACHE_CAPACITY];
size_t g_timelineProfileUsed;
TimelineIDMap g_timelineIDMaps[APPEARANCE_FIELD_COUNT];
TimelineIDMap g_paletteIDMap;
unsigned char g_paletteBlankRows[APPEARANCE_MAX_ID + 1];
volatile LONG g_paletteInfoReady;
volatile LONG g_paletteInfoGeneration;
int g_paletteHeight;
int g_timelineVisualNeedsRefresh;
int g_timelineInitialIndexingComplete;
void* g_timelineValidatedVisual[APPEARANCE_FIELD_COUNT];
int g_timelineValidatedValue[APPEARANCE_FIELD_COUNT];
int g_defaultFrame = APPEARANCE_DEFAULT_FRAME;
int g_skipBlankArt = 1;
int g_symmetryEnabled;
bool g_editorWindowOpen = true;
float g_sliderNavigationHeight = DEBUG_ARROW_HEIGHT;
int g_screenshotState;
int g_screenshotWaitFrames;
static void* g_screenshotUIRoot;
static unsigned char g_screenshotUIRootFlags;
static GLfloat g_screenshotPreviousClearColor[4];
static PatchBackup g_screenshotClearColorPatch;
static POINT g_screenshotCursorPosition;
static int g_screenshotCursorPositionSaved;
fn_gl_clear_color g_screenshotOriginalClearColor;
uint8_t* g_screenshotVisibleBlackFrame;
uint8_t* g_screenshotVisibleWhiteFrame;
uint8_t* g_screenshotBackgroundBlackFrame;
int g_screenshotFrameWidth;
int g_screenshotFrameHeight;
static ScreenshotFrozenClip g_screenshotFrozenClips[SCREENSHOT_FROZEN_CLIP_CAPACITY];
static int g_screenshotFrozenClipCount;
char g_debugMessages[DEBUG_LOG_CAPACITY][DEBUG_LOG_MESSAGE_LENGTH];
int g_debugMessageCount;

static CRITICAL_SECTION g_patchLock;
static HANDLE g_timerQueue;
static HANDLE g_timer;

void Log(const char* format, ...)
{
    char buffer[768];
    va_list arguments;

    if (!g_mj.Log)
    {
        return;
    }

    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    buffer[sizeof(buffer) - 1] = '\0';
    g_mj.Log(MOD_NAME, "%s", buffer);
}

void AddDebugMessage(const char* format, ...)
{
    char* destination;
    va_list arguments;
    int index;

    if (g_debugMessageCount == DEBUG_LOG_CAPACITY)
    {
        for (index = 1; index < DEBUG_LOG_CAPACITY; ++index)
        {
            memcpy(g_debugMessages[index - 1], g_debugMessages[index], DEBUG_LOG_MESSAGE_LENGTH);
        }

        --g_debugMessageCount;
    }

    destination = g_debugMessages[g_debugMessageCount++];
    va_start(arguments, format);
    vsnprintf(destination, DEBUG_LOG_MESSAGE_LENGTH, format, arguments);
    va_end(arguments);
    destination[DEBUG_LOG_MESSAGE_LENGTH - 1] = '\0';
}

void InitMsvcString(MsvcString* value, const char* text)
{
    size_t length;

    memset(value, 0, sizeof(*value));
    length = strlen(text);

    if (length <= 15)
    {
        memcpy(value->storage.small, text, length);
        value->storage.small[length] = '\0';
        value->capacity = 15;
    }
    else
    {
        value->storage.heap = (char*)text;
        value->capacity = length;
    }

    value->length = length;
}

const char* GetMsvcStringText(const MsvcString* value)
{
    if (!value)
    {
        return NULL;
    }

    return value->capacity <= 15 ? value->storage.small : value->storage.heap;
}

static int CopyMsvcStringText(const MsvcString* value, char* output, size_t outputSize)
{
    const char* text;

    if (!value || !output || outputSize == 0 || value->length == 0 || value->length >= outputSize || value->capacity < value->length)
    {
        return 0;
    }

    text = GetMsvcStringText(value);

    if (!text)
    {
        return 0;
    }

    memcpy(output, text, value->length);
    output[value->length] = '\0';
    return 1;
}

int SetMsvcString(MsvcString* value, const char* text)
{
    size_t length;

    if (!value || !text || !g_assignMsvcString)
    {
        return 0;
    }

    length = strlen(text);

    if (length >= VOICE_NAME_BUFFER_SIZE)
    {
        return 0;
    }

    g_assignMsvcString(value, text, length);
    return 1;
}

int IsReadableMemoryRange(const void* address, size_t length)
{
    MEMORY_BASIC_INFORMATION memory;
    UINT_PTR begin;
    UINT_PTR end;
    UINT_PTR regionEnd;
    DWORD blockedProtection;

    if (!address || length == 0)
    {
        return 0;
    }

    begin = (UINT_PTR)address;
    end = begin + length;

    if (end < begin || VirtualQuery(address, &memory, sizeof(memory)) != sizeof(memory) || memory.State != MEM_COMMIT)
    {
        return 0;
    }

    blockedProtection = PAGE_GUARD | PAGE_NOACCESS;

    if ((memory.Protect & blockedProtection) != 0)
    {
        return 0;
    }

    regionEnd = (UINT_PTR)memory.BaseAddress + memory.RegionSize;
    return regionEnd >= (UINT_PTR)memory.BaseAddress && end <= regionEnd;
}

static int GetGonObjectEntries(uint8_t* node, uint8_t** outBegin, uint8_t** outEnd)
{
    uint8_t* begin;
    uint8_t* end;
    size_t byteCount;
    int type;

    if (outBegin)
    {
        *outBegin = NULL;
    }

    if (outEnd)
    {
        *outEnd = NULL;
    }

    if (!node || !outBegin || !outEnd || !IsReadableMemoryRange(node, GON_NODE_TYPE_OFFSET + sizeof(int)))
    {
        return 0;
    }

    type = *(const int*)(node + GON_NODE_TYPE_OFFSET);

    if (type != GON_NODE_OBJECT_TYPE && type != GON_NODE_ORDERED_OBJECT_TYPE)
    {
        return 0;
    }

    begin = *(uint8_t**)(node + GON_NODE_CHILDREN_BEGIN_OFFSET);
    end = *(uint8_t**)(node + GON_NODE_CHILDREN_END_OFFSET);

    if (!begin || !end || (UINT_PTR)end < (UINT_PTR)begin)
    {
        return 0;
    }

    byteCount = (size_t)((UINT_PTR)end - (UINT_PTR)begin);

    if (byteCount % GON_NODE_SIZE != 0 || byteCount / GON_NODE_SIZE > VOICE_SET_MAX_COUNT || (byteCount != 0 && !IsReadableMemoryRange(begin, byteCount)))
    {
        return 0;
    }

    *outBegin = begin;
    *outEnd = end;
    return 1;
}

uint8_t* FindGonObjectChild(uint8_t* object, const char* name)
{
    uint8_t* entry;
    uint8_t* end;
    const char* key;

    if (!name || !GetGonObjectEntries(object, &entry, &end))
    {
        return NULL;
    }

    while (entry < end)
    {
        key = GetMsvcStringText((const MsvcString*)(entry + GON_NODE_KEY_OFFSET));

        if (key && strcmp(key, name) == 0)
        {
            return entry;
        }

        entry += GON_NODE_SIZE;
    }

    return NULL;
}

int GetVoiceSetEntries(uint8_t** outBegin, uint8_t** outEnd)
{
    MsvcString path;
    uint8_t* catgen;
    uint8_t* voiceSets;

    if (outBegin)
    {
        *outBegin = NULL;
    }
    
    if (outEnd)
    {
        *outEnd = NULL;
    }

    if (!g_getGonFile || !outBegin || !outEnd)
    {
        return 0;
    }

    InitMsvcString(&path, "data/catgen.gon");
    catgen = (uint8_t*)g_getGonFile((void*)(g_gameBase + RVA_GON_FILE_REGISTRY), &path, false, true);
    voiceSets = FindGonObjectChild(catgen ? catgen + GON_FILE_ROOT_NODE_OFFSET : NULL, "voice_sets");
    return GetGonObjectEntries(voiceSets, outBegin, outEnd);
}

static int GetCustomCatEntries(uint8_t** outBegin, uint8_t** outEnd)
{
    uint8_t** catgenSlot;
    uint8_t* catgen;

    if (outBegin)
    {
        *outBegin = NULL;
    }

    if (outEnd)
    {
        *outEnd = NULL;
    }

    if (!outBegin || !outEnd)
    {
        return 0;
    }

    /*
    * CatGen loads and merges data/custom_cats.gon during game startup,
    * then deep-copies its root node into this object. The editor reads
    * that already-populated game-owned copy. Never call the GON file
    * loader from the ImGui draw callback.
    */
    catgenSlot = (uint8_t**)(g_gameBase + RVA_CATGEN_INTERNAL_POINTER);
    if (!IsReadableMemoryRange(catgenSlot, sizeof(*catgenSlot)))
    {
        return 0;
    }

    catgen = *catgenSlot;
    if (!catgen || !IsReadableMemoryRange(catgen + CATGEN_CUSTOM_CATS_OFFSET, GON_NODE_TYPE_OFFSET + sizeof(int)))
    {
        return 0;
    }

    return GetGonObjectEntries(catgen + CATGEN_CUSTOM_CATS_OFFSET, outBegin, outEnd);
}

int BuildCustomCatPresetCache(void)
{
    uint8_t* entry;
    uint8_t* end;
    int type;

    if (g_customCatPresetCacheState != 0)
    {
        return g_customCatPresetCacheState > 0;
    }

    g_customCatPresetCount = 0;
    g_customCatPresetPage = 0;

    if (!GetCustomCatEntries(&entry, &end))
    {
        return 0;
    }

    while (entry < end && g_customCatPresetCount < CUSTOM_CAT_PRESET_MAX_COUNT)
    {
        type = *(const int*)(entry + GON_NODE_TYPE_OFFSET);

        if ((type == GON_NODE_OBJECT_TYPE || type == GON_NODE_ORDERED_OBJECT_TYPE) && CopyMsvcStringText((const MsvcString*)(entry + GON_NODE_KEY_OFFSET), g_customCatPresets[g_customCatPresetCount].name, sizeof(g_customCatPresets[g_customCatPresetCount].name)))
        {
            ++g_customCatPresetCount;
        }

        entry += GON_NODE_SIZE;
    }

    g_customCatPresetCacheState = g_customCatPresetCount > 0 ? 1 : 0;
    Log("Cached %d custom_cats.gon preset names for editor!", g_customCatPresetCount);
    return g_customCatPresetCacheState > 0;
}

uint8_t* FindLoadedCustomCatPreset(const char* presetName)
{
    uint8_t* entry;
    uint8_t* end;
    char name[NAMED_ID_BUFFER_SIZE];
    int type;

    if (!presetName || !GetCustomCatEntries(&entry, &end))
    {
        return NULL;
    }

    while (entry < end)
    {
        type = *(const int*)(entry + GON_NODE_TYPE_OFFSET);

        if ((type == GON_NODE_OBJECT_TYPE || type == GON_NODE_ORDERED_OBJECT_TYPE) && CopyMsvcStringText((const MsvcString*)(entry + GON_NODE_KEY_OFFSET), name, sizeof(name)) && strcmp(name, presetName) == 0)
        {
            return entry;
        }

        entry += GON_NODE_SIZE;
    }

    return NULL;
}

static void RefreshCompatibilityResolvers(void)
{
    HMODULE module;

    if (!g_resolvePaletteID)
    {
        module = GetModuleHandleA("MewPaletteExtender.dll");

        if (module)
        {
            g_resolvePaletteID = (fn_resolve_palette_id)GetProcAddress(module, "MewPaletteExtender_ResolvePalette");
        }
    }

    if (!g_resolveCatPartID || !g_syncCatTextureClip)
    {
        module = GetModuleHandleA("MewCatPartFramework.dll");

        if (module)
        {
            if (!g_resolveCatPartID)
            {
                g_resolveCatPartID = (fn_resolve_cat_part_id)GetProcAddress(module, "MewCatPartFramework_ResolvePart");
            }
            if (!g_syncCatTextureClip)
            {
                g_syncCatTextureClip = (fn_sync_cat_texture_clip)GetProcAddress(module, "MewCatPartFramework_SyncTextureClip");
            }
        }
    }
}

const char* NamedKindForField(AppearanceField field)
{
    switch (field)
    {
        case APPEARANCE_FIELD_TEXTURE:
            return "texture";
        case APPEARANCE_FIELD_BODY:
            return "body";
        case APPEARANCE_FIELD_HEAD:
            return "head";
        case APPEARANCE_FIELD_TAIL:
            return "tail";
        case APPEARANCE_FIELD_LEG1:
        case APPEARANCE_FIELD_LEG2:
        case APPEARANCE_FIELD_ARM1:
        case APPEARANCE_FIELD_ARM2:
            return "leg";
        case APPEARANCE_FIELD_LEFTEYE:
        case APPEARANCE_FIELD_RIGHTEYE:
            return "eye";
        case APPEARANCE_FIELD_LEFTEYEBROW:
        case APPEARANCE_FIELD_RIGHTEYEBROW:
            return "eyebrow";
        case APPEARANCE_FIELD_LEFTEAR:
        case APPEARANCE_FIELD_RIGHTEAR:
            return "ear";
        case APPEARANCE_FIELD_MOUTH:
            return "mouth";
        default:
            return NULL;
    }
}

int FieldSupportsNamedID(AppearanceField field)
{
    RefreshCompatibilityResolvers();

    if (field == APPEARANCE_FIELD_PALETTE)
    {
        return g_resolvePaletteID != NULL;
    }

    return NamedKindForField(field) != NULL && g_resolveCatPartID != NULL;
}

int SyncMcpfTextureClip(const char* partKind, void* textureMovieClip)
{
    if (!partKind || !textureMovieClip)
    {
        return 0;
    }

    RefreshCompatibilityResolvers();
    return g_syncCatTextureClip ? g_syncCatTextureClip(partKind, textureMovieClip) : 0;
}

int ResolveNamedAppearanceID(AppearanceField field, const char* token, int* resolvedValue)
{
    int32_t value;
    const char* kind;

    if (!token || token[0] != '@' || !token[1] || !resolvedValue)
    {
        return 0;
    }

    RefreshCompatibilityResolvers();
    value = 0;

    if (field == APPEARANCE_FIELD_PALETTE)
    {
        if (!g_resolvePaletteID || !g_resolvePaletteID(token, &value))
        {
            return 0;
        }
    }
    else
    {
        kind = NamedKindForField(field);

        if (!kind || !g_resolveCatPartID || !g_resolveCatPartID(token, kind, &value))
        {
            return 0;
        }
    }

    *resolvedValue = (int)value;
    return 1;
}

void SetNamedAppearanceID(AppearanceField field, const char* token)
{
    if (field < 0 || field >= APPEARANCE_FIELD_COUNT)
    {
        return;
    }

    snprintf(g_namedAppearanceIDs[field], sizeof(g_namedAppearanceIDs[field]), "%s", token ? token : "");
}

void ClearNamedAppearanceIDs(void)
{
    memset(g_namedAppearanceIDs, 0, sizeof(g_namedAppearanceIDs));
}

static int ApplyPatch(PatchBackup* backup, UINT_PTR rva, const void* bytes, size_t length)
{
    DWORD ignored;

    if (!backup || !bytes || length == 0 || length > sizeof(backup->bytes))
    {
        return 0;
    }

    backup->address = (void*)(g_gameBase + rva);
    backup->length = length;
    backup->protection = 0;
    memcpy(backup->bytes, backup->address, length);

    if (!VirtualProtect(backup->address, length, PAGE_EXECUTE_READWRITE, &backup->protection))
    {
        return 0;
    }

    memcpy(backup->address, bytes, length);
    FlushInstructionCache(GetCurrentProcess(), backup->address, length);
    ignored = 0;
    VirtualProtect(backup->address, length, backup->protection, &ignored);

    return 1;
}

static void RestorePatch(PatchBackup* backup)
{
    DWORD writable;
    DWORD ignored;

    if (!backup || !backup->address || backup->length == 0)
    {
        return;
    }

    writable = 0;

    if (VirtualProtect(backup->address, backup->length, PAGE_EXECUTE_READWRITE, &writable))
    {
        memcpy(backup->address, backup->bytes, backup->length);
        FlushInstructionCache(GetCurrentProcess(), backup->address, backup->length);
        ignored = 0;
        VirtualProtect(backup->address, backup->length, writable, &ignored);
    }

    backup->address = NULL;
    backup->length = 0;
}

static void* ResolveRva(UINT_PTR rva)
{
    return (void*)(g_gameBase + rva);
}

static int PalettePixelIsMarker(const uint8_t* pixel, int channels)
{
    int matchesBgra;
    int matchesRgba;

    if (!pixel || channels < 3)
    {
        return 0;
    }

    // Reverse order BGRA check, sure, why not...
    matchesRgba = pixel[0] == 0xFFU && pixel[1] == 0x24U && pixel[2] == 0x24U;
    matchesBgra = pixel[0] == 0x24U && pixel[1] == 0x24U && pixel[2] == 0xFFU;

    return matchesRgba || matchesBgra;
}

static int PalettePixelIsSolidBlack(const uint8_t* pixel, int channels)
{
    if (!pixel || channels < 3)
    {
        return 0;
    }

    return pixel[0] == 0x00U && pixel[1] == 0x00U && pixel[2] == 0x00U;
}

static int PalettePixelIsTransparent(const uint8_t* pixel, int channels)
{
    if (!pixel || channels < 4)
    {
        return 0;
    }

    return pixel[3] == 0x00U;
}

static int PaletteRowMatchesPredicate(const uint8_t* pixels, int width, int channels, int row, int (*predicate)(const uint8_t*, int))
{
    int x;

    if (!pixels || !predicate || width <= 0 || row < 0)
    {
        return 0;
    }

    for (x = 0; x < width; ++x)
    {
        const uint8_t* pixel;

        pixel = pixels + (((size_t)row * (size_t)width + (size_t)x) * (size_t)channels);

        if (!predicate(pixel, channels))
        {
            return 0;
        }
    }

    return 1;
}

static int PaletteRowIsMarker(const uint8_t* pixels, int width, int channels, int row)
{
    return PaletteRowMatchesPredicate(pixels, width, channels, row, PalettePixelIsMarker);
}

static int PaletteRowIsSolidBlack(const uint8_t* pixels, int width, int channels, int row)
{
    return PaletteRowMatchesPredicate(pixels, width, channels, row, PalettePixelIsSolidBlack);
}

static int PaletteRowIsTransparent(const uint8_t* pixels, int width, int channels, int row)
{
    return PaletteRowMatchesPredicate(pixels, width, channels, row, PalettePixelIsTransparent);
}

static int PaletteRowIsBlank(const uint8_t* pixels, int width, int channels, int row)
{
    return PaletteRowIsMarker(pixels, width, channels, row) || PaletteRowIsSolidBlack(pixels, width, channels, row) || PaletteRowIsTransparent(pixels, width, channels, row);
}

static void CacheDecodedPaletteInfo(const uint8_t* pixels, int width, int height, int channels)
{
    int firstMarkerRow;
    int lastMarkerRow;
    int markerRowCount;
    int row;

    if (!pixels || width != PALETTE_TEXTURE_WIDTH || height < BASE_PALETTE_TEXTURE_ROWS || height > APPEARANCE_MAX_ID + 1 || channels < 3 || channels > 4)
    {
        return;
    }

    firstMarkerRow = -1;
    lastMarkerRow = -1;
    markerRowCount = 0;

    for (row = 0; row < height; ++row)
    {
        if (PaletteRowIsMarker(pixels, width, channels, row))
        {
            if (firstMarkerRow < 0)
            {
                firstMarkerRow = row;
            }

            lastMarkerRow = row;
            ++markerRowCount;
        }
    }

    if (markerRowCount < PALETTE_MARKER_MINIMUM_ROWS)
    {
        return;
    }

    EnterCriticalSection(&g_patchLock);
    InterlockedExchange(&g_paletteInfoReady, 0);
    memset(g_paletteBlankRows, 0, sizeof(g_paletteBlankRows));

    for (row = 0; row < height; ++row)
    {
        g_paletteBlankRows[row] = PaletteRowIsBlank(pixels, width, channels, row) ? 1U : 0U;
    }

    g_paletteHeight = height;
    MemoryBarrier();
    InterlockedIncrement(&g_paletteInfoGeneration);
    InterlockedExchange(&g_paletteInfoReady, 1);
    LeaveCriticalSection(&g_patchLock);
    Log("Captured palette.png: height=%d solid #ff2424 rows=%d range=%d-%d. Black and transparent rows are also treated as blank!", height, markerRowCount, firstMarkerRow, lastMarkerRow);
}

static uint8_t* __fastcall HookImageDecodeFromMemory(void* streamRange, int32_t* width, int32_t* height, int32_t* channels, int32_t requestedChannels)
{
    uint8_t* pixels;
    int actualChannels;
    int actualHeight;
    int actualWidth;
    int outputChannels;

    pixels = g_originalImageDecodeFromMemory ? g_originalImageDecodeFromMemory(streamRange, width, height, channels, requestedChannels) : NULL;

    if (!pixels)
    {
        return NULL;
    }

    actualWidth = width ? *width : 0;
    actualHeight = height ? *height : 0;
    actualChannels = channels ? *channels : 0;
    outputChannels = requestedChannels > 0 ? requestedChannels : actualChannels;
    CacheDecodedPaletteInfo(pixels, actualWidth, actualHeight, outputChannels);

    return pixels;
}

static void* HookGetMovieClipChild(void* movieClip, const void* childName)
{
    const char* childText;
    int isSingingTest;
    void* previousMenu;
    void* result;

    /*
    * Debug tools menu is destroyed and rebuilt when the user leaves it.
    * Refresh the parent on every native "singingtest" lookup...
    *
    * This hook is hot, so keep the non-matching path to one inline MSVC-string
    * view and the original lookup, no extra child searches are performed...
    */
    isSingingTest = childName && ((const MsvcString*)childName)->length == sizeof("singingtest") - 1;
    childText = isSingingTest ? GetMsvcStringText((const MsvcString*)childName) : NULL;
    isSingingTest = childText && memcmp(childText, "singingtest", sizeof("singingtest") - 1) == 0;
    result = g_originalGetMovieClipChild(movieClip, childName);

    if (isSingingTest)
    {
        previousMenu = InterlockedExchangePointer((PVOID volatile*)&g_activeDemoMenu, movieClip);
        
        if (previousMenu != movieClip)
        {
            Log("Captured active menu %p from native singingtest child %p", movieClip, result);
        }
    }

    return result;
}

static void* HookBindSingingButton(void* house, void** movieClip, void** owner)
{
    static const char replacementTitle[17] = "Cat Editor";
    void* result;
    void* customResult;
    void* callback;
    void* demoMenu;
    void* button;
    void* buttonArgument;
    void* ownerArgument;
    MsvcString buttonName;
    PatchBackup titlePatch;
    int titlePatched;

    /*
    * Bind the game's singingtest button exactly as normal. This hook
    * runs from inside the untouched native HousePlaceholder initializer,
    * after the stock menu MovieClip and preceding controls are ready...
    */
    InterlockedExchangePointer((PVOID volatile*)&g_customButtonCallback, NULL);
    result = g_originalBindSingingButton(house, movieClip, owner);

    demoMenu = InterlockedCompareExchangePointer((PVOID volatile*)&g_activeDemoMenu, NULL, NULL);

    if (!demoMenu)
    {
        Log("The active singingtest parent menu was not available while binding singingtest, catappearance was not bound!");
        return result;
    }

    InitMsvcString(&buttonName, "catappearance");
    button = g_originalGetMovieClipChild(demoMenu, &buttonName);

    if (!button)
    {
        Log("The active menu's catappearance child was not found!");
        return result;
    }

    buttonArgument = button;
    ownerArgument = house;
    memset(&titlePatch, 0, sizeof(titlePatch));
    EnterCriticalSection(&g_patchLock);
    titlePatched = ApplyPatch(&titlePatch, RVA_SINGING_BUTTON_TITLE_STRING, replacementTitle, sizeof(replacementTitle));

    if (!titlePatched)
    {
        Log("Could not patch the catappearance button title!");
    }

    customResult = g_originalBindSingingButton(house, &buttonArgument, &ownerArgument);

    if (titlePatched)
    {
        RestorePatch(&titlePatch);
    }

    LeaveCriticalSection(&g_patchLock);

    callback = NULL;

    if (customResult)
    {
        callback = *(void**)((uint8_t*)customResult + BUTTON_CALLBACK_OBJECT_OFFSET);
    }

    InterlockedExchangePointer((PVOID volatile*)&g_customButtonCallback, callback);
    Log("Bound active-menu catappearance and captured callback %p without detouring TestButton presentation setup!", callback);
    return result;
}

static void HookButtonCallback(void* callback)
{
    void* customCallback;
    int isCustom;

    customCallback = InterlockedCompareExchangePointer((PVOID volatile*)&g_customButtonCallback, NULL, NULL);
    isCustom = callback != NULL && callback == customCallback;

    if (isCustom)
    {
        InterlockedIncrement(&g_customLaunchDepth);
    }

    g_originalButtonCallback(callback);

    if (isCustom)
    {
        InterlockedDecrement(&g_customLaunchDepth);
    }
}

int PrepareFreshEditorEntryIfNeeded(void* catVisual, uint8_t* parts)
{
    if (InterlockedCompareExchange(&g_editorNeedsFreshCat, 0, 0) == 0)
    {
        return 0;
    }

    if (!catVisual || !parts)
    {
        return 0;
    }

    /* 
    * Custom editor scene can reuse transient state from the previous visit.
    * Capture the current generated stray before the exhaustive scanner
    * walks any live part/texture timelines, leave the CatParts
    * state in place while indexing runs. Snapshot is restored at the end...
    */
    if (!CaptureInitialIndexingAppearance(parts))
    {
        return 0;
    }

    ClearNamedAppearanceIDs();
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    g_activeAppearancePath[0] = '\0';
    g_selectedPresetName[0] = '\0';
    memset(g_timelineValidatedVisual, 0, sizeof(g_timelineValidatedVisual));
    memset(g_timelineValidatedValue, 0, sizeof(g_timelineValidatedValue));

    InterlockedExchange(&g_editorNeedsFreshCat, 0);
    Log("Prepared fresh editor entry, captured the starting stray for restoration after initial indexing");
    return 1;
}

static void HookSceneInit(void* scene)
{
    static const char replacementClass[16] = "CatAppearanceUI";
    unsigned char skipBindings[7];
    unsigned char oneCat;
    int32_t displacement;
    PatchBackup classPatch;
    PatchBackup bindingsPatch;
    PatchBackup loopPatch;
    void* catVisual;
    uint8_t* parts;

    if (InterlockedCompareExchange(&g_customLaunchDepth, 0, 0) == 0)
    {
        if (InterlockedCompareExchangePointer((PVOID volatile*)&g_customScene, NULL, NULL) == scene)
        {
            InterlockedExchangePointer((PVOID volatile*)&g_customScene, NULL);
        }

        g_originalSceneInit(scene);
        return;
    }

    memset(&classPatch, 0, sizeof(classPatch));
    memset(&bindingsPatch, 0, sizeof(bindingsPatch));
    memset(&loopPatch, 0, sizeof(loopPatch));

    skipBindings[0] = 0xE9;
    displacement = (int32_t)(RVA_SINGING_UI_BINDINGS_END - (RVA_SINGING_UI_BINDINGS_START + 5));
    memcpy(skipBindings + 1, &displacement, sizeof(displacement));
    skipBindings[5] = 0x90;
    skipBindings[6] = 0x90;
    oneCat = 1;

    InterlockedExchangePointer((PVOID volatile*)&g_customScene, scene);
    InterlockedExchange(&g_missingCatLogged, 0);
    g_activeAppearancePath[0] = '\0';
    g_selectedPresetName[0] = '\0';
    g_customCatPresetCount = 0;
    g_customCatPresetCacheState = 0;
    g_customCatPresetPage = 0;
    ClearTimelineProfiles();
    ClearTimelineIdMaps();
    memset(g_timelineValidatedVisual, 0, sizeof(g_timelineValidatedVisual));
    memset(g_timelineValidatedValue, 0, sizeof(g_timelineValidatedValue));
    ClearNamedAppearanceIDs();
    memset(g_idInputStates, 0, sizeof(g_idInputStates));
    memset(g_keyWasDown, 0, sizeof(g_keyWasDown));
    memset(g_keyPressed, 0, sizeof(g_keyPressed));
    g_activeIDInput = APPEARANCE_FIELD_COUNT;
    g_defaultFrame = APPEARANCE_DEFAULT_FRAME;
    g_timelineInitialIndexingComplete = 0;
    ResetInitialIndexingAppearanceSnapshot();
    g_editorWindowOpen = true;
    RestoreScreenshotRenderState();
    g_screenshotState = SCREENSHOT_STATE_IDLE;
    g_screenshotWaitFrames = 0;

    EnterCriticalSection(&g_patchLock);

    if (!ApplyPatch(&classPatch, RVA_SINGING_UI_CLASS_STRING, replacementClass, sizeof(replacementClass)) || !ApplyPatch(&bindingsPatch, RVA_SINGING_UI_BINDINGS_START, skipBindings, sizeof(skipBindings)) || !ApplyPatch(&loopPatch, RVA_SINGING_CAT_LOOP_LIMIT, &oneCat, sizeof(oneCat)))
    {
        RestorePatch(&loopPatch);
        RestorePatch(&bindingsPatch);
        RestorePatch(&classPatch);
        LeaveCriticalSection(&g_patchLock);
        InterlockedExchangePointer((PVOID volatile*)&g_customScene, NULL);
        InterlockedExchange(&g_editorNeedsFreshCat, 0);
        Log("Cat Appearance Debug scene patching failed, init was cancelled!");
        return;
    }

    g_originalSceneInit(scene);

    RestorePatch(&loopPatch);
    RestorePatch(&bindingsPatch);
    RestorePatch(&classPatch);
    LeaveCriticalSection(&g_patchLock);

    InterlockedExchange(&g_editorNeedsFreshCat, 1);
    catVisual = NULL;
    parts = GetCatParts(scene, &catVisual);

    if (!PrepareFreshEditorEntryIfNeeded(catVisual, parts))
    {
        Log("Initialized CatAppearanceUI, fresh editor-entry preparation will retry!");
    }
    else
    {
        Log("Initialized CatAppearanceUI, indexing will restore the same starting stray when complete!");
    }
}

static void HookSceneUpdate(void* scene)
{
    static const unsigned char skipRandomizeOpcode = 0xEB;
    static LONG unexpectedRandomizeBranchLogged;
    PatchBackup randomizeBranchPatch;
    unsigned char* randomizeBranch;
    int editingID;
    int branchPatched;

    /*
    * SingingCatTest::update checks SDL scancode 0x15 (R) and randomizes the
    * scene's cats...
    * 
    * While our ID editor owns keyboard input, we temporarily change only the JNE
    * opcode (75 -> EB), preserving the existing +0x66 displacement. This
    * makes the skip unconditional for exactly one update. Opcode
    * is restored before returning, so R retains its normal behavior whenever no ID field is active!
    */
    memset(&randomizeBranchPatch, 0, sizeof(randomizeBranchPatch));
    randomizeBranch = (unsigned char*)(g_gameBase + RVA_SINGING_R_RANDOMIZE_BRANCH);
    editingID = scene && InterlockedCompareExchangePointer((PVOID volatile*)&g_customScene, NULL, NULL) == scene && g_activeIDInput >= 0 && g_activeIDInput < APPEARANCE_FIELD_COUNT;
    branchPatched = 0;

    if (editingID && IsReadableMemoryRange(randomizeBranch, 2) && randomizeBranch[0] == 0x75 && randomizeBranch[1] == 0x66)
    {
        EnterCriticalSection(&g_patchLock);
        branchPatched = ApplyPatch(&randomizeBranchPatch, RVA_SINGING_R_RANDOMIZE_BRANCH, &skipRandomizeOpcode, sizeof(skipRandomizeOpcode));

        if (!branchPatched)
        {
            LeaveCriticalSection(&g_patchLock);
        }
    }
    else if (editingID && (!IsReadableMemoryRange(randomizeBranch, 2) || randomizeBranch[0] != 0xEB || randomizeBranch[1] != 0x66) && InterlockedCompareExchange(&unexpectedRandomizeBranchLogged, 1, 0) == 0)
    {
        Log("Could not suppress Singing Test R randomize!");
    }

    g_originalSceneUpdate(scene);

    if (branchPatched)
    {
        RestorePatch(&randomizeBranchPatch);
        LeaveCriticalSection(&g_patchLock);
    }
}

uint8_t* GetCatParts(void* scene, void** outCatVisual)
{
    void** begin;
    void* catVisual;
    void* catData;

    if (outCatVisual)
    {
        *outCatVisual = NULL;
    }
    if (!scene)
    {
        return NULL;
    }

    begin = *(void***)((uint8_t*)scene + SCENE_CAT_VECTOR_BEGIN_OFFSET);

    if (!begin)
    {
        return NULL;
    }

    catVisual = *begin;

    if (!catVisual)
    {
        return NULL;
    }

    catData = *(void**)((uint8_t*)catVisual + CAT_VISUAL_CAT_DATA_OFFSET);

    if (outCatVisual)
    {
        *outCatVisual = catVisual;
    }

    if (catData)
    {
        return (uint8_t*)catData + CAT_DATA_PARTS_OFFSET;
    }

    /*
    * Singing Test visuals are valid CatVisual objects, but don't own a CatData model. 
    * Live CatParts copy is embedded at CAT_VISUAL_EMBEDDED_PARTS_OFFSET...
    */
    return (uint8_t*)catVisual + CAT_VISUAL_EMBEDDED_PARTS_OFFSET;
}

static void APIENTRY HookScreenshotClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    fn_gl_clear_color original;

    original = g_screenshotOriginalClearColor;

    if (!original)
    {
        return;
    }

    if (g_screenshotState == SCREENSHOT_STATE_WAIT_VISIBLE_BLACK_FRAME || g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_BLACK_FRAME)
    {
        original(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    if (g_screenshotState == SCREENSHOT_STATE_WAIT_VISIBLE_WHITE_FRAME || g_screenshotState == SCREENSHOT_STATE_WAIT_BACKGROUND_WHITE_FRAME)
    {
        original(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    original(red, green, blue, alpha);
}

static int InstallScreenshotClearColorOverride(void)
{
    fn_gl_clear_color hookAddress;
    void* importedAddress;

    if (g_screenshotClearColorPatch.address)
    {
        return 1;
    }

    if (sizeof(hookAddress) != sizeof(importedAddress) || !IsReadableMemoryRange((void*)(g_gameBase + RVA_OPENGL_CLEAR_COLOR_IAT), sizeof(void*)))
    {
        return 0;
    }

    importedAddress = *(void**)(g_gameBase + RVA_OPENGL_CLEAR_COLOR_IAT);

    if (!importedAddress)
    {
        return 0;
    }

    memcpy(&g_screenshotOriginalClearColor, &importedAddress, sizeof(g_screenshotOriginalClearColor));
    hookAddress = HookScreenshotClearColor;

    if (!ApplyPatch(&g_screenshotClearColorPatch, RVA_OPENGL_CLEAR_COLOR_IAT, &hookAddress, sizeof(hookAddress)))
    {
        g_screenshotOriginalClearColor = NULL;
        return 0;
    }

    return 1;
}

void ParkScreenshotCursor(void)
{
    HDC deviceContext;
    HWND window;
    RECT windowRect;
    POINT parkedPosition;

    if (!g_screenshotCursorPositionSaved)
    {
        if (!GetCursorPos(&g_screenshotCursorPosition))
        {
            return;
        }

        g_screenshotCursorPositionSaved = 1;
    }

    deviceContext = wglGetCurrentDC();
    window = deviceContext ? WindowFromDC(deviceContext) : NULL;

    if (window && GetWindowRect(window, &windowRect))
    {
        parkedPosition.x = windowRect.right + 32;
        parkedPosition.y = windowRect.bottom + 32;
    }
    else
    {
        parkedPosition.x = -32000;
        parkedPosition.y = -32000;
    }

    SetCursorPos(parkedPosition.x, parkedPosition.y);
}

static void RestoreScreenshotCursor(void)
{
    if (!g_screenshotCursorPositionSaved)
    {
        return;
    }

    SetCursorPos(g_screenshotCursorPosition.x, g_screenshotCursorPosition.y);
    g_screenshotCursorPositionSaved = 0;
}

void RestoreScreenshotRenderState(void)
{
    g_screenshotState = SCREENSHOT_STATE_IDLE;

    if (g_screenshotUIRoot && IsReadableMemoryRange((uint8_t*)g_screenshotUIRoot + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET, sizeof(unsigned char)))
    {
        *((unsigned char*)g_screenshotUIRoot + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET) = g_screenshotUIRootFlags;
    }

    RestoreScreenshotCatAnimations();
    RestoreScreenshotCursor();

    if (g_screenshotOriginalClearColor)
    {
        g_screenshotOriginalClearColor(g_screenshotPreviousClearColor[0], g_screenshotPreviousClearColor[1], g_screenshotPreviousClearColor[2], g_screenshotPreviousClearColor[3]);
    }

    RestorePatch(&g_screenshotClearColorPatch);
    g_screenshotOriginalClearColor = NULL;

    free(g_screenshotVisibleBlackFrame);
    free(g_screenshotVisibleWhiteFrame);
    free(g_screenshotBackgroundBlackFrame);
    g_screenshotVisibleBlackFrame = NULL;
    g_screenshotVisibleWhiteFrame = NULL;
    g_screenshotBackgroundBlackFrame = NULL;
    g_screenshotFrameWidth = 0;
    g_screenshotFrameHeight = 0;
    g_screenshotWaitFrames = 0;
    g_screenshotUIRoot = NULL;
}

int BeginNativeAlphaScreenshot(void)
{
    void* scene;
    void* uiRoot;
    unsigned char* renderFlags;

    if (g_screenshotState != SCREENSHOT_STATE_IDLE)
    {
        AddDebugMessage("Screenshot capture is already in progress!");
        return 0;
    }

    scene = InterlockedCompareExchangePointer((PVOID volatile*)&g_customScene, NULL, NULL);

    if (!scene || !IsReadableMemoryRange((uint8_t*)scene + SCENE_UI_ROOT_OFFSET, sizeof(void*)))
    {
        AddDebugMessage("Screenshot failed: Editor scene is unavailable!");
        return 0;
    }

    uiRoot = *(void**)((uint8_t*)scene + SCENE_UI_ROOT_OFFSET);

    if (!uiRoot || !IsReadableMemoryRange((uint8_t*)uiRoot + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET, sizeof(unsigned char)))
    {
        AddDebugMessage("Screenshot failed: Scene background root was not found!");
        return 0;
    }

    if (!FreezeScreenshotCatAnimations(scene))
    {
        AddDebugMessage("Screenshot failed: Cat animation couldn't be paused!");
        return 0;
    }

    glGetFloatv(GL_COLOR_CLEAR_VALUE, g_screenshotPreviousClearColor);

    if (!InstallScreenshotClearColorOverride())
    {
        RestoreScreenshotCatAnimations();
        AddDebugMessage("Screenshot failed: Renderer clear color hook couldn't be installed!");
        return 0;
    }

    renderFlags = (unsigned char*)uiRoot + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET;
    g_screenshotUIRoot = uiRoot;
    g_screenshotUIRootFlags = *renderFlags;

    /* CatAppearanceUI owns the stage artwork, while the CatVisual is drawn by
    * the scene separately. Hide this UI root, then capture visible and cat-hidden
    * black/white pairs. The second pair cancels full-screen post-processing and
    * other stable rendering that cannot be disabled through this display root...
    */
    *renderFlags = (unsigned char)(*renderFlags & (unsigned char)~DISPLAY_OBJECT_ACTIVE_MASK);

    SetScreenshotCatVisible(1);
    ParkScreenshotCursor();
    g_screenshotState = SCREENSHOT_STATE_WAIT_VISIBLE_BLACK_FRAME;
    g_screenshotWaitFrames = 0;
    g_screenshotOriginalClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //AddDebugMessage("Capturing isolated visible/background passes for transparent PNG!");
    return 1;
}

static void FreezeScreenshotMovieClip(void* movieClip)
{
    unsigned char* renderFlags;
    unsigned char* stateFlags;
    int index;

    if (!movieClip || !IsReadableMemoryRange((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET, sizeof(int)) || !IsReadableMemoryRange((uint8_t*)movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET, sizeof(unsigned char)) || !IsReadableMemoryRange((uint8_t*)movieClip + MOVIECLIP_STATE_FLAGS_OFFSET, sizeof(unsigned char)))
    {
        return;
    }

    for (index = 0; index < g_screenshotFrozenClipCount; ++index)
    {
        if (g_screenshotFrozenClips[index].movieClip == movieClip)
        {
            return;
        }
    }

    if (g_screenshotFrozenClipCount >= SCREENSHOT_FROZEN_CLIP_CAPACITY)
    {
        return;
    }

    renderFlags = (unsigned char*)movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET;
    stateFlags = (unsigned char*)movieClip + MOVIECLIP_STATE_FLAGS_OFFSET;
    g_screenshotFrozenClips[g_screenshotFrozenClipCount].movieClip = movieClip;
    g_screenshotFrozenClips[g_screenshotFrozenClipCount].currentFrame = *(const int*)((uint8_t*)movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET);
    g_screenshotFrozenClips[g_screenshotFrozenClipCount].renderFlags = *renderFlags;
    g_screenshotFrozenClips[g_screenshotFrozenClipCount].stateFlags = *stateFlags;
    ++g_screenshotFrozenClipCount;
    *stateFlags &= (unsigned char)~MOVIECLIP_PLAYING_FLAG;
}

static int FreezeScreenshotCatAnimations(void* scene)
{
    static const size_t containerOffsets[] =
    {
        CAT_VISUAL_BODY_GRAPHICS_OFFSET, CAT_VISUAL_HEAD_GRAPHICS_OFFSET, CAT_VISUAL_TAIL_GRAPHICS_OFFSET, CAT_VISUAL_LEG1_GRAPHICS_OFFSET,
        CAT_VISUAL_LEG2_GRAPHICS_OFFSET, CAT_VISUAL_ARM1_GRAPHICS_OFFSET, CAT_VISUAL_ARM2_GRAPHICS_OFFSET, CAT_VISUAL_LEFTEYE_GRAPHICS_OFFSET,
        CAT_VISUAL_RIGHTEYE_GRAPHICS_OFFSET, CAT_VISUAL_LEFTEYEBROW_GRAPHICS_OFFSET, CAT_VISUAL_RIGHTEYEBROW_GRAPHICS_OFFSET,
        CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET, CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET, CAT_VISUAL_MOUTH_GRAPHICS_OFFSET
    };

    void* catVisual;
    uint8_t* entries;
    uint8_t* entry;
    size_t containerIndex;
    int entryIndex;
    int entryTotal;

    RestoreScreenshotCatAnimations();
    catVisual = NULL;

    if (!GetCatParts(scene, &catVisual) || !catVisual)
    {
        return 0;
    }

    for (containerIndex = 0; containerIndex < sizeof(containerOffsets) / sizeof(containerOffsets[0]); ++containerIndex)
    {
        if (!GetGraphicsEntries(catVisual, containerOffsets[containerIndex], &entries, &entryTotal))
        {
            continue;
        }

        for (entryIndex = 0; entryIndex < entryTotal; ++entryIndex)
        {
            entry = entries + (size_t)entryIndex * CATPART_GRAPHICS_ENTRY_SIZE;
            FreezeScreenshotMovieClip(*(void**)(entry + CATPART_GRAPHICS_MOVIECLIP_OFFSET));
            FreezeScreenshotMovieClip(*(void**)(entry + CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET));
            FreezeScreenshotMovieClip(*(void**)(entry + CATPART_GRAPHICS_SCARS_CLIP_OFFSET));
            FreezeScreenshotMovieClip(*(void**)(entry + CATPART_GRAPHICS_AUX_CLIP_OFFSET));
        }
    }

    PinScreenshotCatAnimations();
    return g_screenshotFrozenClipCount > 0;
}

void SetScreenshotCatVisible(int visible)
{
    unsigned char* renderFlags;
    int index;

    for (index = 0; index < g_screenshotFrozenClipCount; ++index)
    {
        if (!g_screenshotFrozenClips[index].movieClip || !IsReadableMemoryRange((uint8_t*)g_screenshotFrozenClips[index].movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET, sizeof(unsigned char)))
        {
            continue;
        }

        renderFlags = (unsigned char*)g_screenshotFrozenClips[index].movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET;

        if (visible)
        {
            *renderFlags = g_screenshotFrozenClips[index].renderFlags;
        }
        else
        {
            *renderFlags = (unsigned char)(g_screenshotFrozenClips[index].renderFlags & (unsigned char)~DISPLAY_OBJECT_ACTIVE_MASK);
        }
    }
}

void PinScreenshotCatAnimations(void)
{
    int index;

    for (index = 0; index < g_screenshotFrozenClipCount; ++index)
    {
        if (!g_screenshotFrozenClips[index].movieClip || !IsReadableMemoryRange((uint8_t*)g_screenshotFrozenClips[index].movieClip + MOVIECLIP_CURRENT_FRAME_OFFSET, sizeof(int)))
        {
            continue;
        }

        SetMovieClipFrameForInspection(g_screenshotFrozenClips[index].movieClip, g_screenshotFrozenClips[index].currentFrame);
    }
}

static void RestoreScreenshotCatAnimations(void)
{
    unsigned char* renderFlags;
    unsigned char* stateFlags;
    int index;

    for (index = 0; index < g_screenshotFrozenClipCount; ++index)
    {
        if (!g_screenshotFrozenClips[index].movieClip || !IsReadableMemoryRange((uint8_t*)g_screenshotFrozenClips[index].movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET, sizeof(unsigned char)) || !IsReadableMemoryRange((uint8_t*)g_screenshotFrozenClips[index].movieClip + MOVIECLIP_STATE_FLAGS_OFFSET, sizeof(unsigned char)))
        {
            continue;
        }

        renderFlags = (unsigned char*)g_screenshotFrozenClips[index].movieClip + DISPLAY_OBJECT_RENDER_FLAGS_OFFSET;
        stateFlags = (unsigned char*)g_screenshotFrozenClips[index].movieClip + MOVIECLIP_STATE_FLAGS_OFFSET;
        *renderFlags = g_screenshotFrozenClips[index].renderFlags;
        *stateFlags = g_screenshotFrozenClips[index].stateFlags;
    }
    
    memset(g_screenshotFrozenClips, 0, sizeof(g_screenshotFrozenClips));
    g_screenshotFrozenClipCount = 0;
}

static int InstallHookAtPriority(UINT_PTR rva, int stolenBytes, void* hook, void** original, int priority)
{
    return g_mj.InstallHook(rva, stolenBytes, hook, original, priority, MOD_NAME);
}

static int InstallHook(UINT_PTR rva, int stolenBytes, void* hook, void** original)
{
    return InstallHookAtPriority(rva, stolenBytes, hook, original, 20);
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4152)
#endif

static void InstallRuntime(void)
{
    void* trampoline;

    if (InterlockedCompareExchange(&g_installed, 0, 0) != 0)
    {
        return;
    }

    if (InterlockedCompareExchange(&g_installStarted, 1, 0) != 0)
    {
        return;
    }

    if (!g_mj.GetGameBase || !g_mj.InstallHook)
    {
        InterlockedExchange(&g_installStarted, 0);
        return;
    }

    g_gameBase = g_mj.GetGameBase();

    if (!g_gameBase)
    {
        InterlockedExchange(&g_installStarted, 0);
        return;
    }

    g_refreshCatVisual = (fn_cat_visual_refresh)ResolveRva(RVA_CAT_VISUAL_REFRESH);
    g_getMovieClipChild = (fn_movieclip_get_child)ResolveRva(RVA_MOVIECLIP_GET_CHILD);
    g_setMovieClipFrame = (fn_movieclip_set_frame)ResolveRva(RVA_MOVIECLIP_SET_FRAME);
    g_randomizeCatParts = (fn_cat_parts_randomize)ResolveRva(RVA_CAT_PARTS_RANDOMIZE);
    g_prepareCatPartsVisual = (fn_cat_parts_prepare_visual)ResolveRva(RVA_CAT_PARTS_PREPARE_VISUAL);
    g_playCatVoice = (fn_cat_visual_play_voice)ResolveRva(RVA_CAT_VISUAL_PLAY_VOICE);
    g_setMaterialInt = (fn_material_set_int)ResolveRva(RVA_MATERIAL_SET_INT);
    g_getGonFile = (fn_gon_get_file)ResolveRva(RVA_GON_GET_FILE);
    g_assignMsvcString = (fn_msvc_string_assign)ResolveRva(RVA_MSVC_STRING_ASSIGN);
    g_imguiBegin = (fn_imgui_begin)ResolveRva(RVA_IMGUI_BEGIN);
    g_imguiEnd = (fn_imgui_end)ResolveRva(RVA_IMGUI_END);
    g_imguiSameLine = (fn_imgui_same_line)ResolveRva(RVA_IMGUI_SAME_LINE);
    g_imguiCheckbox = (fn_imgui_checkbox)ResolveRva(RVA_IMGUI_CHECKBOX);
    g_imguiSliderScalar = (fn_imgui_slider_scalar)ResolveRva(RVA_IMGUI_SLIDER_SCALAR);
    g_imguiSelectable = (fn_imgui_selectable)ResolveRva(RVA_IMGUI_SELECTABLE);
    g_imguiBeginCombo = (fn_imgui_begin_combo)ResolveRva(RVA_IMGUI_BEGIN_COMBO);
    g_imguiEndCombo = (fn_imgui_end_combo)ResolveRva(RVA_IMGUI_END_COMBO);

    trampoline = NULL;

    /*
    * Always ask Mewjector to install the palette-inspection hook. The old safety flag was
    * never initialized, so this block was permanently skipped. Priority 5 keeps us before
    * MewPaletteExtender's priority-10 hook, so our trampoline returns the extender's final
    * resized palette and lets us cache both its real height and the blank marker rows.
    */
    if (InstallHookAtPriority(RVA_IMAGE_DECODE_FROM_MEMORY, IMAGE_DECODE_HOOK_STOLEN_BYTES, HookImageDecodeFromMemory, &trampoline, 5))
    {
        g_originalImageDecodeFromMemory = (fn_image_decode_from_memory)trampoline;
    }
    else
    {
        Log("Could not join the image-decoder hook chain, continuing with the core editor hooks!");
    }

    trampoline = NULL;

    if (!InstallHook(RVA_GET_MOVIECLIP_CHILD, GET_MOVIECLIP_CHILD_STOLEN_BYTES, HookGetMovieClipChild, &trampoline))
    {
        Log("Failed to observe the native singingtest child lookup!");
        return;
    }

    g_originalGetMovieClipChild = (fn_get_movieclip_child)trampoline;

    trampoline = NULL;

    if (!InstallHook(RVA_SINGING_BUTTON_CALLBACK, BUTTON_CALLBACK_STOLEN_BYTES, HookButtonCallback, &trampoline))
    {
        Log("Failed to hook the Singing Test button callback!");
        return;
    }

    g_originalButtonCallback = (fn_button_callback)trampoline;

    trampoline = NULL;

    if (!InstallHook(RVA_BIND_SINGING_TEST_BUTTON, BIND_SINGING_STOLEN_BYTES, HookBindSingingButton, &trampoline))
    {
        Log("Failed to hook the native singingtest button binder!");
        return;
    }

    g_originalBindSingingButton = (fn_bind_singing_button)trampoline;

    trampoline = NULL;

    if (!InstallHook(RVA_SINGING_SCENE_INIT, SINGING_INIT_STOLEN_BYTES, HookSceneInit, &trampoline))
    {
        Log("Failed to hook SingingCatTest::init!");
        return;
    }

    g_originalSceneInit = (fn_scene_init)trampoline;

    trampoline = NULL;

    if (!InstallHook(RVA_SINGING_SCENE_UPDATE, SINGING_UPDATE_STOLEN_BYTES, HookSceneUpdate, &trampoline))
    {
        Log("Failed to hook SingingCatTest::update for ID input shortcut suppression!");
        return;
    }

    g_originalSceneUpdate = (fn_scene_update)trampoline;

    trampoline = NULL;

    if (!InstallHook(RVA_SINGING_SCENE_DRAW_IMGUI, SINGING_DRAW_STOLEN_BYTES, HookSceneDraw, &trampoline))
    {
        Log("Failed to hook SingingCatTest's ImGui panel!");
        return;
    }
    
    g_originalSceneDraw = (fn_scene_draw)trampoline;

    InterlockedExchange(&g_installed, 1);
    Log("Installed, the active debug menu will be refreshed from each native singingtest child lookup!");
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static VOID CALLBACK BootstrapTimerProc(PVOID parameter, BOOLEAN timerOrWaitFired)
{
    (void)parameter;
    (void)timerOrWaitFired;

    if (!g_mj.GetGameBase)
    {
        MJ_Resolve(&g_mj);
    }

    InstallRuntime();
}

static void StartBootstrapTimer(void)
{
    g_timerQueue = CreateTimerQueue();

    if (!g_timerQueue)
    {
        return;
    }

    if (!CreateTimerQueueTimer(&g_timer, g_timerQueue, BootstrapTimerProc, NULL, BOOTSTRAP_INITIAL_DELAY_MS, BOOTSTRAP_POLL_INTERVAL_MS, WT_EXECUTEDEFAULT))
    {
        DeleteTimerQueue(g_timerQueue);
        g_timerQueue = NULL;
    }
}

static void StopBootstrapTimer(void)
{
    if (g_timerQueue)
    {
        DeleteTimerQueueEx(g_timerQueue, INVALID_HANDLE_VALUE);
        g_timerQueue = NULL;
        g_timer = NULL;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);
        InitializeCriticalSection(&g_patchLock);
        MJ_Resolve(&g_mj);
        StartBootstrapTimer();
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        StopBootstrapTimer();
        DeleteCriticalSection(&g_patchLock);
    }

    return TRUE;
}