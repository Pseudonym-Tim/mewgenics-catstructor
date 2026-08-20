#ifndef CAT_EDITOR_H
#define CAT_EDITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <GL/gl.h>

#define MOD_NAME "Catstructor"

#define RVA_SINGING_BUTTON_CALLBACK 0x0032D950 // Singing Test button callback...
#define RVA_BIND_SINGING_TEST_BUTTON 0x0031B300 // Native binder that hooks the Singing Test button to callback...
#define RVA_SINGING_SCENE_INIT 0x0079B2C0 // Singing Test scene setup, where custom editor scene gets attached without disrupting startup...
#define RVA_SINGING_SCENE_UPDATE 0x0079C540 // Singing Test per-frame update...
#define RVA_SINGING_R_RANDOMIZE_BRANCH 0x0079C662 // (For skipping the native SDL scancode R randomize block)...
#define RVA_SINGING_SCENE_DRAW_IMGUI 0x0079C8B0 // Singing Test ImGui draw pass (this is the safe spot to render the editor every frame)...

#define RVA_IMAGE_DECODE_FROM_MEMORY 0x00A729F0 // Game image decoder...
#define RVA_GET_MOVIECLIP_CHILD 0x0005A0B0 // Native child lookup used while the game builds MovieClip display trees, useful as hell for timeline inspection...
#define RVA_MOVIECLIP_GET_CHILD 0x00990550 // MovieClip::GetChild (used to grab live art clips instead of guessing from metadata)...
#define RVA_MOVIECLIP_SET_FRAME 0x0099EC40 // MovieClip frame setter, drives previews and blank-frame probing...
#define RVA_CAT_VISUAL_REFRESH 0x0073E8F0 // Cat visual refresh, called after edits or the UI changes...
#define RVA_CAT_PARTS_RANDOMIZE 0x007365E0 // Native cat-part randomizer...
#define RVA_CAT_PARTS_PREPARE_VISUAL 0x00738500 // Prepares CatParts for visual use after loading or replacing appearance values...
#define RVA_CAT_VISUAL_PLAY_VOICE 0x00743250 // Plays a cat voice line so the voice and pitch controls can be tested...
#define RVA_MATERIAL_SET_INT 0x009B0440 // Writes integer material parameters, (like the palette slot used by the live cat shader)...
#define RVA_GON_GET_FILE 0x00061150 // Loads a GON file through the game's registry...
#define RVA_MSVC_STRING_ASSIGN 0x00052080 // Assigns the game's string object without corrupting its small-string bookkeeping...

#define RVA_IMGUI_BEGIN 0x009DBCE0 // Native Begin used for the editor window...
#define RVA_IMGUI_END 0x009DE180 // Native End paired with the Begin above...
#define RVA_IMGUI_SAME_LINE 0x009E20C0 // Native SameLine used to keep compact controls on one row...
#define RVA_IMGUI_CHECKBOX 0x009EBC70 // Native Checkbox used for real boolean controls...
#define RVA_IMGUI_SLIDER_SCALAR 0x009ED2D0 // Native SliderScalar overload used by IDs and float voice pitch...
#define RVA_IMGUI_SELECTABLE 0x009F22B0 // Native Selectable used for list rows, buttons, etc...
#define RVA_IMGUI_BEGIN_COMBO 0x009ECA10 // Native BeginCombo ImGui dropdown behavior...
#define RVA_IMGUI_END_COMBO 0x009E2F30 // Native EndCombo closes the popup stack opened by BeginCombo...
#define RVA_IMGUI_CONTEXT_POINTER 0x013B86F0 // Pointer to the game's active ImGuiContext, required for width, cursor, and window-state helpers...

#define RVA_GON_FILE_REGISTRY 0x013BCF40 // Global GON registry pointer used by the game's file loader...
#define RVA_CATGEN_INTERNAL_POINTER 0x013BE710 // Internal cat-generator singleton used to enumerate custom cat presets...
#define RVA_SINGING_UI_CLASS_STRING 0x01112850 // Literal SingingTestUI class name used as a cheap useful anchor...
#define RVA_SINGING_BUTTON_TITLE_STRING 0x00F04DC0 // Native title string for the Singing Test button, another anchor...
#define RVA_SINGING_UI_BINDINGS_START 0x0079B7AC // Start of the Singing Test binding block we patch around...
#define RVA_SINGING_UI_BINDINGS_END 0x0079C2A8 // End of the same binding block, used to keep hook inside expected function...
#define RVA_SINGING_CAT_LOOP_LIMIT 0x0079C2AE // Immediate loop-limit byte for the Singing Test cat list...
#define RVA_SINGING_CALLBACK_VTABLE 0x00F05230 // Callback vtable used by the native button object we piggyback on...
#define RVA_OPENGL_CLEAR_COLOR_IAT 0x00E37820 // Imported glClearColor slot (temporarily redirected during deterministic two pass screenshot capture)...

