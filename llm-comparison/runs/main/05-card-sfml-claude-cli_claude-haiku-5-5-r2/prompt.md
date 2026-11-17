당신은 C++20 게임 프로그래머입니다. 아래 요구사항 문서에 적힌 미니게임을 SFML 3.1로 구현합니다.

이번 구현은 SFML 3.1 조건입니다.

- SFML 3.1 헤더(<SFML/...>)와 C++20 표준 라이브러리만 씁니다.
- main.cpp 한 파일만 작성합니다. 제공된 CMakeLists.txt와 assets/는 바꿀 수 없습니다.
- 답변에는 main.cpp 전체를 ```cpp 코드 블록 하나로 넣습니다.

# SFML 3.1 문서

<<< SFML/SFML@8835b6b:readme.md >>>
[![SFML logo](https://www.sfml-dev.org/images/logo.png)](https://www.sfml-dev.org)

# SFML — Simple and Fast Multimedia Library

SFML is a simple, fast, cross-platform and object-oriented multimedia API. It provides access to windowing, graphics, audio and network. It is written in C++ and has bindings for various languages such as C, .Net, Ruby, Python.

## State of Development

Development is focused on version 3 in the `master` branch.
No more features are planned for the 2.x release series.

## CMake Template

The easiest way to get started with SFML is our [CMake-based project template](https://github.com/SFML/cmake-sfml-project/blob/master/README.md).
This template will automatically download and build SFML alongside your own application.
Read the README for full instructions on how to use it.

## Download

-   You can get the latest official release on [SFML's website](https://www.sfml-dev.org/download.php).
-   You can also get the source code of the current development version from the [Git repository](https://github.com/SFML/SFML).
-   Alternatively, you can get the latest snapshot / artifact builds from the [artifacts storage](https://artifacts.sfml-dev.org/by-branch/master/).

## Install

Follow the instructions of the [tutorials](https://www.sfml-dev.org/tutorials/), there is one for each platform/compiler that SFML supports.

## Learn

There are several places to learn SFML:

-   The [official tutorials](https://www.sfml-dev.org/tutorials/)
-   The [online API documentation](https://www.sfml-dev.org/documentation/)
-   The [community wiki](https://github.com/SFML/SFML/wiki/)

## Community

Here are some useful community links:

-   [Discord](https://discord.gg/nr4X7Fh)
-   [Twitter](https://twitter.com/sfmldev)
-   [Forum](https://en.sfml-dev.org/forums/) ([French](https://fr.sfml-dev.org/forums/))

## Contribute

SFML is an open-source project, and it needs your help to go on growing and improving. If you want to get involved and suggest some additional features, file a bug report or submit a patch, please have a look at the [contribution guidelines](https://www.sfml-dev.org/contribute.php).

## Authors

-   Laurent Gomila (laurent@sfml-dev.org)
-   Marco Antognini (hiura@sfml-dev.org)
-   binary1248 (binary1248@hotmail.com)
-   Lukas Dürrenberger (eXpl0it3r@sfml-dev.org)
-   Jonathan De Wachter (dewachter.jonathan@gmail.com)
-   Jan Haller (bromeon@sfml-dev.org)
-   Mario Liebisch (mario@sfml-dev.org)
-   Stefan Schindler (tank@sfml-dev.org)
-   Artur Moreira (artturmoreira@gmail.com)
-   Vittorio Romeo (vittorioromeo@sfml-dev.org)
-   Chris Thrasher (thrasher@sfml-dev.org)
-   And many other members of the SFML community

## License

The SFML libraries and source code are distributed under the [zlib/libpng license](https://opensource.org/licenses/Zlib). See [license.md](license.md). External libraries used by SFML are distributed under their own licenses.

In short, SFML is free for any use (commercial or personal, proprietary or open-source). You can use SFML in your project without any restriction. You can even omit to mention that you use SFML -- although it would be appreciated.

## External libraries used by SFML

-   [_stb_image_ and _stb_image_write_](https://github.com/nothings/stb) are [public domain](https://github.com/nothings/stb/blob/master/LICENSE)
-   [_freetype_](https://gitlab.freedesktop.org/freetype/freetype) is under the [FreeType license or the GPL license](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/LICENSE.TXT)
-   [_libogg_](https://gitlab.xiph.org/xiph/ogg) is under the [BSD license](https://gitlab.xiph.org/xiph/ogg/-/blob/master/COPYING)
-   [_libvorbis_](https://gitlab.xiph.org/xiph/vorbis) is under the [BSD license](https://gitlab.xiph.org/xiph/vorbis/-/blob/master/COPYING)
-   [_libflac_](https://gitlab.xiph.org/xiph/flac) is under the [BSD license](https://gitlab.xiph.org/xiph/flac/-/blob/master/COPYING.Xiph)
-   [_dr\_mp3_](https://github.com/mackron/dr_libs) is [public domain or under the MIT No Attribution license](https://github.com/mackron/dr_libs/blob/master/LICENSE)
-   [_miniaudio_](https://github.com/mackron/miniaudio) is [public domain or under the MIT No Attribution license](https://github.com/mackron/miniaudio/blob/master/LICENSE)
-   [_cpp-unicodelib_](https://github.com/yhirose/cpp-unicodelib) is under the [MIT license](https://github.com/yhirose/cpp-unicodelib/blob/master/LICENSE)
-   [_HarfBuzz_](https://github.com/harfbuzz/harfbuzz) is under the [Old MIT license](https://github.com/harfbuzz/harfbuzz/blob/main/COPYING)
-   [_SheenBidi_](https://github.com/Tehreer/SheenBidi) is under the [Apache license](https://github.com/Tehreer/SheenBidi/blob/master/LICENSE)
-   [_qoi_](https://github.com/phoboslab/qoi) is under the [MIT license](https://github.com/phoboslab/qoi/blob/master/LICENSE)
-   [_Mbed TLS_](https://github.com/Mbed-TLS/mbedtls) is under the [Apache license or the GPL license](https://github.com/Mbed-TLS/mbedtls/blob/main/LICENSE)
-   [_wepoll_](https://github.com/piscisaureus/wepoll) is under the [BSD license](https://github.com/piscisaureus/wepoll/blob/dist/LICENSE)
-   [_libssh2_](https://github.com/libssh2/libssh2) is under the [BSD license](https://github.com/libssh2/libssh2/blob/master/COPYING)

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/migration/sfml-3.0.md >>>
# Migration from SFML 3.0 to SFML 3.1

Welcome to SFML 3.1!  
This release brings major improvements to text layout, networking, audio device management, and more.

## Dependencies

We've introduced a bunch of new dependencies to cover all the improvements:

- FetchContent / System Dependency
    - [HarfBuzz](https://github.com/harfbuzz/harfbuzz) for complex text layouts
    - [SheenBidi](https://github.com/Tehreer/SheenBidi) for correct bidirectional text rendering
    - [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) for TLS support
    - [libssh2](https://github.com/libssh2/libssh2) for SFTP support
- Header-only
    - [cpp-unicodelib](https://github.com/yhirose/cpp-unicodelib) for proper Unicode support
    - [wepoll](https://github.com/piscisaureus/wepoll) for an improved SocketSelector
    - [qoi](https://github.com/phoboslab/qoi) to support the QOI image format

## Deprecations

Deprecated functions remain available and fully functional, though you may receive a compiler warning.
It is recommended to migrate to the alternatives described below, as they generally provide noticable improvements.

### Font Kerning

The `sf::Font::getKerning` overload that uses `std::uint32_t` to represent Unicode code points has been deprecated.
Use the overload that takes `char32_t` instead.
The behavior is identical - only the parameter type changes.

v3.0:
```cpp
float kerning = font.getKerning(static_cast<std::uint32_t>('A'),
                                static_cast<std::uint32_t>('V'),
                                30);
```

v3.1:
```cpp
float kerning = font.getKerning(U'A', U'V', 30);
```

### Character Positions in Text

`sf::Text::findCharacterPos(std::size_t index)` has been deprecated in favor of `getShapedGlyphs()`.

The old function returned the global-space position of a single character.
The new function returns the full list of `ShapedGlyph` objects produced during text shaping, giving you access to each glyph's local position, cluster ID, text direction, and baseline in addition to the underlying `sf::Glyph` data.

```cpp
struct ShapedGlyph
{
    Glyph         glyph;         // Glyph data
    Vector2f      position;      // Local position within the text
    std::uint32_t cluster;       // Cluster ID (multiple glyphs may share a cluster)
    TextDirection textDirection; // Direction of the surrounding text run
    float         baseline;      // Baseline Y position of the line this glyph is on
    std::size_t   vertexOffset;  // Starting offset into the vertex buffer
    std::size_t   vertexCount;   // Number of vertices for this glyph
};
```

To migrate, iterate over the shaped glyphs and use the `position` field.

!!! info "Local vs Global Coordinates"

    Note that `getShapedGlyphs()` returns positions in local coordinates, while `findCharacterPos()` returned global coordinates.
    Apply the text's transform yourself if you need global coordinates.

v3.0:
```cpp
// Find the position of the 5th character (global space)
sf::Vector2f pos = text.findCharacterPos(4);
```

v3.1:
```cpp
// Find the position of the glyph belonging to the 5th character (local space)
const auto& glyphs = text.getShapedGlyphs();
if (glyphs.size() > 4)
{
    sf::Vector2f localPos = glyphs[4].position;
    // Convert to global space if needed:
    sf::Vector2f globalPos = text.getTransform().transformPoint(localPos);
}
```

Multiple Unicode code points can form a single grapheme cluster (for example, a base character combined with a combining accent).
When this happens, several consecutive entries in the glyph list will share the same `cluster` value.
If you are implementing a text cursor, treat all glyphs in the same cluster as a single unit.

### Real-Time Touch Input

`sf::Touch::isDown(unsigned int finger)` and both overloads of `sf::Touch::getPosition(unsigned int finger)` have been deprecated.
On Android these functions could crash when queried for a finger that is not currently down; the event-driven approach is always safe.

Use the `position` member of the relevant touch events instead:

| Deprecated function      | Replacement                                                                                      |
|--------------------------|--------------------------------------------------------------------------------------------------|
| `sf::Touch::isDown`      | `sf::Event::TouchBegan` / `sf::Event::TouchEnded`                                                |
| `sf::Touch::getPosition` | `position` field of `sf::Event::TouchBegan`, `sf::Event::TouchMoved`, or `sf::Event::TouchEnded` |

v3.0:
```cpp
if (sf::Touch::isDown(0))
{
    sf::Vector2i pos = sf::Touch::getPosition(0);
    // use pos
}
```

v3.1:
```cpp
// In your event loop:
window.handleEvents([&](const sf::Event::TouchBegan& touch)
{
    if (touch.finger == 0)
    {
        sf::Vector2i pos = touch.position;
        // use pos
    }
},
[&](const sf::Event::TouchMoved& touch)
{
    if (touch.finger == 0)
    {
        sf::Vector2i pos = touch.position;
        // use pos
    }
});
```

### FTP Client

`sf::Ftp` has been deprecated in favor of the new `sf::Sftp` class (see [New Features](#sftp-client) below).
FTP transmits credentials and data in plain text, making SFTP the strongly preferred alternative for any new code.

### Hostname Resolution

`sf::IpAddress::resolve(std::string_view address)` has been deprecated.
It only resolves to IPv4 addresses and lacks control over the DNS servers or timeout.

Use `sf::Dns::resolve()` instead, which supports both IPv4 and IPv6 and gives you full control over the query (see [New Features](#dns-api) below).

v3.0:
```cpp
std::optional<sf::IpAddress> address = sf::IpAddress::resolve("www.sfml-dev.org");
if (address)
{
    // use *address
}
```

v3.1:
```cpp
std::optional<std::vector<sf::IpAddress>> addresses = sf::Dns::resolve("www.sfml-dev.org");
if (addresses && !addresses->empty())
{
    sf::IpAddress address = addresses->front();
    // use address
}
```

---

## New Features

### Runtime Version Query

`sf::version()` returns a `const sf::Version&` describing the SFML library that is loaded at runtime.
This is useful when your application is dynamically linked and you want to verify that the runtime library matches the headers you compiled against.

```cpp
const sf::Version& v = sf::version();
// v.major, v.minor, v.patch - numeric components
// v.isRelease               - false for development builds
// v.string                  - e.g. "3.1.0" or "3.1.0-dev"

if (v.major != SFML_VERSION_MAJOR || v.minor != SFML_VERSION_MINOR)
{
    // Mismatch between compile-time and runtime SFML version
}
```

### Improved Text Layout and Shaping

SFML 3.1 introduces Unicode-aware text shaping powered by HarfBuzz, SheenBidi, and cpp-unicodelib (Unicode 17.0).
This means glyphs are now positioned according to the rules of their script - including ligature substitution, mark attachment, and bidirectional reordering - rather than being placed character-by-character from a simple glyph table lookup.

For most plain ASCII or Latin-script text, the visual result is identical to before.
The improvements become apparent with complex scripts (Arabic, Hebrew, Devanagari, CJK, etc.), ligatures, combined marks, and right-to-left or vertical text.

#### Right-to-Left and Bidirectional Text

SFML 3.1 automatically detects and handles right-to-left (RTL) scripts such as Arabic and Hebrew, as well as mixed bidirectional (BiDi) text that combines LTR and RTL runs in the same string.
No configuration is required: assign a Unicode string and SFML applies the Unicode Bidirectional Algorithm automatically.

```cpp
// Pure RTL - Arabic or Hebrew text renders right-to-left automatically
sf::Text arabicText(font, U"\u0645\u0631\u062d\u0628\u0627", 30); // "مرحبا"

// Mixed BiDi - LTR English and RTL Hebrew in the same string
sf::Text bidiText(font, U"Hello \u05e9\u05dc\u05d5\u05dd World", 30);
```

Each `ShapedGlyph` in the list returned by `getShapedGlyphs()` carries a `textDirection` field that reflects the resolved direction of the run that glyph belongs to:

```cpp
for (const sf::Text::ShapedGlyph& g : text.getShapedGlyphs())
{
    if (g.textDirection == sf::Text::TextDirection::RightToLeft)
    {
        // This glyph belongs to a right-to-left run
    }
}
```

#### Line Alignment

By default, each line is aligned according to the direction of its dominant script: LTR lines are left-aligned and RTL lines are right-aligned.
You can override this with `setLineAlignment()`:

```cpp
// Default: script-direction-aware alignment
text.setLineAlignment(sf::Text::LineAlignment::Default);

// Force all lines to start from the left, regardless of script direction
text.setLineAlignment(sf::Text::LineAlignment::Left);

// Center all lines
text.setLineAlignment(sf::Text::LineAlignment::Center);

// Force all lines to end at the right
text.setLineAlignment(sf::Text::LineAlignment::Right);
```

This is useful for UI layouts where you want consistent visual alignment regardless of content language.

#### Vertical Text Orientation

`setTextOrientation()` switches the layout axis from horizontal to vertical.
This is primarily intended for East Asian scripts (Chinese, Japanese, Korean) whose fonts natively include vertical metrics.

```cpp
// Horizontal layout (default)
text.setTextOrientation(sf::Text::TextOrientation::Default);

// Vertical, top-to-bottom
text.setTextOrientation(sf::Text::TextOrientation::TopToBottom);

// Vertical, bottom-to-top
text.setTextOrientation(sf::Text::TextOrientation::BottomToTop);
```

!!! note "Note"

    When a vertical orientation is selected, advance widths and baseline positions are interpreted along the vertical axis.
    Fonts that do not provide native vertical metrics will use emulated values; the result may not be visually ideal for all scripts.

#### Cluster Grouping

Shaping can merge multiple input code points into a single rendered glyph (ligature) or decompose a single code point into several glyphs (base + mark).
Cluster grouping controls how this many-to-many mapping is exposed through `getShapedGlyphs()`, which matters when you need to relate a glyph back to a position in your input string - for example, to place a text cursor.

```cpp
// Character clusters (default) - good for most cursor-positioning use cases
text.setClusterGrouping(sf::Text::ClusterGrouping::Character);

// Grapheme clusters - groups combining sequences into a single cluster
// (e.g. a base character + combining accent = one cluster)
text.setClusterGrouping(sf::Text::ClusterGrouping::Grapheme);

// No grouping - each shaped glyph gets its own cluster value
text.setClusterGrouping(sf::Text::ClusterGrouping::None);
```

The `cluster` field of each `ShapedGlyph` maps back to an index into the input string.
Multiple consecutive glyphs may share the same `cluster` value when they form an indivisible unit (e.g. a ligature spanning two input characters).
Treat all glyphs with the same cluster value as a single cursor stop.

```cpp
// Walk shaped glyphs and collect the x-position of each unique cluster
// (suitable for building a cursor position table for LTR text)
std::map<std::uint32_t, float> cursorPositions;
for (const sf::Text::ShapedGlyph& g : text.getShapedGlyphs())
{
    cursorPositions.try_emplace(g.cluster, g.position.x);
}
```

#### Per-Glyph Style with `GlyphPreProcessor`

`setGlyphPreProcessor()` installs a callback that is invoked for every glyph during geometry generation.
It lets you override the `style`, `fillColor`, `outlineColor`, and `outlineThickness` on a glyph-by-glyph basis, enabling effects like syntax highlighting without managing multiple `sf::Text` objects.

The callback receives the shaped glyph (including its `cluster` index) and mutable references to the per-glyph rendering properties:

```cpp
// Color a specific word differently using its codepoint index range in the input string
// For input "I like flowers, muffins and waffles."
// "muffins" spans codepoint indices 16–22
text.setGlyphPreProcessor(
    [](const sf::Text::ShapedGlyph& glyph,
       std::uint32_t& /*style*/,
       sf::Color&     fillColor,
       sf::Color&     /*outlineColor*/,
       float&         /*outlineThickness*/)
    {
        if (glyph.cluster >= 16 && glyph.cluster <= 22)
        {
            fillColor = sf::Color::Red;
        }
    });
```

Because shaping may merge adjacent code points into ligatures (e.g. `fl` → `ﬂ`), safe per-word coloring uses the cluster index range rather than a glyph index range.
Any glyphs whose `cluster` falls within the word's codepoint range belong to that word, even if they are ligatures that span the word boundary.

The callback is invoked in geometry-generation order, which is **visual order** (not logical order).
For RTL text this means cluster values decrease as the callback progresses.

!!! warning

    Do not query `getLocalBounds()` from inside the callback; the geometry is still being built at that point.

### TLS / HTTPS Support

`sf::Http` now supports HTTPS.
Pass an `https://` URL to `sf::Http::setHost()` and SFML will establish a TLS-encrypted connection automatically.

```cpp
sf::Http http;
http.setHost("https://www.sfml-dev.org");

sf::Http::Request request("/");
sf::Http::Response response = http.sendRequest(request);
```

To disable server certificate verification (not recommended for production):
```cpp
sf::Http::Response response = http.sendRequest(request, sf::Time::Zero, /* verifyServer */ false);
```

### DNS API

The new `sf::Dns` namespace provides full DNS query support, replacing the deprecated `sf::IpAddress::resolve`.

**Resolve a hostname (A / AAAA records):**
```cpp
#include <SFML/Network/Dns.hpp>

std::optional<std::vector<sf::IpAddress>> addresses = sf::Dns::resolve("www.sfml-dev.org");
if (addresses)
{
    for (const sf::IpAddress& addr : *addresses)
    {
        if (addr.isV4())
            std::cout << "IPv4: " << addr.toString() << '\n';
        else
            std::cout << "IPv6: " << addr.toString() << '\n';
    }
}
```

**Query other record types:**
```cpp
// Name server records
std::vector<sf::String> ns = sf::Dns::queryNs("sfml-dev.org");

// Mail exchange records
std::vector<sf::Dns::MxRecord> mx = sf::Dns::queryMx("sfml-dev.org");
// mx[i].exchange - hostname of the mail server
// mx[i].preference - lower values are preferred

// Service records
std::vector<sf::Dns::SrvRecord> srv = sf::Dns::querySrv("_irc._tcp.sfml-dev.org");
// srv[i].target, srv[i].port, srv[i].weight, srv[i].priority

// Text records
std::vector<std::vector<sf::String>> txt = sf::Dns::queryTxt("sfml-dev.org");
```

**Get the machine's public IP address:**
```cpp
// IPv4
std::optional<sf::IpAddress> pubV4 = sf::Dns::getPublicAddress(sf::seconds(5));

// IPv6
std::optional<sf::IpAddress> pubV6 = sf::Dns::getPublicAddress(sf::seconds(5), sf::IpAddress::Type::IpV6);
```

All `sf::Dns` functions accept an optional list of custom DNS servers and an optional timeout.

### SFTP Client

The new `sf::Sftp` class provides a full SFTP (SSH File Transfer Protocol) client.
It supports both username/password and public-key authentication and replaces the deprecated `sf::Ftp`.

**Connecting and logging in:**
```cpp
#include <SFML/Network/Sftp.hpp>

sf::Sftp sftp;

auto addresses = sf::Dns::resolve("sftp.example.com");
sf::Sftp::Result result = sftp.connect(addresses->front());
if (result.getValue() != sf::Sftp::Result::Value::Ok)
{
    return; // connection failed
}

// Password authentication
result = sftp.login("alice", "hunter2");

// Or public-key authentication
result = sftp.login("alice",
                    std::string_view{publicKey},
                    std::string_view{privateKey},
                    "passphrase");
```

**File operations:**
```cpp
// List a directory
sf::Sftp::ListingResult listing = sftp.getDirectoryListing("/home/alice");
for (const auto& entry : listing.getListing())
{
    std::cout << entry.name << '\n';
}

// Download a file via callback
std::vector<std::uint8_t> buffer;
sftp.download("/home/alice/data.bin",
    [&buffer](const void* data, std::size_t size)
    {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        buffer.insert(buffer.end(), bytes, bytes + size);
        return true; // return false to abort
    });

// Upload a file via callback
std::string content = "hello, world";
std::size_t offset = 0;
sftp.upload("/home/alice/hello.txt",
    [&content, &offset](void* data, std::size_t& size) -> bool
    {
        std::size_t remaining = content.size() - offset;
        if (remaining == 0)
            return false; // signal end of data
        size = std::min(size, remaining);
        std::memcpy(data, content.data() + offset, size);
        offset += size;
        return true;
    });

// Other operations
sftp.createDirectory("/home/alice/newdir");
sftp.deleteFile("/home/alice/old.txt");
sftp.rename("/home/alice/a.txt", "/home/alice/b.txt");
sftp.disconnect();
```

### `sf::SocketSelector` Callback API

`sf::SocketSelector::add()` now accepts an optional ready callback, enabling a scalable, event-driven design that avoids iterating over every socket after each `wait()` call.
The traditional `isReady()` polling approach continues to work unchanged.

**Traditional approach (still valid):**
```cpp
selector.add(listener);
selector.add(client);

while (selector.wait())
{
    if (selector.isReady(listener))
    {
        // accept new connection
    }
    if (selector.isReady(client))
    {
        // receive from client
    }
}
```

**New callback approach:**
```cpp
selector.add(listener, sf::SocketSelector::Receive, [&](sf::SocketSelector::ReadinessType)
{
    // accept new connection
    auto client = std::make_unique<sf::TcpSocket>();
    listener.accept(*client);
    selector.add(*client, sf::SocketSelector::Receive,
        [c = client.get()](sf::SocketSelector::ReadinessType)
        {
            // receive from client
        });
    clients.push_back(std::move(client));
});

while (selector.wait())
{
    selector.dispatchReadyCallbacks();
}
```

The callback approach only needs to check ready sockets rather than all sockets with the poll approach, which matters when managing many simultaneous connections.

#### `sf::Http::setHost` Return Value

`sf::Http::setHost` now returns `bool` to indicate whether the provided host was resolved and is valid.

v3.0:
```cpp
http.setHost("https://www.sfml-dev.org"); // return value was void
```

v3.1:
```cpp
if (!http.setHost("https://www.sfml-dev.org"))
{
    // host could not be resolved
}
```

#### `sf::Http::sendRequest` Is Now `const`

`sf::Http::sendRequest` is now a `const` member function, allowing HTTP requests to be sent from a `const sf::Http` object or reference.

### Audio Playback Device Sample Rate

`sf::PlaybackDevice::getDeviceSampleRate()` returns the native sample rate of the currently active audio output device.
This can be used to configure audio sources to match the device's preferred rate and avoid unnecessary resampling.

```cpp
if (auto rate = sf::PlaybackDevice::getDeviceSampleRate())
{
    std::cout << "Device sample rate: " << *rate << " Hz\n";
}
else
{
    std::cout << "No playback device available\n";
}
```

