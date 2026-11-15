#pragma once

#include <stddef.h>
#include <stdint.h>

// Color values kept close to the BIOS assembler constants.
enum class BiosColor : uint8_t
{
    BlackOnBlack = 0x00,
    BlueOnBlack = 0x01,
    GreenOnBlack = 0x02,
    CyanOnBlack = 0x03,
    RedOnBlack = 0x04,
    MagentaOnBlack = 0x05,
    BrownOnBlack = 0x06,
    GreyOnBlack = 0x07,
    WhiteOnBlack = 0x0F,
    GreyOnBlue = 23,
    BlueOnLightBlue = 48,
    WhiteOnLightBlue = 63,
    WhiteOnBlue = 0x1F,
    BlackOnGrey = 0x70,
    BlueOnGrey = 0x71,
    RedOnGrey = 0x74,
};

// A semantic role lets a page be rendered with any selected theme.
enum class MenuColorRole : uint8_t
{
    TopBar,
    MainPageBackground,
    MenuText,
    Label,
    SectionTitle,
    Accent,
    Warning,
    Footer,
    Value,
};

// One text entry in a BIOS-style screen.
// This mirrors the assembler format: X,Y,Role,Repeat,String
struct MenuTextEntry
{
    uint8_t x;
    uint8_t y;
    MenuColorRole role;
    uint8_t repeat;
    const char* text;
};

// Small fixed layout for the BIOS main page header/footer text blocks.
struct MenuTextLine
{
    uint8_t x;
    uint8_t y;
    MenuColorRole role;
    const char* text;
};

// Theme description for a complete menu color palette.
struct MenuColorTheme
{
    const char* name;
    BiosColor topBar;
    BiosColor mainPageBackground;
    BiosColor menuText;
    BiosColor label;
    BiosColor sectionTitle;
    BiosColor accent;
    BiosColor warning;
    BiosColor footer;
    BiosColor value;
};

inline constexpr BiosColor ResolveColor(const MenuColorTheme& theme, MenuColorRole role)
{
    switch (role)
    {
        case MenuColorRole::TopBar: return theme.topBar;
        case MenuColorRole::MainPageBackground: return theme.mainPageBackground;
        case MenuColorRole::MenuText: return theme.menuText;
        case MenuColorRole::Label: return theme.label;
        case MenuColorRole::SectionTitle: return theme.sectionTitle;
        case MenuColorRole::Accent: return theme.accent;
        case MenuColorRole::Warning: return theme.warning;
        case MenuColorRole::Footer: return theme.footer;
        case MenuColorRole::Value: return theme.value;
        default: return theme.menuText;
    }
}

// One full BIOS-like page.
struct MenuPage
{
    const char* title;
    const MenuTextEntry* entries;
    size_t entry_count;
};

// Small helper to keep render logic consistent.
inline constexpr const MenuTextEntry* MenuPageEntries(const MenuPage& page)
{
    return page.entries;
}

inline constexpr size_t MenuPageCount(const MenuPage& page)
{
    return page.entry_count;
}

//-------------------------------------------------------------------------
// Theme table: multiple palettes can be selected without changing the page data.
//-------------------------------------------------------------------------
static constexpr MenuColorTheme BIOS_Theme_Default = {
    "Default",
    BiosColor::BlueOnLightBlue,
    BiosColor::BlackOnGrey,
    BiosColor::GreyOnBlue,
    BiosColor::BlueOnGrey,
    BiosColor::BlackOnGrey,
    BiosColor::BlueOnGrey,
    BiosColor::RedOnGrey,
    BiosColor::WhiteOnLightBlue,
    BiosColor::BlueOnGrey
};

static constexpr MenuColorTheme BIOS_Theme_Classic = BIOS_Theme_Default;

static constexpr MenuColorTheme BIOS_Theme_Mono = {
    "Mono",
    BiosColor::BlackOnBlack,
    BiosColor::BlackOnBlack,
    BiosColor::GreyOnBlack,
    BiosColor::GreyOnBlack,
    BiosColor::GreyOnBlack,
    BiosColor::GreyOnBlack,
    BiosColor::RedOnBlack,
    BiosColor::GreyOnBlack,
    BiosColor::GreyOnBlack
};