#define IMAGE_DECODE_HOOK_STOLEN_BYTES 20
#define GET_MOVIECLIP_CHILD_STOLEN_BYTES 15
#define BIND_SINGING_STOLEN_BYTES 15
#define BUTTON_CALLBACK_STOLEN_BYTES 18
#define SINGING_INIT_STOLEN_BYTES 18
#define SINGING_UPDATE_STOLEN_BYTES 15
#define SINGING_DRAW_STOLEN_BYTES 21

#define BUTTON_CALLBACK_OBJECT_OFFSET 0x0F0

#define SCENE_UI_ROOT_OFFSET 0x0E8
#define SCENE_CAT_VECTOR_BEGIN_OFFSET 0x168
#define CAT_VISUAL_PALETTE_MATERIAL_OFFSET 0x160
#define CAT_VISUAL_EMBEDDED_PARTS_OFFSET 0x198
#define CAT_VISUAL_PROPERTIES_OFFSET 0x830
#define CAT_VISUAL_CAT_DATA_OFFSET 0x8A8
#define CAT_VISUAL_BODY_GRAPHICS_OFFSET 0x8B0
#define CAT_VISUAL_TAIL_GRAPHICS_OFFSET 0x8E0
#define CAT_VISUAL_ARM2_GRAPHICS_OFFSET 0x8F8
#define CAT_VISUAL_ARM1_GRAPHICS_OFFSET 0x910
#define CAT_VISUAL_LEG1_GRAPHICS_OFFSET 0x928
#define CAT_VISUAL_LEG2_GRAPHICS_OFFSET 0x940
#define CAT_VISUAL_HEAD_GRAPHICS_OFFSET 0x958
#define CAT_VISUAL_LEFTEAR_GRAPHICS_OFFSET 0x970
#define CAT_VISUAL_RIGHTEAR_GRAPHICS_OFFSET 0x988
#define CAT_VISUAL_LEFTEYE_GRAPHICS_OFFSET 0x9A0
#define CAT_VISUAL_RIGHTEYE_GRAPHICS_OFFSET 0x9B8
#define CAT_VISUAL_MOUTH_GRAPHICS_OFFSET 0x9D0
#define CAT_VISUAL_LEFTEYEBROW_GRAPHICS_OFFSET 0x9E8
#define CAT_VISUAL_RIGHTEYEBROW_GRAPHICS_OFFSET 0xA00
#define CAT_DATA_PARTS_OFFSET 0x060

#define CATPART_GRAPHICS_ENTRY_TOTAL_OFFSET 0x00C
#define CATPART_GRAPHICS_ENTRIES_OFFSET 0x010
#define CATPART_GRAPHICS_ENTRY_SIZE 0x030
#define CATPART_GRAPHICS_MOVIECLIP_OFFSET 0x000
#define CATPART_GRAPHICS_TEXTURE_CLIP_OFFSET 0x008
#define CATPART_GRAPHICS_SCARS_CLIP_OFFSET 0x010
#define CATPART_GRAPHICS_AUX_CLIP_OFFSET 0x018

#define MOVIECLIP_CHILD_CAPACITY_OFFSET 0x0A8
#define MOVIECLIP_CHILD_TOTAL_OFFSET 0x0AC
#define MOVIECLIP_CHILDREN_OFFSET 0x0B0
#define MOVIECLIP_INLINE_CHILD_CAPACITY 4
#define MOVIECLIP_DEFINITION_OFFSET 0x0D0
#define MOVIECLIP_CURRENT_FRAME_OFFSET 0x0D8
#define MOVIECLIP_STATE_FLAGS_OFFSET 0x009
#define MOVIECLIP_PLAYING_FLAG 0x002

#define DISPLAY_OBJECT_CHARACTER_ID_OFFSET 0x00C
#define DISPLAY_OBJECT_LIBRARY_ID_OFFSET 0x00E
#define DISPLAY_OBJECT_RENDER_FLAGS_OFFSET 0x008
#define DISPLAY_OBJECT_ACTIVE_MASK 0x060
#define DISPLAY_OBJECT_CLIP_DEPTH_OFFSET 0x018
#define DISPLAY_OBJECT_NAME_OFFSET 0x048

