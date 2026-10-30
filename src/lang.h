#ifndef H_LANG
#define H_LANG

// Thanks: SuiKaze Raider

enum StringID {
      STR_EMPTY
// common
    , STR_LOADING
    , STR_HELP_PRESS
    , STR_HELP_TEXT
    , STR_LEVEL_STATS
    , STR_HINT_SAVING
    , STR_HINT_SAVING_DONE
    , STR_HINT_SAVING_ERROR
    , STR_YES
    , STR_NO
    , STR_OFF
    , STR_ON
    , STR_NO_STEREO
    , STR_SBS
    , STR_ANAGLYPH
    , STR_SPLIT
    , STR_VR
    , STR_QUALITY_LOW
    , STR_QUALITY_MEDIUM
    , STR_QUALITY_HIGH
    , STR_LANG_EN
    , STR_LANG_FR
    , STR_LANG_DE
    , STR_LANG_ES
    , STR_LANG_IT
    , STR_LANG_PL
    , STR_LANG_PT
    , STR_LANG_RU
    , STR_LANG_JA
    , STR_LANG_GR
    , STR_LANG_FI
    , STR_LANG_CZ
    , STR_LANG_CN
    , STR_LANG_HU
    , STR_LANG_SV
    , STR_LANG_KO	// for Korean support
    , STR_APPLY
    , STR_GAMEPAD_1
    , STR_GAMEPAD_2
    , STR_GAMEPAD_3
    , STR_GAMEPAD_4
    , STR_NOT_READY
    , STR_PLAYER_1
    , STR_PLAYER_2
    , STR_PRESS_ANY_KEY
    , STR_HELP_SELECT
    , STR_HELP_BACK
// inventory pages
    , STR_OPTION
    , STR_INVENTORY
    , STR_ITEMS
// save game page
    , STR_SAVEGAME
    , STR_CURRENT_POSITION
// inventory option
    , STR_GAME
    , STR_MAP
    , STR_COMPASS
    , STR_STOPWATCH
    , STR_HOME
    , STR_DETAIL
    , STR_SOUND
    , STR_CONTROLS
    , STR_GAMMA
// passport menu
    , STR_LOAD_GAME
    , STR_START_GAME
    , STR_RESTART_LEVEL
    , STR_EXIT_TO_TITLE
    , STR_EXIT_GAME
    , STR_SELECT_LEVEL
// detail options
    , STR_SELECT_DETAIL
    , STR_OPT_DETAIL_FILTER
    , STR_OPT_DETAIL_LIGHTING
    , STR_OPT_DETAIL_SHADOWS
    , STR_OPT_DETAIL_WATER
    , STR_OPT_DETAIL_VSYNC
    , STR_OPT_DETAIL_STEREO
    , STR_OPT_SIMPLE_ITEMS
    , STR_OPT_RESOLUTION
    , STR_SCALE_100
    , STR_SCALE_75
    , STR_SCALE_50
    , STR_SCALE_25
// sound options
    , STR_SET_VOLUMES
    , STR_REVERBERATION
    , STR_OPT_SUBTITLES
    , STR_OPT_LANGUAGE
// controls options
    , STR_SET_CONTROLS
    , STR_OPT_CONTROLS_KEYBOARD
    , STR_OPT_CONTROLS_GAMEPAD
    , STR_OPT_CONTROLS_VIBRATION
    , STR_OPT_CONTROLS_RETARGET
    , STR_OPT_CONTROLS_MULTIAIM
    // controls
    , STR_CTRL_FIRST
    , STR_CTRL_LAST = STR_CTRL_FIRST + cMAX - 1
    // keys
    , STR_KEY_FIRST
    , STR_KEY_LAST  = STR_KEY_FIRST + ikBack
    // gamepad
    , STR_JOY_FIRST
    , STR_JOY_LAST  = STR_JOY_FIRST + jkMAX - 1
// inventory items
    , STR_UNKNOWN
    , STR_EXPLOSIVE
    , STR_PISTOLS
    , STR_SHOTGUN
    , STR_MAGNUMS
    , STR_UZIS
    , STR_AMMO_PISTOLS
    , STR_AMMO_SHOTGUN
    , STR_AMMO_MAGNUMS
    , STR_AMMO_UZIS
    , STR_MEDI_SMALL
    , STR_MEDI_BIG
    , STR_LEAD_BAR
    , STR_SCION
// keys
    , STR_KEY
    , STR_KEY_SILVER
    , STR_KEY_RUSTY
    , STR_KEY_GOLD
    , STR_KEY_SAPPHIRE
    , STR_KEY_NEPTUNE
    , STR_KEY_ATLAS
    , STR_KEY_DAMOCLES
    , STR_KEY_THOR
    , STR_KEY_ORNATE
// puzzles
    , STR_PUZZLE
    , STR_PUZZLE_GOLD_IDOL
    , STR_PUZZLE_GOLD_BAR
    , STR_PUZZLE_COG
    , STR_PUZZLE_FUSE
    , STR_PUZZLE_ANKH
    , STR_PUZZLE_HORUS
    , STR_PUZZLE_ANUBIS
    , STR_PUZZLE_SCARAB
    , STR_PUZZLE_PYRAMID
// TR2 items (the names of the original script)
    , STR_TR2_KEY_GUARDHOUSE
    , STR_TR2_KEY_BOATHOUSE
    , STR_TR2_KEY_STEEL
    , STR_TR2_KEY_IRON
    , STR_TR2_KEY_LIBRARY
    , STR_TR2_KEY_DETONATOR
    , STR_TR2_KEY_PASS_RED
    , STR_TR2_KEY_PASS_YELLOW
    , STR_TR2_KEY_PASS_GREEN
    , STR_TR2_KEY_PASS_BLUE
    , STR_TR2_KEY_REST_ROOM
    , STR_TR2_KEY_CABIN
    , STR_TR2_KEY_THEATRE
    , STR_TR2_KEY_STERN
    , STR_TR2_KEY_STORAGE
    , STR_TR2_KEY_DRAWBRIDGE
    , STR_TR2_KEY_HUT
    , STR_TR2_KEY_STRONGROOM
    , STR_TR2_KEY_TRAPDOOR
    , STR_TR2_KEY_ROOFTOPS
    , STR_TR2_KEY_MAIN_HALL
    , STR_TR2_KEY_MAIN_CHAMBER
    , STR_TR2_KEY_GUN_CUPBOARD
    , STR_TR2_GONG_HAMMER
    , STR_TR2_PUZZLE_RELAY_BOX
    , STR_TR2_PUZZLE_CIRCUIT_BOARD
    , STR_TR2_PUZZLE_MACHINE_CHIP
    , STR_TR2_PUZZLE_CIRCUIT_BREAKER
    , STR_TR2_PUZZLE_SERAPH
    , STR_TR2_PUZZLE_PRAYER_WHEELS
    , STR_TR2_PUZZLE_GEMSTONES
    , STR_TR2_PUZZLE_TIBETAN_MASK
    , STR_TR2_PUZZLE_DRAGON_SEAL
    , STR_TR2_PUZZLE_MYSTIC_PLAQUE
    , STR_TR2_PUZZLE_DAGGER
    , STR_TR2_QUEST_TALION
// TR1 subtitles
    , STR_TR1_SUB_CAFE
    , STR_TR1_SUB_LIFT
    , STR_TR1_SUB_CANYON
    , STR_TR1_SUB_PRISON
    , STR_TR1_SUB_22 // CUT4
    , STR_TR1_SUB_23 // CUT1
    , STR_TR1_SUB_24
    , STR_TR1_SUB_25 // CUT3
    , STR_TR1_SUB_26
    , STR_TR1_SUB_27
    , STR_TR1_SUB_28
    , STR_TR1_SUB_29
    , STR_TR1_SUB_30
    , STR_TR1_SUB_31
    , STR_TR1_SUB_32
    , STR_TR1_SUB_33
    , STR_TR1_SUB_34
    , STR_TR1_SUB_35
    , STR_TR1_SUB_36
    , STR_TR1_SUB_37
    , STR_TR1_SUB_38
    , STR_TR1_SUB_39
    , STR_TR1_SUB_40
    , STR_TR1_SUB_41
    , STR_TR1_SUB_42
    , STR_TR1_SUB_43
    , STR_TR1_SUB_44
    , STR_TR1_SUB_45
    , STR_TR1_SUB_46
    , STR_TR1_SUB_47
    , STR_TR1_SUB_48
    , STR_TR1_SUB_49
    , STR_TR1_SUB_50
    , STR_TR1_SUB_51
    , STR_TR1_SUB_52
    , STR_TR1_SUB_53
    , STR_TR1_SUB_54
    , STR_TR1_SUB_55
    , STR_TR1_SUB_56
// TR2 subtitles
    , STR_TR2_SUB_CUT1
    , STR_TR2_SUB_CUT2
    , STR_TR2_SUB_CUT3
// TR1 levels
    , STR_TR1_GYM
    , STR_TR1_LEVEL1
    , STR_TR1_LEVEL2
    , STR_TR1_LEVEL3A
    , STR_TR1_LEVEL3B
    , STR_TR1_LEVEL4
    , STR_TR1_LEVEL5
    , STR_TR1_LEVEL6
    , STR_TR1_LEVEL7A
    , STR_TR1_LEVEL7B
    , STR_TR1_LEVEL8A
    , STR_TR1_LEVEL8B
    , STR_TR1_LEVEL8C
    , STR_TR1_LEVEL10A
    , STR_TR1_LEVEL10B
    , STR_TR1_LEVEL10C
    , STR_TR1_EGYPT
    , STR_TR1_CAT
    , STR_TR1_END
    , STR_TR1_END2
// TR2 levels
    , STR_TR2_ASSAULT
    , STR_TR2_WALL
    , STR_TR2_BOAT
    , STR_TR2_VENICE
    , STR_TR2_OPERA
    , STR_TR2_RIG
    , STR_TR2_PLATFORM
    , STR_TR2_UNWATER
    , STR_TR2_KEEL
    , STR_TR2_LIVING
    , STR_TR2_DECK
    , STR_TR2_SKIDOO
    , STR_TR2_MONASTRY
    , STR_TR2_CATACOMB
    , STR_TR2_ICECAVE
    , STR_TR2_EMPRTOMB
    , STR_TR2_FLOATING
    , STR_TR2_XIAN
    , STR_TR2_HOUSE
// TR3 levels
    , STR_TR3_HOUSE
    , STR_TR3_JUNGLE
    , STR_TR3_TEMPLE
    , STR_TR3_QUADCHAS
    , STR_TR3_TONYBOSS
    , STR_TR3_SHORE
    , STR_TR3_CRASH
    , STR_TR3_RAPIDS
    , STR_TR3_TRIBOSS
    , STR_TR3_ROOFS
    , STR_TR3_SEWER
    , STR_TR3_TOWER
    , STR_TR3_OFFICE
    , STR_TR3_NEVADA
    , STR_TR3_COMPOUND
    , STR_TR3_AREA51
    , STR_TR3_ANTARC
    , STR_TR3_MINES
    , STR_TR3_CITY
    , STR_TR3_CHAMBER
    , STR_TR3_STPAUL
// TR2+ inventory items
    , STR_AUTOPISTOLS
    , STR_HARPOON
    , STR_M16
    , STR_GRENADE
    , STR_AMMO_AUTOPISTOLS
    , STR_AMMO_HARPOON
    , STR_AMMO_M16
    , STR_AMMO_GRENADE
    , STR_FLARES

// modern effects options
    , STR_OPT_FX_LIGHTS
    , STR_OPT_FX_SKY
    , STR_MAX
};

