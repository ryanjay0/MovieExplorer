# MovieExplorer - Project Guide

## Overview

MovieExplorer is a native Win32 C++ application that catalogues movie and TV files on disk, fetches metadata from TMDB (primary) and OMDb (ratings fallback), and displays them in a grid or detail view with posters, ratings, and other info. It is a fork of `anlarke/MovieExplorer`, heavily modernized.

- **Language**: C++17 (compiled with MSVC v145 / VS 2026)
- **Platform**: Windows 10+ (x86 and x64)
- **UI Framework**: Custom Win32 (no MFC, no WTL) — all controls are hand-drawn via `RWindow`, `RStatic`, `RButton`, `RComboBox`, etc.
- **Build**: Visual Studio solution at `MovieExplorer/VS2015/MovieExplorer.vcxproj` (4 configs: Debug/Release × Win32/x64)
- **CI**: GitHub Actions on `windows-latest` (currently Server 2025 with VS 2026 Enterprise)
- **Repo**: `https://github.com/ryanjay0/MovieExplorer`, branch `agentic`

## Architecture

### Directory Structure

```
MovieExplorer/
├── MovieExplorer/          # Main application source
│   ├── VS2015/             # VS project files (.vcxproj, .sln)
│   ├── strings/            # String resource DLL project
│   ├── *.cpp, *.h          # App source files
│   └── ...
├── RClasses/               # Custom class library (RWindow, RString, RArray, etc.)
│   ├── RWindow.h           # Base window class with message routing
│   ├── RString.h           # Custom string class (NOT std::string)
│   ├── RArray.h            # Dynamic array
│   ├── RCriticalSection.h  # Wrapper for CRITICAL_SECTION
│   ├── RXML2.h             # Tiny XML parser/writer
│   ├── general.h           # Utility functions (IsWin8, FillSolidRect, etc.)
│   └── ...
├── DynamicDll/             # User32 DLL hook for theming
└── .github/workflows/      # CI configuration
```

### Core Classes

| Class | File | Purpose |
|-------|------|---------|
| `CMainWnd` | MainWnd.h/cpp | Main application window, hosts all child windows |
| `CListView` | ListView.h/cpp | Detail view — shows one movie at a time with full metadata |
| `CGridView` | GridView.h/cpp | Grid view — shows movie posters in columns |
| `CReBar` | ReBar.h/cpp | Top toolbar bar with search, category buttons, view toggle |
| `CCategoryBar` | CategoryBar.h/cpp | Left sidebar with category filter buttons (All, Movies, TV, custom) |
| `CStatusBar` | StatusBar.h/cpp | Bottom status bar |
| `CLogWnd` | LogWnd.h/cpp | Bottom log window showing update progress |
| `CDatabase` | Database.h/cpp | In-memory movie database, XML load/save, update orchestration |
| `COptionsDlg` | OptionsDlg.h/cpp | Settings dialog with property pages |
| `CDatabasePage` | DatabasePage.h/cpp | Database settings property page |
| `CGeneralPage` | GeneralPage.h/cpp | General settings property page (theme, language, search) |
| `CEditDlg` | EditDlg.h/cpp | Edit movie metadata dialog |
| `ImageHash` | ImageHash.h/cpp | In-memory cache of actor images (RCriticalSection-protected) |

### Data Flow

1. **Startup**: `WinMain` → `MovieExplorer.cpp` → Load preferences → Load theme → Load language → Create `CDatabase` → Create `CMainWnd`
2. **Load database**: `CDatabase::Load()` reads `Database.xml` (movie filenames + IMDb IDs + seen/hidden flags)
3. **Sync + Update**: `CDatabase::SyncAndUpdate()` → `Sync()` scans disk for new/removed files → `Update()` spawns worker threads
4. **Update thread**: Each movie gets its own thread via `UpdateThread()`. Thread reads cache XML, falls back to OMDb API call, saves cache XML
5. **Display**: `CListView` or `CGridView` reads from `CDatabase::m_movies` array

### Key Data Structures