#define CATPART_PALETTE_OFFSET 0x01C
#define CATPART_BODY_ID_OFFSET 0x030
#define CATPART_BODY_TEXTURE_OFFSET 0x034
#define CATPART_HEAD_ID_OFFSET 0x084
#define CATPART_HEAD_TEXTURE_OFFSET 0x088
#define CATPART_TAIL_ID_OFFSET 0x0D8
#define CATPART_TAIL_TEXTURE_OFFSET 0x0DC
#define CATPART_LEG1_ID_OFFSET 0x12C
#define CATPART_LEG1_TEXTURE_OFFSET 0x130
#define CATPART_LEG2_ID_OFFSET 0x180
#define CATPART_LEG2_TEXTURE_OFFSET 0x184
#define CATPART_ARM1_ID_OFFSET 0x1D4
#define CATPART_ARM1_TEXTURE_OFFSET 0x1D8
#define CATPART_ARM1_CLAWS_OFFSET 0x1E0
#define CATPART_ARM2_ID_OFFSET 0x228
#define CATPART_ARM2_TEXTURE_OFFSET 0x22C
#define CATPART_ARM2_CLAWS_OFFSET 0x234
#define CATPART_LEFTEYE_ID_OFFSET 0x27C
#define CATPART_LEFTEYE_TEXTURE_OFFSET 0x280
#define CATPART_RIGHTEYE_ID_OFFSET 0x2D0
#define CATPART_RIGHTEYE_TEXTURE_OFFSET 0x2D4
#define CATPART_LEFTEYEBROW_ID_OFFSET 0x324
#define CATPART_LEFTEYEBROW_TEXTURE_OFFSET 0x328
#define CATPART_RIGHTEYEBROW_ID_OFFSET 0x378
#define CATPART_RIGHTEYEBROW_TEXTURE_OFFSET 0x37C
#define CATPART_LEFTEAR_ID_OFFSET 0x3CC
#define CATPART_LEFTEAR_TEXTURE_OFFSET 0x3D0
#define CATPART_RIGHTEAR_ID_OFFSET 0x420
#define CATPART_RIGHTEAR_TEXTURE_OFFSET 0x424
#define CATPART_MOUTH_ID_OFFSET 0x474
#define CATPART_MOUTH_TEXTURE_OFFSET 0x478
#define CATPART_VOICE_OFFSET 0x668
#define CATPART_VOICE_PITCH_OFFSET 0x688
#define GON_FILE_ROOT_NODE_OFFSET 0x028
#define CATGEN_CUSTOM_CATS_OFFSET 0x7C8
#define GON_NODE_CHILDREN_BEGIN_OFFSET 0x038
#define GON_NODE_CHILDREN_END_OFFSET 0x040
#define GON_NODE_KEY_OFFSET 0x088
#define GON_NODE_TYPE_OFFSET 0x0A8
#define GON_NODE_SIZE 0x0B0
#define GON_NODE_INT_OFFSET 0x050
#define GON_NODE_FLOAT_OFFSET 0x058
#define GON_NODE_STRING_OFFSET 0x068
#define GON_NODE_STRING_TYPE 1
#define GON_NODE_NUMBER_TYPE 2
#define GON_NODE_OBJECT_TYPE 3
#define GON_NODE_ORDERED_OBJECT_TYPE 4
#define IMGUI_DATA_TYPE_FLOAT 0
#define IMGUI_NEXT_WINDOW_HAS_POS 0x00000001
#define IMGUI_NEXT_WINDOW_HAS_SIZE 0x00000002
#define IMGUI_COND_ALWAYS 0x00000001
#define IMGUI_NEXT_WINDOW_FLAGS_OFFSET 0x00004A78
#define IMGUI_NEXT_WINDOW_POS_COND_OFFSET 0x00004A7C
#define IMGUI_NEXT_WINDOW_SIZE_COND_OFFSET 0x00004A80
#define IMGUI_NEXT_WINDOW_POS_OFFSET 0x00004A88
#define IMGUI_NEXT_WINDOW_SIZE_OFFSET 0x00004A98
#define IMGUI_WINDOW_FLAGS_NO_RESIZE 0x00000002
#define IMGUI_WINDOW_FLAGS_NO_MOVE 0x00000004
#define IMGUI_WINDOW_FLAGS_NO_COLLAPSE 0x00000020
#define IMGUI_WINDOW_FLAGS_NO_SAVED_SETTINGS 0x00000100
#define IMGUI_CURRENT_WINDOW_OFFSET 0x00004068
#define IMGUI_NEXT_ITEM_DATA_FLAGS_OFFSET 0x00004A00
#define IMGUI_NEXT_ITEM_ITEM_FLAGS_OFFSET 0x00004A04
#define IMGUI_NEXT_ITEM_WIDTH_OFFSET 0x00004A10
#define IMGUI_NEXT_ITEM_HAS_WIDTH 0x00000001
#define IMGUI_ITEM_SELECTABLE_DONT_CLOSE_POPUP 0x00000020
#define IMGUI_COMBO_DEPTH_OFFSET 0x00005E90
#define IMGUI_WINDOW_POS_X_OFFSET 0x00000028
#define IMGUI_WINDOW_CURSOR_POS_X_OFFSET 0x00000118
#define IMGUI_WINDOW_CURSOR_POS_Y_OFFSET 0x0000011C
#define IMGUI_WINDOW_CURR_LINE_SIZE_Y_OFFSET 0x00000144
#define IMGUI_LAST_ITEM_RECT_MIN_Y_OFFSET 0x00004A38
#define IMGUI_LAST_ITEM_RECT_MAX_Y_OFFSET 0x00004A40
#define IMGUI_STYLE_ITEM_SPACING_Y_OFFSET 0x00003948
#define IMGUI_WINDOW_CURR_LINE_TEXT_BASE_OFFSET 0x00000150
#define IMGUI_STYLE_SELECTABLE_TEXT_ALIGN_OFFSET 0x000039A8
#define IMGUI_WINDOW_INDENT_X_OFFSET 0x0000015C
#define IMGUI_WINDOW_COLUMNS_X_OFFSET 0x00000160
#define DEBUG_WINDOW_LOCKED_X 10.0f
#define DEBUG_WINDOW_LOCKED_Y 20.0f
#define DEBUG_WINDOW_LOCKED_WIDTH 620.0f
#define DEBUG_WINDOW_LOCKED_HEIGHT 680.0f
#define DEBUG_SECTION_INDENT 16.0f
#define DEBUG_SLIDER_WIDTH 230.0f
#define DEBUG_ARROW_WIDTH 32.0f
#define DEBUG_ARROW_HEIGHT 22.0f
#define DEBUG_ID_INPUT_WIDTH 168.0f
#define DEBUG_SAVE_BUTTON_WIDTH 92.0f
#define DEBUG_NEW_BUTTON_WIDTH 92.0f
#define DEBUG_RANDOM_BUTTON_WIDTH 112.0f
#define DEBUG_COPY_BUTTON_WIDTH 84.0f
#define DEBUG_SCREENSHOT_BUTTON_WIDTH 156.0f
#define DEBUG_EXIT_BUTTON_WIDTH 96.0f
#define DEBUG_FILE_NAME_WIDTH 232.0f
#define DEBUG_LOAD_BUTTON_WIDTH 72.0f
#define DEBUG_DELETE_BUTTON_WIDTH 82.0f
#define DEBUG_PRESET_COMBO_WIDTH 320.0f
#define DEBUG_HORIZONTAL_GAP 8.0f
#define DEBUG_CONTROL_ROW_GAP 3.0f
#define DEBUG_SECTION_CONTENT_GAP 4.0f
#define DEBUG_SECTION_VERTICAL_GAP 8.0f
#define DEBUG_FILE_ROW_VERTICAL_GAP 4.0f
#define DEBUG_LOG_VERTICAL_GAP 12.0f
#define DEBUG_VOICE_COMBO_WIDTH 230.0f
#define DEBUG_MEOW_BUTTON_WIDTH 112.0f
#define DEBUG_MEOW_BUTTON_TOP_GAP 8.0f
#define DEBUG_LOG_CAPACITY 5
#define DEBUG_LOG_MESSAGE_LENGTH 160
#define APPEARANCE_TEXT_BUFFER_SIZE 8192
#define APPEARANCE_MAX_ID 9999
#define PALETTE_TEXTURE_WIDTH 16
#define BASE_PALETTE_TEXTURE_ROWS 256
#define PALETTE_MARKER_MINIMUM_ROWS 8
#define TIMELINE_CONTAINER_CAPACITY 16
#define TIMELINE_PROFILE_CACHE_CAPACITY 1024
#define TIMELINE_INITIAL_TEXTURE_DISCOVERY_BUDGET_PER_FRAME 8
#define TIMELINE_PROFILE_PROBE_BUDGET_PER_FRAME 128
#define TIMELINE_BACKGROUND_REPEAT_LENGTH 8
#define VOICE_NAME_BUFFER_SIZE 128
#define NAMED_ID_BUFFER_SIZE 128
#define VOICE_SET_MAX_COUNT 512
#define CUSTOM_CAT_PRESET_MAX_COUNT 512
#define CUSTOM_CAT_PRESET_PAGE_SIZE 32
#define VOICE_PITCH_MINIMUM 0.5f
#define VOICE_PITCH_MAXIMUM 2.0f
#define VOICE_PITCH_DEFAULT 1.0
#define APPEARANCE_DEFAULT_FRAME 1000
#define SCREENSHOT_STATE_IDLE 0
#define SCREENSHOT_STATE_WAIT_VISIBLE_BLACK_FRAME 1
#define SCREENSHOT_STATE_WAIT_VISIBLE_WHITE_FRAME 2
#define SCREENSHOT_STATE_WAIT_BACKGROUND_BLACK_FRAME 3
#define SCREENSHOT_STATE_WAIT_BACKGROUND_WHITE_FRAME 4
#define SCREENSHOT_ALPHA_THRESHOLD 2
#define SCREENSHOT_PASS_WAIT_FRAMES 2
#define SCREENSHOT_MAX_WAIT_FRAMES 300
#define SCREENSHOT_FROZEN_CLIP_CAPACITY 256
#define SCREENSHOT_MINIMUM_MATTE_DELTA 24
#define SCREENSHOT_MINIMUM_MATTE_COVERAGE_PERCENT 5
#define SCREENSHOT_MINIMUM_TRANSMISSION_RANGE 4
#define SCREENSHOT_MINIMUM_COMPONENT_PIXELS 64
#define SCREENSHOT_PADDING 8
#define SCREENSHOT_MAX_DIMENSION 8192
#define SCREENSHOT_MAX_PIXELS 20000000U
#define CLAWS_ENABLED_VALUE 1
#define CLAWS_DISABLED_VALUE 2