static constexpr MenuColorTheme BIOS_Theme_HighContrast = {
    "HighContrast",
    BiosColor::WhiteOnLightBlue,
    BiosColor::BlackOnGrey,
    BiosColor::GreyOnBlue,
    BiosColor::BlueOnGrey,
    BiosColor::BlackOnGrey,
    BiosColor::BlueOnGrey,
    BiosColor::RedOnGrey,
    BiosColor::WhiteOnLightBlue,
    BiosColor::RedOnGrey
};

// Reference palette tuned to the BIOS screenshot look: blue title bar,
// grey main screen background, and blue-on-grey sub-menu content.
static constexpr MenuColorTheme BIOS_Theme_Reference = {
    "Reference",
    BiosColor::BlueOnLightBlue,
    BiosColor::BlackOnGrey,
    BiosColor::GreyOnBlue,
    BiosColor::BlueOnGrey,
    BiosColor::BlackOnGrey,
    BiosColor::BlueOnGrey,
    BiosColor::RedOnGrey,
    BiosColor::WhiteOnLightBlue,
    BiosColor::BlueOnGrey
};

// Award palette: bright blue header, subtle grey main screen, and a strong
// blue-on-grey selection treatment to match the attached reference image.
static constexpr MenuColorTheme BIOS_Theme_Award = {
    "Award",
    BiosColor::BlueOnLightBlue,
    BiosColor::BlackOnGrey,
    BiosColor::GreyOnBlue,
    BiosColor::BlueOnGrey,
    BiosColor::BlackOnGrey,
    BiosColor::BlueOnGrey,
    BiosColor::RedOnGrey,
    BiosColor::WhiteOnLightBlue,
    BiosColor::BlueOnGrey
};

static constexpr const MenuColorTheme* BIOS_ColorThemes[] = {
    &BIOS_Theme_Default,
    &BIOS_Theme_Mono,
    &BIOS_Theme_HighContrast,
    &BIOS_Theme_Reference,
    &BIOS_Theme_Award,
};

inline constexpr size_t BIOS_ColorThemeCount()
{
    return sizeof(BIOS_ColorThemes) / sizeof(BIOS_ColorThemes[0]);
}

//-------------------------------------------------------------------------
// Main page definition
//-------------------------------------------------------------------------
static constexpr MenuTextLine BIOS_MainHeaderLines[] = {
    { 1, 0, MenuColorRole::TopBar, "V0.8" },
#if BOARD_PM15 || BOARD_PM20    
    { 29, 0, MenuColorRole::TopBar, "PicoMEM Setup Utility" },
#else
    { 29, 0, MenuColorRole::TopBar, "PicoMEM 2 Setup Utility" },
#endif    
    { 69, 0, MenuColorRole::TopBar, "By FreddyV" },
};


static constexpr MenuTextLine BIOS_MainFooterLines[] = {
    { 2, 23, MenuColorRole::Footer, "Esc" },
    { 14, 23, MenuColorRole::Footer, "Select Item" },
    { 32, 23, MenuColorRole::Footer, "Enter" },
    { 18, 24, MenuColorRole::Footer, "Select Menu" },
};

static constexpr MenuTextLine BIOS_SubMenuTitleLines[] = {
    { 3, 1, MenuColorRole::MenuText, "Main" },
    { 11, 1, MenuColorRole::MenuText, "Memory" },
    { 21, 1, MenuColorRole::MenuText, "Disk" },
    { 29, 1, MenuColorRole::MenuText, "Audio" },
    { 38, 1, MenuColorRole::MenuText, "Other" },
    { 46, 1, MenuColorRole::MenuText, "Tools" },
};

//-------------------------------------------------------------------------
// Main page definition
//-------------------------------------------------------------------------
static constexpr MenuTextEntry BIOS_MainPageEntries[] = {
    { 3, 4, MenuColorRole::SectionTitle, 1, "BIOS Build: " },
    { 3, 6, MenuColorRole::Label, 1, "System Memory:" },
    { 22, 6, MenuColorRole::Label, 1, "- Floppy Drives:" },
    { 41, 6, MenuColorRole::Label, 1, "- Hard Disks:" },
    { 3, 7, MenuColorRole::Label, 1, "PicoMEM IRQ:" },
    { 3, 9, MenuColorRole::SectionTitle, 1, "- PicoMEM Base Settings -" },
    { 3, 11, MenuColorRole::Label, 1, "Fast Boot: " },
    { 3, 21, MenuColorRole::Label, 1, "PicoMEM Firmware and DOC: https://github.com/FreddyVRetro/ISA-PicoMEM" },
};