```cpp
// Database.h

struct DBMOVIE {
    RString strFileName, strTitle, strYear, strGenres, strCountries;
    RString strContentRating, strStoryline, strDirectors, strWriters, strStars;
    RString strIMDbID, strTMDBID;
    RString strEpisodeName, strEpisodeID, strAirDate;
    int nYear, nRuntime, nSeason, nEpisode, nVotes, nIMDbVotes, nMetascore;
    float fRating, fRatingMax, fIMDbRating, fIMDbRatingMax;
    BYTE bType; // DB_TYPE_UNKNOWN=0, DB_TYPE_MOVIE=1, DB_TYPE_TV=2
    bool bHide, bSeen, bUpdated, bOMDbRatingsFetched;
    INT64 fileSize, fileTime, resumeTime;
    RArray<BYTE> posterData;
    RString strActorId[5]; // DBI_STAR_NUMBER=5
    ARBYTE* actorImageData[5];
    DBDIRECTORY *pDirectory;
};

struct DBINFO { // Same fields as DBMOVIE plus search/status fields
    RString strSearchTitle, strSearchYear, strServiceName, strID;
    RString strTMDBID, strIMDbID;
    bool bOMDbRatingsFetched;
    BYTE status; // DBI_STATUS_NONE=0, UPDATED=1, UNKNOWN=2, CONNERROR=3, SCRAPEERROR=4, RATELIMITED=5
    UINT64 timestamp;
};

struct DBDIRECTORY {
    RString strPath, strComputerName;
    RObArray<DBMOVIE> movies;
    DBCATEGORY *pCategory;
};

struct DBCATEGORY {
    RString strName;
    RObArray<DBDIRECTORY> directories;
};
```

## Data Sources

### TMDB (Primary)

`ScrapeTMDB.cpp` handles all TMDB API communication. TMDB is the default primary metadata source — it has no daily request cap, provides actor headshot images, and covers more titles than OMDb.

**Flow for a single movie:**

1. **Parse filename** → `ParseFileName.cpp` extracts title, year, season, episode from filename
2. **ID resolution** (in priority order):
   - If `strID` is already a numeric TMDB ID (not `tt...`), use it directly
   - If `strIMDbID` starts with `tt`, call `/find/{imdb_id}?external_source=imdb_id` (migration path)
   - Search by title+year via `/search/movie` or `/search/tv`
   - Fallback: strip `" - <Genre>"` suffix, retry search without year
3. **Fetch details** — `/movie/{id}?append_to_response=credits,release_dates` or `/tv/{id}?append_to_response=credits,content_ratings,external_ids`
4. **Parse details** — title, year, poster (w500), vote_average, overview, genres (pipe-delimited), countries, content rating (US), runtime, IMDb ID
5. **Parse credits** — directors, writers, stars (pipe-delimited), actor headshot images (w185)
6. **For TV episodes** — season cache system (see below)
7. **OMDb ratings fallback** — if `bOMDbRatingsFetched` is false, fetch IMDb rating + votes + Metascore from OMDb by IMDb ID

**API key**: Stored in preferences as `TMDBAPIKey`. Free tier, no daily cap, 50 req/sec rate limit. A 30ms throttle (`TMDBThrottle()`) enforces minimum gap between requests.

**Throttle**: `g_tmdbCS` (RCriticalSection) + `g_tmdbLastRequest` (GetTickCount) — 30ms minimum between TMDB requests.

**"The/A" prefix normalization**: After fetching, `"The Matrix"` → `"Matrix, The"` and `"A Movie"` → `"Movie, A"` — same as ScrapeIMDb, for sort consistency.

### OMDb (Ratings Fallback)

`ScrapeIMDb.cpp` handles OMDb API communication. When TMDB is primary, OMDb is only called for IMDb ratings/votes/Metascore (via `TMDBFetchRatingsFromOMDb`). OMDb can also be used as the primary source if `OnlyUse=imdb.com`.

**Flow for a single movie:**

1. **Parse filename** → `ParseFileName.cpp` extracts title, year, season, episode from filename
2. **Multi-strategy search** — tries in order:
   - Title + year
   - Title without year
   - Apostrophe variants (e.g. "ONeal" → "O'Neal") via `TryApostropheVariants()`
   - Strip country suffix for TV (e.g. "Battlestar Galactica (2004)" → "Battlestar Galactica 2004")
   - "Part X" retry via `TryStripPart()` (e.g. "Fear Street Part 2" → "Fear Street 2" or "Fear Street")