#define KEY_BACKSPACE 0x08
#define KEY_RETURN 0x0D
#define KEY_ESCAPE 0x1B
#define KEY_DELETE 0x2E
#define KEY_SHIFT 0x10
#define KEY_NUMPAD_ZERO 0x60
#define KEY_DECIMAL 0x6E
#define KEY_SUBTRACT 0x6D
#define KEY_OEM_MINUS 0xBD
#define KEY_OEM_PERIOD 0xBE

#define BOOTSTRAP_INITIAL_DELAY_MS 0U
#define BOOTSTRAP_POLL_INTERVAL_MS 1000U

typedef void (__fastcall *fn_house_init)(void* house);
typedef void (__fastcall *fn_button_callback)(void* callback);
typedef void (__fastcall *fn_scene_init)(void* scene);
typedef void (__fastcall *fn_scene_update)(void* scene);
typedef void (__fastcall *fn_scene_draw)(void* scene);

typedef void* (__fastcall *fn_get_movieclip_child)(void* movieClip, const void* childName);
typedef void* (__fastcall *fn_movieclip_get_child)(void* movieClip, const void* childName);
typedef void (__fastcall *fn_movieclip_set_frame)(void* movieClip, int zeroBasedFrame);
typedef void (__fastcall *fn_movieclip_update)(void* movieClip);
typedef void* (__fastcall *fn_bind_singing_button)(void* house, void** movieClip, void** owner);
typedef void (__fastcall *fn_cat_visual_refresh)(void* catVisual);
typedef void (__fastcall *fn_cat_parts_randomize)(void* parts, int mode);
typedef void (__fastcall *fn_cat_parts_prepare_visual)(void* parts);
typedef void (__fastcall *fn_cat_visual_play_voice)(void* catVisual, const void* emotion, bool force, double volume, double pitchScale);
typedef void* (__fastcall *fn_gon_get_file)(void* registry, const void* path, bool reload, bool required);
typedef void (__fastcall *fn_msvc_string_assign)(void* destination, const char* text, size_t length);