#ifdef _XBOX // TODO: illegal escape sequence
    #define STR_RUSSIAN "Russian"
#else
    #define STR_RUSSIAN "–усски{и"
#endif

#define STR_LANGUAGES \
      "English"       \
    , "Fran|cais"     \
    , "Deutsch"       \
    , "Espa+nol"      \
    , "Italiano"      \
    , "Polski"        \
    , "Portugu(es"    \
    , STR_RUSSIAN     \
    , "\x11\x02\x70\x01\x97\x01\xD6\xFF\xFF" \
    , "\x11\x01\x22\x01\x0F\x01\x0F\x01\x0E\x01\x06\x01\x04\x01\x0C\x01\x0B\xFF\xFF" \
    , "Suomi"         \
    , "{Cesky"        \
    , "\x11\x02\x8A\x02\x6C\x01\x54\x03\x02\xFF\xFF" \
    , "Magyar" \
    , "Svenska" \
    , "Korean"

// TR2 cutscene subtitles (English, the other languages fall back to them)
#define STR_TR2_SUBS \
    /* CUT1 */ , \
    "[10980]Pardon me, if that was just your@way of trying the doors for me." \
    "[15200]Well, that tour may@go on my keyring." \
    "[18940]Though not anymore. So, after you." \
    "[21260]Somehow, you don't behave@like you got a monk's blood." \
    "[25900]I understand that@'somehow' is in my favor." \
    "[29429]So indulge me about the dagger.@I'd be indebted with your life." \
    "[34600]These doors are waiting for the@right one, the right time to arrive." \
    "[40196]And then, the dagger's blade will" \
    "[42912]honor the hearts of@those who believe." \
    "[46039]So unless you pledge@your loyalty as well..." \
    "[49660]" \
    "[50000]And which one is that?" \
    "[52340]" \
    "[53740]To the sins and fortunes@of Marco Bartoli!" \
    "[59580]" \
    "[60300]Perhaps not just yet then." \
    "[62600]" \
    "[77460]Aha, Gianni Bartoli,@Via Caravelli, Venice." \
    "[84980]" \
    /* CUT2 */ , \
    "[18640]...Believe me, but it's not@quite the same now, is it?" \
    "[21960]" \
    "[34200]Someday you'll get a speeding@ticket for that tongue, Fabio." \
    "[43900]Eh, it's just a gut feeling that@maybe you are wrong to look there..." \
    "[55140]" \
    "[56400]Is your belief so fragile?@Relax, breathe deep." \
    "[60253]The gut, Fabio, has no more" \
    "[62466]direction than a@simple through-and-out." \
    "[65745]When my father left when I was a@boy, he confided to me that he was" \
    "[71236]enlightened, echo of his heart's@inclination rather than impulse." \
    "[76564]He possessed the Seraph." \
    "[78532]But he was just a disciple@in this design, his death" \
    "[82794]plotting a path to be short by the@lower one. His son, you understand?" \
    "[88532]Have faith, Fabio, not gut rot.@We are searching the right place." \
    "[93860]I know it. I believe it, Marco." \
    "[98160]" \
    "[99140]Good. Eros, have you...@fixed that rail yet?" \
    "[104380]Si." \
    "[105660]" \
    /* CUT3 */ , \
    "[22120]Ah... You are not one of them." \
    "[24440]But you are a monk." \
    "[26960]Brother Chen Barkhang.@You have come for me!" \
    "[30769]I saw bright lights around me..." \
    "[33600]That was gunfire. I think it was@them who got taken away by it." \
    "[40340]But you are my guide, my@path-beater to the next incarnation." \
    "[45610]I have done my time here, haven't I?" \
    "[48920]What are you doing@here? And Marco Bartoli?" \
    "[53300]" \
    "[54440]Nothing! I led a@righteous life, here for" \
    "[57391]reasons rooted only@in necessary evil." \
    "[60125]As my father was before@me, when he bombed" \
    "[63148]Gianni's vessel deep@into these waters." \
    "[65954]Now I'm here, was here, to prevent@his son from salvaging the Seraph." \
    "[70920]The Seraph?" \
    "[73040]" \
    "[75500]You not know my life's work well.@You sure you're not here for them?" \
    "[80840]Their Jackaknoori@days are well over." \
    "[84200]They want the Seraph@to unlock a malignant" \
    "[86460]treasure we contain in@our monastery in Tibet." \
    "[88935]Since being stolen by@imbecile vagabonds centuries" \
    "[91625]ago, we've been without key@to it, relying solely on" \
    "[94423]cleansing of our prayers@to keep it subdued." \
    "[96790]Then the occultist@Gianni re-acquired it." \
    "[98996]Trouble we knew: he breathed@life back into ancient belief," \
    "[102170]one not to be stopped by@any amount of head-bowing." \
    "[104914]And now again, it is here:@Marco, infected with madness." \
    "[107927]He has violent mind, but not@yet the power to satiate it." \
    "[110994]So, we reach for our@weapons once more..." \
    "[113200]" \
    "[114100]The true detox of evil." \
    "[118460]" \
    "[121400]Where can you be taking me?@Thought this was my big break." \
    "[128434]Guess changes are as good@as rest. Oh, I need one..." \
    "[134740]"