3. **OMDb search** — `http://www.omdbapi.com/?apikey=KEY&s=TITLE&y=YEAR&type=movie|series`
4. **Pick best result** — `OMDbPickBestResult()` scores results by title similarity
5. **Fetch details** — `http://www.omdbapi.com/?apikey=KEY&i=IMDB_ID&plot=full&r=xml`
6. **For TV episodes** — season cache system (see below)

### API Keys

| Key | Pref Name | Default | Notes |
|-----|-----------|---------|-------|
| TMDB | `TMDBAPIKey` | (empty) | Free, no daily cap, 50 req/sec |
| OMDb | `OMDbAPIKey` | (empty) | Free tier = 1,000 req/day |

### Rate Limiting

- **OMDb**: `OMDbUsageTracker` (daily count, persisted to `Cache/omdb_usage.txt`). Default limit 900 (100-request buffer). When hit → `DBI_STATUS_RATELIMITED` → `strID = "rateLimited"` → remaining movies skipped. The "Recheck Failed" button clears this.
- **TMDB**: 30ms throttle between requests (in-memory, not persisted). No daily cap.
- **`bOMDbRatingsFetched` flag**: Set to `true` only when OMDb returns a definitive answer (rating or "no data"). Stays `false` on rate-limit/connection error (will retry next cycle). Prevents infinite retry loop for movies with no OMDb ratings.

### Season Cache (TV Episodes)

For TV shows, episodes share a series ID. The season cache system avoids per-episode API calls:

- **`SeriesDedup`** — shared across all update threads (member of `CDatabase`). Maps `"service|title|year|type"` → series ID. Avoids redundant searches. Used for both `tmdb.org` and `imdb.com`.
- **`SeriesCache`** — per-thread `std::map<RString, SeriesCache>`. When first episode of a season is fetched, the entire season is fetched in one call (OMDb `&Season=N` or TMDB `/tv/{id}/season/{n}`), caching all episode data.
- **`strEpisodeID`** — stored separately from `strID`/`strIMDbID`/`strTMDBID`. Series ID is the primary key; episode ID goes to `strEpisodeID`. Prevents stale cache issues.

### Cache File Naming

- **Movies**: `{cacheDir}\{service}\{id}.xml` (e.g. `Cache\tmdb.org\12345.xml` or `Cache\imdb.com\tt1234567.xml`)
- **TV episodes**: `{cacheDir}\{service}\{id}_S{n}_E{n}.xml` (e.g. `Cache\tmdb.org\12345_S1_E3.xml`)
- **Fallback**: If new-format `_S{n}_E{n}.xml` not found for TV, tries old format `{id}.xml` for backward compatibility
- **Posters**: Always `{id}.jpg` (shared across episodes of same series)
- **Actor images**: `{cacheDir}\{service}\actors\{name}.jpg`
- **TMDB poster fallback**: If `tmdb.org\{id}.jpg` not found, tries `imdb.com\{id}.jpg`

### Cache XML Format

```xml
<ThemeFile> <!-- actually the root tag, name is legacy -->
  <MovieInfo>
    <ID>tt1234567</ID>
    <Title>The Matrix</Title>
    <Year>1999</Year>
    <Genres>Action, Sci-Fi</Genres>
    <ContentRating>R</ContentRating>
    <Countries>USA</Countries>
    <Runtime>136</Runtime>
    <Storyline>...</Storyline>
    <Directors>Lana Wachowski, Lilly Wachowski</Directors>
    <Writers>Lana Wachowski, Lilly Wachowski</Writers>
    <Stars>Keanu Reeves, Laurence Fishburne, Carrie-Anne Moss</Stars>
    <Rating>8.7</Rating>
    <RatingMax>10</RatingMax>
    <Votes>1234567</Votes>
    <Metascore>73</Metascore>
    <Season>-1</Season>
    <Episode>-1</Episode>
    <EpisodeName></EpisodeName>
    <EpisodeID></EpisodeID>
    <AirDate></AirDate>
    <Type>1</Type>
    <ActorId0>nm0000206</ActorId0>
    <ActorId1>nm0000401</ActorId1>
    ...
    <IMDbID>tt0133093</IMDbID>
    <IMDbRating>8.7</IMDbRating>
    <IMDbRatingMax>10</IMDbRatingMax>
    <IMDbVotes>1234567</IMDbVotes>
    <OMDbRatingsFetched>1</OMDbRatingsFetched>
    <Timestamp>133000000000000000</Timestamp>
  </MovieInfo>
</ThemeFile>
```