typedef int (__fastcall *fn_imgui_begin)(const char* name, bool* open, int flags);
typedef void (__fastcall *fn_imgui_end)(void);
typedef void (__fastcall *fn_imgui_same_line)(void);
typedef bool (__fastcall *fn_imgui_checkbox)(const char* label, bool* value);
typedef int (__fastcall *fn_imgui_slider_scalar)(const char* label, int dataType, void* value, const void* minimum, const void* maximum, const char* format, int flags);
typedef int (__fastcall *fn_imgui_selectable)(const char* label, bool selected, int flags, const float* size);
typedef bool (__fastcall *fn_imgui_begin_combo)(const char* label, const char* preview, int flags);
typedef void (__fastcall *fn_imgui_end_combo)(void);

#define MJ_API_VERSION 3

typedef int (__cdecl *MJ_fn_InstallHook)(UINT_PTR rva, int stolenBytes, void* hookFn, void** outTrampoline, int priority, const char* owner);
typedef int (__cdecl *MJ_fn_QueryHook)(UINT_PTR rva);
typedef UINT_PTR (__cdecl *MJ_fn_AllocTypeIdPair)(const char* owner);
typedef int (__cdecl *MJ_fn_RegisterName)(const char* category, const char* name, const char* owner);
typedef const char* (__cdecl *MJ_fn_LookupName)(const char* category, const char* name);
typedef UINT_PTR (__cdecl *MJ_fn_GetGameBase)(void);
typedef void (__cdecl *MJ_fn_Log)(const char* owner, const char* format, ...);
typedef int (__cdecl *MJ_fn_VerifyHooks)(void);
typedef int (__cdecl *MJ_fn_GetVersion)(void);