// TR2 item names (English, the other languages fall back to them)
#define STR_TR2_ITEMS \
    , "Guardhouse Key" \
    , "Boathouse Key" \
    , "Steel Key" \
    , "Iron Key" \
    , "Library Key" \
    , "Detonator Key" \
    , "Red Pass Card" \
    , "Yellow Pass Card" \
    , "Green Pass Card" \
    , "Blue Pass Card" \
    , "Rest Room Key" \
    , "Cabin Key" \
    , "Theatre Key" \
    , "Stern Key" \
    , "Storage Key" \
    , "Drawbridge Key" \
    , "Hut Key" \
    , "Strongroom Key" \
    , "Trapdoor Key" \
    , "Rooftops Key" \
    , "Main Hall Key" \
    , "Main Chamber Key" \
    , "Gun Cupboard Key" \
    , "Gong Hammer" \
    , "Relay Box" \
    , "Circuit Board" \
    , "Machine Chip" \
    , "Circuit Breaker" \
    , "The Seraph" \
    , "Prayer Wheels" \
    , "Gemstones" \
    , "Tibetan Mask" \
    , "The Dragon Seal" \
    , "Mystic Plaque" \
    , "Dagger of Xian" \
    , "Talion"

// modern effects options (English, the other languages fall back to them)
#define STR_FX_OPTIONS \
    , "Dynamic Lights" \
    , "Sky Effects"



