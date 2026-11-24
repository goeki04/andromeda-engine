# Plugins tab – design spec

Design for **Project Settings → Plugins** in the Andromeda editor (ImGui). Two screens:

| File | Screen |
|---|---|
| [`plugin_list.html`](plugin_list.html) | Plugin list with filter, search and *Update all* |
| [`plugin_detail.html`](plugin_detail.html) | Detail page of one plugin (opens on card click) |

Both HTML files are interactive mockups (open them in a browser), **not engine code**. Colors come from
[`../../styleguide.txt`](../../styleguide.txt); the HTML maps them to CSS variables at the top of each
file. This README is the source of truth for behavior; where the HTML and this text disagree, this text wins.

---

## Core principle: plugins belong to the project

- Plugins live in `<project>/Plugins/<PluginName>/` (`ProjectPaths::pluginsDirectory()`), one folder per
  plugin with exactly one `*.aplugin` manifest. Nothing is installed globally.
- Enabled/disabled state and plugin settings are stored **per project** in `project.json`, e.g.
  `"plugins": { "com.bosch.smarthome": { "enabled": true, "settings": { ... } } }`.
  A plugin folder alone never enables anything.
- Two projects can use the same plugin with different settings, or one enabled and one disabled.
- The UI makes this visible: the list starts with *"Plugins for **<project>** · stored in `Plugins/` of
  this project"*, the add button says **"Add to project"** (not "Install"), settings say
  *"Saved with this project."*
- Plugins are native libraries: changes apply when the project is reopened (footer hint in the list).

---

## Screen 1: plugin list

### Layout
- Left: the Project Settings tab bar (General, Rendering, Assets, **Plugins**, Input, Scripting).
- Top: project hint bar (see above).
- Toolbar, one row: **Filter button** · search field (fills the width) · **Add from file** ·
  **Update all slot** (fixed width, right edge).
- Below: one card per plugin, 8 px gap.
- Footer: *"Changes apply when the project is reopened."*

### Filter
- **One** filter button with a dropdown, not separate chips. Options: All, Installed, Available, Updates.
- The button label shows the active filter (`[filter icon] All ▾`).

### Card
Three columns: icon (48×48) · content · actions (right, vertically centered).

Content column:
1. **Name** (bold) + **version pill** + optional state pill (`Active` green, `vX available` yellow,
   `Can't load` red).
2. **One-line description** from the manifest, truncated with `…`.
3. **Meta line** (small, muted): author and platform icons.
   - **Author is optional**: shown as *"by <name>"* only if the manifest has one, otherwise nothing.
   - **Plugins shipped by the engine** always show the badge **"Andromeda Engine"** (rocket icon) instead
     of an author. This comes from a manifest/catalog field like `"source": "engine"`; the free-text
     `author` field must not be able to fake it.
   - **Platform icons** (Windows, macOS) from a manifest field `"platforms": ["windows", "macos"]`.
     Unsupported platforms are simply absent. If the plugin does not support the current OS, the card
     goes into the error state with *"Not available on <OS>"*.

**Not on the card (removed on purpose):** plugin ID, load phase (Editor/Runtime), API version, a
configure/gear button. ID/API/platform details live on the detail page; settings live in its Settings tab.

### Card states and actions (layout "A")
Actions sit **side by side in one row**; the toggle is always the **rightmost** element, so toggles line
up across all cards.