### QOI Image Format

SFML 3.1 adds support for the [QOI (Quite OK Image)](https://qoiformat.org/) format.
QOI files can be loaded and saved just like any other supported format.
Detection on load is automatic via the `.qoi` file extension or the file's magic bytes.

```cpp
// Loading
sf::Image image;
if (!image.loadFromFile("texture.qoi"))
{
    return -1;
}

// Saving to file
image.saveToFile("output.qoi");

// Saving to memory
std::optional<std::vector<std::uint8_t>> data = image.saveToMemory("qoi");
```

### `sf::String` Accepts `std::string_view`

`sf::String` now has constructors that accept `std::string_view`, `std::wstring_view`, and `std::u32string_view`.
This removes the need to construct a `std::string` solely for the purpose of passing it to SFML.

```cpp
std::string_view sv = "hello, world";
sf::String s(sv); // no temporary std::string needed
```

### `sf::Vector3u` Type Alias

`sf::Vector3u` has been added as a type alias for `sf::Vector3<unsigned int>`, matching the existing `sf::Vector2u` convention.

```cpp
sf::Vector3u size{800u, 600u, 32u};
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/migration/sfml-2.md >>>
# Migration from SFML 2 to SFML 3

Welcome to SFML 3!
The SFML Team has put a lot of effort into delivering a library that is both familiar to existing users while also making significant improvements.
This document will walk you through how to upgrade your SFML 2 application to SFML 3.

One of the headline changes in SFML 3 is raising the C++ standard to C++17 thus bringing SFML into the world of modern C++!
This change has enabled a vast number of internal improvements as well as new opportunities for improving the API that will be discussed in this document.

## Compiler Requirements

Along with the upgrade from C++03 to C++17 you may need to upgrade your compiler.
C++17 support has been widespread in all major compiler implementations for years prior to SFML 3's release so in all likelihood the compiler you're already using will work.
In case you do need to upgrade, here are the minimum compiler versions.

| Compiler   | Version      |
| ---------- | ------------ |
| MSVC       | 16 (VS 2019) |
| GCC        | 9            |
| Clang      | 9            |
| AppleClang | 12           |

## CMake Targets

SFML 3 uses modern CMake convention for library targets which entails having a namespace in front of the target name.
These namespaces tell CMake "this is a target" whereas something like `sfml-graphics` might be a target or might be a precompiled library on disk like `libsfml-graphics.so`.
Fixing this ambiguity leads to more useful error messages when a given target can't be found due to, for example, forgetting a `find_package` call.
The component names used when calling `find_package` were also changed to capitalize the first letter.

| v2 Target       | v3 Target        |
| --------------- | ---------------- |
| `sfml-system`   | `SFML::System`   |
| `sfml-window`   | `SFML::Window`   |
| `sfml-graphics` | `SFML::Graphics` |
| `sfml-network`  | `SFML::Network`  |
| `sfml-audio`    | `SFML::Audio`    |
| `sfml-main`     | `SFML::Main`     |

| v2 Component | v3 Component |
| ------------ | ------------ |
| `system`     | `System`     |
| `window`     | `Window`     |
| `graphics`   | `Graphics`   |
| `network`    | `Network`    |
| `audio`      | `Audio`      |
| `main`       | `Main`       |


v2:
```cmake
find_package(SFML 2 REQUIRED COMPONENTS graphics audio network)
...
target_link_libraries(my_app PRIVATE sfml-graphics sfml-audio sfml-network)
```

v3:
```cmake
find_package(SFML 3 REQUIRED COMPONENTS Graphics Audio Network)
...
target_link_libraries(my_app PRIVATE SFML::Graphics SFML::Audio SFML::Network)
```

## Linux Dependencies

When using X11 as the backend on Linux, as opposed to DRM, `libxi-dev` is a newly required dependency.
This was introduced with the [raw mouse input support](../../../documentation/3.1.0/structsf_1_1Event_1_1MouseMovedRaw.html).

## `sf::Vector2<T>` Parameters

A common pattern in SFML 2 was to use pairs of scalar parameters to represent concepts like sizes or positions.
Take `sf::Transformable::setPosition(float, float)` for example.
The two parameters combine to represent a position in world space.

SFML 3 takes all of the APIs with pairs of parameters like `(float, float)` or `(unsigned int, unsigned int)` and converts them to their corresponding `sf::Vector2<T>` type like `sf::Vector2f` or `sf::Vector2u` to make the interface more expressive and composable.
This transition is often as simple as wrapping the two adjacent parameters with braces to construct the vector.

v2:
```cpp
sf::VideoMode videoMode(480, 640, 24);
sf::CircleShape circle(10);
circle.setPosition(10, 20);
sf::IntRect rect(250, 400, 50, 100);
```

v3:
```cpp
sf::VideoMode videoMode({480, 640}, 24);
sf::CircleShape circle(10);
circle.setPosition({10, 20});
sf::IntRect rect({250, 400}, {50, 100});
```

## Fixed Width Integers

SFML 2 contained various typedefs for fixed width integers.
Those are now replaced with the fixed width integers provided in the `<cstdint>` header from the standard library.

| v2           | v3              |
| ------------ | --------------- |
| `sf::Int8`   | `std::int8_t`   |
| `sf::Uint8`  | `std::uint8_t`  |
| `sf::Int16`  | `std::int16_t`  |
| `sf::Uint16` | `std::uint16_t` |
| `sf::Int32`  | `std::int32_t`  |
| `sf::Uint32` | `std::uint32_t` |
| `sf::Int64`  | `std::int64_t`  |
| `sf::Uint64` | `std::uint64_t` |

## `sf::Event`

SFML 3 uses `std::variant` under the hood to implement a totally new, type-safe API for events.
There are two main ways to use this new API.
Check out the new EventHandling example program to see these methods in practice.

### `sf::Event::getIf<T>`

The first option is based around `sf::Event::getIf<T>` and `sf::Event::is<T>`.
`getIf<T>` works by providing a template parameter which must be an event subtype.
Event subtypes are types like `sf::Event::Resized` or `sf::Event::MouseMoved`.
If the template argument matches the active event subtype then a pointer to the data is returned.
If that template argument is not the active event subtype then `nullptr` is returned.
`sf::Event::is<T>` more simply returns `true` if `T` matches the active event subtype.
`is<T>` is often used for subtypes like `sf::Event::Closed` which contain no data.
Here's what that looks like:

```cpp
while (window.isOpen())
{
    while (const std::optional event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window.close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
                window.close();
        }
    }

    // Remainder of main loop
}
```

Note how the API for getting events has changed slightly.
`sf::WindowBase::pollEvent` and `sf::WindowBase::waitEvent` now return a `std::optional<sf::Event>`.
These two functions _might_ return an event but they might not.
C++ lets you deduce the template parameter which is why you can write `const std::optional event` instead of `const std::optional<sf::Event> event`.
`const auto event` is another valid choice if you prefer a more terse expression.

### `sf::WindowBase::handleEvents`

The second option for processing events is via the new `sf::WindowBase::handleEvents` function.
This functions performs event visitation.
What this means is that you can provide callbacks which take different event subtypes as arguments.
Alternatively you may provide an object (or objects) with `operator()` implementations which handle the event subtypes you want to process.
Notably you do not have to provide callbacks for all possible event subtypes.
Depending on the current active event subtype, the corresponding callback is called.
Here's what that looks like:

```cpp
const auto onClose = [&window](const sf::Event::Closed&)
{
    window.close();
};

const auto onKeyPressed = [&window](const sf::Event::KeyPressed& keyPressed)
{
    if (keyPressed.scancode == sf::Keyboard::Scancode::Escape)
        window.close();
};

while (window.isOpen())
{
    window.handleEvents(onClose, onKeyPressed);

    // Remainder of main loop
}
```

## Window Styles and States

A new `sf::State` enumeration was added for specifying the state of the window which means whether the window is floating or fullscreen.
Here's a before-and-after showing how this affects constructing a window.

v2:
```cpp
sf::RenderWindow window(sf::VideoMode::getFullscreenModes().at(0), "Title", sf::Style::Fullscreen);
```

v3:
```cpp
sf::RenderWindow window(sf::VideoMode::getFullscreenModes().at(0), "Title", sf::State::Fullscreen);
```

## Scoped Enumerations

SFML 3 converts all old style unscoped enumerations to scoped enumerations.
This improves the type safety of the interface.
This means that the name of the enumeration is now part of the namespace required to access values of that enumeration.

For example, take the enumeration `sf::Keyboard::Key`.
`sf::Keyboard::A` becomes `sf::Keyboard::Key::A`.
The name of the enumeration now appears as a nested namespace when accessing one of the enumeration's values.

Here is a complete list of all enumerations which have undergone this change:

* `sf::BlendMode::Equation`
* `sf::BlendMode::Factor`
* `sf::Cursor::Type`
* `sf::Ftp::Response::Status`
* `sf::Ftp::TransferMode`
* `sf::Http::Request::Method`
* `sf::Http::Response::Status`
* `sf::Joystick::Axis`
* `sf::Keyboard::Key`
* `sf::Keyboard::Scan`
* `sf::Mouse::Button`
* `sf::Mouse::Wheel`
* `sf::PrimitiveType`
* `sf::Sensor::Type`
* `sf::Shader::Type`
* `sf::Socket::Status`
* `sf::Socket::Type`
* `sf::SoundSource::Status`
* `sf::VertexBuffer::Usage`

## `sf::Rect<T>`

`sf::Rect<T>` has been refactored from the four scalar values `top`, `left`, `width`, and `height` into two `sf::Vector2<T>`s named `position` and `size`.
This means that `sf::Rect<T>::getPosition()` and `sf::Rect<T>::getSize()` have been removed in favor of directly accessing the `position` and `size` data members.
The 4-parameter constructor was also removed in favor of the constructor which takes two `sf::Vector2<T>`s.

| v2        | v3            |
| --------- | ------------- |
| `.left`   | `.position.x` |
| `.top`    | `.position.y` |
| `.width`  | `.size.x`     |
| `.height` | `.size.y`     |

v2:
```cpp
sf::FloatRect rect(10, 20, 30, 40);
sf::Vector2f position = rect.getPosition();
sf::Vector2f size = rect.getSize();
```

v3:
```cpp
sf::FloatRect rect({10, 20}, {30, 40});
sf::Vector2f position = rect.position;
sf::Vector2f size = rect.size;
```

The two overloads of `sf::Rect<T>::intersects` have been replaced with one unified function called `sf::Rect<T>::findIntersection` which returns a `std::optional<Rect<T>>`.
This optional contains the overlapping area if the rectangles overlap.
Otherwise the optional is empty.

```cpp
sf::IntRect rect1({0, 0}, {200, 200});
sf::IntRect rect2({100, 100}, {200, 200});
std::optional<sf::IntRect> intersection = rect1.findIntersection(rect2);
// position={100, 100} size={100, 100}
```

## `sf::Angle`

All angles are now represented with a strong type named `sf::Angle`.
This type provides two functions for creating angles called `sf::degrees(float)` and `sf::radians(float)` which construct an angle from either some value of degrees or radians.
Operators (`+`, `-`, etc.) are provided to perform mathematical operations with angles.
If you need access to the raw angle as a `float` then you can use either `sf::Angle::asDegrees()` or `sf::Angle::asRadians()`.

v2:
```cpp
sf::RectangleShape shape(sf::Vector2f(50, 50));
shape.setRotation(90);
std::cout << "Rotation: " << shape.getRotation() << '\n';
```

v3:
```cpp
sf::RectangleShape shape({50, 50});
shape.setRotation(sf::degrees(90));
std::cout << "Rotation: " << shape.getRotation().asDegrees() << '\n';
```

## Renamed Functions

A number of functions have new names but otherwise have not changed their semantics.

| v2                                | v3                                |
| --------------------------------- | --------------------------------- |
| `sf::Font::loadFromFile`          | `sf::Font::openFromFile`          |
| `sf::Socket::getHandle`           | `sf::Socket::getNativeHandle`     |
| `sf::WindowBase::getSystemHandle` | `sf::WindowBase::getNativeHandle` |
| `sf::Texture::create`             | `sf::Texture::resize`             |
| `sf::RenderTexture::create`       | `sf::RenderTexture::resize`       |
| `sf::Image::create`               | `sf::Image::resize`               |
| `sf::Sound::getLoop`              | `sf::Sound::isLooping`            |
| `sf::Sound::setLoop`              | `sf::Sound::setLooping`           |
| `sf::SoundStream::getLoop`        | `sf::SoundStream::isLooping`      |
| `sf::SoundStream::setLoop`        | `sf::SoundStream::setLooping`     |

## Removal of Default Constructors

The default constructors `sf::Sound::Sound()`, `sf::Text::Text()`, and `sf::Sprite::Sprite()` were removed.
They can be replaced by the corresponding constructors which accept a resource type.

| v2                     | v3                                         |
| ---------------------- | ------------------------------------------ |
| `sf::Sound::Sound()`   | `sf::Sound::Sound(const sf::SoundBuffer&)` |
| `sf::Text::Text()`     | `sf::Text::Text(const sf::Font&)`          |
| `sf::Sprite::Sprite()` | `sf::Sprite::Sprite(const sf::Texture&)`   |

Now that these classes are guaranteed to be holding a reference to their corresponding resource type, the functions used to access to those resources can return a reference instead of a pointer.
These functions are `sf::Sound::getBuffer()`, `sf::Text::getFont()`, and `sf::Sprite::getTexture()`.

v2:
```cpp
const sf::SoundBuffer soundBuffer("sound.flac");
sf::Sound sound;
sound.setBuffer(soundBuffer);
```

v3:
```cpp
const sf::SoundBuffer soundBuffer("sound.flac");
sf::Sound sound(soundBuffer);
```

## `std::optional` Usage

SFML 3 makes liberal use of `std::optional` to express when a given function may not return a value.
Some of these usages have already been mentioned like `sf::WindowBase::pollEvent`.
Here are some more places where SFML 3 makes use of `std::optional`.

* `sf::IpAddress` uses `std::optional` to express how resolving an address from a string may not yield a usable IP address.
* `sf::Image::saveToMemory` returns a `std::optional` because the `sf::Image` may be empty or the underlying implementation may fail.
* `sf::SoundFileReader::open` returns a `std::optional` because the stream being opened may not be valid.
* `sf::Music::onLoop` and `sf::SoundStream::onLoop` returns a `std::optional` because if the objects are not in a looping state then there is nothing to return.
* `sf::InputStream` uses `std::optional` in various places.
Instead of returning `-1` to signal an error, `std::nullopt` can be returned.

LearnCpp.com is a great place to learn more about using `std::optional`.
Read more about that [here](https://www.learncpp.com/cpp-tutorial/stdoptional/).

## New Constructors for Loading Resources

The following classes gained constructors that allow for loading/opening resources in a single expression.
Upon failure they throw an `sf::Exception`.

* `sf::InputSoundFile`
* `sf::OutputSoundFile`
* `sf::Music`
* `sf::SoundBuffer`
* `sf::Font`
* `sf::Image`
* `sf::RenderTexture`
* `sf::Shader`
* `sf::Texture`
* `sf::FileInputStream`
* `sf::Cursor`

SFML 3 still supports the SFML 2 style of error handling in addition to these new constructors.

v2:
```cpp
sf::SoundBuffer soundBuffer;
if (!soundBuffer.loadFromFile("sound.wav"))
{
    // Handle error
}
```

v3:
```cpp
sf::SoundBuffer soundBuffer;
if (!soundBuffer.loadFromFile("sound.wav"))
{
    // Handle error
}

// OR

const sf::SoundBuffer soundBuffer("sound.wav");
```

## `sf::Vector2<T>` and `sf::Vector3<T>` Utility Functions

`sf::Vector2<T>` and `sf::Vector3<T>` gained a number of new functions for performing common mathematic operations on vectors.

| `sf::Vector2<T>` Function       | Description                              |
| ------------------------------- | ---------------------------------------- |
| `Vector2(T, sf::Angle)`         | Construct from polar coordinates         |
| `length()`                      | Get length                               |
| `lengthSquared()`               | Get length squared                       |
| `normalized()`                  | Get vector normalized to unit circle     |
| `angleTo(sf::Vector2)`          | Get angle to another vector              |
| `angle()`                       | Get angle from X axis                    |
| `rotatedBy(sf::Angle)`          | Get vector rotated by a given angle      |
| `projectedOnto(sf::Vector2)`    | Get vector projected onto another vector |
| `perpendicular()`               | Get perpendicular vector                 |
| `dot(sf::Vector2)`              | Get dot product                          |
| `cross(sf::Vector2)`            | Get Z component of cross product         |
| `componentWiseMul(sf::Vector2)` | Get component-wise multiple              |
| `componentWiseDiv(sf::Vector2)` | Get component-wise divisor               |

| `sf::Vector3<T>` Function       | Description                           |
| ------------------------------- | ------------------------------------- |
| `length()`                      | Get length                            |
| `lengthSquared()`               | Get length squared                    |
| `normalized()`                  | Get vector normalized to unit circle  |
| `dot(sf::Vector3)`              | Get dot product                       |
| `cross(sf::Vector3)`            | Get cross product                     |
| `componentWiseMul(sf::Vector3)` | Get component-wise multiple           |
| `componentWiseDiv(sf::Vector3)` | Get component-wise divisor            |

## Threading Primitives

`sf::Lock`, `sf::Mutex`, `sf::Thread`, `sf::ThreadLocal`, and `sf::ThreadLocalPtr` were removed and replaced with their equivalents from the standard library.
The standard library provides multiple options for threads, locks, and mutexes among other threading primitives.

| v2                   | v3                                               |
| -------------------- | ------------------------------------------------ |
| `sf::Lock `          | `std::lock_guard` or `std::unique_lock`          |
| `sf::Mutex`          | `std::mutex` or `std::recursive_mutex`           |
| `sf::Thread`         | `std::thread` or `std::jthread` (requires C++20) |
| `sf::ThreadLocal`    | `thread_local`                                   |
| `sf::ThreadLocalPtr` | `thread_local`                                   |

## Sound Samples and Channel Map

SFML 3 introduces the concept of a Channel Map which defines the mapping of the position in sample frame to the sound channel.
For example, if you have a sound frame with six different samples for a 5.1 sound system, the Channel Map defines how each of those samples map to which speaker channel.

SFML 2 always assumed the order as specified by OpenAL.

```cpp
auto samples = std::vector<std::int16_t>();
// ...

auto channelMap = std::vector<sf::SoundChannel>{
    sf::SoundChannel::FrontLeft,
    sf::SoundChannel::FrontCenter,
    sf::SoundChannel::FrontRight,
    sf::SoundChannel::BackRight,
    sf::SoundChannel::BackLeft,
    sf::SoundChannel::LowFrequencyEffects
};
auto soundBuffer = sf::SoundBuffer(samples.data(), samples.size(), channelMap.size(), 44100, channelMap);
auto sound = sf::Sound(soundBuffer);
```

This a breaking change for the following APIs:

- `bool sf::SoundBuffer::loadFromSamples(...)`
- `bool sf::SoundBuffer::update(...)`
- `void sf::SoundStream::initialize(...)`
- `bool sf::OutputSoundFile::openFromFile(...)`
- `bool sf::SoundFileWriter::open(...)`

## Deprecated APIs

SFML 3 removes all of the deprecated APIs in SFML 2.

| Deprecated API                      | Replacement                        |
| ----------------------------------- | ---------------------------------- |
| `sf::Event::MouseWheelEvent`        | `sf::Event::MouseWheelScrolled`    |
| `sf::RenderWindow::capture`         | See 1                              |
| `sf::RenderTexture::create`         | `sf::RenderTexture::resize`        |
| `sf::Shader::setParameter`          | `sf::Shader::setUniform`           |
| `sf::Text::setColor`                | `sf::Text::setFillColor`           |
| `sf::Text::getColor`                | `sf::Text::getFillColor`           |
| `sf::PrimitiveType::LinesStrip`     | `sf::PrimitiveType::LineStrip`     |
| `sf::PrimitiveType::TrianglesStrip` | `sf::PrimitiveType::TriangleStrip` |
| `sf::PrimitiveType::TrianglesFan`   | `sf::PrimitiveType::TriangleFan`   |
| `sf::PrimitiveType::Quads`          | See 2                              |
| `sf::Keyboard::BackSlash`           | `sf::Keyboard::Key::Backslash`     |
| `sf::Keyboard::BackSpace`           | `sf::Keyboard::Key::Backspace`     |
| `sf::Keyboard::Dash`                | `sf::Keyboard::Key::Dash`          |
| `sf::Keyboard::Quote`               | `sf::Keyboard::Key::Apostrophe`    |
| `sf::Keyboard::Return`              | `sf::Keyboard::Key::Enter`         |
| `sf::Keyboard::SemiColon`           | `sf::Keyboard::Key::Semicolon`     |
| `sf::Keyboard::Tilde`               | `sf::Keyboard::Key::Grave`         |

1. `sf::RenderWindow::capture` can be recreated by using an `sf::Texture` and its `sf::Texture::update(const Window&)` function to copy its contents into an `sf::Image` instead.
2. `sf::PrimitiveType::Quads` can be replaced by another primitive type.
This is not a drop-in replacement but rather will require refactoring your code to work with a new geometry.
One viable option is to use `sf::PrimitiveType::Triangles` where two adjacent triangles join to form what was previously one quad.

## Anti-Aliasing Renamed

SFML 3 capitalizes the `A` of `aliasing` for all the APIs.

* `sf::RenderTexture::getMaximumAntialiasingLevel()` becomes `sf::RenderTexture::getMaximumAntiAliasingLevel()`
* `sf::ContextSettings::antialiasingLevel` becomes `sf::ContextSettings::antiAliasingLevel`

## CoordinateType for RenderStates

The enum `sf::CoordinateType` was moved from the `sf::Texture` into its own dedicated enum class.

The `sf::RenderStates` class got a new member of type `sf::CoordinateType` to control how the texture coordinates will be interpreted.
By default SFML uses `sf::CoordinateType::Pixels`, while `sf::CoordinateType::Normalized` is the default for OpenGL.
Using `sf::CoordinateType::Normalized` with `sf::RenderStates` allows for using normalized textures with `sf::VertexArray` and `sf::VertexBuffer`.

The constructor for `sf::RenderStates` has changed.

v2:
```cpp
auto renderStates = sf::RenderStates(sf::BlendAlpha,
                                     transform,
                                     texture,
                                     nullptr);
```

v3:
```cpp
auto renderStates = sf::RenderStates(sf::BlendMode::BlendAlpha,
                                     transform,
                                     sf::CoordinateType::Pixels,
                                     texture,
                                     nullptr);
```

## Other Minor Changes

SFML 3 includes various smaller changes that ought to be mentioned.

* Changed the parameter order of the `sf::Text` constructor, so that the provided font is always the first parameter
* Reverted to default value of CMake's `BUILD_SHARED_LIBS` which means SFML now builds static libraries by default
* Changed `sf::String` interface to use `std::u16string` and `std::u32string`
* Removed `sf::ContextSettings` constructor in favor of aggregate initialization
* Removed `sf::View::reset` in favor of assigning from a new `sf::View` object
* Removed `sf::Vertex` constructors in favor of aggregate initialization
* Renamed `sf::Mouse::Button::XButton1` and `sf::Mouse::Button::XButton2` enumerators to `sf::Mouse::Button::Extra1` and `sf::Mouse::Button::Extra2`
* Removed NonCopyable.hpp header in favor of using built-in language features for disabling copy operators
* Converted the following classes to namespaces: `sf::Clipboard`, `sf::Keyboard`, `sf::Joystick`, `sf::Listener`, `sf::Mouse`, `sf::Sensor`, `sf::Touch`, `sf::Vulkan`
* Removed `sf::SoundStream::setProcessingInterval` as miniaudio matches the internal processing rate to the underlying backend

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/system/time.md >>>
# Handling time

## Time in SFML

Unlike many other libraries where time is a uint32 number of milliseconds or a float number of seconds, SFML doesn't impose any specific unit or type for time values.
Instead it leaves this choice to the user through a flexible class: [`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation").
All SFML classes and functions that manipulate time values use this class.