### JSON Parser

`JsonParser.h/cpp` — minimal recursive descent JSON parser for TMDB responses. Pool-allocated `JsonVal` nodes (stable integer indices). Public API:

| Method | Purpose |
|--------|---------|
| `Parse(const TCHAR*)` | Parse JSON string, return success |
| `Root()` / `Node(idx)` | Get root or child `JsonVal*` |
| `RootIdx()` | Get root node index |
| `FindKey(objIdx, key)` | Find child node by key in an object → returns node index or -1 |
| `GetStr/GetDbl/GetInt(key, def)` | Root-level scalar accessors |
| `GetArrLen(key)` / `GetArrAt(key, i)` | Root-level array accessors |
| `NGetStr/NGetDbl/NGetInt(idx, key, def)` | Node-level scalar accessors |
| `NGetArrLen/NGetArrAt(idx, key, i)` | Node-level array accessors |
| `NCount(idx)` / `NAt(idx, i)` | Array iteration (count + element index) |

**Note**: `FindKey()` is the primitive for obtaining array node indices. Use `FindKey(doc.RootIdx(), key)` for root-level arrays, `FindKey(idx, key)` for nested arrays.

## Filename Parsing

`ParseFileName.cpp` extracts movie metadata from filenames. Pipeline:

1. Strip extension
2. Strip release descriptors (1080p, HEVC, x265, AMZN, DDP5.1, remastered, extended, rarbg, yts, etc. — ~60 patterns)
3. Smart hyphen replacement — only replaces hyphens surrounded by spaces (separators), keeps word-internal hyphens like "Spider-Man"
4. Replace underscores with spaces
5. Extract year (4-digit number in parentheses or standalone)
6. Extract season/episode (S01E02 pattern)
7. Extract episode title (after S01E02)

## Preferences System

Preferences stored in `Preferences.xml` alongside the exe. Managed by `RPreferencesMgr2`.

| Section | Key | Default | Description |
|---------|-----|---------|-------------|
| (root) | LanguageFile | Languages\English.xml | UI language |
| (root) | ThemeFile | Themes\Dark.xml | UI theme |
| (root) | OMDbAPIKey | (empty) | OMDb API key |
| (root) | TMDBAPIKey | (empty) | TMDB API key |
| (root) | OMDbDailyLimit | 900 | OMDb daily request limit (buffer under 1000 cap) |
| (root) | AutoCategories | true | Auto-add Movies/TV categories |
| (root) | NormalizeRatings | false | Normalize ratings to 0-10 scale |
| Database | DatabaseFile | Database.xml | Database XML file |
| Database | IndexExtensions | asf\|avi\|mkv\|mp4\|mpeg\|mpg\|wmv | File extensions to index |
| Database | IndexDirectories | true | Index directory names as movies |
| Database | MaxInfoAge | 2 | Weeks before cache is considered stale |
| Database | CacheDirectory | Cache | Cache folder for XML/poster data |
| InfoService | OnlyUse | tmdb.org | Use only this service (empty = combined) |
| InfoService | Title/Year/Genres/etc. | tmdb.org | Per-field service assignment |
| MainWnd | x, y, cx, cy | 150, 30, 950, 750 | Window position/size |
| Search | Instantly | true | Search as you type |
| Search | Literally | false | Literal search (no stemming) |
| Search | Storyline | false | Search in storyline text |

## Theme System

Themes are XML files in `Themes/` directory. Two built-in themes:

- **Dark.xml** — Dark background (#171717), light text, dark toolbar. **Default.**
- **Light.xml** — White background, dark text, system-colored toolbar.

Theme files define colors, alphas, and fonts for: ReBar, ListView, CategoryBarButton, StatusBar, LogWnd, ToolBarButton, SearchBox, ScrollBar.

`CorrectThemes()` runs at startup and ensures theme files exist with correct default values (only writes missing entries via `false` parameter on `SetStr`).

`GeneralPage` has a theme dropdown that enumerates all XML files in the Themes directory.

## RClasses Library

### RString

Custom string class (NOT std::string). Key differences:

- **No `operator+=(TCHAR)`** — single char append doesn't work. Use 2-char string buffer instead:
  ```cpp
  TCHAR sz[2] = { ch, 0 };
  str += sz;
  ```
- **Has `operator const TCHAR*()`** — implicit cast to `const TCHAR*`, handles null by returning `""`
- **Has `operator<`** — added for `std::map<RString, ...>` compatibility, uses `_tcscmp`
- **`m_lpsz` and `m_cch` are protected** — don't access directly in external code; use the cast operator or public methods
- **Key methods**: `GetLength()`, `IsEmpty()`, `Find()`, `Left()`, `Mid()`, `Replace()`, `Trim()`, `MakeLower()`, `GetBuffer()`/`ReleaseBuffer()`

### RWindow

Base window class with a static `WndProc` that routes messages to virtual methods. Message map is in `RWindow.h` around line 200-300. To add a new message handler:

1. Add `case WM_XXX:` to the message map in `RWindow.h`
2. Add `void OnXxx(...)` virtual method with default empty implementation
3. Override in derived class

### RArray / RObArray

- `RArray<T>` — dynamic array of pointers
- `RObArray<T>` — dynamic array of objects (has copy constructor for `std::map` compatibility)

### RCriticalSection / RLock

- `RCriticalSection` — wraps Windows `CRITICAL_SECTION`
- `RLock` — RAII lock guard, acquired in constructor, released in destructor
- Located at `RClasses/RCriticalSection.h`

## Build System

### Visual Studio Project

- Project file: `MovieExplorer/VS2015/MovieExplorer.vcxproj`
- Solution file: `MovieExplorer/VS2015/MovieExplorer.sln`
- 4 configurations: Debug|Win32, Debug|x64, Release|Win32, Release|x64
- C++ standard: `/std:c++17`
- Runtime library: `/MDd` (Debug), `/MD` (Release) — dynamic linking to avoid Windows Defender false positives
- Preprocessor defines: `WIN32;_DEBUG;_WINDOWS;_USE_WINHTTP_;_UNICODE;UNICODE` (Debug)
- Key include: `..\\RClasses` for RClasses headers
- Linked libraries: `winhttp.lib` (for OMDb API calls via WinHTTP)

### CI (GitHub Actions)

Workflow: `.github/workflows/build.yml`

1. Checkout
2. Setup MSBuild (x86)
3. Setup MSVC dev cmd (x86)
4. Build Debug|Win32
5. Setup MSVC dev cmd (x64)
6. Build Debug|x64
7. Build Release|Win32
8. Build Release|x64
9. Upload Release Win32 exe as artifact
10. Upload Release x64 exe as artifact

### Build Quirks / Known Issues

- **`_USE_WINHTTP_`** macro — MUST be defined. The old name `_WINHTTPX_` collided with an internal `winhttp.h` macro that hid all WinHTTP API declarations.
- **No boost dependency** — `boost::regex` was replaced with `std::regex`. Boost include/lib paths were removed from the project.
- **`MinimalRebuild` disabled** in Debug|x64 — incompatible with `/std:c++17`
- **`__stat64` → `struct _stat64`** — was fixed in `RClasses/general.h` for MSVC compatibility
- **`RemoveDirectory` 2-arg call** — fixed in `CorrectThemes.cpp` (Windows API takes 1 arg)

## Error Status System

Movies can have these status values stored in `strIMDbID` and/or `strTMDBID`:

| Status | ID value | Meaning | Behavior |
|--------|----------|---------|----------|
| `DBI_STATUS_NONE` | (empty or valid ID) | Update in progress or pending | Will be updated |
| `DBI_STATUS_UPDATED` | Valid ID (e.g. `tt1234567` or TMDB numeric) | Successfully updated | Cache is valid |
| `DBI_STATUS_UNKNOWN` | `unknown` | Movie not found | Skipped on future updates |
| `DBI_STATUS_CONNERROR` | `connError` | Network error | Skipped on future updates |
| `DBI_STATUS_SCRAPEERROR` | `scrapeError` | Parse error in response | Skipped on future updates |
| `DBI_STATUS_RATELIMITED` | `rateLimited` | API rate limit hit | Skipped on future updates |

The "Recheck Failed" button (Database options page) clears `unknown`, `connError`, `scrapeError`, and `rateLimited` from both `strIMDbID` and `strTMDBID`, sets `bUpdated = false`, allowing the next update cycle to retry them.

## Navigation

### View System

Two views: **Grid** (`m_bListView = false`) and **List** (`m_bListView = true`).

- Grid view: Shows posters in columns. `m_nColumns = (width - 30) / 200 + 1` — partial columns render at right edge.
- List view: Shows one movie's full details (poster, metadata, links, episode info).

### Switching Views

- **ReBar toggle button** → sends `WM_SWITCHVIEW` to MainWnd → toggles `m_bListView`
- **Click movie in grid** → sends `WM_LISTVIEW_ITEM` to MainWnd → switches to list view, scrolls to that movie
- **Mouse back button (XBUTTON1)** → if in list view, switches to grid view
- **Mouse forward button (XBUTTON2)** → if in grid view, switches to list view
- **Category bar click** → filters movies, resets scroll to top (does NOT change view)

### Window Layout

```
┌─────────────────────────────────────────────┐
│ ReBar (toolbar + search + category buttons)  │
├──────┬──────────────────────────────────────┤
│      │                                       │
│ Cat  │  ListView or GridView                 │
│ Bar  │  (main content area)                  │
│      │                                       │
├──────┴──────────────────────────────────────┤
│ StatusBar                                    │
├──────────────────────────────────────────────┤
│ LogWnd (optional, resizable)                 │
└──────────────────────────────────────────────┘
```

## Custom Messages

Defined in `messages.h`:

| Message | Value | Purpose |
|---------|-------|---------|
| `WM_DBUPDATED` | WM_USER+513 | Database update completed — triggers redraw |
| `WM_SWITCHVIEW` | WM_USER+514 | Toggle between grid and list view |
| `WM_LISTVIEW_ITEM` | WM_USER+515 | Switch to list view and scroll to specific movie |
| `WM_LOG_WRITE` | WM_USER+516 | Write text to log window |
| `WM_STATUS` | WM_USER+517 | Update status bar text |
| `WM_PREFCHANGED` | WM_USER+518 | Preferences changed — reload theme/colors/fonts |
| `WM_SCALECHANGED` | WM_USER+519 | DPI/scale changed — resize fonts and sprites |
| `WM_CAPTUREM` | WM_USER+520 | Mouse capture notification |
| `WM_SETMOVIEUPDATE` | WM_USER+521 | Update specific movie in database after thread completes |
| `WM_UPDATETHREADEND` | WM_USER+522 | Update thread finished |

## Database File Format (Database.xml)

```xml
<DatabaseFile applicationID="MOVIEEXPLORER090">
  <Category name="All">
    <Directory path="C:\Movies" computerName="DESKTOP-ABC">
      <File name="The Matrix (1999).mkv" size="1234567890" time="1234567890" 
            resumeTime="-1" seen="true" hide="false"
            imdb.com="tt0133093" tmdb.org="603" />
    </Directory>
  </Category>
</DatabaseFile>
```

Note: `strEpisodeID` is NOT stored in the database file — it's only in the cache XML. The `strIMDbID` stores the series ID for TV shows, and `strTMDBID` stores the TMDB series ID.

## Important Implementation Details

### Threading Model

- `CDatabase::Update()` spawns one thread per movie via `_beginthreadex`
- Each thread gets a `UPDATETHREADDATA` struct with `hDatabaseWnd`, `eReady` (event), and `pDedup` (series dedup map)
- Threads send `DBM_SETMOVIEUPDATE` message back to database window when done
- `CDatabase::CancelUpdate()` waits for all threads to finish
- The `SeriesDedup` map is shared across threads (CRITICAL_SECTION protected)
- The `SeriesCache` map is per-thread (no synchronization needed)

### WinHTTP Usage

`ScrapeIMDb.cpp` uses WinHTTP (not WinInet) for HTTP requests:

```cpp
HINTERNET hSession = WinHttpOpen(L"MovieExplorer", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, ...);
HINTERNET hConnect = WinHttpConnect(hSession, L"www.omdbapi.com", INTERNET_DEFAULT_HTTPS_PORT, ...);
HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER, ...);
WinHttpSendRequest(hRequest, ...);
WinHttpReceiveResponse(hRequest, ...);
// Read response data...
WinHttpCloseHandle(hRequest/hConnect/hSession);
```

### Poster Rendering

- Posters are stored as JPEG byte arrays in `DBMOVIE::posterData`
- Loaded into `RMemoryDC` at scaled size (200×300 pixels base, scaled by DPI)
- Shadow sprite drawn behind poster via `m_sprShadow`
- Actor images loaded from cache or placeholder rendered

### DPI Scaling

All dimensions use `SCX()`/`SCY()` (scale) and `DUX()`/`DUY()` (dialog units) macros. Scale factor stored in preferences, adjustable via zoom controls.

## v1.0 Feature Summary

### What Was Changed from Upstream

1. **OMDb API migration** — Replaced broken IMDb HTML scraping with OMDb REST API
2. **WinHTTP** — Replaced `__stat64` / old HTTP code with proper WinHTTP calls
3. **Modern filename matching** — Retry without year, apostrophe variants, country suffix stripping, "Part X" retry, `OMDbPickBestResult` scoring
4. **Modern release descriptors** — ~60 patterns (1080p, HEVC, x265, AMZN, DDP5.1, etc.)
5. **Episode dedup** — `SeriesDedup` map avoids redundant series searches
6. **Season cache** — Per-thread `SeriesCache` fetches entire season in one API call
7. **Episode ID separation** — `strEpisodeID` stored separately from series ID
8. **Rate limit detection** — `DBI_STATUS_RATELIMITED` status, early exit on rate limit
9. **TV year range fix** — `"1989-2023"` extracts start year
10. **Hyphen-smart replacement** — Only replaces separator hyphens, keeps "Spider-Man"
11. **O'+Name detection** — "ONeal" → "O'Neal" apostrophe pattern
12. **Boost removal** — `boost::regex` → `std::regex`, entire boost dependency eliminated
13. **C++17** — `/std:c++17` required
14. **CI/CD** — GitHub Actions builds all 4 configs, uploads Release artifacts
15. **Dark theme default** — New installs default to Dark.xml theme
16. **Win8 theme removed** — 160 lines of dead code deleted
17. **Recheck Failed button** — One-click retry for failed movie lookups
18. **Partial grid columns** — Posters clip at right edge instead of blank space
19. **Mouse back/forward** — XBUTTON1/2 navigate between grid and list views
20. **Cache fallback** — Old-format cache files still readable for TV episodes
21. **Dynamic linking** — Release builds use `/MD` to avoid Windows Defender false positives
22. **TMDB integration** — TMDB as primary metadata source with OMDb for ratings only; `JsonParser` for JSON responses; `strTMDBID` + `bOMDbRatingsFetched` fields; `/find/{imdb_id}` migration path; 30ms throttle; actor headshot images; TMDB attribution label
23. **OMDb usage tracker** — Daily request counter (default limit 900), persisted to `Cache/omdb_usage.txt`, auto-resets on date change, "Used: X/Y today" display on Database page
24. **Resume/VLC integration** — Launch VLC with `--start-time`, snapshot-on-close resume position reading, auto-mark seen at 95%
25. **Refresh toolbar button** — One-click re-fetch: deletes cache XMLs + clears metadata + `bUpdated=false`
26. **Colored progress bar** — Resume progress shown as colored bar (green/yellow/text) with percentage in ListView
27. **Sort label renames** — Intuitive labels ("Title (A-Z)" not "Title / Title (descending)"); default sort changed to File Time Descending
28. **Dark theme scrollbar** — Custom dark scrollbar palette in `CorrectThemes.cpp`; light theme uses Windows default
29. **Metadata in database XML** — `<MovieInfo>` child element on each `<File>` makes DB self-contained for text data
30. **MovieMeter removal** — Deleted `ScrapeMovieMeter.h/cpp`, removed `strMovieMeterID` field, removed `moviemeter.nl` from all service dropdowns/logic
31. **Dutch language dropped** — Removed incomplete Dutch translations (85/93 strings, encoding issues); English built-in + French XML only
32. **Dead translations removed** — Deleted Croatian/Italian/Greek XML (76/93 strings, never loaded); fixed `.rc` `XMLFILE` case inconsistency
33. **Old VS projects removed** — Deleted VS2010/ and VS2013/ directories (CI uses VS2015 only)
34. **Win8/WinVista/Win2K helpers removed** — Deleted `IsWin8()`, `IsWinVista()`, `IsWin2K()` from `general.h`; removed last `IsWin8()` caller in `CategoryBar.cpp`
35. **Dead code cleanup** — Removed orphaned `.new` backup files, `IndonesiaLanguage` file, commented-out sort constants, commented-out theme fallback, dead ListView experiment

### What Still Needs Work

- **Parent directory fallback** — When filename is garbage, try directory name as movie title. Requires plumbing full path through to scraper.
- **Strip genre tags after " - "** — Risky, needs careful handling to avoid removing real title text.
- **Strip leading track numbers** — Risky because "P2" and "M3GAN" are real titles.
- **System theme option** — Auto-detect Windows dark/light mode preference. Currently just defaults to Dark.

## Common Gotchas

1. **`RString += TCHAR` doesn't compile** — Use `TCHAR sz[2] = { ch, 0 }; str += sz;`
2. **`RString::m_lpsz` is protected** — Use `(const TCHAR*)str` cast operator instead
3. **`_WINHTTPX_` must NOT be defined** — It's an internal `winhttp.h` macro. Use `_USE_WINHTTP_` instead.
4. **`std::map<RString, ...>` requires `operator<`** — Added to `RString.h`, uses `_tcscmp`
5. **`SeriesDedup` must be fully defined before use** — Can't forward-declare if storing by value. `Database.h` includes `UpdateThread.h`.
6. **`RObArray` in `std::map`** — Works because `RObArray` has copy constructor, but may be inefficient for large arrays.
7. **`CorrectPreferences` uses `false` parameter** — The `false` on `SetStr`/`SetInt`/`SetBool` means "only set if not already present", so existing user preferences aren't overwritten.
8. **`CorrectThemes` also uses `false` parameter** — Same pattern. Theme files are updated with new entries but existing customizations are preserved.
9. **Cache timestamp is in 100-nanosecond intervals** — `GetSystemTime()` returns FILETIME-style timestamp. `MaxInfoAge` (weeks) × 7 × 24 × 60 × 60 × 10000000 = maxTimeDiff.
10. **`DBM_GETMOVIEUPDATE` drives the update loop** — Each TV episode is a separate entry in the update queue.
11. **`JsonDoc::GetArrAt(key, i)` returns element at index i** — NOT the array node index. Use `FindKey(rootIdx, key)` to get array node indices for iteration. This was a critical bug that silently broke all array parsing in ScrapeTMDB.
12. **`JsonDoc` method names avoid single letters** — `S`, `D`, `I` etc. collide with Windows macros. Use `GetStr`, `GetDbl`, `GetInt` etc.
13. **`SeriesDedup` key includes service name** — Key format is `"service|title|year|type"` to avoid cross-service ID collision (IMDb `tt...` vs TMDB numeric).
14. **`bOMDbRatingsFetched` only set on definitive answer** — `true` on OMDb success or "no data" response; stays `false` on rate-limit/connection error to allow retry.
15. **TMDB `episode_run_time` is an array** — Not a scalar int. Must use `FindKey` + `Node()` to read first element.