typedef struct MewjectorAPI
{
    MJ_fn_InstallHook InstallHook;
    MJ_fn_QueryHook QueryHook;
    MJ_fn_AllocTypeIdPair AllocTypeIdPair;
    MJ_fn_RegisterName RegisterName;
    MJ_fn_LookupName LookupName;
    MJ_fn_GetGameBase GetGameBase;
    MJ_fn_Log Log;
    MJ_fn_VerifyHooks VerifyHooks;
    MJ_fn_GetVersion GetVersion;
} MewjectorAPI;

static inline int MJ_Resolve(MewjectorAPI* api)
{
    HMODULE module;

    if (!api)
    {
        return 0;
    }

    memset(api, 0, sizeof(*api));
    module = GetModuleHandleA("version.dll");

    if (!module)
    {
        return 0;
    }

    api->GetVersion = (MJ_fn_GetVersion)GetProcAddress(module, "MJ_GetVersion");

    if (!api->GetVersion || api->GetVersion() < MJ_API_VERSION)
    {
        memset(api, 0, sizeof(*api));
        return 0;
    }

#define MJ_RESOLVE(field, exportName) \
    do \
    { \
        api->field = (MJ_fn_##field)GetProcAddress(module, "MJ_" exportName); \
        if (!api->field) \
        { \
            memset(api, 0, sizeof(*api)); \
            return 0; \
        } \
    } while (0)

    MJ_RESOLVE(InstallHook, "InstallHook");
    MJ_RESOLVE(QueryHook, "QueryHook");
    MJ_RESOLVE(AllocTypeIdPair, "AllocTypeIdPair");
    MJ_RESOLVE(RegisterName, "RegisterName");
    MJ_RESOLVE(LookupName, "LookupName");
    MJ_RESOLVE(GetGameBase, "GetGameBase");
    MJ_RESOLVE(Log, "Log");
    MJ_RESOLVE(VerifyHooks, "VerifyHooks");

#undef MJ_RESOLVE

    return 1;
}

typedef void (APIENTRY *fn_gl_clear_color)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);

typedef struct ScreenshotFrozenClip
{
    void* movieClip;
    int currentFrame;
    unsigned char renderFlags;
    unsigned char stateFlags;
} ScreenshotFrozenClip;

typedef struct MsvcString
{
    union
    {
        char small[16];
        char* heap;
    } storage;
    size_t length;
    size_t capacity;
} MsvcString;

typedef void (__fastcall *fn_material_set_int)(void* material, MsvcString* parameterName, int value);
typedef uint8_t* (__fastcall *fn_image_decode_from_memory)(void* streamRange, int32_t* width, int32_t* height, int32_t* channels, int32_t requestedChannels);
typedef int (__cdecl *fn_resolve_palette_id)(const char* id, int32_t* resolvedRow);
typedef int (__cdecl *fn_resolve_cat_part_id)(const char* id, const char* expectedKind, int32_t* resolvedFrame);
typedef int (__cdecl *fn_sync_cat_texture_clip)(const char* partKind, void* textureMovieClip);

typedef struct PatchBackup
{
    void* address;
    unsigned char bytes[32];
    size_t length;
    DWORD protection;
} PatchBackup;

typedef struct AppearanceSnapshot
{
    int texture;
    int claws;
    int palette;
    int body;
    int head;
    int tail;
    int leg1;
    int leg2;
    int arm1;
    int arm2;
    int lefteye;
    int righteye;
    int lefteyebrow;
    int righteyebrow;
    int leftear;
    int rightear;
    int mouth;
    char voice[VOICE_NAME_BUFFER_SIZE];
    double voicePitch;
} AppearanceSnapshot;

typedef enum AppearanceSection
{
    APPEARANCE_SECTION_COAT = 0, APPEARANCE_SECTION_BODY, APPEARANCE_SECTION_LIMBS, APPEARANCE_SECTION_FACE, APPEARANCE_SECTION_VOICE, APPEARANCE_SECTION_COUNT
} AppearanceSection;

typedef enum AppearanceField
{
    APPEARANCE_FIELD_TEXTURE = 0, APPEARANCE_FIELD_PALETTE, APPEARANCE_FIELD_BODY, APPEARANCE_FIELD_HEAD, APPEARANCE_FIELD_TAIL, APPEARANCE_FIELD_LEG1,
    APPEARANCE_FIELD_LEG2, APPEARANCE_FIELD_ARM1, APPEARANCE_FIELD_ARM2, APPEARANCE_FIELD_LEFTEYE, APPEARANCE_FIELD_RIGHTEYE, APPEARANCE_FIELD_LEFTEYEBROW,
    APPEARANCE_FIELD_RIGHTEYEBROW, APPEARANCE_FIELD_LEFTEAR, APPEARANCE_FIELD_RIGHTEAR, APPEARANCE_FIELD_MOUTH, APPEARANCE_FIELD_COUNT
} AppearanceField;