#define LANG_PREFIXES "_EN", "_FR", "_DE", "_ES", "_IT", "_PL", "_PT", "_RU", "_JA", "_GR", "_FI", "_CZ", "_CN", "_HU", "_SV", "_KO"

#define STR_KEYS \
      "NONE", "LEFT", "RIGHT", "UP", "DOWN", "SPACE", "TAB", "ENTER", "ESCAPE", "SHIFT", "CTRL", "ALT" \
    , "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" \
    , "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M" \
    , "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" \
    , "PAD0", "PAD1", "PAD2", "PAD3", "PAD4", "PAD5", "PAD6", "PAD7", "PAD8", "PAD9", "PAD+", "PAD-", "PADx", "PAD/", "PAD." \
    , "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12" \
    , "-", "+", "<", ">", "/", "\\", ",", ".", "$", ":", "'", "PGUP", "PGDN", "HOME", "END", "DEL", "INS", "BKSP" \
    , "NONE", "A", "B", "X", "Y", "L BUMPER", "R BUMPER", "SELECT", "START", "L STICK", "R STICK", "L TRIGGER", "R TRIGGER", "D-LEFT", "D-RIGHT", "D-UP", "D-DOWN"

#define STR_SCALE "25", "50", "75", "100"

static char helpText[1024];
const char *helpTextFormat =
    "Start - add second player or restore Lara@"
    "H - Show or hide this help@"
    "ALT and ENTER - Fullscreen@"
    "5 - Save Game@"
    "9 - Load Game@"
    "%s - Look@"
    "R - Slow motion@"
    "T - Fast motion@"
    "Roll - Up & Down@"
    "Step Left - Walk & Left@"
    "Step Right - Walk & Right@"
    "Out of water - Up & Action@"
    "Handstand - Up & Walk@"
    "Swan dive - Up & Walk & Jump@"
    "First Person View - Look & Action@"
    "DOZY on - Look & Duck & Action & Jump@"
    "DOZY off - Walk@"
    "Free Camera - hold L & R stick";
