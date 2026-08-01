import QtQuick

// Warm editorial palette + spacing tokens. Doubao-style structure (layered
// neutrals, soft radii), warm hue (paper/ink/clay) instead of AI blue/purple.
// Single source of truth for all colors/radii/spacing in the UI. Every color
// is a token switched by `dark` — light (cream/paper) and dark (tinted-black)
// share token names, so components never branch on theme.
QtObject {
    id: root
    // surfaces — layered, never pure white/black. Dark is warm tinted-black,
    // not neutral gray, per CLAUDE.md "tinted-black dark".
    readonly property var colors: typeof themeManager !== "undefined" ? themeManager.colors : ({})
    readonly property var fonts: typeof themeManager !== "undefined" ? themeManager.fonts : ({})
    readonly property var wallpaper: typeof themeManager !== "undefined" ? themeManager.wallpaper : ({})

    function fontFamily(name, fallback) {
        const value = fonts[name]
        if (typeof value === "string")
            return value
        if (value && value.family)
            return value.family
        return fallback
    }

    function accent(alpha) {
        return Qt.rgba(clay.r, clay.g, clay.b, alpha)
    }

    property color paper:      colors.paper || (dark ? "#1A1714" : "#F5EFE3")   // app background
    property color surface:    colors.surface || (dark ? "#221E19" : "#FBF6EC")   // cards / surfaces
    property color surfaceAlt: colors.surfaceAlt || (dark ? "#2A251F" : "#EFE7D6")   // sidebar / alt

    // ink
    property color ink:      colors.ink || (dark ? "#E8E1D0" : "#1C1916")     // primary text
    property color inkSoft: colors.inkSoft || (dark ? "#948A79" : "#6B6357")     // secondary text

    // lines & accents
    property color line:     colors.line || (dark ? "#3B342C" : "#D9CFBC")     // hairline
    property color clay:     colors.clay || (dark ? "#D2693A" : "#C2502A")     // brand accent
    property color clayDeep: colors.clayDeep || (dark ? "#B0542E" : "#9E3D1F")     // hover / active
    property color moss:     colors.moss || (dark ? "#76815F" : "#4A5340")     // secondary accent

    // radii scale (Doubao-measured)
    property int rSm: 8
    property int rMd: 12
    property int rLg: 16
    property int rPill: 9999

    // spacing scale
    property int sp1: 4
    property int sp2: 8
    property int sp3: 12
    property int sp4: 16
    property int sp5: 24
    property int sp6: 32

    // Bundled OFL font families (registered in main.cpp from :/fonts/*).
    // Qt's automatic font merging makes Noto Sans/Serif SC the CJK fallback for
    // the Latin display/body faces — matching pairing, not system fallback.
    property string fontDisplay: fontFamily("display", "Fraunces")          // display serif (CJK → Noto Serif SC)
    property string fontBody:    fontFamily("body", "Hanken Grotesk")    // body sans (CJK → Noto Sans SC)
    property string fontMono:    fontFamily("mono", "IBM Plex Mono")     // mono

    property string wallpaperSource: wallpaper.source || ""
    property string wallpaperMode: wallpaper.mode || "cover"
    property real wallpaperOpacity: wallpaper.opacity === undefined ? 0 : wallpaper.opacity
    property bool hasWallpaper: wallpaperSource.length > 0 && wallpaperOpacity > 0
    property color pageOverlay: hasWallpaper
                                ? (dark ? Qt.rgba(paper.r, paper.g, paper.b, 0.74)
                                        : Qt.rgba(paper.r, paper.g, paper.b, 0.82))
                                : paper
    // User message bubble — a flat right-aligned warm block in the brand clay
    // family (matches the sparkle avatar mark). Flat fill, no drop shadow; the
    // ChatView disables MultiEffect on this bubble. Text sits on clay at full
    // contrast, border is a deeper clay hairline.
    property color userBubble:      colors.userBubble || (dark ? "#342015" : "#7C5A47")
    property color userBubbleText:  colors.userBubbleText || "#ffffff"
    property color userBubbleBorder: colors.userBubbleBorder || (dark ? "#24160f" : "#654635")

    // soft shadow color (warm, low opacity — no AI blue)
    property color shadowColor: colors.shadowColor || (dark ? Qt.rgba(0, 0, 0, 0.45) : Qt.rgba(0.11, 0.08, 0.05, 0.12))

    // theme mode — bound to settings.theme in main.qml
    property bool dark: false

    onDarkChanged: if (typeof themeManager !== "undefined") themeManager.dark = dark

    // Soft drop-shadow for cards/dialogs. Use via MultiEffect where available.
    property real shadowBlur: 16
    property real shadowY: 6
}