[`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation") represents a time period (in other words, the time that elapses between two events).
It is _not_ a date-time class which would represent the current year/month/day/hour/minute/second as a timestamp.
It's just a value that represents a certain amount of time, and how to interpret it depends on the context where it is used.

## Converting time

A [`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation") value can be constructed from different source units: seconds, milliseconds and microseconds.
There is a free function to turn each of them into a [`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation"):

```cpp
sf::Time t1 = sf::microseconds(10000);
sf::Time t2 = sf::milliseconds(10);
sf::Time t3 = sf::seconds(0.01f);
```

Note that these three times are all equal.

Similarly, a [`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation") can be converted back to either seconds, milliseconds or microseconds:

```cpp
sf::Time time = ...;

std::int64_t usec = time.asMicroseconds();
std::int32_t msec = time.asMilliseconds();
float        sec  = time.asSeconds();
```

Conversions with the C++ standard library's `std::chrono::duration` are supported in two ways.

1. An implicit constructor which accepts any `std::chrono::duration` specialization.
1. An implicit conversion operator to any `std::chrono::duration` specialization.

```cpp
sf::Time time = std::chrono::milliseconds(100); // (1) Implicitly constructs from std::chrono::milliseconds
std::this_thread::sleep_for(time); // (2) Implicitly converts to std::chrono::nanoseconds
```

These two conversions allow for seamless interoperation with interfaces that use the `<chrono>` header.

## Playing with time values

[`sf::Time`](../../../documentation/3.1.0/classsf_1_1Time.html "sf::Time documentation") is just an amount of time, so it supports arithmetic operations such as addition, subtraction, comparison, etc.
Times can also be negative.

```cpp
sf::Time t1 = ...;
sf::Time t2 = t1 * 2;
sf::Time t3 = t1 + t2;
sf::Time t4 = -t3;

bool b1 = (t1 == t2);
bool b2 = (t3 > t4);
```

`sf::Time` is fully `constexpr`-enabled so all of these operations can be performed in a compile-time context.

## Measuring time

Now that we've seen how to manipulate time values with SFML, let's see how to do something that almost every program needs: measuring the time elapsed.

SFML has a very simple class for measuring time: [`sf::Clock`](../../../documentation/3.1.0/classsf_1_1Clock.html "sf::Clock documentation").
It provides a variety of functions for manipulating and querying the elapsed time.

```cpp
sf::Clock clock; // starts the clock
...
sf::Time elapsed1 = clock.getElapsedTime();
std::cout << elapsed1.asSeconds() << std::endl;
clock.restart();
...
sf::Time elapsed2 = clock.getElapsedTime();
std::cout << elapsed2.asSeconds() << std::endl;
...
clock.stop(); // stops the clock
std::cout << std::boolalpha << clock.isRunning() << std::endl;
clock.reset(); // resets elapsed time to zero
...
clock.start(); // starts the clock
sf::Time elapsed3 = clock.getElapsedTime();
std::cout << elapsed3.asSeconds() << std::endl;
```

Note that `restart` also returns the elapsed time, so that you can avoid the slight gap that would exist if you had to call `getElapsedTime` explicitly before `restart`.

Here is an example that uses the time elapsed at each iteration of the game loop to update the game logic:

```cpp
sf::Clock clock;
while (window.isOpen())
{
    sf::Time elapsed = clock.restart();
    updateGame(elapsed);
    ...
}
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/system/angle.md >>>
# Handling Angles

## Angles in SFML

[`sf::Angle`](../../../documentation/3.1.0/classsf_1_1Angle.html "sf::Angle documentation") represents an angle in a unit-agnostic manner so that you can reason about angles in terms of degrees or radians.
SFML defines angles as following the [right-hand rule](https://en.wikipedia.org/wiki/Right-hand_rule).
This means that if the X-axis points to the right in the window and the Y-axis points down, then a positive angle rotates clockwise.
All SFML classes that work with angles use `sf::Angle`

All `sf::Angle` functions are `constexpr`-enabled so anything shown here can be done at compile-time.

## Constructing angles

There are two factory functions for constructing an angle: `sf::degrees` and `sf::radians`.

```cpp
sf::Angle angle1 = sf::degrees(180);
sf::Angle angle2 = sf::radians(3.1415f);
```

These two functions construct approximately equal angles from either degrees or radians so that you don't have to think about units in your own code.
Feel free to use either function as you see fit.
`sf::Angle` will handle any necessary unit conversions internally.

## Manipulating angles

`sf::Angle` provides a collection of operators for arithmetic operations.
You can add, subtract, multiply, and divide angles.
There are also modulo, equality, inequality, and comparison operators.

```cpp
sf::Angle angle1 = sf::degrees(10);
angle1 *= 2.f; // 20 degrees
sf::Angle angle2 = angle1 + sf::radians(0.5f); // 48.6 degrees
angle2 = -angle2; // -48.6 degrees
angle2 /= 3.f; // -16.2 degrees

bool equal = (angle1 == angle2); // false
bool inequal = (angle1 != angle2); // true
```

`sf::Angle`s can exist outside range [-pi, pi) or [0, 2pi).
An angle may have a value of 720 degrees, for example.
If you would like to get a new angle wrapped to its equivalent value within a smaller range, there are two functions for doing so, `sf::Angle::wrapSigned` and `sf::Angle::wrapUnsigned`.

- `wrapSigned` will return a new angle wrapped to the range [-pi, pi).
- `wrapUnsigned` will return a new angle wrapped to the range [0, 2pi).

```cpp
sf::Angle angle1 = sf::degrees(540).wrapUnsigned(); // 180 degrees

sf::Angle angle2 = sf::radians(2 * 3.1415f).wrapSigned(); // 0 degrees
```

After transformation the angles will occupy the same location on the unit circle.

## User defined literals

C++ user-defined literals provide a convenient shorthand for writing angles.
These literals exist in the `sf::Literals` namespace.
Bring that namespace into the current scope to access them.

```cpp
using namespace sf::Literals;
sf::Angle angle1 = 45_deg; // 45 degrees
sf::Angle angle2 = angle1 + 3.1415_rad; // 225 degrees
```

## Accessing the underlying value

If you would like to format an angle as text or pass the value to a function like `std::sin`, there are two functions for accessing the angle as a raw `float`: `sf::Angle::asDegrees` and `sf::Angle::asRadians`.

```cpp
sf::Angle angle1 = sf::radians(2);
std::cout << angle1.asDegrees() << std::endl;

sf::Angle angle2 = sf::degrees(270);
std::cout << std::sin(angle2.asRadians()) << std::endl;
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/system/stream.md >>>
# User data streams

!!! advanced "Advanced topic"

    This tutorial requires some familiarity with inheritance and virtual functions.
    SFML can already load resources from files and from memory. You only need a custom stream if your data comes from somewhere else, such as a ZIP archive, an encrypted file or the network, so feel free to skip this tutorial for now.

## Introduction

SFML has several resource classes: images, fonts, sounds, etc.
In most programs, these resources will be loaded from files with the help of their `{load|open}FromFile` function or corresponding constructor.
In a few other situations, resources will be packed directly into the executable or in a big data file and loaded from memory with `{load|open}FromMemory`.
These functions cover _almost_ all the possible use cases -- but not all.

Sometimes you want to load files from unusual places, such as a compressed/encrypted archive, or a remote network location for example.
For these special situations, SFML provides a third loading option: `loadFromStream` or `openFromStream`.
This function reads data using an abstract [`sf::InputStream`](../../../documentation/3.1.0/classsf_1_1InputStream.html "sf::InputStream documentation") interface, which allows you to provide your own implementation of a stream class that works with SFML.

In this tutorial you'll learn how to write and use your own derived input stream.

## And standard streams?

Like many other languages, C++ already has a class for input data streams: `std::istream`.
In fact it has two.
`std::istream` is only the front-end.
The abstract interface to the custom data is `std::streambuf`.

Unfortunately, these classes are not very user friendly and can become very complicated if you want to implement non-trivial stuff.
The [Boost.Iostreams](http://www.boost.org/doc/libs/1_49_0/libs/iostreams/doc/index.html "Boost.Iostreams") library tries to provide a simpler interface to standard streams, but Boost is a big dependency and SFML cannot depend on it.

That's why SFML provides its own stream interface which is hopefully a lot more _simple and fast_.

## InputStream

The [`sf::InputStream`](../../../documentation/3.1.0/classsf_1_1InputStream.html "sf::InputStream documentation") class declares four virtual functions:

```cpp
class InputStream
{
public:
    virtual ~InputStream() = default;

    virtual std::optional<std::size_t> read(void* data, std::size_t size) = 0;

    virtual std::optional<std::size_t> seek(std::size_t position) = 0;

    virtual std::optional<std::size_t> tell() = 0;

    virtual std::optional<std::size_t> getSize() = 0;
};
```

**read** must extract _size_ bytes of data from the stream, and copy them to the supplied _data_ address.
It returns the number of bytes read, or `std::nullopt` on error.

**seek** must change the current reading position in the stream.
Its _position_ argument is the absolute byte offset to jump to (so it is relative to the beginning of the data, not to the current position).
It returns the new position, or `std::nullopt` on error.

**tell** must return the current reading position (in bytes) in the stream, or `std::nullopt` on error.

**getSize** must return the total size (in bytes) of the data which is contained in the stream, or `std::nullopt` on error.

To create your own working stream, you must implement every one of these four functions according to their requirements.

## FileInputStream and MemoryInputStream

Two classes exist to provide streams for the internal audio management.
`sf::FileInputStream` provides the read-only data stream of a file, while `sf::MemoryInputStream` serves the read-only stream from memory.
Both are derived from `sf::InputStream` and can thus be used polymorphically.

## Using an InputStream

Using a custom stream class is straightforward: instantiate it, and pass it to the constructor of the object that you want to load.

```cpp
sf::FileInputStream stream("image.png");
sf::Texture texture(stream);
```

## Examples

If you need a demonstration that helps you focus on how the code works, and not get lost in implementation details, you could take a look at the implementation of `sf::FileInputStream` or `sf::MemoryInputStream`.

Don't forget to check the forum and wiki.
Chances are that another user already wrote a [`sf::InputStream`](../../../documentation/3.1.0/classsf_1_1InputStream.html "sf::InputStream documentation") class that suits your needs.
And if you write a new one and feel like it could be useful to other people as well, don't hesitate to share!

## Common mistakes

Some resource classes are not loaded completely after `openFromStream` has been called.
Instead, they continue to read from their data source as long as they are used.
This is the case for [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation"), which streams audio samples as they are played, and for [`sf::Font`](../../../documentation/3.1.0/classsf_1_1Font.html "sf::Font documentation"), which loads glyphs on the fly depending on the text that is displayed.

As a consequence, the stream instance that you used to load a music or a font, as well as its data source, must remain alive as long as the resource uses it.
If it is destroyed while still being used, it results in undefined behavior (can be a crash, corrupt data, or nothing visible).

Another common mistake is to return whatever the internal functions return directly, but sometimes it doesn't match what SFML expects.
For example, in the `sf::FileInputStream` code, one might be tempted to write the `seek` function as follows:

```cpp
std::optional<std::size_t> FileInputStream::seek(std::size_t position)
{
    return std::fseek(m_file, position, SEEK_SET);
}
```

This code is wrong, because `std::fseek` returns zero on success, whereas SFML expects the new position to be returned.

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/window/window.md >>>
# Opening and managing an SFML window

## Introduction

This tutorial only explains how to open and manage a window.
Drawing stuff is beyond the scope of the sfml-window module: it is handled by the sfml-graphics module.
However, the window management remains exactly the same so reading this tutorial is important in any case.

## Opening a window

Windows in SFML are defined by the [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation") class.
A window can be created and opened directly upon construction:

```cpp
#include <SFML/Window.hpp>

int main()
{
    sf::Window window(sf::VideoMode({800, 600}), "My window");
    ...
}
```

The first argument, the *video mode*, defines the size of the window (the inner size, without the title bar and borders).
Here, we create a window with a size of 800x600 pixels.
The [`sf::VideoMode`](../../../documentation/3.1.0/classsf_1_1VideoMode.html "sf::VideoMode documentation") class has some interesting static functions to get the desktop resolution or the list of valid video modes for fullscreen mode.
Don't hesitate to have a look at its documentation.

The second argument is simply the title of the window.

This constructor accepts a third optional argument: a style, which allows you to choose which decorations and features you want.
You can use any combination of the following styles:

| Style                   | Description                                                                                                |
| ----------------------- | ---------------------------------------------------------------------------------------------------------- |
| `sf::Style::None`       | No decoration at all (useful for splash screens, for example)<br>This style cannot be combined with others |
| `sf::Style::Titlebar`   | The window has a titlebar                                                                                  |
| `sf::Style::Resize`     | The window can be resized and has a maximize button                                                        |
| `sf::Style::Close`      | The window has a close button                                                                              |
| `sf::Style::Default`    | The default style, which is a shortcut for `Titlebar | Resize | Close`                                     |

The fourth argument defines the window states which lets you pick between a floating window or a fullscreen window.

| State                   | Description       |
| ----------------------- | ----------------- |
| `sf::State::Windowed`   | Floating window   |
| `sf::State::Fullscreen` | Fullscreen window |

There's also a fifth optional argument, which defines OpenGL specific options which are explained in the [dedicated OpenGL tutorial](opengl.md "OpenGL tutorial").

If you want to create the window *after* the construction of the [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation") instance, or re-create it with a different video mode or title, you can use the `create` function instead.
It takes the exact same arguments as the constructor.

```cpp
#include <SFML/Window.hpp>

int main()
{
    sf::Window window;
    window.create(sf::VideoMode({800, 600}), "My window");

    ...
}
```

## Bringing the window to life

If you try to execute the code above with nothing in place of the "...", you will hardly see something.
First, because the program ends immediately.
Second, because there's no event handling -- so even if you added an endless loop to this code, you would see a dead window, unable to be moved, resized, or closed.

Let's add some code to make this program a bit more interesting:

```cpp
#include <SFML/Window.hpp>

int main()
{
    sf::Window window(sf::VideoMode({800, 600}), "My window");

    // run the program as long as the window is open
    while (window.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            // "close requested" event: we close the window
            if (event->is<sf::Event::Closed>())
                window.close();
        }
    }
}
```

The above code will open a window, and terminate when the user closes it.
Let's see how it works in detail.

First, we added a loop that ensures that the application will be refreshed/updated until the window is closed.
Most (if not all) SFML programs will have this kind of loop, sometimes called the *main loop* or *game loop*.

Then, the first thing that we want to do inside our game loop is check for any events that occurred.
Note that we use a `while` loop so that all pending events are processed in case there were several.
The `pollEvent` function returns an event if an event was pending, or `std::nullopt` if there was none.

Whenever we get an event, we must check its type (window closed? key pressed? mouse moved? joystick connected? ...), and react accordingly if we are interested in it.
In this case, we only care about the `sf::Event::Closed` event, which is triggered when the user wants to close the window.
At this point, the window is still open and we have to close it explicitly with the `close` function.
This enables you to do something before the window is closed, such as saving the current state of the application, or displaying a message.

A mistake that people often make is to forget the event loop, simply because they don't yet care about handling events (they use real-time inputs instead).
Without an event loop, the window will become unresponsive.
It is important to note that the event loop has two roles: in addition to providing events to the user, it gives the window a chance to process its internal events too, which is required so that it can react to move or resize user actions.

After the window has been closed, the main loop exits and the program terminates.

At this point, you probably noticed that we haven't talked about *drawing something* to the window yet.
As stated in the introduction, this is not the job of the sfml-window module, and you'll have to jump to the sfml-graphics tutorials if you want to draw things such as sprites, text, or shapes.

To draw stuff, you can also use OpenGL directly and totally ignore the sfml-graphics module.
[`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation") internally creates an OpenGL context and is ready to accept your OpenGL calls.
You can learn more about that in the [corresponding tutorial](opengl.md "OpenGL tutorial").

Don't expect to see something interesting in this window: you may see a uniform color (black or white), or the last contents of the previous application that used OpenGL, or... something else.

## Playing with the window

Of course, SFML allows you to play with your windows a bit.
Basic window operations such as changing the size, position, title or icon are supported, but unlike dedicated GUI libraries (Qt, wxWidgets), SFML doesn't provide advanced features.
SFML windows are only meant to provide an environment for OpenGL or SFML drawing.

```cpp
// change the position of the window (relatively to the desktop)
window.setPosition({10, 50});

// change the size of the window
window.setSize({640, 480});

// change the title of the window
window.setTitle("SFML window");

// get the size of the window
sf::Vector2u size = window.getSize();
auto [width, height] = size;

// check whether the window has the focus
bool focus = window.hasFocus();

...
```

You can refer to the API documentation for a complete list of [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation")'s functions.

In case you really need advanced features for your window, you can create one (or even a full GUI) with another library, and embed SFML into it.
To do so, you can use the other constructor, or `create` function, of [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation") which takes the OS-specific handle of an existing window.
In this case, SFML will create a drawing context inside the given window and catch all its events without interfering with the parent window management.

```cpp
sf::WindowHandle handle = /* specific to what you're doing and the library you're using */;
sf::Window window(handle);
```

If you just want an additional, very specific feature, you can also do it the other way round: create an SFML window and get its OS-specific handle to implement things that SFML itself doesn't support.

```cpp
sf::Window window(sf::VideoMode({800, 600}), "SFML window");
sf::WindowHandle handle = window.getNativeHandle();

// you can now use the handle with OS specific functions
```

Integrating SFML with other libraries requires some work and won't be described here, but you can refer to the dedicated tutorials, examples or forum posts.

## Controlling the framerate

Sometimes, when your application runs fast, you may notice visual artifacts such as tearing.
The reason is that your application's refresh rate is not synchronized with the vertical frequency of the monitor, and as a result, the bottom of the previous frame is mixed with the top of the next one.  
The solution to this problem is to activate *vertical synchronization*.
It is automatically handled by the graphics card, and can easily be switched on and off with the `setVerticalSyncEnabled` function:

```cpp
window.setVerticalSyncEnabled(true); // call it once after creating the window
```

After this call, your application will run at the same frequency as the monitor's refresh rate.

Sometimes `setVerticalSyncEnabled` will have no effect: this is most likely because vertical synchronization is forced to "off" in your graphics driver's settings.
It should be set to "controlled by application" instead.

In other situations, you may also want your application to run at a given framerate, instead of the monitor's frequency.
This can be done by calling `setFramerateLimit`:

```cpp
window.setFramerateLimit(60); // call it once after creating the window
```

Unlike `setVerticalSyncEnabled`, this feature is implemented by SFML itself, using a combination of [`sf::Clock`](../../../documentation/3.1.0/classsf_1_1Clock.html "sf::Clock documentation") and `sf::sleep`.
An important consequence is that it is not 100% reliable, especially for high framerates: `sf::sleep`'s resolution depends on the underlying operating system and hardware, and can be as high as 10 or 15 milliseconds.
Don't rely on this feature to implement precise timing.

Never use both `setVerticalSyncEnabled` and `setFramerateLimit` at the same time!
They would badly mix and make things worse.

## Things to know about windows

Here is a brief list of what you can and cannot do with SFML windows.

### You can create multiple windows

SFML allows you to create multiple windows, and to handle them either all in the main thread, or each one in its own thread (but... see below).
In this case, don't forget to have an event loop for each window.

### Multiple monitors are not correctly supported yet

SFML doesn't explicitly manage multiple monitors.
As a consequence, you won't be able to choose which monitor a window appears on, and you won't be able to create more than one fullscreen window.
This should be improved in a future version.

### Events must be polled in the window's thread

This is an important limitation of most operating systems: the event loop (more precisely, the `pollEvent` or `waitEvent` function) must be called in the same thread that created the window.
This means that if you want to create a dedicated thread for event handling, you'll have to make sure that the window is created in this thread too.
If you really want to split things between threads, it is more convenient to keep event handling in the main thread and move the rest (rendering, physics, logic, ...) to a separate thread instead.
This configuration will also be compatible with the other limitation described below.

### On macOS, windows and events must be managed in the main thread

Yep, that's true; macOS just won't agree if you try to create a window or handle events in a thread other than the main one.

### On Windows, a window which is bigger than the desktop will not behave correctly

For some reason, Windows doesn't like windows that are bigger than the desktop.
This includes windows created with `VideoMode::getDesktopMode()`: with the window decorations (borders and titlebar) added, you end up with a window which is slightly bigger than the desktop.

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/window/events.md >>>
# Events explained

## Introduction

This tutorial is a detailed list of window events.
It describes them, and shows how to (and how not to) use them.

## The sf::Event type

SFML uses a type-safe API for events.
There are two main ways to use this API.
For a comprehensive example of both approaches, check out the [EventHandling example program](https://github.com/SFML/SFML/tree/master/examples/event_handling "EventHandling example on GitHub").

### `sf::Event::getIf<T>`

The first option is based around `sf::Event`'s member functions `getIf<T>` and `is<T>`.
The template argument `T` must be an event subtype such as `sf::Event::Resized` or `sf::Event::MouseMoved`.
`getIf<T>` returns a pointer to the specific event subtype if the template argument matches the active event subtype.
Otherwise it returns `nullptr`.

`is<T>` returns `true` if the template argument matches the active event subtype.
Otherwise it returns `false`.
This is especially useful for subtypes like `sf::Event::Closed` which does not contain any data.

Note how the API for getting events has changed slightly from SFML 2.
`sf::WindowBase::pollEvent` and `sf::WindowBase::waitEvent` returns a `std::optional<sf::Event>`.
These two functions _might_ return an event but they might not.

Here's what that looks like:

```cpp
while (window.isOpen())
{
    while (const std::optional event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window.close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
                window.close();
        }
    }

    // Rest of the main loop
}
```

C++ lets you deduce the template parameter which is why you can write `const std::optional event` instead of `const std::optional<sf::Event> event`.
`const auto event` is another valid choice if you prefer a shorter expression.

### `sf::WindowBase::handleEvents`

The second option for processing events is via the `sf::WindowBase::handleEvents` function.
This functions performs event visitation.
What this means is that you can provide lambdas or other callables which take different event subtypes as arguments.
Alternatively you may provide an object (or objects) with `operator()` implementations which handle the event subtypes you want to process.
Depending on the current active event subtype, the corresponding callback is called.

!!! note

    You do not have to provide callbacks for all possible event subtypes.

Here's what that looks like:

```cpp
const auto onClose = [&window](const sf::Event::Closed&)
{
    window.close();
};

const auto onKeyPressed = [&window](const sf::Event::KeyPressed& keyPressed)
{
    if (keyPressed.scancode == sf::Keyboard::Scancode::Escape)
        window.close();
};

while (window.isOpen())
{
    window.handleEvents(onClose, onKeyPressed);

    // Rest of the main loop
}
```

## The Closed event

The `sf::Event::Closed` event is triggered when the user wants to close the window through any of the possible methods the window manager provides ("close" button, keyboard shortcut, etc.).
This event only represents a close request, meaning the window is not yet closed when the event is received.

Typically, one will just call `window.close()` in reaction to this event to actually close the window.
However, you may also want to do something else first, like saving the current application state or asking the user what to do.
If you don't do anything, the window remains open.

There's no event data associated with this event.

```cpp
if (event->is<sf::Event::Closed>())
    window.close();
```

## The Resized event

The `sf::Event::Resized` event is triggered when the window is resized, either through user action or programmatically by calling `window.setSize`.
You can use this event to adjust the rendering settings: the viewport if you use OpenGL directly, or the current view if you use sfml-graphics.

The data associated with this event contains the new size of the window.

```cpp
if (const auto* resized = event->getIf<sf::Event::Resized>())
{
    std::cout << "new width: " << resized->size.x << std::endl;
    std::cout << "new height: " << resized->size.y << std::endl;
}
```

## The FocusLost and FocusGained events

The `sf::Event::FocusLost` and `sf::Event::FocusGained` events are triggered when the window loses/gains focus, which happens when the user switches the currently active window.
When the window is out of focus, it doesn't receive keyboard events.
This event can be used e.g. if you want to pause your game when the window is inactive.

There's no event data associated with this event.

```cpp
if (event->is<sf::Event::FocusLost>())
    myGame.pause();

if (event->is<sf::Event::FocusGained>())
    myGame.resume();
```

## The TextEntered event

The `sf::Event::TextEntered` event is triggered when a character is typed.
This must not be confused with the `KeyPressed` event.
`TextEntered` interprets the user input and produces the appropriate printable character.
For example, pressing '^' then 'e' on a French keyboard will produce two `KeyPressed` events, but a single `TextEntered` event containing the 'ê' character.
It works with all the input methods provided by the operating system, even the most specific or complex ones.

This event is typically used to catch user input in a text field.

The data associated with this event contains the Unicode value of the entered character.
You can either put this data directly in a [`sf::String`](../../../documentation/3.1.0/classsf_1_1String.html "sf::String documentation"), or cast it to a `char` after making sure that it is in the ASCII range (0 - 127).

```cpp
if (const auto* textEntered = event->getIf<sf::Event::TextEntered>())
{
    if (textEntered->unicode < 128)
        std::cout << "ASCII character typed: " << static_cast<char>(textEntered->unicode) << std::endl;
}
```

Note that since they are part of the Unicode standard, some non-printable characters such as _backspace_ are generated by this event.
In most cases you'll need to filter them out.

!!! warning

    Many programmers use the `KeyPressed` event to get user input and start to implement crazy algorithms that try to interpret all the possible key combinations to produce correct characters.
    Don't do that!

## The KeyPressed and KeyReleased events

The `sf::Event::KeyPressed` and `sf::Event::KeyReleased` events are triggered when a keyboard key is pressed/released.

If a key is held, multiple `KeyPressed` events will be generated, at the default operating system delay (i.e. the same delay that applies when you hold a letter in a text editor).
To disable repeated `KeyPressed` events, you can call `window.setKeyRepeatEnabled(false)`.
On the flip side, it is obvious that `KeyReleased` events can never be repeated.

This event is the one to use if you want to trigger an action exactly once when a key is pressed or released, like making a character jump with space, or exiting something with escape.

Sometimes, people try to react to `KeyPressed` events directly to implement smooth movement.
Doing so will _not_ produce the expected effect, because when you hold a key you only get a few events (remember, the repeat delay).
To achieve smooth movement with events, you must use a boolean that you set on `KeyPressed` and clear on `KeyReleased`; you can then move (independently of events) as long as the boolean is set.
 
The other (easier) solution to produce smooth movement is to use real-time keyboard input with [`sf::Keyboard`](../../../documentation/3.1.0/namespacesf_1_1Keyboard.html "sf::Keyboard documentation") (see the [dedicated tutorial](inputs.md "Real-time inputs tutorial")).

The data associated with these events contains the scancode and key code of the pressed/released key, as well as the current state of the modifier keys (alt, control, shift, system).

Scancodes are unique values for each physical key on a keyboard, regardless of the language or layout, whereas key codes represent keys based on the user's chosen layout.
For instance, the Z key is in the bottom row to the left of the X key on a US layout.
Referring to the scancode for Z would identify this physical key location on any keyboard.
However, on a German layout, the same physical key is labeled Y.
Thus, using the key code for Y can lead to different physical key locations, depending on the chosen layout.

It is strongly recommended to use scancodes over key codes if the physical location of the keys is more important than the key values that depend on the keyboard layout, such as using WASD keys for movement.

```cpp
if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
{
    if (keyPressed->scancode == sf::Keyboard::Scan::Escape)
    {
        std::cout << "the escape key was pressed" << std::endl;
        std::cout << "scancode: " << static_cast<int>(keyPressed->scancode) << std::endl;
        std::cout << "code: " << static_cast<int>(keyPressed->code) << std::endl;
        std::cout << "control: " << keyPressed->control << std::endl;
        std::cout << "alt: " << keyPressed->alt << std::endl;
        std::cout << "shift: " << keyPressed->shift << std::endl;
        std::cout << "system: " << keyPressed->system << std::endl;
        std::cout << "description: " << sf::Keyboard::getDescription(keyPressed->scancode).toAnsiString() << std::endl;
        std::cout << "localize: " << static_cast<int>(sf::Keyboard::localize(keyPressed->scancode)) << std::endl;
        std::cout << "delocalize: " << static_cast<int>(sf::Keyboard::delocalize(keyPressed->code)) << std::endl;
    }
}
```

!!! note

    Note that some keys have a special meaning for the operating system, and will lead to unexpected behavior.
    An example is the F10 key on Windows, which "steals" the focus, or the F12 key which starts the debugger when using Visual Studio.

## The MouseWheelScrolled event

The `sf::Event::MouseWheelScrolled` event is triggered when a mouse wheel moves up or down, but also laterally if the mouse supports it.

The data associated with this event contains the number of ticks the wheel has moved, what the orientation of the wheel is and the current position of the mouse cursor.

```cpp
if (const auto* mouseWheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>())
{
    switch (mouseWheelScrolled->wheel)
    {
        case sf::Mouse::Wheel::Vertical:
            std::cout << "wheel type: vertical" << std::endl;
            break;
        case sf::Mouse::Wheel::Horizontal:
            std::cout << "wheel type: horizontal" << std::endl;
            break;
    }
    std::cout << "wheel movement: " << mouseWheelScrolled->delta << std::endl;
    std::cout << "mouse x: " << mouseWheelScrolled->position.x << std::endl;
    std::cout << "mouse y: " << mouseWheelScrolled->position.y << std::endl;
}
```

## The MouseButtonPressed and MouseButtonReleased events

The `sf::Event::MouseButtonPressed` and `sf::Event::MouseButtonReleased` events are triggered when a mouse button is pressed/released.
SFML supports 5 mouse buttons: left, right, middle (wheel), extra #1 and extra #2 (side buttons).

The data associated with these events contains the code of the pressed/released button, as well as the current position of the mouse cursor.

```cpp
if (const auto* mouseButtonPressed = event->getIf<sf::Event::MouseButtonPressed>())
{
    if (mouseButtonPressed->button == sf::Mouse::Button::Right)
    {
        std::cout << "the right button was pressed" << std::endl;
        std::cout << "mouse x: " << mouseButtonPressed->position.x << std::endl;
        std::cout << "mouse y: " << mouseButtonPressed->position.y << std::endl;
    }
}
```

## The MouseMoved event

The `sf::Event::MouseMoved` event is triggered when the mouse moves within the window.

!!! note

    This event is triggered even if the window isn't focused but only when the mouse moves within the inner area of the window, not when it moves over the title bar or window borders.

The data associated with this event contains the current position of the mouse cursor relative to the window.

```cpp
if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
{
    std::cout << "new mouse x: " << mouseMoved->position.x << std::endl;
    std::cout << "new mouse y: " << mouseMoved->position.y << std::endl;
}
```

## The MouseMovedRaw event

The `sf::Event::MouseMovedRaw` event is triggered when the mouse moved within the window even if the mouse is moved a distance too small to perceive.

While the `sf::Event::MouseMoved` position value is dependent on the screen resolution, the raw data of this event is not.
If the physical mouse is moved too little to cause the cursor on the screen to move at least a single pixel, no `sf::Event::MouseMoved` event will be generated.
In contrast, any movement information generated by the mouse independent of its sensor resolution will always generate an `sf::Event::MouseMovedRaw` event.

The data associated with this event contains the change in position of the mouse cursor relative to the window.

```cpp
if (const auto* mouseMovedRaw = event->getIf<sf::Event::MouseMovedRaw>())
{
    std::cout << "new mouse x: " << mouseMoved->delta.x << std::endl;
    std::cout << "new mouse y: " << mouseMoved->delta.y << std::endl;
}
```

!!! note

    Currently, raw mouse input events will only be generated on Windows and Linux.

## The MouseEntered and MouseLeft event

The `sf::Event::MouseEntered` and `sf::Event::MouseLeft` events are triggered when the mouse cursor enters/leaves the window.
There's no event data associated with these events.

```cpp
if (event->is<sf::Event::MouseEntered>())
    std::cout << "the mouse cursor has entered the window" << std::endl;

if (event->is<sf::Event::MouseLeft>())
    std::cout << "the mouse cursor has left the window" << std::endl;
```

## The JoystickButtonPressed and JoystickButtonReleased events

The `sf::Event::JoystickButtonPressed` and `sf::Event::JoystickButtonReleased` events are triggered when a joystick button is pressed/released.

SFML supports up to 8 joysticks and 32 buttons.

The data associated with these events contains the identifier of the joystick and the index of the pressed/released button.

```cpp
if (const auto* joystickButtonPressed = event->getIf<sf::Event::JoystickButtonPressed>())
{
    std::cout << "joystick button pressed!" << std::endl;
    std::cout << "joystick id: " << joystickButtonPressed->joystickId << std::endl;
    std::cout << "button: " << joystickButtonPressed->button << std::endl;
}
```

## The JoystickMoved event

The `sf::Event::JoystickMoved` event is triggered when a joystick axis moves.
Joystick axes are typically very sensitive.
That's why SFML uses a detection threshold to avoid spamming your event loop with tons of `JoystickMoved` events.
This threshold can be changed with the `sf::Window::setJoystickThreshold` function, in case you want to receive more or less joystick move events.

SFML supports 8 joystick axes: X, Y, Z, R, U, V, POV X and POV Y.
How they map to your joystick depends on its driver.

The data associated with this event contains the identifier of the joystick, the name of the axis, and its current position (in the range [-100, 100]).

```cpp
if (const auto* joystickMoved = event->getIf<sf::Event::JoystickMoved>())
{
    if (joystickMoved->axis == sf::Joystick::Axis::X)
    {
        std::cout << "X axis moved!" << std::endl;
        std::cout << "joystick id: " << joystickMoved->joystickId << std::endl;
        std::cout << "new position: " << joystickMoved->position << std::endl;
    }
}
```

## The JoystickConnected and JoystickDisconnected events

The `sf::Event::JoystickConnected` and `sf::Event::JoystickDisconnected` events are triggered when a joystick is connected/disconnected.

The data associated with this event contains the identifier of the connected/disconnected joystick.

```cpp
if (const auto* joystickConnected = event->getIf<sf::Event::JoystickConnected>())
    std::cout << "joystick connected: " << joystickConnected->joystickId << std::endl;

if (const auto* joystickDisconnected = event->getIf<sf::Event::JoystickDisconnected>())
    std::cout << "joystick disconnected: " << joystickDisconnected->joystickId << std::endl;
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/window/inputs.md >>>
# Keyboard, mouse and joystick

## Introduction

This tutorial explains how to access global input devices: keyboard, mouse and joysticks.
This must not be confused with events.
Real-time input allows you to query the global state of keyboard, mouse and joysticks at any time ("_is this button currently pressed?_", "_where is the mouse currently?_") while events notify you when something happens ("_this button was pressed_", "_the mouse has moved_").

## Keyboard

The namespace that provides access to the keyboard state is [`sf::Keyboard`](../../../documentation/3.1.0/namespacesf_1_1Keyboard.html "sf::Keyboard documentation").
It contains two overloads of the same function, `isKeyPressed`, which checks the current state of a key (pressed or released).

This function directly reads the keyboard state, ignoring the focus state of your window.
This means that `isKeyPressed` may return true, even if your window is inactive.

```cpp
if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
{
    // left key is pressed: move our character
    character.move({-1.f, 0.f});
}
```

Key codes are defined in the `sf::Keyboard::Key` enum.

```cpp
if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Right))
{
    // right key is pressed: move our character
    character.move({1.f, 0.f});
}
```

Scancodes are defined in the `sf::Keyboard::Scancode` enum.

## Mouse

The namespace that provides access to the mouse state is [`sf::Mouse`](../../../documentation/3.1.0/namespacesf_1_1Mouse.html "sf::Mouse documentation").

You can check if buttons are pressed:

```cpp
if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
{
    // left mouse button is pressed: shoot
    gun.fire();
}
```

Mouse button codes are defined in the `sf::Mouse::Button` enum.
SFML supports up to 5 buttons: left, right, middle (wheel), and two additional buttons whatever they may be.

You can also get and set the current position of the mouse, either relative to the desktop or to a window:

```cpp
// get the global mouse position (relative to the desktop)
sf::Vector2i globalPosition = sf::Mouse::getPosition();

// get the local mouse position (relative to a window)
sf::Vector2i localPosition = sf::Mouse::getPosition(window); // window is a sf::Window
```

```cpp
// set the mouse position globally (relative to the desktop)
sf::Mouse::setPosition({10, 50});

// set the mouse position locally (relative to a window)
sf::Mouse::setPosition({10, 50}, window); // window is a sf::Window
```

There is no function for reading the current state of the mouse wheel.
Since the wheel can only be moved relatively, it has no absolute state that can be queried.
By looking at a key you can tell whether it's pressed or released.
By looking at the mouse cursor you can tell where it is located on the screen.
However, looking at the mouse wheel doesn't tell you which "tick" it is on.
You can only be notified when it moves (`MouseWheelScrolled` event).

For more information on how to use events, see the [events tutorial](events.md).

## Joystick

The namespace that provides access to the joysticks' states is [`sf::Joystick`](../../../documentation/3.1.0/namespacesf_1_1Joystick.html "sf::Joystick documentation").

Joysticks are identified by their index (0 to 7, since SFML supports up to 8 joysticks).
Therefore, the first argument of every function of [`sf::Joystick`](../../../documentation/3.1.0/namespacesf_1_1Joystick.html "sf::Joystick documentation") is the index of the joystick that you want to query.

You can check whether a joystick is connected or not:

```cpp
if (sf::Joystick::isConnected(0))
{
    // joystick number 0 is connected
    ...
}
```

You can also get the capabilities of a connected joystick:

```cpp
// check how many buttons joystick number 0 has
unsigned int buttonCount = sf::Joystick::getButtonCount(0);

// check if joystick number 0 has a Z axis
bool hasZ = sf::Joystick::hasAxis(0, sf::Joystick::Axis::Z);
```

Joystick axes are defined in the `sf::Joystick::Axis` enum.
Since buttons have no special meaning, they are simply numbered from 0 to 31.

Finally, you can query the state of a joystick's axes and buttons as well:

```cpp
// is button 1 of joystick number 0 pressed?
if (sf::Joystick::isButtonPressed(0, 1))
{
    // yes: shoot!
    gun.fire();
}

// what's the current position of the X and Y axes of joystick number 0?
float x = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::X);
float y = sf::Joystick::getAxisPosition(0, sf::Joystick::Axis::Y);
character.move({x, y});
```

Joystick states are automatically updated when you check for events.
If you don't check for events, or need to query a joystick state (for example, checking which joysticks are connected) before starting your game loop, you'll have to manually call the `sf::Joystick::update()` function yourself to make sure that the joystick states are up to date.

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/draw.md >>>
# Drawing 2D stuff

## Introduction

As you learned in the previous tutorials, SFML's window module provides an easy way to open an OpenGL window and handle its events, but it doesn't help when it comes to drawing something.
The only option which is left to you is to use the powerful, yet complex and low level OpenGL API.

Fortunately, SFML provides a graphics module which will help you draw 2D entities in a much simpler way than with OpenGL.

## The drawing window

To draw the entities provided by the graphics module, you must use a specialized window class: [`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation").
This class is derived from [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation"), and inherits all its functions.
Everything that you've learnt about [`sf::Window`](../../../documentation/3.1.0/classsf_1_1Window.html "sf::Window documentation") (creation, event handling, controlling the framerate, mixing with OpenGL, etc.) is applicable to [`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation") as well.

On top of that, [`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation") adds high-level functions to help you draw things easily.
In this tutorial we'll focus on two of these functions: `clear` and `draw`.
They are as simple as their name implies: `clear` clears the whole window with the chosen color, and `draw` draws whatever object you pass to it.

Here is what a typical main loop looks like with a render window:

```cpp
#include <SFML/Graphics.hpp>

int main()
{
    // create the window
    sf::RenderWindow window(sf::VideoMode({800, 600}), "My window");

    // run the program as long as the window is open
    while (window.isOpen())
    {
        // check all the window's events that were triggered since the last iteration of the loop
        while (const std::optional event = window.pollEvent())
        {
            // "close requested" event: we close the window
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // clear the window with black color
        window.clear(sf::Color::Black);

        // draw everything here...
        // window.draw(...);

        // end the current frame
        window.display();
    }
}
```

Calling `clear` before drawing anything is mandatory, otherwise the contents from previous frames will be present behind anything you draw.
The only exception is when you cover the entire window with what you draw such that every pixel is drawn to.
In this case you can avoid calling `clear` (although it won't have a noticeable impact on performance).

Calling `display` is also mandatory, it takes what was drawn since the last call to `display` and displays it on the window.
Indeed, things are not drawn directly to the window, but to a hidden buffer.
This buffer is then copied to the window when you call `display` -- this is called *double-buffering*.

This clear/draw/display cycle is the only good way to draw things.
Don't try other strategies, such as keeping pixels from the previous frame, "erasing" pixels, or drawing once and calling display multiple times.
You'll get strange results due to double-buffering.
 
Modern graphics hardware and APIs are *really* made for repeated clear/draw/display cycles where everything is completely refreshed at each iteration of the main loop.
Don't be scared to draw 1000 sprites 60 times per second.
You're far below the millions of triangles that your computer can handle.

## What can I draw now?

Now that you have a main loop which is ready to draw, let's see what, and how, you can actually draw there.

SFML provides four kinds of drawable entities: three of them are ready to be used (_sprites_, *text* and *shapes*), the last one is the building block that will help you create your own drawable entities (_vertex arrays_).

Although they share some common properties, each of these entities come with their own nuances and are therefore explained in dedicated tutorials:

- [Sprite tutorial](sprite.md "Learn how to create and draw sprites")
- [Text tutorial](text.md "Learn how to create and draw text")
- [Shape tutorial](shape.md "Learn how to create and draw shapes")
- [Vertex array tutorial](vertex-array.md "Learn how to create and draw vertex arrays")

## Off-screen drawing

SFML also provides a way to draw to a texture instead of directly to a window.
To do so, use a [`sf::RenderTexture`](../../../documentation/3.1.0/classsf_1_1RenderTexture.html "sf::RenderTexture documentation") instead of a [`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation").
It has the same functions for drawing, inherited from their common base: [`sf::RenderTarget`](../../../documentation/3.1.0/classsf_1_1RenderTarget.html "sf::RenderTarget documentation").

```cpp
// create a 500x500 render-texture
sf::RenderTexture renderTexture({500, 500});

// drawing uses the same functions
renderTexture.clear();
renderTexture.draw(sprite); // or any other drawable
renderTexture.display();

// get the target texture (where the stuff has been drawn)
const sf::Texture& texture = renderTexture.getTexture();

// draw it to the window
sf::Sprite sprite(texture);
window.draw(sprite);
```

The `getTexture` function returns a read-only texture, which means that you can only use it, not modify it.
If you need to modify it before using it, you can copy it to your own [`sf::Texture`](../../../documentation/3.1.0/classsf_1_1Texture.html "sf::Texture documentation") instance and modify that instead.

[`sf::RenderTexture`](../../../documentation/3.1.0/classsf_1_1RenderTexture.html "sf::RenderTexture documentation") also has the same functions as [`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation") for handling views and OpenGL (see the corresponding tutorials for more details).

## Drawing from threads

!!! advanced "Advanced topic"

    The rest of this tutorial is about drawing from multiple threads.
    Most programs only draw from the main thread, so you can skip it unless you know that you need it.

SFML supports multi-threaded drawing, and you don't even have to do anything to make it work.
The only thing to remember is to deactivate a window before using it in another thread.
That's because a window (more precisely its OpenGL context) cannot be active in more than one thread at the same time.

```cpp
void renderingThread(sf::RenderWindow* window)
{
    // activate the window's context
    window->setActive(true);

    // the rendering loop
    while (window->isOpen())
    {
        // draw...

        // end the current frame
        window->display();
    }
}

int main()
{
    // create the window (remember: it's safer to create it in the main thread due to OS limitations)
    sf::RenderWindow window(sf::VideoMode({800, 600}), "OpenGL");

    // deactivate its OpenGL context
    window.setActive(false);

    // launch the rendering thread
    std::thread thread(&renderingThread, &window);

    // the event/logic/whatever loop
    while (window.isOpen())
    {
        ...
    }

    thread.join();
}
```

As you can see, you don't even need to bother with the activation of the window in the rendering thread, SFML does it automatically for you whenever it needs to be done.

Remember to always create the window and handle its events in the main thread for maximum portability.
This is explained in the [window tutorial](../window/window.md "Window tutorial").

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/sprite.md >>>
# Sprites and textures

## Vocabulary

Most (if not all) of you are already familiar with these two very common objects, so let's define them very briefly.

A texture is an image.
We call it "texture" because it has a very specific role: being mapped to a 2D entity.

A sprite is nothing more than a textured rectangle.

![](sprites-definition.png "Rectangular entity + texture = sprite!")

Ok, that was short but if you really don't understand what sprites and textures are, then you'll find a much better description on Wikipedia.

## Loading a texture

Before creating any sprite, we need a valid texture.
The class that encapsulates textures in SFML is, surprisingly, [`sf::Texture`](../../../documentation/3.1.0/classsf_1_1Texture.html "sf::Texture documentation").
Since the only role of a texture is to be loaded and mapped to graphical entities, almost all its functions are about loading and updating it.

The most common way of loading a texture is from an image file on disk, which is done with either the `loadFromFile` function or the corresponding constructor.

```cpp
sf::Texture texture("image.png"); // Throws sf::Exception if an error occurs
```

The `loadFromFile` function or constructor can sometimes fail with no obvious reason.
First, check the error message that SFML prints to the standard output (check the console).
If the message is unable to open file, make sure that the *working directory* (which is the directory that any file path will be interpreted relative to) is what you think it is: When you run the application from your desktop environment, the working directory is the executable folder.
However, when you launch your program from your IDE (Visual Studio, Code::Blocks, ...) the working directory might sometimes be set to the *project* directory instead.
This can usually be changed quite easily in the project settings.

You can also load an image file from memory (`loadFromMemory`), from a [custom input stream](../system/stream.md "Input streams tutorial") (`loadFromStream`), or from an image that has already been loaded (`loadFromImage`).
Corresponding constructors exist with the same parameters that throw an exception upon failure.
`loadFromImage` loads the texture from an [`sf::Image`](../../../documentation/3.1.0/classsf_1_1Image.html "sf::Image documentation"), which is a utility class that helps store and manipulate image data (modify pixels, create transparency channel, etc.).
The pixels of an [`sf::Image`](../../../documentation/3.1.0/classsf_1_1Image.html "sf::Image documentation") stay in system memory, which ensures that operations on them will be as fast as possible, in contrast to the pixels of a texture which reside in video memory and are therefore slow to retrieve or update but very fast to draw.

SFML supports most common image file formats.
The full list is available in the API documentation.

All these loading functions have an optional argument, which can be used if you want to load a smaller part of the image.

```cpp
// load a 32x32 rectangle that starts at (10, 10)
sf::Texture texture("image.png", false, sf::IntRect({10, 10}, {32, 32})); // Throws sf::Exception if an error occurs

// OR

sf::Texture texture;
if (!texture.loadFromFile("image.png", false, sf::IntRect({10, 10}, {32, 32})))
{
    // error...
}
```

The [`sf::IntRect`](../../../documentation/3.1.0/classsf_1_1Rect.html "sf::IntRect documentation") class is a simple utility type that represents a rectangle.
Its constructor takes the coordinates of the top-left corner, and the size of the rectangle.

If you don't want to load a texture from an image, but instead want to update it directly from an array of pixels, you can create it empty and update it later:

```cpp
// create an empty 200x200 texture
sf::Texture texture(sf::Vector2u(200, 200)); // Throws sf::Exception if an error occurs

// OR

if (!texture.resize({200, 200}))
{
    // error...
}
```

!!! note

    The contents of the texture are undefined at this point.

To update the pixels of an existing texture, you have to use the `update` function.
It has overloads for many kinds of data sources:

```cpp
// update a texture from an array of pixels
auto [width, height] = texture.getSize();
std::vector<std::uint8_t> pixels(width * height * 4); // * 4 because pixels have 4 components (RGBA)
...
texture.update(pixels.data());

// update a texture from a sf::Image
sf::Image image;
...
texture.update(image);

// update the texture from the current contents of the window
sf::RenderWindow window;
...
texture.update(window);
```

These examples all assume that the source is of the same size as the texture.
If this is not the case, i.e. if you want to update only a part of the texture, you can specify the coordinates of the sub-rectangle that you want to update.
You can refer to the documentation for further details.

Additionally, a texture has two properties that change how it is rendered.

The first property allows one to smooth the texture.
Smoothing a texture makes pixel boundaries less visible (but the image a little more blurry), which can be desirable if it is up-scaled.

```cpp
texture.setSmooth(true);
```

![](sprites-smooth.png "Smooth vs not smooth")

Since smoothing samples from adjacent pixels in the texture as well, it can lead to the unwanted side effect of factoring in pixels outside the selected texture area.
This can happen when your sprite is located at non-integer coordinates.

The second property allows a texture to be repeatedly tiled within a single sprite.

```cpp
texture.setRepeated(true);
```

![](sprites-repeated.png "Repeated vs not repeated")

This only works if your sprite is configured to show a rectangle which is larger than the texture, otherwise this property has no effect.

## Ok, can I have my sprite now?

Yes, you can now create your sprite.

```cpp
sf::Sprite sprite(texture);
```

... and finally draw it.

```cpp
// inside the main loop, between window.clear() and window.display()
window.draw(sprite);
```

If you don't want your sprite to use the entire texture, you can set its texture rectangle.

```cpp
sprite.setTextureRect(sf::IntRect({10, 10}, {32, 32}));
```

You can also change the color of a sprite.
The color that you set is modulated (multiplied) with the texture of the sprite.
This can also be used to change the global transparency (alpha) of the sprite.

```cpp
sprite.setColor(sf::Color(0, 255, 0)); // green
sprite.setColor(sf::Color(255, 255, 255, 128)); // half transparent
```

These sprites all use the same texture, but have a different color:

![](sprites-color.png "Coloring sprites")

Sprites can also be transformed: They have a position, an orientation, and a scale.

```cpp
// position
sprite.setPosition({10.f, 50.f}); // absolute position
sprite.move({5.f, 10.f}); // offset relative to the current position

// rotation
sprite.setRotation(sf::degrees(90)); // absolute angle
sprite.rotate(sf::degrees(14)); // offset relative to the current angle

// scale
sprite.setScale({0.5f, 2.f}); // absolute scale factor
sprite.scale({1.5f, 3.f}); // factor relative to the current scale
```

By default, the origin for these three transformations is the top-left corner of the sprite.
If you want to set the origin to a different point (for example the center of the sprite, or another corner), you can use the `setOrigin` function.

```cpp
sprite.setOrigin({25.f, 25.f});
```

Since transformation functions are common to all SFML entities, they are explained in a separate tutorial: [Transforming entities](transform.md "'Transforming entities' tutorial").

## The white square problem

You successfully loaded a texture, constructed a sprite correctly, and... all you see on your screen now is a white square.
What happened?

This is a common mistake.
When you set the texture of a sprite, all it does internally is store a *pointer* to the texture instance.
Therefore, if the texture is destroyed or moves elsewhere in memory, the sprite ends up with an invalid texture pointer.

This problem occurs when you write this kind of function:

```cpp
sf::Sprite loadSprite(const std::filesystem::path& filename)
{
    sf::Texture texture;
    texture.loadFromFile(filename);

    return sf::Sprite(texture);
} // error: the texture is destroyed here
```

Another common way to make this mistake is with containers like `std::vector` that can reallocate all of its elements when adding a new element.
Here's an example:

```cpp
std::vector<sf::Texture> textures;

auto& texture1 = textures.emplace_back("image1.png");
sf::Sprite sprite1(texture1);
auto& texture2 = textures.emplace_back("image2.png"); // This may reallocate!
sf::Sprite sprite2(texture2);
```

The second call to `emplace_back` may cause the entire vector to reallocate.
This means that `texture1` used to create `sprite1` now exists in a different location in memory and therefore `sprite1` contains an invalid texture pointer.

You must correctly manage the lifetime of your textures and make sure that they live as long as they are used by any sprite.

## The importance of using as few textures as possible

Using as few textures as possible is a good strategy, and the reason is simple: Changing the current texture is an expensive operation for the graphics card.
Drawing many sprites that use the same texture will yield the best performance.

Additionally, using a single texture allows you to group static geometry into a single entity (you can only use one texture per `draw` call), which will be much faster to draw than a set of many entities.
Batching static geometry involves other classes and is therefore beyond the scope of this tutorial.
For further details see the [vertex array](vertex-array.md "Vertex array tutorial") tutorial.

Try to keep this in mind when you create your animation sheets or your tilesets: Use as few textures as possible.

## Using sf::Texture with OpenGL code

!!! advanced "Advanced topic"

    The last section is only relevant if you mix SFML with your own OpenGL code.

If you're using OpenGL rather than the graphics entities of SFML, you can still use [`sf::Texture`](../../../documentation/3.1.0/classsf_1_1Texture.html "sf::Texture documentation") as a wrapper around an OpenGL texture object and use it along with the rest of your OpenGL code.

To bind a [`sf::Texture`](../../../documentation/3.1.0/classsf_1_1Texture.html "sf::Texture documentation") for drawing (basically `glBindTexture`), you call the `bind` static function:

```cpp
sf::Texture texture;
...

// bind the texture
sf::Texture::bind(&texture);

// draw your textured OpenGL entity here...

// bind no texture
sf::Texture::bind(nullptr);
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/shape.md >>>
# Shapes

## Introduction

SFML provides a set of classes that represent simple shape entities.
Each type of shape is a separate class, but they all derive from the same base class so that they have access to the same subset of common features.
Each class then adds its own specifics: a radius property for the circle class, a size for the rectangle class, points for the polygon class, etc.

## Common shape properties

### Transformation (position, rotation, scale)

These properties are common to all the SFML graphical classes, so they are explained in a separate tutorial: [Transforming entities](transform.md "'Transforming entities' tutorial").

### Color

One of the basic properties of a shape is its color.
You can change with the `setFillColor` function.

```cpp
sf::CircleShape shape(50.f);

// set the shape color to green
shape.setFillColor(sf::Color(100, 250, 50));
```

![A colored shape](shape-color.png)

### Outline

Shapes can have an outline.
You can set the thickness and color of the outline with the `setOutlineThickness` and `setOutlineColor` functions.

```cpp
sf::CircleShape shape(50.f);
shape.setFillColor(sf::Color(150, 50, 250));

// set a 10-pixel wide orange outline
shape.setOutlineThickness(10.f);
shape.setOutlineColor(sf::Color(250, 150, 100));
```

![An outlined shape](shape-outline.png)

By default, the outline is extruded outwards from the shape (e.g. if you have a circle with a radius of 10 and an outline thickness of 5, the total radius of the circle will be 15).
You can make it extrude towards the center of the shape instead, by setting a negative thickness.

To disable the outline, set its thickness to 0.
If you only want the outline, you can set the fill color to `sf::Color::Transparent`.

### Texture

Shapes can also be textured, just like sprites.
To specify a part of the texture to be mapped to the shape, you must use the `setTextureRect` function.
It takes the texture rectangle to map to the bounding rectangle of the shape.
This method doesn't offer maximum flexibility, but it is much easier to use than individually setting the texture coordinates of each point of the shape.

```cpp
sf::CircleShape shape(50);

// map a 100x100 textured rectangle to the shape
shape.setTexture(&texture); // texture is a sf::Texture
shape.setTextureRect(sf::IntRect({10, 10}, {100, 100}));
```

![A textured shape](shape-texture.png)

Note that the outline is not textured.  
It is important to know that the texture is modulated (multiplied) with the shape's fill color.
If its fill color is `sf::Color::White`, the texture will appear unmodified.  
To disable texturing, call `setTexture(nullptr)`.

## Drawing a shape

Drawing a shape is as simple as drawing any other SFML entity:

```cpp
window.draw(shape);
```

## Built-in shape types

### Rectangles

To draw rectangles, you can use the [`sf::RectangleShape`](../../../documentation/3.1.0/classsf_1_1RectangleShape.html "sf::RectangleShape documentation") class.
It has a single attribute: The size of the rectangle.

```cpp
// define a 120x50 rectangle
sf::RectangleShape rectangle({120.f, 50.f});

// change the size to 100x100
rectangle.setSize({100.f, 100.f});
```

![A rectangle shape](shape-rectangle.png)

### Circles

Circles are represented by the [`sf::CircleShape`](../../../documentation/3.1.0/classsf_1_1CircleShape.html "sf::CircleShape documentation") class.
It has two attributes: The radius and the number of sides.
The number of sides is an optional attribute.
It allows you to adjust the "quality" of the circle: Circles have to be approximated by polygons with many sides (the graphics card is unable to draw a perfect circle directly), and this attribute defines how many sides your circle approximation will have.
If you draw small circles, you'll probably only need a few sides.
If you draw big circles, or zoom on regular circles, you'll most likely need more sides.

```cpp
// define a circle with radius = 200
sf::CircleShape circle(200.f);

// change the radius to 40
circle.setRadius(40.f);

// change the number of sides (points) to 100
circle.setPointCount(100);
```

![A circle shape](shape-circle.png)

### Regular polygons

There's no dedicated class for regular polygons, in fact you can represent a regular polygon with any number of sides using the [`sf::CircleShape`](../../../documentation/3.1.0/classsf_1_1CircleShape.html "sf::CircleShape documentation") class: Since circles are approximated by polygons with many sides, you just have to play with the number of sides to get the desired polygons.
A [`sf::CircleShape`](../../../documentation/3.1.0/classsf_1_1CircleShape.html "sf::CircleShape documentation") with 3 points is a triangle, with 4 points it's a square, etc.

```cpp
// define a triangle
sf::CircleShape triangle(80.f, 3);

// define a square
sf::CircleShape square(80.f, 4);

// define an octagon
sf::CircleShape octagon(80.f, 8);
```

![Regular polygons](shape-regular.png)

### Convex shapes

The [`sf::ConvexShape`](../../../documentation/3.1.0/classsf_1_1ConvexShape.html "sf::ConvexShape documentation") class is the ultimate shape class: It allows you to define any *convex* shape.
SFML is unable to draw concave shapes.
If you need to draw a concave shape, you'll have to split it into multiple convex polygons.

To construct a convex shape, you must first set the number of points it should have and then define the points.

```cpp
// create an empty shape
sf::ConvexShape convex;

// resize it to 5 points
convex.setPointCount(5);

// define the points
convex.setPoint(0, {0.f, 0.f});
convex.setPoint(1, {150.f, 10.f});
convex.setPoint(2, {120.f, 90.f});
convex.setPoint(3, {30.f, 100.f});
convex.setPoint(4, {0.f, 50.f});
```

The order in which you define the points is very important.
They must *all* be defined either in clockwise or counter-clockwise order.
If you define them in an inconsistent order, the shape will be constructed incorrectly.

![A convex shape](shape-convex.png)

Although the name of [`sf::ConvexShape`](../../../documentation/3.1.0/classsf_1_1ConvexShape.html "sf::ConvexShape documentation") implies that it should only be used to represent convex shapes, its requirements are a little more relaxed.
In fact, the only requirement that your shape must meet is that if you went ahead and drew lines from its *center of gravity* to all of its points, these lines must be drawn in the same order.
You are not allowed to "jump behind a previous line".
Internally, convex shapes are automatically constructed using [triangle fans](http://en.wikipedia.org/wiki/Triangle_fan "Go to Wikipedia's article about triangle fans"), so if your shape is representable by a triangle fan, you can use [`sf::ConvexShape`](../../../documentation/3.1.0/classsf_1_1ConvexShape.html "sf::ConvexShape documentation").
With this relaxed definition, you can draw stars using [`sf::ConvexShape`](../../../documentation/3.1.0/classsf_1_1ConvexShape.html "sf::ConvexShape documentation") for example.

### Lines

There's no shape class for lines.
The reason is simple: If your line has a thickness, it is a rectangle.
If it doesn't, it can be drawn with a line primitive.

Line with thickness:

```cpp
sf::RectangleShape line({150.f, 5.f});
line.rotate(sf::degrees(45));
```

![A line shape drawn as a rectangle](shape-line-rectangle.png)

Line without thickness:

```cpp
std::array line =
{
    sf::Vertex{sf::Vector2f(10.f, 10.f)},
    sf::Vertex{sf::Vector2f(150.f, 150.f)}
};

window.draw(line.data(), line.size(), sf::PrimitiveType::Lines);
```

![A line shape drawn as a primitive](shape-line-primitive.png)

To learn more about vertices and primitives, you can read the tutorial on [vertex arrays](vertex-array.md "'Vertex arrays' tutorial").

## Antialiased shapes

There's no option to anti-alias a single shape.
To get anti-aliased shapes (i.e. shapes with smoothed edges), you have to enable anti-aliasing globally when you create the window or the render texture, with the corresponding attribute of the [`sf::ContextSettings`](../../../documentation/3.1.0/structsf_1_1ContextSettings.html "sf::ContextSettings documentation") structure.

```cpp
sf::ContextSettings settings;
settings.antiAliasingLevel = 8;

sf::RenderWindow window(sf::VideoMode({800, 600}), "SFML shapes", sf::Style::Default, sf::State::Windowed, settings);
```

![Aliased vs antialiased shape](shape-antialiasing.png)

Remember that anti-aliasing availability depends on the graphics card: It might not support it, or have it forced to disabled in the driver settings.

## Custom shape types

!!! advanced "Advanced topic"

    The rest of this tutorial shows how to create your own shape types by deriving from `sf::Shape`.
    The built-in shapes are enough for most needs, so you can skip it for now.

You can extend the set of shape classes with your own shape types.
To do so, you must derive from [`sf::Shape`](../../../documentation/3.1.0/classsf_1_1Shape.html "sf::Shape documentation") and override two functions:

- `getPointCount`: return the number of points in the shape
- `getPoint`: return a point of the shape

You must also call the `update()` protected function whenever any point in your shape changes, so that the base class is informed and can update its internal geometry.

Here is a complete example of a custom shape class: EllipseShape.

```cpp
class EllipseShape : public sf::Shape
{
public:
    explicit EllipseShape(sf::Vector2f radius = {0, 0}) : m_radius(radius)
    {
        update();
    }

    void setRadius(sf::Vector2f radius)
    {
        m_radius = radius;
        update();
    }

    sf::Vector2f getRadius() const
    {
        return m_radius;
    }

    std::size_t getPointCount() const override
    {
        return 30; // fixed, but could be an attribute of the class if needed
    }

    sf::Vector2f getPoint(std::size_t index) const override
    {
        static constexpr float pi = 3.141592654f;

        float angle = index * 2 * pi / getPointCount() - pi / 2;
        float x     = std::cos(angle) * m_radius.x;
        float y     = std::sin(angle) * m_radius.y;

        return m_radius + sf::Vector2f(x, y);
    }

private:
    sf::Vector2f m_radius;
};
```

![An ellipse shape](shape-ellipse.png)

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/text.md >>>
# Text and fonts

## Loading a font

Before drawing any text, you need to have an available font, just like any other program that prints text.
Fonts are encapsulated in the [`sf::Font`](../../../documentation/3.1.0/classsf_1_1Font.html "sf::Font documentation") class, which provides three main features: loading a font, getting glyphs (i.e. visual characters) from it, and reading its attributes.
In a typical program, you'll only have to make use of the first feature, loading the font, so let's focus on that first.

The most common way of loading a font is from a file on disk, which is done with the `openFromFile` function or its corresponding constructor.

```cpp
sf::Font font("arial.ttf"); // Throws sf::Exception if an error occurs

// OR

sf::Font font;
if (!font.openFromFile("arial.ttf"))
{
    // error...
}
```

Note that SFML won't load your system fonts automatically, i.e. `font.openFromFile("Courier New")` won't work.
Firstly, because SFML requires _file names_, not font names, and secondly because SFML doesn't have magical access to your system's font folder.
If you want to load a font, you will need to include the font file with your application just like every other resource (images, sounds, ...).

The `openFromFile` function or the corresponding constructor can sometimes fail with no obvious reason.
First, check the error message that SFML prints to the standard output (check the console).
If the message is unable to open file, make sure that the _working directory_ (which is the directory that any file path will be interpreted relative to) is what you think it is: When you run the application from your desktop environment, the working directory is the executable folder.
However, when you launch your program from your IDE (Visual Studio, Code::Blocks, ...) the working directory might sometimes be set to the _project_ directory instead.
This can usually be changed quite easily in the project settings.

You can also load a font file from memory (`openFromMemory`), or from a [custom input stream](../system/stream.md "Input streams tutorial") (`openFromStream`).
Corresponding constructors exist with the same parameters that throw an exception upon failure.

SFML supports most common font formats.
The full list is available in the API documentation.

That's all you need to do.
Once your font is loaded, you can start drawing text.

## Drawing text

To draw text, you will be using the [`sf::Text`](../../../documentation/3.1.0/classsf_1_1Text.html "sf::Text documentation") class.
It's very simple to use:

```cpp
sf::Text text(font); // a font is required to make a text object

// set the string to display
text.setString("Hello world");

// set the character size
text.setCharacterSize(24); // in pixels, not points!

// set the color
text.setFillColor(sf::Color::Red);

// set the text style
text.setStyle(sf::Text::Bold | sf::Text::Underlined);

...

// inside the main loop, between window.clear() and window.display()
window.draw(text);
```

![](text-draw.png "Drawing text")

Text can also be transformed: They have a position, an orientation, and a scale.
The functions involved are the same as for the [`sf::Sprite`](../../../documentation/3.1.0/classsf_1_1Sprite.html "sf::Sprite documentation") class and other SFML entities.
They are explained in the [Transforming entities](transform.md "'Transforming entities' tutorial") tutorial.

## How to avoid problems with non-ASCII characters?

Handling non-ASCII characters (such as accented European, Arabic, or Chinese characters) correctly can be tricky.
It requires a good understanding of the various encodings involved in the process of interpreting and drawing your text.
To avoid having to bother with these encodings, there's a simple solution: Use _wide literal strings_.

```cpp
text.setString(L"éèàç");
```

It is this simple "L" prefix in front of the string that makes it work by telling the compiler to produce a wide string.
Wide strings are a strange beast in C++: the standard doesn't say anything about their size (16-bit? 32-bit?), nor about the encoding that they use (UTF-16? UTF-32?).
However we know that on most platforms, if not all, they'll produce Unicode strings, and SFML knows how to handle them correctly.

Note that the C++11 standard supports new character types and prefixes to build UTF-8, UTF-16 and UTF-32 string literals.

It may seem obvious, but you also have to make sure that the font that you use contains the characters that you want to draw.
Indeed, fonts don't contain glyphs for all possible characters (there are more than 100,000 in the Unicode standard!).
An Arabic font won't be able to display Japanese text, for example.

## Making your own text class

!!! advanced "Advanced topic"

    The rest of this tutorial is about rendering text yourself with the glyphs of a font.
    `sf::Text` covers most needs, so you can skip it.

If [`sf::Text`](../../../documentation/3.1.0/classsf_1_1Text.html "sf::Text documentation") is too limited, or if you want to do something else with pre-rendered glyphs, [`sf::Font`](../../../documentation/3.1.0/classsf_1_1Font.html "sf::Font documentation") provides everything that you need.

You can retrieve the texture which contains all the pre-rendered glyphs of a certain size:

```cpp
const sf::Texture& texture = font.getTexture(characterSize);
```

It is important to note that glyphs are added to the texture when they are requested.
There are so many characters (remember, more than 100,000) that they can't all be generated when you load the font.
Instead, they are rendered on the fly when you call the `getGlyph` function (see below).

To do something meaningful with the font texture, you must get the texture coordinates of glyphs that are contained in it:

```cpp
sf::Glyph glyph = font.getGlyph(character, characterSize, bold);
```

`character` is the UTF-32 code of the character whose glyph that you want to get.
You must also specify the character size, and whether you want the bold or the regular version of the glyph.

The [`sf::Glyph`](../../../documentation/3.1.0/structsf_1_1Glyph.html "sf::Glyph documentation") structure contains three data members of interest.

- `advance` is the horizontal offset to apply to get the starting position of the next glyph in the text
- `bounds` contains the bounding rectangle of the glyph, which helps position it relative to the baseline of the text
- `textureRect` contains the texture coordinates of the glyph within the texture

You can also get some of the font's other metrics, such as the kerning between two characters or the line spacing (always for a certain character size):

```cpp
float lineSpacing = font.getLineSpacing(characterSize);

float kerning = font.getKerning(character1, character2, characterSize);
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/transform.md >>>

# Position, rotation, scale: Transforming entities

## Transforming SFML entities

All SFML classes (sprites, text, shapes) use the same interface for transformations: [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation").
This base class provides a simple API to move, rotate, and scale your entities.
It doesn't provide maximum flexibility, but instead defines an interface which is easy to understand and to use, and which covers 99% of all use cases -- for the remaining 1%, see the last chapters.

[`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") (and all its derived classes) defines four properties: **position**, **rotation**, **scale** and **origin**.
They all have their respective getters and setters.
These transformation components are all independent of one another: If you want to change the orientation of the entity, you just have to set its rotation property.
You don't have to care about the current position and scale.

### Position

The position of the entity refers to its position in world coordinates, not screen coordinates.

```cpp
// 'entity' can be a sf::Sprite, a sf::Text, a sf::Shape or any other transformable class

// set the absolute position of the entity
entity.setPosition({10.f, 50.f});

// move the entity relatively to its current position
entity.move({5.f, 5.f});

// retrieve the absolute position of the entity
sf::Vector2f position = entity.getPosition(); // = (15, 55)
```

![A translated entity](transform-position.png)

By default, entities are positioned relative to their top-left corner.
We'll see how to change that with the 'origin' property later.

### Rotation

The rotation is the orientation of the entity in the 2D world clockwise from the origin (because the Y axis is pointing down in SFML).

```cpp
// 'entity' can be a sf::Sprite, a sf::Text, a sf::Shape or any other transformable class

// set the absolute rotation of the entity
entity.setRotation(sf::degrees(45));

// rotate the entity relatively to its current orientation
entity.rotate(sf::degrees(10));

// retrieve the absolute rotation of the entity
sf::Angle rotation = entity.getRotation(); // = 55 degrees
```

![A rotated entity](transform-rotation.png)

Note that SFML always returns an `sf::Angle` in range [0, 360) degrees or [0, 2 * pi) radian when you call `getRotation`.

As with the position, the rotation is performed around the top-left corner by default, but this can be changed by setting the origin.

### Scale

The scale factor allows the entity to be resized.
The default scale is 1.
Setting it to a value less than 1 makes the entity smaller, greater than 1 makes it bigger.
Negative scale values are also allowed, so that you can mirror the entity.

```cpp
// 'entity' can be a sf::Sprite, a sf::Text, a sf::Shape or any other transformable class

// set the absolute scale of the entity
entity.setScale({4.f, 1.6f});

// scale the entity relatively to its current scale
entity.scale({0.5f, 0.5f});

// retrieve the absolute scale of the entity
sf::Vector2f scale = entity.getScale(); // = (2.f, 0.8f)
```

![A scaled entity](transform-scale.png)

### Origin

The origin is the center point of the three other transformations.
The entity's position is the position of its origin, its rotation is performed around the origin, and the scale is applied relative to the origin as well.
By default, the origin is positioned at the top-left corner of the entity (point (0, 0)), but you can set it to the center of the entity, or any other corner of the entity for example.

To keep things simple, there's only a single origin for all three transformation components.
This means that you can't position an entity relative to its top-left corner while rotating it around its center for example.
If you need to do such things, have a look at the next chapters.

```cpp
// 'entity' can be a sf::Sprite, a sf::Text, a sf::Shape or any other transformable class

// set the origin of the entity
entity.setOrigin({10.f, 20.f});

// retrieve the origin of the entity
sf::Vector2f origin = entity.getOrigin(); // = (10.f, 20.f)
```

Note that changing the origin also changes where the entity is drawn on screen, even though its position property hasn't changed.
If you don't understand why, read this tutorial one more time!

## Bounding boxes

After transforming entities and drawing them, you might want to perform some computations using them e.g. checking for collisions.

SFML entities can give you their bounding box.
The bounding box is the minimal rectangle that contains all points belonging to the entity with sides aligned to the X and Y axes.

![Bounding box of entities](transform-bounds.png)

The bounding box is very useful when implementing collision detection: Checks against a point or another axis-aligned rectangle can be done very quickly, and its area is close enough to that of the real entity to provide a good approximation.

```cpp
// get the bounding box of the entity
sf::FloatRect boundingBox = entity.getGlobalBounds();

// check collision with a point
sf::Vector2f point = ...;
if (boundingBox.contains(point))
{
    // collision!
}

// check collision with another box (like the bounding box of another entity)
sf::FloatRect otherBox = ...;
if (const std::optional intersection = boundingBox.findIntersection(otherBox))
{
    // collision!
}
```

The function is named `getGlobalBounds` because it returns the bounding box of the entity in the global coordinate system, i.e. after all of its transformations (position, rotation, scale) have been applied.

There's another function that returns the bounding box of the entity in its *local* coordinate system (before its transformations are applied): `getLocalBounds`.
This function can be used to get the initial size of an entity, for example, or to perform more specific calculations.

## Transforming your own classes

!!! advanced "Advanced topic"

    The rest of this tutorial covers transforming your own classes, building transforms by hand and object hierarchies.
    You'll need them once you write your own drawable classes, until then you can skip ahead to the next tutorial.

[`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") is not only made for SFML classes, it can also be a base (or member) of your own classes.

```cpp
class MyGraphicalEntity : public sf::Transformable
{
    // ...
};

MyGraphicalEntity entity;
entity.setPosition({10.f, 30.f});
entity.setRotation(sf::degrees(110.f));
entity.setScale({0.5f, 0.2f});
```

To retrieve the final transform of the entity (commonly needed when drawing it), call the `getTransform` function.
This function returns a [`sf::Transform`](../../../documentation/3.1.0/classsf_1_1Transform.html "sf::Transform documentation") object.
See below for an explanation about it, and how to use it to transform an SFML entity.

If you don't need/want the complete set of functions provided by the [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") interface, don't hesitate to simply use it as a member instead and provide your own functions on top of it.
It is not abstract, so it is possible to instantiate it instead of only being able to use it as a base class.

## Custom transforms

The [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") class is easy to use, but it is also limited.
Some users might need more flexibility.
They might need to specify a final transformation as a custom combination of individual transformations.
For these users, a lower-level class is available: [`sf::Transform`](../../../documentation/3.1.0/classsf_1_1Transform.html "sf::Transform documentation").
It is nothing more than a 3x3 matrix, so it can represent any transformation in 2D space.

There are many ways to construct a [`sf::Transform`](../../../documentation/3.1.0/classsf_1_1Transform.html "sf::Transform documentation"):

- by using the predefined functions for the most common transformations (translation, rotation, scale)
- by combining two transforms
- by specifying its 9 elements directly

Here are a few examples:

```cpp
// the identity transform (does nothing)
sf::Transform t1 = sf::Transform::Identity;

// a rotation transform
sf::Transform t2;
t2.rotate(sf::degrees(45.f));

// a custom matrix
sf::Transform t3(2.f, 0.f, 20.f,
                 0.f, 1.f, 50.f,
                 0.f, 0.f, 1.f);

// a combined transform
sf::Transform t4 = t1 * t2 * t3;
```

You can apply several predefined transformations to the same transform as well.
They will all be combined sequentially.
Note that transforming an object by combining multiple transformations is equivalent to applying each operation in reverse order.
The last operation (here `scale`) is applied first, and will be affected by operations above it in code (second would be `translate({-10.f, 50.f})`, for example).

```cpp
sf::Transform t;
t.translate({10.f, 100.f});
t.rotate(sf::degrees(90.f));
t.translate({-10.f, 50.f});
t.scale({0.5f, 0.75f});
```

Back to the point: How can a custom transform be applied to a graphical entity?
Simple: Pass it to the draw function.

```cpp
window.draw(entity, transform);
```

... which is in fact a short-cut for:

```cpp
sf::RenderStates states;
states.transform = transform;
window.draw(entity, states);
```

If your entity is a [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") (sprite, text, shape), which contains its own internal transform, both the internal and the passed transform are combined to produce the final transform.

## Object hierarchies (scene graph)

With the custom transforms seen previously, it becomes easy to implement a hierarchy of objects in which children are transformed relative to their parent.
All you have to do is pass the combined transform from parent to children when you draw them, all the way until you reach the final drawable entities (sprites, text, shapes, vertex arrays or your own drawables).

```cpp
// the abstract base class
class Node
{
public:
    // ... functions to transform the node

    // ... functions to manage the node's children

    void draw(sf::RenderTarget& target, const sf::Transform& parentTransform) const
    {
        // combine the parent transform with the node's one
        sf::Transform combinedTransform = parentTransform * m_transform;

        // let the node draw itself
        onDraw(target, combinedTransform);

        // draw its children
        for (auto& child : m_children)
            child->draw(target, combinedTransform);
    }

private:
    virtual void onDraw(sf::RenderTarget& target, const sf::Transform& transform) const = 0;

    sf::Transform      m_transform;
    std::vector<Node*> m_children;
};

// a simple derived class: a node that draws a sprite
class SpriteNode : public Node
{
public:
    // ... functions to define the sprite

private:
    void onDraw(sf::RenderTarget& target, const sf::Transform& transform) const override
    {
        target.draw(m_sprite, transform);
    }

    sf::Sprite m_sprite;
};
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/view.md >>>
# Controlling the 2D camera with views

## What is a view?

In games, it is not uncommon to have levels which are much bigger than the window itself.
You only see is a small part of them.
This is typically the case in RPGs, platform games, and many other genres.
What developers might tend to forget is that they define entities *in a 2D world*, not directly in the window.
The window is just a view, it shows a specific area of the whole world.
It is perfectly fine to draw several views of the same world in parallel, or draw the world to a texture rather than to a window.
The world itself remains unchanged, what changes is just the way it is seen.

Since what is seen in the window is just a small part of the entire 2D world, you need a way to specify which part of the world is shown in the window.
Additionally, you may also want to define where/how this area will be shown *within* the window.
These are the two main features of SFML views.

To summarize, views are what you need if you want to scroll, rotate or zoom your world.
They are also the key to creating split screens and mini-maps.

## Defining what the view views

The class which encapsulates views in SFML is [`sf::View`](../../../documentation/3.1.0/classsf_1_1View.html "sf::View documentation").
It can be constructed directly with a definition of the area to view:

```cpp
// create a view with the rectangular area of the 2D world to show
sf::View view1(sf::FloatRect({200.f, 200.f}, {300.f, 200.f}));

// create a view with its center and size
sf::View view2({350.f, 300.f}, {300.f, 200.f});
```

These two definitions are equivalent: Both views will show the same area of the 2D world, a 300x200 rectangle *centered* on the point (350, 300).

![](view-initial.png "A view")

If you don't want to define the view upon construction or want to modify it later, you can assign to it or use the equivalent setters:

```cpp
sf::View view1;
view1 = sf::View(sf::FloatRect({200.f, 200.f}, {300.f, 200.f}));

sf::View view2;
view2.setCenter({350.f, 300.f});
view2.setSize({200.f, 200.f});
```

Once your view is defined, you can transform it to make it show a translated/rotated/scaled version of your 2D world.

### Moving (scrolling) the view

Unlike drawable entities, such as sprites or shapes whose positions are defined by their top-left corner (and can be changed to any other point), views are always manipulated by their center -- this is simply more convenient.
That's why the function to change the position of a view is named `setCenter`, and not `setPosition`.

```cpp
// move the view at point (200, 200)
view.setCenter({200.f, 200.f});

// move the view by an offset of (100, 100) (so its final position is (300, 300))
view.move({100.f, 100.f});
```

![](view-translated.png "A translated view")

### Rotating the view

To rotate a view, use the `setRotation` function.

```cpp
// rotate the view at 20 degrees
view.setRotation(sf::degrees(20));

// rotate the view by 5 degrees relatively to its current orientation (so its final orientation is 25 degrees)
view.rotate(sf::degrees(5));
```

![](view-rotated.png "A rotated view")

### Zooming (scaling) the view

Zooming in (or out) a view is done through to resizing it, so the function to use is `setSize`.

```cpp
// resize the view to show a 1200x800 area (we see a bigger area, so this is a zoom out)
view.setSize({1200.f, 800.f});

// zoom the view relatively to its current size (apply a factor 0.5, so its final size is 600x400)
view.zoom(0.5f);
```

![](view-scaled.png "A scaled view")

## Defining how the view is viewed

Now that you've defined which part of the 2D world is seen in the window, let's define *where* it is shown.
By default, the viewed contents occupy the full window.
If the view has the same size as the window, everything is rendered 1:1.
If the view is smaller or larger than the window, everything is scaled to fit in the window.

This default behavior is suitable for most situations, but it might need to be changed sometimes.
For example, to split the screen in a multiplayer game, you may want to use two views which each only occupy half of the window.
You can also implement a minimap by drawing your entire world to a view which is rendered in a small area in a corner of the window.
The area in which the contents of the view is shown is called the *viewport*.

To set the viewport of a view, you can use the `setViewport` function.

```cpp
// define a centered viewport, with half the size of the window
view.setViewport(sf::FloatRect({0.25f, 0.25}, {0.5f, 0.5f}));
```

![](view-viewport.png "A viewport")

You might have noticed something very important: The viewport is not defined in pixels, but instead as a ratio of the window size.
This is more convenient: It allows you to not have to track resize events in order to update the size of the viewport every time the size of the window changes.
It is also more intuitive: You would probably define your viewport as a fraction of the entire window area anyway, not as a fixed-size rectangle.

Using a viewport, it is straightforward to split the screen for multiplayer games:

```cpp
// player 1 (left side of the screen)
player1View.setViewport(sf::FloatRect({0.f, 0.f}, {0.5f, 1.f}));

// player 2 (right side of the screen)
player2View.setViewport(sf::FloatRect({0.5f, 0.f}, {0.5f, 1.f}));
```

![](view-split-screen.png "A split screen")

... or a mini-map:

```cpp
// the game view (full window)
gameView.setViewport(sf::FloatRect({0.f, 0.f}, {1.f, 1.f}));

// mini-map (upper-right corner)
minimapView.setViewport(sf::FloatRect({0.75f, 0.f}, {0.25f, 0.25f}));
```

![](view-minimap.png "A minimap")

## Using a view

To draw something using a view, you must draw it after calling the `setView` function of the target to which you are drawing ([`sf::RenderWindow`](../../../documentation/3.1.0/classsf_1_1RenderWindow.html "sf::RenderWindow documentation") or [`sf::RenderTexture`](../../../documentation/3.1.0/classsf_1_1RenderTexture.html "sf::RenderTexture documentation")).

```cpp
// let's define a view
sf::View view(sf::FloatRect({0.f, 0.f}, {1000.f, 600.f}));

// activate it
window.setView(view);

// draw something to that view
window.draw(some_sprite);

// want to do visibility checks? retrieve the view
sf::View currentView = window.getView();
...
```

The view remains active until you set another one.
This means that there is always a view which defines what appears in the target and where it is drawn.
If you don't explicitly set any view, the render-target uses its own default view which matches its size 1:1.
You can get the default view of a render-target with the `getDefaultView` function.
This can be useful if you want to define your own view based on it, or restore it to draw fixed entities (like a GUI) on top of your scene.

```cpp
// create a view half the size of the default view
sf::View view = window.getDefaultView();
view.zoom(0.5f);
window.setView(view);

// restore the default view
window.setView(window.getDefaultView());
```

When you call `setView`, the render-target makes a *copy* of the view, and doesn't store a pointer to the one that is passed.
This means that whenever you update your view, you need to call `setView` again to apply the modifications.  
Don't be afraid to copy views or create them on the fly, they aren't expensive objects (they just hold a few floats).

## Showing more when the window is resized

Since the default view never changes after the window is created, the viewed contents are always the same.
So when the window is resized, everything is squeezed/stretched to the new size.

If, instead of this default behavior, you'd like to show more/less stuff depending on the new size of the window, all you have to do is update the size of the view with the size of the window.

```cpp
// the event loop
while (const std::optional event = window.pollEvent())
{
    ...

    // catch the resize events
    if (const auto* resized = event->getIf<sf::Event::Resized>())
    {
        // update the view to the new size of the window
        sf::FloatRect visibleArea({0.f, 0.f}, sf::Vector2f(resized->size));
        window.setView(sf::View(visibleArea));
    }
}
```

## Coordinates conversions

When you use a custom view, or when you resize the window without using the code above, pixels displayed on the target no longer match units in the 2D world.
For example, clicking on pixel (10, 50) may hit the point (26.5, -84) of your world.
You end up having to use a conversion function to map your pixel coordinates to world coordinates: `mapPixelToCoords`.

```cpp
// get the current mouse position in the window
sf::Vector2i pixelPos = sf::Mouse::getPosition(window);

// convert it to world coordinates
sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
```

By default, `mapPixelToCoords` uses the current view.
If you want to convert the coordinates using view which is not the active one, you can pass it as an additional argument to the function.

The opposite, converting world coordinates to pixel coordinates, is also possible with the `mapCoordsToPixel` function.

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/vertex-array.md >>>
# Designing your own entities with vertex arrays

!!! advanced "Advanced topic"

    Vertex arrays are lower-level than sprites and shapes, as you define every single point yourself.
    They're the way to go for efficiently drawing lots of similar things, such as tile maps or particle systems, but you don't need them for your first projects.

## Introduction

SFML provides simple classes for the most common 2D entities.
And while more complex entities can easily be created from these building blocks, it isn't always the most efficient solution.
For example, you'll reach the limits of your graphics card very quickly if you draw a large number of sprites.
The reason is that performance depends in large part on the number of calls to the `draw` function.
Indeed, each call involves setting a set of OpenGL states, resetting matrices, changing textures, etc.
All of this is required even when simply drawing two triangles (a sprite).
This is far from optimal for your graphics card: Today's GPUs are designed to process large batches of triangles, typically several thousand to millions.

To fill this gap, SFML provides a lower-level mechanism to draw things: Vertex arrays.
As a matter of fact, vertex arrays are used internally by all other SFML classes.
They allow for a more flexible definition of 2D entities, containing as many triangles as you need.
They even allow drawing points or lines.

## What is a vertex, and why are they always in arrays?

A vertex is the smallest graphical entity that you can manipulate.
In short, it is a graphical point: Naturally, it has a 2D position (x, y) but also a color and a pair of texture coordinates.
We'll go into the roles of these attributes later.

Vertices alone don't do much.
They are always grouped into *primitives*: Points (1 vertex), lines (2 vertices), or triangles (3 vertices).
You can then combine multiple primitives together to create the final geometry of the entity.

Now you understand why we always talk about vertex arrays, and not vertices alone.

## A simple vertex array

Let's have a look at the [`sf::Vertex`](../../../documentation/3.1.0/structsf_1_1Vertex.html "sf::Vertex documentation") class now.
It's simply an aggregate type which contains three public members.

```cpp
// create a new vertex
sf::Vertex vertex;

// set its position
vertex.position = sf::Vector2f(10.f, 50.f);

// set its color
vertex.color = sf::Color::Red;

// set its texture coordinates
vertex.texCoords = sf::Vector2f(100.f, 100.f);
```

... or, using aggregate initialization:

```cpp
sf::Vertex vertex{{10.f, 50.f}, sf::Color::Red, {100.f, 100.f}};
```

Now, let's define a primitive.
Remember, a primitive consists of several vertices, therefore we need a vertex array.
SFML provides a simple wrapper for this: [`sf::VertexArray`](../../../documentation/3.1.0/classsf_1_1VertexArray.html "sf::VertexArray documentation").
It provides the semantics of an array (similar to `std::vector`), and also stores the type of primitive its vertices define.

```cpp
// create an array of 3 vertices that define a triangle primitive
sf::VertexArray triangle(sf::PrimitiveType::Triangles, 3);

// define the position of the triangle's points
triangle[0].position = sf::Vector2f(10.f, 10.f);
triangle[1].position = sf::Vector2f(100.f, 10.f);
triangle[2].position = sf::Vector2f(100.f, 100.f);

// define the color of the triangle's points
triangle[0].color = sf::Color::Red;
triangle[1].color = sf::Color::Blue;
triangle[2].color = sf::Color::Green;

// no texture coordinates here, we'll see that later
```

Your triangle is ready and you can now draw it.
Drawing a vertex array can be done similarly to drawing any other SFML entity, by using the `draw` function:

```cpp
window.draw(triangle);
```

![A triangle made with vertices](vertex-array-triangle.png)

You can see that the vertices' color is interpolated to fill the primitive.
This is a nice way of creating gradients.

Note that you don't have to use the [`sf::VertexArray`](../../../documentation/3.1.0/classsf_1_1VertexArray.html "sf::VertexArray documentation") class.
It's just defined for convenience, it's nothing more than a `std::vector<sf::Vertex>` along with an `sf::PrimitiveType`.
If you need more flexibility, or a static array, you can use your own storage.
You must then use the overload of the `draw` function which takes a pointer to the vertices, the vertex count and the primitive type.

```cpp
std::vector<sf::Vertex> vertices;
vertices.push_back(sf::Vertex{...});
...

window.draw(vertices.data(), vertices.size(), sf::PrimitiveType::Triangles);
```

```cpp
std::array vertices =
{
    sf::Vertex{...},
    sf::Vertex{...}
};

window.draw(vertices.data(), vertices.size(), sf::PrimitiveType::Lines);
```

## Primitive types

Let's pause for a while and see what kind of primitives you can create.
As explained above, you can define the most basic 2D primitives: Point, line, and triangle.
There are also "chained" variants of these primitive types which allow for sharing of vertices among two consecutive primitives.
This can be useful because consecutive primitives are often connected in some way.

Let's have a look at the full list:

| Primitive type                          | Description                                                                                                                                                             | Example                                                                                                                                        |
| --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `sf::PrimitiveType::Points`             | A set of unconnected points. These points have no thickness: They will always occupy a single pixel, regardless of the current transform and view.                      | ![The sf::PrimitiveType::Points primitive type](vertex-array-points.png)                |
| `sf::PrimitiveType::Lines`              | A set of unconnected lines. These lines have no thickness: They will always be one pixel wide, regardless of the current transform and view.                            | ![The sf::PrimitiveType::Lines primitive type](vertex-array-lines.png)                  |
| `sf::PrimitiveType::LineStrip`          | A set of connected lines. The end vertex of one line is used as the start vertex of the next one.                                                                       | ![The sf::PrimitiveType::LineStrip primitive type](vertex-array-line-strip.png)         |
| `sf::PrimitiveType::Triangles`          | A set of unconnected triangles.                                                                                                                                         | ![The sf::PrimitiveType::Triangles primitive type](vertex-array-triangles.png)          |
| `sf::PrimitiveType::TriangleStrip`      | A set of connected triangles. Each triangle shares its two last vertices with the next one.                                                                             | ![The sf::PrimitiveType::TriangleStrip primitive type](vertex-array-triangle-strip.png) |
| `sf::PrimitiveType::TriangleFan`        | A set of triangles connected to a central point. The first vertex is the center, then each new vertex defines a new triangle, using the center and the previous vertex. | ![The sf::PrimitiveType::TriangleFan primitive type](vertex-array-triangle-fan.png)     |

## Texturing

Like other SFML entities, vertex arrays can also be textured.
To do so, you'll need to manipulate the `texCoords` attribute of the vertices.
This attribute defines which pixel of the texture is mapped to the vertex.

```cpp
// create a triangle strip
sf::VertexArray triangleStrip(sf::PrimitiveType::TriangleStrip, 4);

// define it as a rectangle, located at (10, 10) and with size 100x100
triangleStrip[0].position = sf::Vector2f(10.f, 10.f);
triangleStrip[1].position = sf::Vector2f(10.f, 110.f);
triangleStrip[2].position = sf::Vector2f(110.f, 10.f);
triangleStrip[3].position = sf::Vector2f(110.f, 110.f);

// define its texture area to be a 25x50 rectangle starting at (0, 0)
triangleStrip[0].texCoords = sf::Vector2f(0.f, 0.f);
triangleStrip[1].texCoords = sf::Vector2f(0.f, 50.f);
triangleStrip[2].texCoords = sf::Vector2f(25.f, 0.f);
triangleStrip[3].texCoords = sf::Vector2f(25.f, 50.f);
```

Texture coordinates are defined in *pixels* (just like the `textureRect` of sprites and shapes).
They are *not* normalized (between 0 and 1), as people who are used to OpenGL programming might expect.

Vertex arrays are low-level entities, they only deal with geometry and do not store additional attributes like a texture.
To draw a vertex array with a texture, you must pass it directly to the `draw` function:

```cpp
sf::VertexArray vertices;
sf::Texture texture;

...

window.draw(vertices, &texture);
```

This is the short version, if you need to pass other render states (like a blend mode or a transform), you can use the explicit version which takes a [`sf::RenderStates`](../../../documentation/3.1.0/structsf_1_1RenderStates.html "sf::RenderStates documentation") object:

```cpp
sf::VertexArray vertices;
sf::Texture texture;

...

sf::RenderStates states;
states.texture = &texture;

window.draw(vertices, states);
```

## Transforming a vertex array

Transforming is similar to texturing.
The transform is not stored in the vertex array, you must pass it to the `draw` function.

```cpp
sf::VertexArray vertices;
sf::Transform transform;

...

window.draw(vertices, transform);
```

Or, if you need to pass other render states:

```cpp
sf::VertexArray vertices;
sf::Transform transform;

...

sf::RenderStates states;
states.transform = transform;

window.draw(vertices, states);
```

To know more about transformations and the [`sf::Transform`](../../../documentation/3.1.0/classsf_1_1Transform.html "sf::Transform documentation") class, you can read the tutorial on [transforming entities](transform.md "Transforming entities tutorial").

## Creating an SFML-like entity

Now that you know how to define your own textured/colored/transformed entity, wouldn't it be nice to wrap it in an SFML-style class?
Fortunately, SFML makes this easy for you by providing the [`sf::Drawable`](../../../documentation/3.1.0/classsf_1_1Drawable.html "sf::Drawable documentation") and [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation") base classes.
These two classes are the base of the built-in SFML entities [`sf::Sprite`](../../../documentation/3.1.0/classsf_1_1Sprite.html "sf::Sprite documentation"), [`sf::Text`](../../../documentation/3.1.0/classsf_1_1Text.html "sf::Text documentation") and [`sf::Shape`](../../../documentation/3.1.0/classsf_1_1Shape.html "sf::Shape documentation").

[`sf::Drawable`](../../../documentation/3.1.0/classsf_1_1Drawable.html "sf::Drawable documentation") is an interface: It declares a single pure virtual function and has no members nor concrete functions.
Inheriting from [`sf::Drawable`](../../../documentation/3.1.0/classsf_1_1Drawable.html "sf::Drawable documentation") allows you to draw instances of your class the same way as SFML classes:

```cpp
class MyEntity : public sf::Drawable
{
private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};

MyEntity entity;
window.draw(entity); // internally calls entity.draw
```

Note that doing this is not mandatory, you could also just have a similar `draw` function in your class and simply call it with `entity.draw(window)`.
But the other way, with [`sf::Drawable`](../../../documentation/3.1.0/classsf_1_1Drawable.html "sf::Drawable documentation") as a base class, is nicer and more consistent.
This also means that if you plan on storing an array of drawable objects, you can do it without any additional effort since all drawable objects (SFML's and yours) derive from the same class.

The other base class, [`sf::Transformable`](../../../documentation/3.1.0/classsf_1_1Transformable.html "sf::Transformable documentation"), has no virtual function.
Inheriting from it automatically adds the same transformation functions to your class as other SFML classes (`setPosition`, `setRotation`, `move`, `scale`, ...).
You can learn more about this class in the tutorial on [transforming entities](transform.md "Transforming entities tutorial").

Using these two base classes and a vertex array (in this example we'll also add a texture), here is what a typical SFML-like graphical class would look like:

```cpp
class MyEntity : public sf::Drawable, public sf::Transformable
{
public:
    // add functions to play with the entity's geometry / colors / texturing...

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override
    {
        // apply the entity's transform -- combine it with the one that was passed by the caller
        states.transform *= getTransform(); // getTransform() is defined by sf::Transformable

        // apply the texture
        states.texture = &m_texture;

        // you may also override states.shader or states.blendMode if you want

        // draw the vertex array
        target.draw(m_vertices, states);
    }

    sf::VertexArray m_vertices;
    sf::Texture     m_texture;
};
```

You can then use this class as if it were a built-in SFML class:

```cpp
MyEntity entity;

// you can transform it
entity.setPosition({10.f, 50.f});
entity.setRotation(sf::degrees(45));

// you can draw it
window.draw(entity);
```

## Example: tile map

With what we've seen above, let's create a class that encapsulates a tile map.
The whole map will be contained in a single vertex array, therefore it will be super fast to draw.
Note that we can apply this strategy only if the whole tile set can fit into a single texture.
Otherwise, we would have to use at least one vertex array per texture.

```cpp
class TileMap : public sf::Drawable, public sf::Transformable
{
public:
    bool load(const std::filesystem::path& tileset, sf::Vector2u tileSize, const int* tiles, unsigned int width, unsigned int height)
    {
        // load the tileset texture
        if (!m_tileset.loadFromFile(tileset))
            return false;

        // resize the vertex array to fit the level size
        m_vertices.setPrimitiveType(sf::PrimitiveType::Triangles);
        m_vertices.resize(width * height * 6);

        // populate the vertex array, with two triangles per tile
        for (unsigned int i = 0; i < width; ++i)
        {
            for (unsigned int j = 0; j < height; ++j)
            {
                // get the current tile number
                const int tileNumber = tiles[i + j * width];

                // find its position in the tileset texture
                const int tu = tileNumber % (m_tileset.getSize().x / tileSize.x);
                const int tv = tileNumber / (m_tileset.getSize().x / tileSize.x);

                // get a pointer to the triangles' vertices of the current tile
                sf::Vertex* triangles = &m_vertices[(i + j * width) * 6];

                // define the 6 corners of the two triangles
                triangles[0].position = sf::Vector2f(i * tileSize.x, j * tileSize.y);
                triangles[1].position = sf::Vector2f((i + 1) * tileSize.x, j * tileSize.y);
                triangles[2].position = sf::Vector2f(i * tileSize.x, (j + 1) * tileSize.y);
                triangles[3].position = sf::Vector2f(i * tileSize.x, (j + 1) * tileSize.y);
                triangles[4].position = sf::Vector2f((i + 1) * tileSize.x, j * tileSize.y);
                triangles[5].position = sf::Vector2f((i + 1) * tileSize.x, (j + 1) * tileSize.y);

                // define the 6 matching texture coordinates
                triangles[0].texCoords = sf::Vector2f(tu * tileSize.x, tv * tileSize.y);
                triangles[1].texCoords = sf::Vector2f((tu + 1) * tileSize.x, tv * tileSize.y);
                triangles[2].texCoords = sf::Vector2f(tu * tileSize.x, (tv + 1) * tileSize.y);
                triangles[3].texCoords = sf::Vector2f(tu * tileSize.x, (tv + 1) * tileSize.y);
                triangles[4].texCoords = sf::Vector2f((tu + 1) * tileSize.x, tv * tileSize.y);
                triangles[5].texCoords = sf::Vector2f((tu + 1) * tileSize.x, (tv + 1) * tileSize.y);
            }
        }

        return true;
    }

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override
    {
        // apply the transform
        states.transform *= getTransform();

        // apply the tileset texture
        states.texture = &m_tileset;

        // draw the vertex array
        target.draw(m_vertices, states);
    }

    sf::VertexArray m_vertices;
    sf::Texture     m_tileset;
};
```

And now, the application that uses it:

```cpp
int main()
{
    // create the window
    sf::RenderWindow window(sf::VideoMode({512, 256}), "Tilemap");

    // define the level with an array of tile indices
    constexpr std::array level = {
        0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 0, 0, 0, 0,
        1, 1, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 3,
        0, 1, 0, 0, 2, 0, 3, 3, 3, 0, 1, 1, 1, 0, 0, 0,
        0, 1, 1, 0, 3, 3, 3, 0, 0, 0, 1, 1, 1, 2, 0, 0,
        0, 0, 1, 0, 3, 0, 2, 2, 0, 0, 1, 1, 1, 1, 2, 0,
        2, 0, 1, 0, 3, 0, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1,
        0, 0, 1, 0, 3, 2, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1,
    };

    // create the tilemap from the level definition
    TileMap map;
    if (!map.load("tileset.png", {32, 32}, level.data(), 16, 8))
        return -1;

    // run the main loop
    while (window.isOpen())
    {
        // handle events
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // draw the map
        window.clear();
        window.draw(map);
        window.display();
    }
}
```

![The tilemap example](vertex-array-tilemap.png)

You can download the tileset used for this tilemap example [here](vertex-array-tilemap-tileset.png "Tilemap example tileset").

## Example: particle system

This second example implements another common entity: The particle system.
This one is very simple, with no texture and as few parameters as possible.
It demonstrates the use of the `sf::PrimitiveType::Points` primitive type with a dynamic vertex array which changes every frame.

```cpp
class ParticleSystem : public sf::Drawable, public sf::Transformable
{
public:
    ParticleSystem(unsigned int count) : m_particles(count), m_vertices(sf::PrimitiveType::Points, count)
    {
    }

    void setEmitter(sf::Vector2f position)
    {
        m_emitter = position;
    }

    void update(sf::Time elapsed)
    {
        for (std::size_t i = 0; i < m_particles.size(); ++i)
        {
            // update the particle lifetime
            Particle& p = m_particles[i];
            p.lifetime -= elapsed;

            // if the particle is dead, respawn it
            if (p.lifetime <= sf::Time::Zero)
                resetParticle(i);

            // update the position of the corresponding vertex
            m_vertices[i].position += p.velocity * elapsed.asSeconds();

            // update the alpha (transparency) of the particle according to its lifetime
            float ratio           = p.lifetime.asSeconds() / m_lifetime.asSeconds();
            m_vertices[i].color.a = static_cast<std::uint8_t>(ratio * 255);
        }
    }

private:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override
    {
        // apply the transform
        states.transform *= getTransform();

        // our particles don't use a texture
        states.texture = nullptr;

        // draw the vertex array
        target.draw(m_vertices, states);
    }

    struct Particle
    {
        sf::Vector2f velocity;
        sf::Time     lifetime;
    };

    void resetParticle(std::size_t index)
    {
        // create random number generator
        static std::random_device rd;
        static std::mt19937       rng(rd());

        // give a random velocity and lifetime to the particle
        const sf::Angle angle       = sf::degrees(std::uniform_real_distribution(0.f, 360.f)(rng));
        const float     speed       = std::uniform_real_distribution(50.f, 100.f)(rng);
        m_particles[index].velocity = sf::Vector2f(speed, angle);
        m_particles[index].lifetime = sf::milliseconds(std::uniform_int_distribution(1000, 3000)(rng));

        // reset the position of the corresponding vertex
        m_vertices[index].position = m_emitter;
    }

    std::vector<Particle> m_particles;
    sf::VertexArray       m_vertices;
    sf::Time              m_lifetime{sf::seconds(3)};
    sf::Vector2f          m_emitter;
};
```

And a little demo that uses it:

```cpp
int main()
{
    // create the window
    sf::RenderWindow window(sf::VideoMode({512, 256}), "Particles");

    // create the particle system
    ParticleSystem particles(1000);

    // create a clock to track the elapsed time
    sf::Clock clock;

    // run the main loop
    while (window.isOpen())
    {
        // handle events
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // make the particle system emitter follow the mouse
        sf::Vector2i mouse = sf::Mouse::getPosition(window);
        particles.setEmitter(window.mapPixelToCoords(mouse));

        // update it
        sf::Time elapsed = clock.restart();
        particles.update(elapsed);

        // draw it
        window.clear();
        window.draw(particles);
        window.display();
    }
}
```

![The particles example](vertex-array-particles.png)

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/graphics/shader.md >>>
# Adding special effects with shaders

!!! advanced "Advanced topic"

    Shaders are small programs written in GLSL, a separate language that runs on the graphics card.
    This tutorial explains how to use them with SFML, but doesn't teach GLSL itself.
    You don't need shaders to make a game, so feel free to skip this tutorial until you want special effects.

## Introduction

A shader is a small program that is executed on the graphics card.
It provides the programmer with more control over the drawing process and in a more flexible and simple way than using the fixed set of states and operations provided by OpenGL.
With this additional flexibility, shaders are used to create effects that would be too complicated, if not impossible, to describe with regular OpenGL functions: Per-pixel lighting, shadows, etc.
Today's graphics cards and newer versions of OpenGL are already entirely shader-based, and the fixed set of states and functions (which is called the "fixed pipeline") that you might know of has been deprecated and will likely be removed in the future.

Shaders are written in GLSL (_OpenGL Shading Language_), which is very similar to the C programming language.

There are two types of shaders: vertex shaders and fragment (or pixel) shaders.
Vertex shaders are run for each vertex, while fragment shaders are run for every generated fragment (pixel).
Depending on what kind of effect you want to achieve, you can provide a vertex shader, a fragment shader, or both.

To understand what shaders do and how to use them efficiently, it is important to understand the basics of the rendering pipeline.
You must also learn how to write GLSL programs and find good tutorials and examples to get started.
You can also have a look at the "Shader" example that comes with the SFML SDK.

This tutorial will only focus on the SFML specific part: Loading and applying your shaders -- not writing them.

## Loading shaders

In SFML, shaders are represented by the [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") class.
It handles both the vertex and fragment shaders: A [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") object is a combination of both (or only one, if the other is not provided).
 
Even though shaders have become commonplace, there are still old graphics cards that might not support them.
The first thing you shoulddo in your program is check if shaders are available on the system:

```cpp
if (!sf::Shader::isAvailable())
{
    // shaders are not available...
}
```

Any attempt to use the [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") class will fail if `sf::Shader::isAvailable()` returns `false`.

The most common way of loading a shader is from a file on disk, which is done with the `loadFromFile` function or the corresponding constructors.

```cpp
sf::Shader shader;

// load only the vertex shader
if (!shader.loadFromFile("vertex_shader.vert", sf::Shader::Type::Vertex))
{
    // error...
}

// load only the fragment shader
if (!shader.loadFromFile("fragment_shader.frag", sf::Shader::Type::Fragment))
{
    // error...
}

// load both shaders
if (!shader.loadFromFile("vertex_shader.vert", "fragment_shader.frag"))
{
    // error...
}
```

Shader source is contained in simple text files (like your C++ code).
Their extension doesn't really matter.
It can be anything you want.
You can even omit it.
".vert" and ".frag" are just examples of possible extensions.

The `loadFromFile` function can sometimes fail with no obvious reason.
First, check the error message that SFML prints to the standard output (check the console).
If the message is unable to open file, make sure that the *working directory* (which is the directory that any file path will be interpreted relative to) is what you think it is: When you run the application from your desktop environment, the working directory is the executable folder.
However, when you launch your program from your IDE (Visual Studio, Code::Blocks, ...) the working directory might sometimes be set to the *project* directory instead.
This can usually be changed quite easily in the project settings.

Shaders can also be loaded directly from strings, with the `loadFromMemory` function or the corresponding constructors.
This can be useful if you want to embed the shader source directly into your program.

```cpp
constexpr std::string_view vertexShader = R"(
void main()
{
    ... 
}
)";

constexpr std::string_view fragmentShader = R"(
void main()
{
    ... 
}
)";

// load only the vertex shader
if (!shader.loadFromMemory(vertexShader, sf::Shader::Type::Vertex))
{
    // error...
}

// load only the fragment shader
if (!shader.loadFromMemory(fragmentShader, sf::Shader::Type::Fragment))
{
    // error...
}

// load both shaders
if (!shader.loadFromMemory(vertexShader, fragmentShader))
{
    // error...
}
```

And finally, like all other SFML resources, shaders can also be loaded from a [custom input stream](../system/stream.md "Input streams tutorial") with the `loadFromStream` function or the corresponding constructors.

If loading fails, don't forget to check the standard error output (the console) to see a detailed report from the GLSL compiler.

## Using a shader

Using a shader is simple.
Just pass it as an additional argument to the `draw` function.

```cpp
window.draw(whatever, &shader);
```

## Passing variables to a shader

Like any other program, a shader can take parameters so that it is able to behave differently from one draw to another.
These parameters are declared as global variables known as *uniforms* in the shader.

```glsl
uniform float myvar;

void main()
{
    // use myvar...
}
```

Uniforms can be set by the C++ program using the various overloads of the `setUniform` function in the [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") class.

```cpp
shader.setUniform("myvar", 5.f);
```

`setUniform`'s overloads support all the types provided by SFML:

- `float` (GLSL type `float`)
- `2 floats, sf::Vector2f` (GLSL type `vec2`)
- `3 floats, sf::Vector3f` (GLSL type `vec3`)
- `4 floats` (GLSL type `vec4`)
- `sf::Color` (GLSL type `vec4`)
- `sf::Transform` (GLSL type `mat4`)
- `sf::Texture` (GLSL type `sampler2D`)

The GLSL compiler optimizes out unused variables (here, "unused" means "not involved in the calculation of the final vertex/pixel").
So don't be surprised if you get error messages such as Failed to find variable "xxx" in shader when you call `setUniform` during your tests.

## Minimal shaders

You won't learn how to write GLSL shaders here, but it is essential that you know what input SFML provides to the shaders and what it expects you to do with it.

### Vertex shader

SFML has a fixed vertex format which is described by the [`sf::Vertex`](../../../documentation/3.1.0/structsf_1_1Vertex.html "sf::Vertex documentation") structure.
An SFML vertex contains a 2D position, a color, and 2D texture coordinates.
This is the exact input that you will get in the vertex shader, stored in the built-in `gl_Vertex`, `gl_Color` and `gl_MultiTexCoord0` variables (you don't need to declare them).

```glsl
void main()
{
    // transform the vertex position
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

    // transform the texture coordinates
    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

    // forward the vertex color
    gl_FrontColor = gl_Color;
}
```

The position usually needs to be transformed by the model-view and projection matrices, which contain the entity transform combined with the current view.
The texture coordinates need to be transformed by the texture matrix (this matrix likely doesn't mean anything to you, it is just an SFML implementation detail).
And finally, the color just needs to be forwarded.
Of course, you can ignore the texture coordinates and/or the color if you don't make use of them.

All these variables will then be interpolated over the primitive by the graphics card, and passed to the fragment shader.

### Fragment shader

The fragment shader functions quite similarly: It receives the texture coordinates and the color of a generated fragment.
There's no position any more, at this point the graphics card has already computed the final raster position of the fragment.
However if you deal with textured entities, you'll also need the current texture.

```glsl
uniform sampler2D texture;

void main()
{
    // lookup the pixel in the texture
    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

    // multiply it by the color
    gl_FragColor = gl_Color * pixel;
}
```

The current texture is not automatic.
You need to treat it like you do the other input variables, and explicitly set it from your C++ program.
Since each entity can have a different texture, and worse, there might be no way for you to get it and pass it to the shader, SFML provides a special overload of the `setUniform` function that does this job for you.

```cpp
shader.setUniform("texture", sf::Shader::CurrentTexture);
```

This special parameter automatically sets the texture of the entity being drawn to the shader variable with the given name.
Every time you draw a new entity, SFML will update the shader texture variable accordingly.

If you want to see nice examples of shaders in action, you can have a look at the Shader example in the SFML SDK.

## Using a sf::Shader with OpenGL code

If you're using OpenGL rather than the graphics entities of SFML, you can still use [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") as a wrapper around an OpenGL program object and use it within your OpenGL code.

To activate a [`sf::Shader`](../../../documentation/3.1.0/classsf_1_1Shader.html "sf::Shader documentation") for drawing (the equivalent of `glUseProgram`), you have to call the `bind` static function:

```cpp
sf::Shader shader;
...

// bind the shader
sf::Shader::bind(&shader);

// draw your OpenGL entity here...

// bind no shader
sf::Shader::bind(nullptr);
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/audio/sounds.md >>>
# Playing sounds and music

## Sound or music?

SFML provides two classes for playing audio: [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation") and [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation").
They both provide more or less the same features, the main difference is how they work.

[`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation") is a lightweight object that plays loaded audio data from an [`sf::SoundBuffer`](../../../documentation/3.1.0/classsf_1_1SoundBuffer.html "sf::SoundBuffer documentation").
It should be used for small sounds that can fit in memory and should suffer no lag when they are played.
Examples are gun shots, foot steps, etc.

[`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") doesn't load all the audio data into memory, instead it streams it on the fly from the source file.
It is typically used to play compressed music that lasts several minutes, and would otherwise take many seconds to load and eat hundreds of MB in memory.

## Loading and playing a sound

As mentioned above, the sound data is not stored directly in [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation") but in a separate class named [`sf::SoundBuffer`](../../../documentation/3.1.0/classsf_1_1SoundBuffer.html "sf::SoundBuffer documentation").
This class encapsulates the audio data, which is basically an array of 16-bit signed integers called "audio samples".
A sample is the amplitude of the sound signal at a given point in time, and an array of samples therefore represents a full sound.

In fact, the [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation")/[`sf::SoundBuffer`](../../../documentation/3.1.0/classsf_1_1SoundBuffer.html "sf::SoundBuffer documentation") classes work the same way as [`sf::Sprite`](../../../documentation/3.1.0/classsf_1_1Sprite.html "sf::Sprite documentation")/[`sf::Texture`](../../../documentation/3.1.0/classsf_1_1Texture.html "sf::Texture documentation") from the graphics module.
So if you understand how sprites and textures work together, you can apply the same concept to sounds and sound buffers.

You can load a sound buffer from a file on disk with its `loadFromFile` function or its corresponding constructor:

```cpp
#include <SFML/Audio.hpp>

int main()
{
    sf::SoundBuffer buffer("sound.wav"); // Throws sf::Exception if an error occurs

    // OR

    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile("sound.wav"))
        return -1;

    ...
}
```

As with everything else, you can also load an audio file from memory (`loadFromMemory`) or from a [custom input stream](../system/stream.md "Input streams tutorial") (`loadFromStream`) along with corresponding constructors that throw if an error occurs.

SFML supports the audio file formats WAV, OGG/Vorbis and FLAC.
MP3 is supported for decoding but not encoding.

You can also load a sound buffer directly from an array of samples, in the case they originate from another source:

```cpp
std::vector<std::int16_t> samples = ...;

sf::SoundBuffer buffer(samples.data(), samples.size(), 2, 44100);

// OR

sf::SoundBuffer buffer;
if (!buffer.loadFromSamples(samples.data(), samples.size(), 2, 44100))
    // Handle error
```

Since `loadFromSamples` and its corresponding constructor loads a raw array of samples rather than an audio file, it requires additional arguments in order to have a complete description of the sound.
The first one (third argument) is the number of channels; 1 channel defines a mono sound, 2 channels define a stereo sound, etc.
The second additional attribute (fourth argument) is the sample rate; it defines how many samples must be played per second in order to reconstruct the original sound.

Now that the audio data is loaded, we can play it with an [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation") instance.

```cpp
sf::SoundBuffer buffer;
// load something into the sound buffer...

sf::Sound sound(buffer);
sound.play();
```

The cool thing is that you can assign the same sound buffer to multiple sounds if you want.
You can even play them together without any issues.

Sounds (and music) are played in a separate thread.
This means that you are free to do whatever you want after calling `play()` (except destroying the sound or its data, of course), the sound will continue to play until it's finished or explicitly stopped.

## Playing a music

Unlike [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation"), [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") doesn't pre-load the audio data.
Instead it streams the data directly from the source.
The initialization of music is thus more direct:

```cpp
sf::Music music("music.ogg"); // Throws sf::Exception if an error occurs

// OR

sf::Music music;
if (!music.openFromFile("music.ogg"))
    return -1; // error

music.play();
```

It is important to note that, unlike most other SFML resources, the loading function is named `openFromFile` instead of `loadFromFile`.
This is because the music is not really loaded.
This function merely opens it.
The data is only loaded later when the music is played.
It also helps to keep in mind that the audio file has to remain available as long as it is played.
 
The other loading functions of [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") follow the same convention: `openFromMemory`, `openFromStream`.

## What's next?

Now that you are able to load and play a sound or music, let's see what you can do with it.

To control playback, the following functions are available:

- `play` starts or resumes playback
- `pause` pauses playback
- `stop` stops playback and rewind
- `setPlayingOffset` changes the current playing position

Example:

```cpp
// start playback
sound.play();

// advance to 2 seconds
sound.setPlayingOffset(sf::seconds(2.f));

// pause playback
sound.pause();

// resume playback
sound.play();

// stop playback and rewind
sound.stop();
```

The `getStatus` function returns the current status of a sound or music, you can use it to know whether it is stopped, playing or paused.

Sound and music playback is also controlled by a few attributes which can be changed at any moment.

The *pitch* is a factor that changes the perceived frequency of the sound: greater than 1 plays the sound at a higher pitch, less than 1 plays the sound at a lower pitch, and 1 leaves it unchanged.
Changing the pitch has a side effect: it impacts the playing speed.

```cpp
sound.setPitch(1.2f);
```

The *volume* is... the volume.
The value ranges from 0 (mute) to 100 (full volume).
The default value is 100, which means that you can't make a sound louder than its initial volume.

```cpp
sound.setVolume(50.f);
```

The *loop* attribute controls whether the sound/music automatically loops or not.
If it loops, it will restart playing from the beginning when it's finished, again and again until you explicitly call `stop`.
If not set to loop, it will stop automatically when it's finished.

```cpp
sound.setLooping(true);
```

More attributes are available, but they are related to spatialization and are explained in the [corresponding tutorial](spatialization.md "Spatialization tutorial").

## Looping music

`sf::Music` provides the ability to loop a given segment of the audio.
`sf::Music::setLoopPoints` sets the beginning and duration of the sound's looping sequence.
Here's what that looks like:

```cpp
sf::Music music("music.wav");
music.setLoopPoints({sf::milliseconds(500), sf::seconds(4)});
```

This code will loop a 4 second long segment of the music starting 500 milliseconds from the beginning.
`sf::Music` may tweak this loop range to match up with the internally held samples so you can check the actual loop points with `getLoopPoints`.

## Common mistakes

### Destroyed sound buffer

The most common mistake is to let a sound buffer go out of scope (and therefore be destroyed) while a sound still uses it.

```cpp
sf::Sound loadSound(const std::filesystem::path& filename)
{
    sf::SoundBuffer buffer(filename); // this buffer is local to the function, it will be destroyed...
    return sf::Sound(buffer);
} // ... here

sf::Sound sound = loadSound("s.wav");
sound.play(); // ERROR: the sound's buffer no longer exists, the behavior is undefined
```

Remember that a sound only keeps a *pointer* to the sound buffer that you give to it, it doesn't contain its own copy.
You have to correctly manage the lifetime of your sound buffers so that they remain alive as long as they are used by sounds.

### Destroying the music source while it plays

Remember that a music needs its source as long as it is played.
A music file on your disk probably won't be deleted or moved while your application plays it, however things get more complicated when you play a music from a file in memory, or from a custom input stream:

```cpp
// we start with a music file in memory (imagine that we extracted it from a zip archive)
std::vector<char> fileData = ...;

// we play it
sf::Music music;
music.openFromMemory(filedata.data(), fileData.size());
music.play();

// "ok, it seems that we don't need the source file any longer"
fileData.clear();

// ERROR: the music was still streaming the contents of fileData! The behavior is now undefined
```

### sf::Music is not copyable

The final "mistake" is a reminder: the [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") class is *not copyable*, so you won't be allowed to do that:

```cpp
sf::Music music;
sf::Music anotherMusic = music; // ERROR

void doSomething(sf::Music music)
{
    ...
}
sf::Music music;
doSomething(music); // ERROR (the function should take its argument by reference, not by value)
```

`sf::Music` is however moveable.
You can transfer ownership of a music object like this:

```cpp
class MyMusic
{
public:
    MyMusic(sf::Music&& music) : m_music(std::move(music))
    {
    }

    // ... other public functions

private:
    sf::Music m_music;
};

sf::Music music("sound.ogg");
MyMusic   myMusic(std::move(music));
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/audio/recording.md >>>
# Recording audio

## Recording to a sound buffer

The most common use for captured audio data is for it to be saved to a sound buffer ([`sf::SoundBuffer`](../../../documentation/3.1.0/classsf_1_1SoundBuffer.html "sf::SoundBuffer documentation")) so that it can either be played or saved to a file.

This can be achieved with the very simple interface of the [`sf::SoundBufferRecorder`](../../../documentation/3.1.0/classsf_1_1SoundBufferRecorder.html "sf::SoundBufferRecorder documentation") class:

```cpp
// first check if an input audio device is available on the system
if (!sf::SoundBufferRecorder::isAvailable())
{
    // error: audio capture is not available on this system
    ...
}

// create the recorder
sf::SoundBufferRecorder recorder;

// start the capture
recorder.start();

// wait...

// stop the capture
recorder.stop();

// retrieve the buffer that contains the captured audio data
const sf::SoundBuffer& buffer = recorder.getBuffer();
```

The `SoundBufferRecorder::isAvailable` static function checks if audio recording is supported by the system.
It if returns `false`, you won't be able to use the [`sf::SoundBufferRecorder`](../../../documentation/3.1.0/classsf_1_1SoundBufferRecorder.html "sf::SoundBufferRecorder documentation") class at all.

The `start` and `stop` functions are self-explanatory.
The capture runs in its own thread, which means that you can do whatever you want between start and stop.
After the end of the capture, the recorded audio data is available in a sound buffer that you can get with the `getBuffer` function.

With the recorded data, you can then:

- Save it to a file
    
    ```cpp
    buffer.saveToFile("my_record.ogg");
    ```
    
- Play it directly
    
    ```cpp
    sf::Sound sound(buffer);
    sound.play();
    ```
    
- Access the raw audio data and analyze it, transform it, etc.
    
    ```cpp
    const std::int16_t* samples = buffer.getSamples();
    std::size_t count = buffer.getSampleCount();
    doSomething(samples, count);
    ```
    

If you want to use the captured audio data after the recorder is destroyed or restarted, don't forget to make a _copy_ of the buffer.

## Selecting the input device

If you have multiple sound input devices connected to your computer (for example a microphone, a sound interface (external soundcard) or a webcam microphone) you can specify the device that is used for recording.
A sound input device is identified by its name.
A `std::vector<std::string>` containing the names of all connected devices is available through the static `SoundBufferRecorder::getAvailableDevices()` function.
You can then select a device from the list for recording, by passing the chosen device name to the `setDevice()` method.
It is even possible to change the device on the fly (i.e. while recording).

The name of the currently used device can be obtained by calling `getDevice()`.
If you don't choose a device yourself, the default device will be used.
Its name can be obtained through the static `SoundBufferRecorder::getDefaultDevice()` function.

Here is a small example of how to set the input device:

```cpp
// get the available sound input device names
std::vector<std::string> availableDevices = sf::SoundRecorder::getAvailableDevices();

// choose a device
std::string inputDevice = availableDevices[0];

// create the recorder
sf::SoundBufferRecorder recorder;

// set the device
if (!recorder.setDevice(inputDevice))
{
    // error: device selection failed
    ...
}

// use recorder as usual
```

## Custom recording

!!! advanced "Advanced topic"

    The rest of this tutorial covers writing your own recorder, which involves inheritance and code running on a separate thread.
    If recording into a sound buffer is all you need, you can stop here.

If storing the captured data in a sound buffer is not what you want, you can write your own recorder.
Doing so will allow you to process the audio data while it is captured, (almost) directly from the recording device.
This way you can, for example, stream the captured audio over the network, perform real-time analysis on it, etc.

To write your own recorder, you must inherit from the [`sf::SoundRecorder`](../../../documentation/3.1.0/classsf_1_1SoundRecorder.html "sf::SoundRecorder documentation") abstract base class.
In fact, [`sf::SoundBufferRecorder`](../../../documentation/3.1.0/classsf_1_1SoundBufferRecorder.html "sf::SoundBufferRecorder documentation") is just a built-in specialization of this class.

You only have a single virtual function to override in your derived class: `onProcessSamples`.
It is called every time a new chunk of audio samples is captured, so this is where you implement your specific stuff.

By default Audio samples are provided to the `onProcessSamples` method every 100 ms.
You can change the interval by using the `setProcessingInterval` method.
You may want to use a smaller interval if you want to process the recorded data in real time, for example.
Note that this is only a hint and that the actual period may vary, so don't rely on it to implement precise timing.

There are also two additional virtual functions that you can optionally override: `onStart` and `onStop`.
They are called when the capture starts/stops respectively.
They are useful for initialization/cleanup tasks.

Here is the skeleton of a complete derived class:

```cpp
class MyRecorder : public sf::SoundRecorder
{
    bool onStart() override // optional
    {
        // initialize whatever has to be done before the capture starts
        ...

        // return true to start the capture, or false to cancel it
        return true;
    }

    bool onProcessSamples(const std::int16_t* samples, std::size_t sampleCount) override
    {
        // do something useful with the new chunk of samples
        ...

        // return true to continue the capture, or false to stop it
        return true;
    }

    void onStop() override // optional
    {
        // clean up whatever has to be done after the capture is finished
        ...
    }
};
```

The `isAvailable`/`start`/`stop` functions are defined in the [`sf::SoundRecorder`](../../../documentation/3.1.0/classsf_1_1SoundRecorder.html "sf::SoundRecorder documentation") base, and thus inherited in every derived classes.
This means that you can use any recorder class exactly the same way as the [`sf::SoundBufferRecorder`](../../../documentation/3.1.0/classsf_1_1SoundBufferRecorder.html "sf::SoundBufferRecorder documentation") class above.

```cpp
if (!MyRecorder::isAvailable())
{
    // error...
}

MyRecorder recorder;
recorder.start();
...
recorder.stop();
```

## Threading issues

Since recording is done in a separate thread, it is important to know what exactly happens, and where.

`onStart` will be called directly by the `start` function, so it is executed in the same thread that called it.
However, `onProcessSample` and `onStop` will always be called from the internal recording thread that SFML creates.

If your recorder uses data that may be accessed _concurrently_ in both the caller thread and in the recording thread, you have to protect it (with a mutex for example) in order to avoid concurrent access, which may cause undefined behavior -- corrupt data being recorded, crashes, etc.

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/audio/spatialization.md >>>
# Spatialization: Sounds in 3D

## Introduction

By default, sounds and music are played at full volume in each speaker; they are not _spatialized_.

If a sound is emitted by an entity which is to the right of the screen, you would probably want to hear it from the right speaker.
If a music is being played behind the player, you would want to hear it from the rear speakers of your Dolby 5.1 sound system.

How can this be achieved?

## Spatialized sounds are mono

A sound can be spatialized only if it has a single channel, i.e. if it's a mono sound.  
Spatialization is disabled for sounds with more channels, since they already explicitly decide how to use the speakers.
This is very important to keep in mind.

## The listener

All the sounds and music in your audio environment will be heard by a single actor: the _listener_.
What is output from your speakers is determined by what the listener hears.

The namespace which defines the listener's properties is [`sf::Listener`](../../../documentation/3.1.0/namespacesf_1_1Listener.html "sf::Listener documentation").

First, you can set the listener's position in the scene:

```cpp
sf::Listener::setPosition({10.f, 0.f, 5.f});
```

If you have a 2D world you can just use the same Y value everywhere, usually 0.

In addition to its position, you can define the listener's orientation:

```cpp
sf::Listener::setDirection({1.f, 0.f, 0.f});
```

Here, the listener is oriented along the +X axis.
This means that, for example, a sound emitted at (15, 0, 5) will be heard from the right speaker.

The "up" vector of the listener is set to (0, 1, 0) by default, in other words, the top of the listener's head is pointing towards +Y.
You can change the "up" vector if you want.
It is rarely necessary though.

```cpp
sf::Listener::setUpVector({1.f, 1.f, 0.f});
```

This corresponds to the listener tilting their head towards the right (+X).

Finally, the listener can adjust the global volume of the scene:

```cpp
sf::Listener::setGlobalVolume(50.f);
```

The value of the volume is in the range [0 .. 100], so setting it to 50 reduces it to half of the original volume.

Of course, all these properties can be read with the corresponding `get` functions.

## Audio sources

Every audio source provided by SFML (sounds, music, streams) defines the same properties for spatialization.

The main property is the position of the audio source.

```cpp
sound.setPosition({2.f, 0.f, -5.f});
```

This position is absolute by default, but it can be relative to the listener if needed.

```cpp
sound.setRelativeToListener(true);
```

This can be useful for sounds emitted by the listener itself (like a gun shot, or foot steps).
It also has the interesting side-effect of disabling spatialization if you set the position of the audio source to (0, 0, 0).
Non-spatialized sounds can be required in various situations: GUI sounds (clicks), ambient music, etc.

You can also set the factor by which audio sources will be attenuated depending on their distance to the listener.

```cpp
sound.setMinDistance(5.f);
sound.setAttenuation(10.f);
```

The _minimum distance_ is the distance under which the sound will be heard at its maximum volume.
As an example, louder sounds such as explosions should have a higher minimum distance to ensure that they will be heard from far away.
Please note that a minimum distance of 0 (the sound is inside the head of the listener!) would lead to an incorrect spatialization and result in a non-attenuated sound.
0 is an invalid value, never use it.

The _attenuation_ is a multiplicative factor.
The greater the attenuation, the less it will be heard when the sound moves away from the listener.
To get a non-attenuated sound, you can use 0.
On the other hand, using a value like 100 will highly attenuate the sound, which means that it will be heard only if very close to the listener.

Here is the exact attenuation formula, in case you need accurate values:

```
MinDistance   is the sound's minimum distance, set with setMinDistance
Attenuation   is the sound's attenuation, set with setAttenuation
Distance      is the distance between the sound and the listener
Volume factor is the calculated factor, in range [0 .. 1], that will be applied to the sound's volume

Volume factor = MinDistance / (MinDistance + Attenuation * (max(Distance, MinDistance) - MinDistance))
```

<<< SFML/SFML-Website@6d461d9:pages/tutorials/3.1/audio/streams.md >>>
# Custom audio streams

!!! advanced "Advanced topic"

    Custom audio streams require deriving from `sf::SoundStream` and handling data requests that come from another thread.
    To play music files, `sf::Music` already does all of that for you, so you only need a custom stream when you generate or receive the audio data yourself.

## Audio stream? What's that?

An audio stream is similar to music (remember the [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") class?).
It has almost the same functions and behaves the same.
The only difference is that an audio stream doesn't play an audio file: Instead, it plays a custom audio source that _you_ directly provide.
In other words, defining your own audio stream allows you to play from more than just a file: A sound streamed over the network, music generated by your program, an audio format that SFML doesn't support, etc.

In fact, the [`sf::Music`](../../../documentation/3.1.0/classsf_1_1Music.html "sf::Music documentation") class is just a specialized audio stream that gets its audio samples from a file.

Since we're talking about _streaming_, we'll deal with audio data that cannot be loaded entirely in memory, and will instead be loaded in small chunks while it is being played.
If your sound can be loaded completely and can fit in memory, then audio streams won't help you: Just load the audio data into a [`sf::SoundBuffer`](../../../documentation/3.1.0/classsf_1_1SoundBuffer.html "sf::SoundBuffer documentation") and use a regular [`sf::Sound`](../../../documentation/3.1.0/classsf_1_1Sound.html "sf::Sound documentation") to play it.

## sf::SoundStream

In order to define your own audio stream, you need to inherit from the [`sf::SoundStream`](../../../documentation/3.1.0/classsf_1_1SoundStream.html "sf::SoundStream documentation") abstract base class.
There are two virtual functions to override in your derived class: `onGetData` and `onSeek`.

```cpp
class MyAudioStream : public sf::SoundStream
{
    bool onGetData(Chunk& data) override;

    void onSeek(sf::Time timeOffset) override;
};
```

`onGetData` is called by the base class whenever it runs out of audio samples and needs more of them.
You must provide new audio samples by filling the `data` argument:

```cpp
bool MyAudioStream::onGetData(Chunk& data)
{
    data.samples = /* put the pointer to the new audio samples */;
    data.sampleCount = /* put the number of audio samples available in the new chunk */;
    return true;
}
```

You must return `true` when everything is all right, or `false` if playback must be stopped, either because an error has occurred or because there's simply no more audio data to play.

SFML makes an internal copy of the audio samples as soon as `onGetData` returns, so you don't have to keep the original data alive if you don't want to.

The `onSeek` function is called when the `setPlayingOffset` public function is called.
Its purpose is to change the current playing position in the source data.
The parameter is a time value representing the new position from the beginning of the sound (_not_ from the current position).
This function is sometimes impossible to implement.
In those cases leave it empty, and tell the users of your class that changing the playing position is not supported.

Now your class is almost ready to work.
The only thing that [`sf::SoundStream`](../../../documentation/3.1.0/classsf_1_1SoundStream.html "sf::SoundStream documentation") needs to know now is the channel count, the sample rate of your stream, and a channel map, so that it can be played as expected.
To let the base class know about these parameters, you must call the `initialize` protected function as soon as they are known in your stream class (which is most likely when the stream is loaded/initialized).

```cpp
// where this is done totally depends on how your stream class is designed
unsigned int channelCount = ...;
unsigned int sampleRate = ...;
std::vector<SoundChannel> channelMap = ...;
initialize(channelCount, sampleRate, channelMap);
```

## Threading issues

Audio streams are always played in a separate thread, therefore it is important to know what happens exactly, and where.

`onSeek` is called directly by the `setPlayingOffset` function, so it is always executed in the caller thread.
However, the `onGetData` function will be called repeatedly as long as the stream is being played, in a separate thread created by SFML.
If your stream uses data that may be accessed _concurrently_ in both the caller thread and in the playing thread, you have to protect it (with a mutex for example) in order to avoid concurrent access, which may cause undefined behavior -- corrupt data being played, crashes, etc.

## Using your audio stream

Now that you have defined your own audio stream class, let's see how to use it.
In fact, things are very similar to what's shown in the [tutorial about `sf::Music`](sounds.md "Playing sounds and musics").
You can control playback with the `play`, `pause`, `stop` and `setPlayingOffset` functions.
You can also play with the sound's properties, such as the volume or the pitch.
You can refer to the API documentation or to the other audio tutorials for more details.

## A simple example

Here is a very simple example of a custom audio stream class which plays the data of a sound buffer.
Such a class might seem totally useless, but the point here is to focus on how the data is streamed by the class, regardless of where it comes from.

```cpp
#include <SFML/Audio.hpp>

#include <vector>

// custom audio stream that plays a loaded buffer
class MyStream : public sf::SoundStream
{
public:
    void load(const sf::SoundBuffer& buffer)
    {
        // extract the audio samples from the sound buffer to our own container
        m_samples.assign(buffer.getSamples(), buffer.getSamples() + buffer.getSampleCount());

        // reset the current playing position
        m_currentSample = 0;

        // initialize the base class
        initialize(buffer.getChannelCount(),
                   buffer.getSampleRate(),
                   {sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight});
    }

private:
    bool onGetData(Chunk& data) override
    {
        // number of samples to stream every time the function is called;
        // in a more robust implementation, it should be a fixed
        // amount of time rather than an arbitrary number of samples
        const int samplesToStream = 50000;

        // set the pointer to the next audio samples to be played
        data.samples = &m_samples[m_currentSample];

        // have we reached the end of the sound?
        if (m_currentSample + samplesToStream <= m_samples.size())
        {
            // end not reached: stream the samples and continue
            data.sampleCount = samplesToStream;
            m_currentSample += samplesToStream;
            return true;
        }
        else
        {
            // end of stream reached: stream the remaining samples and stop playback
            data.sampleCount = m_samples.size() - m_currentSample;
            m_currentSample  = m_samples.size();
            return false;
        }
    }

    void onSeek(sf::Time timeOffset) override
    {
        // compute the corresponding sample index according to the sample rate and channel count
        m_currentSample = static_cast<std::size_t>(timeOffset.asSeconds() * getSampleRate() * getChannelCount());
    }

    std::vector<std::int16_t> m_samples;
    std::size_t               m_currentSample{};
};

int main()
{
    // load an audio buffer from a sound file
    const sf::SoundBuffer buffer("sound.wav");

    // initialize and play our custom stream
    MyStream stream;
    stream.load(buffer);
    stream.play();

    // let it play until it is finished
    while (stream.getStatus() == MyStream::Status::Playing)
        sf::sleep(sf::seconds(0.1f));
}
```

# 빌드 템플릿 (CMakeLists.txt)

```cmake
# SFML 3.1 조건 빌드 템플릿. LLM은 main.cpp만 작성하며, 이 파일은 고치지 않는다.
#
#     cmake -S . -B build -DMINIGAME_ASSETS_DIR=<공통 assets 폴더>
#     cmake --build build --target game
#     cd build && ./game        # 작업 디렉터리에 assets/가 있어야 한다
cmake_minimum_required(VERSION 3.28)
project(minigame_sfml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# meno v0.2.0이 내부에서 쓰는 SFML 설정과 같게 맞춘다: 3.1.0, 정적 빌드, network 모듈 제외.
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_DOC      OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK  OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF)

# 태그는 옮겨질 수 있으므로 커밋 해시로 고정한다 (meno 템플릿과 같은 해시. 얕은 clone은 해시를 받지 못해 쓰지 않는다).
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(SFML)

add_executable(game main.cpp)
# Graphics는 Window·System을 함께 끌어온다.
target_link_libraries(game PRIVATE SFML::Graphics SFML::Audio)

# 두 조건이 같은 에셋을 쓰도록 공통 폴더를 실행 파일 옆 assets/로 복사한다.
set(MINIGAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "공통 에셋 폴더")
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MINIGAME_ASSETS_DIR}" "$<TARGET_FILE_DIR:game>/assets")
```

# 요구사항 문서

# 카드 짝 맞추기 요구사항

meno vs SFML 3 : LLM 생성 비교 실험

## 카드 짝 맞추기 게임 개발

- 이 문서는 LLM 생성 비교 실험에 쓰는 미니게임 중 카드 짝 맞추기의 요구사항을 정리함.
- 같은 명세를 meno 조건과 SFML 3.1 조건에 그대로 주며, LLM은 main.cpp 하나만 작성함.
- 창 제목은 "PAIRS"임.
- 색은 '화면 색' 절과 제공된 에셋을 따름.

## Major Functional Requirements

- 카드 짝 맞추기의 주요 기능적 요구사항(Functional Requirement)은 다음과 같음.
  - 타이틀·플레이·결과로 이어지는 화면 흐름
  - 카드 보드와 클릭으로 뒤집기
  - 짝 판정과 타이머
  - 결과 화면의 이름 입력과 기록
- 게임 실행 후 종료까지 거치는 기본 기능이며, 자세한 요구사항은 이어지는 절에서 설명함.

## Major Non-functional Requirements

- 주요 비기능적 요구사항(Non-Functional Requirement)은 다음과 같음.
  - 제공된 템플릿에서 main.cpp 한 파일로 오류 없이 빌드되어야 함.
  - 60 FPS 제한에서 프레임레이트와 무관하게 같은 속도로 진행해야 함.
  - 입력을 놓치지 않고 다음 갱신에 반영해야 함.
  - 입력 없이 실행해도 비정상 종료 없이 명세된 흐름을 보여야 함.
  - 클릭에 바로 반응하고, 창 크기를 바꿔도 클릭 판정이 정확해야 함.
- 자세한 요구사항은 이어지는 절에서 설명함.

## 화면 흐름과 공통 조작

- 창은 960×540이며, 제목 표시줄에는 "PAIRS"을 표시해야 함.
- 화면은 타이틀 → 플레이 → 결과 순서로 진행함.
  - 타이틀: 게임 이름 "PAIRS"(창 제목과 같음)과 "PRESS ENTER"를 표시. Enter를 누르거나 2초가 지나면 플레이 시작.
  - 결과: 결과 문구, 최종 점수, "R: RESTART  ESC: QUIT"를 표시.
- 결과 화면에서 R을 누르면 게임 상태를 모두 초기화하고(프로그램이 켜져 있는 동안 유지한다고 명세에 적힌 기록은 제외) 플레이를 다시 시작함 (타이틀은 거치지 않음).
- Esc는 어느 화면에서든 프로그램을 종료함. 창 닫기 버튼도 같음.
- 화면 문구는 영문 대문자이며, 명세에 적힌 문자열을 그대로 씀.
- 글자는 assets/fonts/game.ttf로 그리며, 크기는 16의 배수(16·32·48px)로 함.
- 효과음은 명세에 지정된 사건에서, 지정된 파일로만 재생함.

## 보드와 뒤집기

- 카드는 80×100이며 4행×4열 16장(8쌍)을 간격 12px로 화면 가운데에 놓음. 화면 위쪽에 "MOVES 0   TIME 0.0"을 표시함.
- assets/textures/cards.png는 80×100 칸 9개를 가로로 이은 그림이며, 0번 칸은 뒷면, 1~8번 칸은 앞면임. 카드는 이 그림에서 해당 칸만 잘라 그림.
- 새 게임마다 카드를 무작위로 섞음.
- 뒷면 카드를 마우스 왼쪽 버튼으로 누르면 앞면으로 뒤집고 assets/sfx/flip.wav를 재생함. 앞면 카드, 짝을 맞춘 카드, 판정을 기다리는 동안의 클릭은 무시함.
- 클릭 판정은 카드 사각형 안쪽만임. 카드 사이 간격을 누르면 아무 일도 없음.
- 플레이 중에는 assets/music/bgm_card.ogg를 볼륨 40%로 반복 재생하고, 결과 화면에서는 멈춤.

## 판정과 기록

- 두 장이 앞면이 되면 판정하고 MOVES를 1 늘림.
  - 같으면 두 장을 앞면으로 남기고 assets/sfx/point.wav를 재생함.
  - 다르면 0.8초 뒤 두 장을 뒷면으로 돌리며, 그 0.8초 동안의 클릭은 무시함.
- TIME은 첫 카드를 뒤집은 순간부터 0.1초 단위로 늘어나며, 모든 짝을 맞추면 멈춤.
- 8쌍을 모두 맞추면 결과 화면으로 감. 문구는 "CLEAR", 기록은 "MOVES n   TIME t" 형식임 (최종 점수에 해당).

## 이름 입력과 순위표

- 결과 화면에서 이름을 입력받음. 영문자와 숫자만, 최대 8자이며, 영문자는 대문자로 저장함. Backspace로 지우고 Enter로 확정함.
  - 입력 중에는 "NAME: " 뒤에 입력한 글자를 표시함. 이름이 비어 있으면 Enter를 무시함.
  - 입력 중에는 R도 글자로 들어가며, R 재시작은 확정한 뒤에만 동작함.
- 확정하면 이번 실행 중의 기록 상위 5개를 "1. ABC   MOVES 12   TIME 34.5" 형식으로 한 줄에 하나씩 보여 줌. 기록은 프로그램을 끄면 사라짐.
- 순서는 MOVES가 적은 순, 같으면 TIME(0.1초 단위 표시값)이 짧은 순, 그것도 같으면 먼저 낸 기록이 위임.
- 방금 기록은 강조색 글자로 표시함. 5위 밖이면 순위표 아래 줄에 같은 형식(순위 포함)으로 따로 표시함.

## 화면 색

- 배경: #1B2A1F
- 글자: 흰색 #FFFFFF
- 순위표의 방금 기록: #FFD54A
- 텍스처가 있는 요소는 텍스처의 색을 그대로 씀.

## 클릭 판정과 무입력 실행

- 클릭하면 다음 프레임에 카드가 뒤집혀야 함. 0.1초 간격의 빠른 클릭도 놓치지 않아야 함.
- 창 크기를 바꾸면 16:9 비율을 유지(검은 띠)하고, 클릭 판정이 화면에 보이는 카드 위치와 일치해야 함.
- 무입력 실행: 보드가 나타나고 모든 카드가 뒷면이며, 카드를 뒤집기 전이므로 TIME이 0.0에 머물러야 함.
  - 예상 흐름 (실행 시작 기준, 근사값): 0초 타이틀 → 2초 보드 표시, 모두 뒷면 → 2~10초 변화 없음 (TIME 0.0)

## 코드와 빌드

- 코드는 main.cpp 한 파일이며, 제공된 CMakeLists.txt와 assets/는 수정하지 않음.
- meno 조건은 meno 공개 헤더만 쓰고 SFML 헤더를 직접 include하지 않음. SFML 조건은 SFML 3.1 모듈만 씀. 두 조건 모두 C++20 표준 라이브러리 외의 라이브러리는 쓰지 않음.
- 기준 환경에서 오류 없이 빌드되어야 함.
- 에셋을 읽지 못하면 실패한 경로를 표준 에러에 출력하고 종료 코드 1로 끝내야 함.

## 실행 품질

- 프레임은 60 FPS로 제한함. 모든 속도와 시간은 초 단위이며, 프레임레이트가 달라져도 게임 진행 속도는 같아야 함.
- 키와 마우스 입력은 다음 갱신에 반영하며, 한 프레임보다 짧게 눌렀다 뗀 입력도 놓치지 않아야 함.
- 입력 없이 10초 실행하는 동안 비정상 종료나 처리되지 않은 예외가 없어야 함. Esc나 창 닫기로 끝나면 종료 코드 0.
- 기준 환경에서 프레임 시간 95백분위가 20ms 이하여야 함.
  - 이 항목은 측정 하니스로만 확인함 (플레이 채점 제외).

## 게임 실행 환경

- 기준 환경: Ubuntu 24.04, GCC 13.3, CMake 3.28 이상
- SFML 3.1.0 (meno의 백엔드와 같은 버전)
- 화면 없는 실행은 xvfb-run을 씀.
- 제공물: 조건별 CMakeLists.txt 템플릿과 공통 assets/ 폴더. 두 조건이 같은 에셋을 씀.
- 에셋은 상대 경로 assets/…로 엶. 실행 파일은 assets/와 같은 폴더에 있으며, 그 폴더를 작업 디렉터리로 하여 실행함.
- 텍스처는 제공된 크기 그대로 게임 안에서 그림.

## 제공 에셋

- 이 게임이 쓰는 에셋:
  - assets/fonts/game.ttf
  - assets/sfx/point.wav
  - assets/sfx/flip.wav
  - assets/music/bgm_card.ogg
  - assets/textures/cards.png (720×100)

## 주의 사항

- 명세의 수치와 문자열은 정확히 따라야 함.
- 색과 그림은 명세와 제공된 에셋에 정한 것을 씀.
- 명세가 모호하면 가장 단순한 해석을 택함.

# 할 일

위 요구사항 문서의 게임을 SFML 3.1로 구현한 main.cpp 전체를 ```cpp 코드 블록 하나로 보내 주세요.