inline const char* getKeyName(InputKey key) {
    static const char* keyNames[] = {
        STR_KEYS  
    };
    if ((int)key >= 0 && (int)key < COUNT(keyNames))
        return keyNames[(int)key];
    return "UNKNOWN";
}

inline void updateHelpText() {
    InputKey lookKey = (InputKey)Core::settings.controls[0].keys[cLook].key;
    const char* keyName = getKeyName(lookKey);
    sprintf(helpText, helpTextFormat, keyName);
}

#include "lang/en.h"
#include "lang/fr.h"
#include "lang/de.h"
#include "lang/es.h"
#include "lang/it.h"
#include "lang/pl.h"
#include "lang/pt.h"
#include "lang/ru.h"
#include "lang/ja.h"
#include "lang/gr.h"
#include "lang/fi.h"
#include "lang/cz.h"
#include "lang/cn.h"
#include "lang/hu.h"
#include "lang/sv.h"
#include "lang/ko.h"

char **STR = NULL;

void ensureLanguage(int lang) {
    ASSERT(COUNT(STR_EN) == STR_MAX);
    ASSERT(COUNT(STR_FR) == STR_MAX);
    ASSERT(COUNT(STR_DE) == STR_MAX);
    ASSERT(COUNT(STR_ES) == STR_MAX);
    ASSERT(COUNT(STR_IT) == STR_MAX);
    ASSERT(COUNT(STR_PL) == STR_MAX);
    ASSERT(COUNT(STR_PT) == STR_MAX);
    ASSERT(COUNT(STR_RU) == STR_MAX);
    ASSERT(COUNT(STR_JA) == STR_MAX);
    ASSERT(COUNT(STR_GR) == STR_MAX);
    ASSERT(COUNT(STR_FI) == STR_MAX);
    ASSERT(COUNT(STR_CZ) == STR_MAX);
    ASSERT(COUNT(STR_CN) == STR_MAX);
    ASSERT(COUNT(STR_HU) == STR_MAX);
    ASSERT(COUNT(STR_SV) == STR_MAX);
    ASSERT(COUNT(STR_KO) == STR_MAX);

    lang += STR_LANG_EN;

    switch (lang) {
        case STR_LANG_FR : STR = (char**)STR_FR; break;
        case STR_LANG_DE : STR = (char**)STR_DE; break;
        case STR_LANG_ES : STR = (char**)STR_ES; break;
        case STR_LANG_IT : STR = (char**)STR_IT; break;
        case STR_LANG_PL : STR = (char**)STR_PL; break;
        case STR_LANG_PT : STR = (char**)STR_PT; break;
        case STR_LANG_RU : STR = (char**)STR_RU; break;
        case STR_LANG_JA : STR = (char**)STR_JA; break;
        case STR_LANG_GR : STR = (char**)STR_GR; break;
        case STR_LANG_FI : STR = (char**)STR_FI; break;
        case STR_LANG_CZ : STR = (char**)STR_CZ; break;
        case STR_LANG_CN : STR = (char**)STR_CN; break;
        case STR_LANG_HU : STR = (char**)STR_HU; break;
        case STR_LANG_SV : STR = (char**)STR_SV; break;
        case STR_LANG_KO : STR = (char**)STR_KO; break;
        default          : STR = (char**)STR_EN; break;
    }
}

#endif