typedef struct IDInputState
{
    char text[NAMED_ID_BUFFER_SIZE];
    int initialized;
    int replaceOnType;
} IDInputState;

typedef struct CustomCatPresetEntry
{
    char name[NAMED_ID_BUFFER_SIZE];
} CustomCatPresetEntry;

typedef struct TimelineChildKey
{
    uint16_t characterID;
    uint16_t libraryID;
} TimelineChildKey;

typedef struct TimelineProfile
{
    const void* definition;
    TimelineChildKey* commonChildren;
    TimelineChildKey* backgroundChildren;
    TimelineChildKey* soleSpecificKeys;
    unsigned char* backgroundFirstFrames;
    unsigned char* distinctArtFrames;
    unsigned short* specificChildTotals;
    unsigned short* renderedChildTotals;
    UINT_PTR* frameFingerprintFirst;
    UINT_PTR* frameFingerprintSecond;
    void* buildState;
    int frameExtent;
    int commonChildTotal;
    int backgroundChildTotal;
    int learnsBackgroundChildren;
    int buildStage;
    int buildCursor;
    int buildComplete;
} TimelineProfile;

typedef struct TimelineIDMap
{
    void* catVisual;
    UINT_PTR signature;
    int* validIDs;
    int minimum;
    int maximum;
    int validTotal;
    int building;
} TimelineIDMap;

typedef struct TimelineFrameFingerprint
{
    UINT_PTR first;
    UINT_PTR second;
    int childTotal;
    int timelineTotal;
} TimelineFrameFingerprint;

// Shared state stuff...
extern UINT_PTR g_gameBase;
extern HMODULE g_module;
extern fn_scene_draw g_originalSceneDraw;
extern fn_movieclip_get_child g_getMovieClipChild;
extern fn_movieclip_set_frame g_setMovieClipFrame;
extern fn_cat_visual_refresh g_refreshCatVisual;
extern fn_cat_parts_randomize g_randomizeCatParts;
extern fn_cat_parts_prepare_visual g_prepareCatPartsVisual;
extern fn_cat_visual_play_voice g_playCatVoice;
extern fn_material_set_int g_setMaterialInt;
extern fn_imgui_begin g_imguiBegin;
extern fn_imgui_end g_imguiEnd;
extern fn_imgui_same_line g_imguiSameLine;
extern fn_imgui_checkbox g_imguiCheckbox;
extern fn_imgui_slider_scalar g_imguiSliderScalar;
extern fn_imgui_selectable g_imguiSelectable;
extern fn_imgui_begin_combo g_imguiBeginCombo;
extern fn_imgui_end_combo g_imguiEndCombo;
extern void* volatile g_customScene;
extern volatile LONG g_missingCatLogged;
extern bool g_sectionOpen[APPEARANCE_SECTION_COUNT];
extern IDInputState g_idInputStates[APPEARANCE_FIELD_COUNT];
extern AppearanceField g_activeIDInput;
extern unsigned char g_keyWasDown[256];
extern unsigned char g_keyPressed[256];
extern char g_namedAppearanceIDs[APPEARANCE_FIELD_COUNT][NAMED_ID_BUFFER_SIZE];
extern char g_activeAppearancePath[800];
extern char g_selectedPresetName[NAMED_ID_BUFFER_SIZE];
extern CustomCatPresetEntry g_customCatPresets[CUSTOM_CAT_PRESET_MAX_COUNT];
extern int g_customCatPresetCount;
extern int g_customCatPresetPage;
extern TimelineProfile g_timelineProfiles[TIMELINE_PROFILE_CACHE_CAPACITY];
extern size_t g_timelineProfileUsed;
extern TimelineIDMap g_timelineIDMaps[APPEARANCE_FIELD_COUNT];
extern TimelineIDMap g_paletteIDMap;
extern unsigned char g_paletteBlankRows[APPEARANCE_MAX_ID + 1];
extern volatile LONG g_paletteInfoReady;
extern volatile LONG g_paletteInfoGeneration;
extern int g_paletteHeight;
extern int g_timelineVisualNeedsRefresh;
extern int g_timelineInitialIndexingComplete;
extern void* g_timelineValidatedVisual[APPEARANCE_FIELD_COUNT];
extern int g_timelineValidatedValue[APPEARANCE_FIELD_COUNT];
extern int g_defaultFrame;
extern int g_skipBlankArt;
extern int g_symmetryEnabled;
extern bool g_editorWindowOpen;
extern float g_sliderNavigationHeight;
extern int g_screenshotState;
extern int g_screenshotWaitFrames;
extern fn_gl_clear_color g_screenshotOriginalClearColor;
extern uint8_t* g_screenshotVisibleBlackFrame;
extern uint8_t* g_screenshotVisibleWhiteFrame;
extern uint8_t* g_screenshotBackgroundBlackFrame;
extern int g_screenshotFrameWidth;
extern int g_screenshotFrameHeight;
extern char g_debugMessages[DEBUG_LOG_CAPACITY][DEBUG_LOG_MESSAGE_LENGTH];
extern int g_debugMessageCount;

