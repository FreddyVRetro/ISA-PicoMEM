#include "pm_bios_menu.h"
#include "pm_debug.h"
#include "pm_pccmd.h"

static void DrawTextLines(const MenuTextLine* lines, size_t count, const MenuColorTheme& theme)
{
    for (size_t i = 0; i < count; ++i)
    {
        const MenuTextLine& line = lines[i];
        PC_SetTextAttr((uint8_t)ResolveColor(theme, line.role));
        PC_Setpos(line.x, line.y);
        PC_printf("%s", line.text);
    }
}

static void DrawHeader(const MenuColorTheme& theme)
{
    DrawTextLines(BIOS_MainHeaderLines, sizeof(BIOS_MainHeaderLines) / sizeof(BIOS_MainHeaderLines[0]), theme);
}

static void DrawFooter(const MenuColorTheme& theme)
{
    DrawTextLines(BIOS_MainFooterLines, sizeof(BIOS_MainFooterLines) / sizeof(BIOS_MainFooterLines[0]), theme);
}

static void DrawSubMenuTitleLine(size_t selected_index, const MenuColorTheme& theme)
{
    for (size_t i = 0; i < sizeof(BIOS_SubMenuTitleLines) / sizeof(BIOS_SubMenuTitleLines[0]); ++i)
    {
        const MenuTextLine& line = BIOS_SubMenuTitleLines[i];
        const uint8_t attr = (i == selected_index)
            ? static_cast<uint8_t>(BiosColor::BlueOnGrey)
            : static_cast<uint8_t>(ResolveColor(theme, MenuColorRole::MenuText));

        PC_SetTextAttr(attr);
        PC_Setpos(line.x, line.y);
        PC_printf("%s", line.text);
    }
}

static void DrawPageEntries(const MenuPage& page, const MenuColorTheme& theme)
{
    PM_INFO("DrawPageEntries: %d entries\n", page.entry_count);
    for (size_t i = 0; i < page.entry_count; ++i)
    {
        const MenuTextEntry& entry = page.entries[i];
        const BiosColor resolved = ResolveColor(theme, entry.role);

        PC_SetTextAttr((uint8_t)resolved);
        PC_Setpos(entry.x, entry.y);
        PC_printf("%s", entry.text);
    }
}

void DrawBIOSMain()
{
    const MenuColorTheme& theme = BIOS_Theme_Default;

    // Match the BIOS main screen layout: the main-page background attribute
    // must be selected first, just like the assembler code does with BK_G
    // before calling BIOS_PrintRectangle.
    PM_INFO("DrawBIOSMain: Set Text Attr\n");
    PC_SetTextAttr((uint8_t)ResolveColor(theme, MenuColorRole::MainPageBackground));
    PM_INFO("DrawBIOSMain: Print Rectangle\n");
    PC_PrintRectangle(0, 2, 78, 19);
    DrawHeader(theme);
    DrawSubMenuTitleLine(0, theme);
    DrawFooter(theme);
    DrawPageEntries(BIOS_Pages[0], theme);
}

void DrawPage(const MenuPage& page)
{
    const MenuColorTheme& theme = BIOS_Theme_Default;

    DrawHeader(theme);
    DrawSubMenuTitleLine(1, theme);
//    DrawFooter(theme);  // Footer is not drawn on subpages, only on the main page
    DrawPageEntries(page, theme);
}

void DrawPageById(size_t index)
{
    PM_INFO("DrawPageById: %d\n", index);
    if (index == 0)
    {
        DrawBIOSMain();
        return;
    }

    if (index < BIOS_PagesCount())
        DrawPage(BIOS_Pages[index]);
}