| State | Pills | Actions (left → right) |
|---|---|---|
| Installed, enabled | version, `Active` | toggle (on) |
| Installed, update available | version, `vX available` (yellow) | **Update** button · toggle |
| Available, not in project | version | **Add to project** |
| Error (can't load, wrong API, wrong OS) | `Can't load` (red), red card border, red reason text | **Remove** |

### Toggle switch
- Custom widget, 30×16, knob 12×12.
- **Animated**: knob position and track color interpolate **linearly** over **160 ms**, no jumping.
- First frame shows the current state immediately (no animation into place).

### Update all – no layout jumps
The whole flow must not move anything else on screen.

1. **Fixed slot in the toolbar** (150 px, right edge). The *Update all (n)* button only exists while at
   least one update is available. During the run the same slot shows *"Updating i of n"* with a spinner;
   afterwards *"All up to date"* (green check) fades in **in the same slot**. The search field never
   changes width.
2. **Progress on the card itself**: the card's Update button has a **fixed width** (92 px) and shows a
   spinner, the percentage and a thin progress bar at its bottom edge.
3. **Confirm, then collapse**: when done, the button shows a green *"✓ Updated"* for ~1.4 s, then
   collapses to width 0 with a linear 220 ms width + opacity animation. The toggle does not move
   (it is right-aligned). The yellow *available* pill fades out, the version pill switches to the new
   version.
4. **Sequential**: *Update all* updates one plugin after another, not all at once.
5. **The Updates filter keeps finished cards** visible until the filter changes, so the list does not
   shrink mid-action. With nothing left it shows *"All plugins are up to date."*

---

## Screen 2: plugin detail page

Opens **in place of the list** inside the same Project Settings window (no second window). A back arrow
returns to the list. In ImGui: a `m_SelectedPlugin` state in the panel.

### Header (one row)
`[←]` · icon 56×56 · name (15 px, bold) + version pill + update pill ·
actions: **Update** · **Show in Finder/Explorer** (icon button) · **Remove from project** (red icon
button) · toggle.
Below the name, small and muted: *"by <author>"* (or the engine badge) · platform icons.
**Keep the platform icons here** – the user explicitly likes them in the meta line.

### Tabs
**Overview · Settings · Changelog**. Only one tab is visible at a time; this keeps the page short.
Earlier versions stacked all sections vertically and were rejected as far too long; a version with only
a few lines was rejected as too empty. The current one uses the width (two columns) and is about the
height of the list.

**Overview** – two columns:
- Left: short description paragraph, a *"What it adds"* bullet list (from the plugin's `README.md`),
  then a row of **screenshots** (88 px high, sharing the width). Screenshots are **optional**, read from
  the plugin's `images/` folder; without images the row is omitted, not left empty.
- Right: info sidebar (156 px, top-aligned, not stretched): Installed version, Latest version, Size,
  License, Added (date added to this project); divider; links: Documentation, Repository, Report an
  issue. Every entry is optional and only shown when the manifest provides it.

**Settings** – per-plugin settings, **saved per project**:
- The plugin declares its settings in the manifest (type, name, label, default; types e.g. `string`,
  `enum`, `bool`, `int`). The engine builds the form from that, so plugins need no UI code of their own.
- Values are stored in `project.json` under the plugin ID.
- Hint: *"Saved with this project."*

**Changelog** – entries for the available version and the installed one (marked *installed*), so the
user sees what changed before updating.

### Actions
- **Show in Finder/Explorer** opens the plugin folder in the project (`OS::revealInFileBrowser`).
- **Remove from project** moves the folder to the trash (`OS::moveToTrash`), with confirmation.

### Dependencies
**Not shown in the UI.** Dependencies are installed automatically together with a plugin
("Add to project"). Open point: on removal, auto-installed dependencies that nothing else needs should
be removed too – but only if the user did not add them explicitly. The project therefore has to record
which plugins were added by the user and which as a dependency.

---

## Visual tokens (from styleguide.txt)

| Role | Value |
|---|---|
| Panel background | `#121418` |
| Card background | `#1a1d24` |
| Card border | `#2a2e39` |
| Text primary / secondary | `#f0f2f5` / `#8b949e` |
| Accent (primary actions, active tab, toggle on) | `#3b82f6` (hover `#2563eb`) |
| Success (Active, Updated, up to date) | `#10b981` |
| Warning (update available) | amber |
| Error (can't load) | `#ef4444` |

Sizes: card padding 10–12 px, card gap 8 px, list icon 48 px, detail icon 56 px, corner radius ~6 px,
small text 11 px, body 13 px, plugin name bold. Exactly one blue (primary) button per view.

---

## ImGui implementation notes

- **Animation state** per widget via `ImGui::GetStateStorage()->GetFloatRef(id, initial)` and
  `ImGui::GetIO().DeltaTime`; linear step `t += dt / duration`, `ImLerp` for positions and colors.
- **Toggle**: `InvisibleButton` + `ImDrawList` (rounded track, circle knob), ~40 lines.
- **Card**: custom drawing with `ImDrawList` (rounded rect, icon square), text truncation as in
  `a_BrowserPanel.cpp`.
- **Fixed-width slots**: `ImGui::Button(label, ImVec2(150, 0))` regardless of the state shown.
- **Collapsing Update button**: lerp width and `ImGuiStyleVar_Alpha` from the card's state machine
  (idle → running → done → collapsed) with timestamps.
- **Filter**: `BeginPopup` or the existing `a_Dropdown_Button.hpp`.
- **Fonts**: only `Roboto-Regular` is loaded today; the bold plugin name needs `Roboto-Medium`/`Bold`.

## Data the UI needs

| Field | Source |
|---|---|
| id, name, version, description, icon, library, apiVersion | `*.aplugin` manifest (`PluginManifest`) |
| author (optional), source (`engine` or not) | manifest / catalog |
| platforms | manifest |
| settings schema | manifest |
| long description, features | `README.md` in the plugin folder |
| screenshots | `images/` in the plugin folder (optional) |
| changelog | `CHANGELOG.md` or catalog |
| size, license, links | manifest / catalog (optional) |
| enabled, settings values, added date, added-by-user vs. dependency | `project.json` |
| available plugins, latest versions | catalog (source still open: e.g. JSON in a GitHub repo) |