// Module implementation functions...
void AddDebugMessage(const char* format, ...);
void AddHorizontalGap(float amount);
void AddVerticalGap(float amount);
void ApplyAppearance(uint8_t* parts, const AppearanceSnapshot* appearance);
void ApplyPaletteMaterial(void* catVisual, const uint8_t* parts);
int BeginNativeAlphaScreenshot(void);
int BuildCustomCatPresetCache(void);
int CaptureCatScreenshot(void);
int CaptureInitialIndexingAppearance(const uint8_t* parts);
int CaptureScreenshotBackgroundBlackPass(void);
int CaptureScreenshotBlackPass(void);
int CaptureScreenshotWhitePass(void);
int ClampAppearanceID(int value, int minimum);
double ClampVoicePitch(double value);
void ClearNamedAppearanceIDs(void);
void ClearTimelineIdMaps(void);
void ClearTimelineProfiles(void);
int CopyAppearance(const uint8_t* parts);
int DrawSavedAppearanceFiles(uint8_t* parts, int* defaultFrame);
void EndImGuiCombo(void);
int ExportAppearance(const uint8_t* parts);
int FieldSkipsBlankFrames(AppearanceField field);
int FieldSupportsNamedID(AppearanceField field);
int FindClosestTimelineID(void* catVisual, AppearanceField field, int requested, int preferredDirection, int minimum, int maximum, int* outValue);
uint8_t* FindGonObjectChild(uint8_t* object, const char* name);
uint8_t* FindLoadedCustomCatPreset(const char* presetName);
int FindTimelineIDForNavigation(void* catVisual, AppearanceField field, int requested, int preferredDirection, int minimum, int maximum, int* outValue);
int FindTimelineIDInDirection(void* catVisual, AppearanceField field, int start, int direction, int minimum, int maximum, int* outValue);
int FindTimelineMapIDForNavigation(const TimelineIDMap* map, int requested, int preferredDirection, int* outValue);
int FindTimelineMapIDInDirection(const TimelineIDMap* map, int current, int direction, int* outValue);
int FindTimelineMapIndex(const TimelineIDMap* map, int actualId);
uint8_t* GetCatParts(void* scene, void** outCatVisual);
int GetFieldTimelineExtent(void* catVisual, AppearanceField field);
int GetGraphicsEntries(void* catVisual, size_t containerOffset, uint8_t** outEntries, int* outEntryTotal);
const char* GetMsvcStringText(const MsvcString* value);
TimelineIDMap* GetPaletteIDMap(int minimum, int maximum, int skipBlankRows);
int GetRuntimePaletteHeight(void);
void BeginTimelineIndexingFrame(void);
int AdvanceAllPartTextureIndexes(void* catVisual, int* outPercent);
TimelineIDMap* GetTimelineIDMap(void* catVisual, AppearanceField field, int minimum, int maximum);
int GetVoiceSetEntries(uint8_t** outBegin, uint8_t** outEnd);
int PrepareFreshEditorEntryIfNeeded(void* catVisual, uint8_t* parts);
void HookSceneDraw(void* scene);
void InitMsvcString(MsvcString* value, const char* text);
int IsReadableMemoryRange(const void* address, size_t length);
void Log(const char* format, ...);
const char* NamedKindForField(AppearanceField field);
void ParkScreenshotCursor(void);
void PinScreenshotCatAnimations(void);
void ReadAppearance(const uint8_t* parts, AppearanceSnapshot* appearance);
void ResetInitialIndexingAppearanceSnapshot(void);
int ResolveNamedAppearanceID(AppearanceField field, const char* token, int* resolvedValue);
int SyncMcpfTextureClip(const char* partKind, void* textureMovieClip);
void RestoreScreenshotRenderState(void);
void SetScreenshotCatVisible(int visible);
int RuntimePaletteInfoIsReady(void);
int SelectableKeepPopupOpen(const char* label, bool selected, const float size[2]);
void SetAllPartTextures(uint8_t* parts, int texture);
void SyncMcpfVisualTextureState(void* catVisual, int oneBasedTexture);
void SetMovieClipFrameForInspection(void* movieClip, int zeroBasedFrame);
int SetMsvcString(MsvcString* value, const char* text);
void SetNamedAppearanceID(AppearanceField field, const char* token);
void SetNextItemWidth(float width);

#endif