//-------------------------------------------------------------------------
// Memory page definition
//-------------------------------------------------------------------------
static constexpr MenuTextEntry BIOS_MemoryPageEntries[] = {
    { 3, 8, MenuColorRole::Label, 1, "EMS  Port:" },
    { 3, 9, MenuColorRole::Label, 1, "EMS  Addr:" },
    { 3, 10, MenuColorRole::Label, 1, "PM RAM Extension :" },
    { 3, 11, MenuColorRole::Label, 1, "PSRAM  Extension :" },
    { 3, 13, MenuColorRole::Label, 1, "Ignore Video Segment:" },
    { 3, 14, MenuColorRole::Label, 1, "Maximize Conv MEM   :" },
    { 3, 19, MenuColorRole::SectionTitle, 1, "The Memory map show one Char per 16kB :" },
    { 3, 20, MenuColorRole::Label, 1, "Computer Memory - S:System V:Video R:ROM" },
    { 3, 21, MenuColorRole::Label, 1, "PicoMEM  Memory - B:BIOS   M:RAM   P:PSRAM  E:EMS  0/1:ROM Bank" },
};

//-------------------------------------------------------------------------
// Disk page definition
//-------------------------------------------------------------------------
static constexpr MenuTextEntry BIOS_DiskPageEntries[] = {
    { 3, 4,  MenuColorRole::Label, 1, "Floppy Drive A:" },
    { 3, 5,  MenuColorRole::Label, 1, "Floppy Drive B:" },
    { 3, 7,  MenuColorRole::Label, 1, "HD Drive 0:" },
    { 3, 8,  MenuColorRole::Label, 1, "HD Drive 1:" },
    { 3, 9,  MenuColorRole::Label, 1, "HD Drive 2:" },
    { 3, 10, MenuColorRole::Label, 1, "HD Drive 3:" },
    { 3, 12, MenuColorRole::Label, 1, "PicoMEM Boot Code:" },
    { 3, 15, MenuColorRole::Label, 1, "New Disk Image:" },
    { 20, 15, MenuColorRole::SectionTitle, 1, "Select" },
    { 45, 12, MenuColorRole::SectionTitle, 1, "- Set Off for XTIDE/Disk BIOS" },
    { 3, 20, MenuColorRole::SectionTitle, 1, "Store the Floppy and Disk Images under FLOPPY\\ and HDD\\ Folders" },
    { 3, 21, MenuColorRole::SectionTitle, 1, "Images files name <13 character and extention .img" },
};

//-------------------------------------------------------------------------
// Audio page definition
//-------------------------------------------------------------------------
static constexpr MenuTextEntry BIOS_AudioPageEntries[] = {
    { 2, 4,  MenuColorRole::SectionTitle, 1, "Global Audio" },
    { 3, 6,  MenuColorRole::Label, 1, "Output:" },
    { 2, 8,  MenuColorRole::SectionTitle, 1, "Sound Cards" },
    { 3, 10, MenuColorRole::Label, 1, "Adlib:" },
    { 3, 11, MenuColorRole::Label, 1, "CMS  :" },
    { 3, 12, MenuColorRole::Label, 1, "Tandy:" },
    { 3, 13, MenuColorRole::Label, 1, "Mindscape:" },
#if BOARD_PM15 || BOARD_PM20
    { 3, 20, MenuColorRole::Label, 1, "Covox/Sound Blaster/GUS/GM : Use PMINIT" },
#else
    { 3, 20, MenuColorRole::Label, 1, "Covox/Sound Blaster : Use PMINIT" },
#endif    
};

static constexpr MenuPage BIOS_Pages[] = {
    { "Main", BIOS_MainPageEntries, sizeof(BIOS_MainPageEntries) / sizeof(BIOS_MainPageEntries[0]) },
    { "Memory", BIOS_MemoryPageEntries, sizeof(BIOS_MemoryPageEntries) / sizeof(BIOS_MemoryPageEntries[0]) },
    { "Disk", BIOS_DiskPageEntries, sizeof(BIOS_DiskPageEntries) / sizeof(BIOS_DiskPageEntries[0]) },
    { "Audio", BIOS_AudioPageEntries, sizeof(BIOS_AudioPageEntries) / sizeof(BIOS_AudioPageEntries[0]) },
};

inline constexpr size_t BIOS_PagesCount()
{
    return sizeof(BIOS_Pages) / sizeof(BIOS_Pages[0]);
}

void DrawBIOSMain();
void DrawPageById(size_t index);

