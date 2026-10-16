#include "Patterns.h"

// Built-in grooves, written as step grids.
//
//   @Genre                  starts a genre
//   =Name;bars;steps        starts a pattern (steps per bar: 16 or 12 for triplets)
//   CODE  steps             one row, see drumRows() for the codes
//
// Steps: X accent, x normal, o soft, g ghost, . rest. Spaces and | are ignored.

namespace woody
{
namespace
{
const char* const libraryText = R"(
@Rock
=Verso A;1;16
H   X.x.X.x.X.x.X.x.
S   ....X.......X...
K   x.......x.x.....
=Verso B;2;16
H   X.x.X.x.X.x.X.x.|X.x.X.x.X.x.X.x.
S   ....X.......X...|....X.......X..g
K   x.......x.x.....|x.....x.x.x.....
=Refrão;2;16
C1  X...............|................
R   x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.
S   ....X.......X...|....X.......X...
K   x.....x.x.x.....|x.....x.x.......
=Virada 1;1;16
H   X.x.X.x.........
S   ....X...x.x.....
T1  ..........xx....
T2  ............xx..
FT  ..............xX
K   x.......x.......
=Virada 2;1;16
H   X.x.X.x.........
S   ....X...oxxxXxXX
K   x.......x.......
=Final;1;16
C1  X...............
K   X...............

@Pop
=Verso A;1;16
H   x.o.x.o.x.o.x.o.
S   ....x.......x...
K   x......x..x.....
=Verso B;1;16
H   xoxoxoxoxoxoxoxo
SS  ....x.......x...
K   x......x..x.....
=Refrão;2;16
C1  X...............|................
HO  ..x...x...x...x.|..x...x...x...x.
H   x...x...x...x...|x...x...x...x...
S   ....X.......X...|....X.......X...
P1  ....x.......x...|....x.......x...
K   x.....x.x.x.....|x.....x.x.x.....
=Virada 1;1;16
H   x.o.x.o.x.......
S   ....x...xxx.x.x.
K   x.......x.......
=Virada 2;1;16
H   x.o.x.o.........
T1  ........xx......
FT  ..........xx....
S   ....x.......x.xx
K   x.......x.......

@Funk
=Groove A;1;16
H   XoxoXoxoXoxoXoxo
S   ....X..g.g..X..g
K   x.x.......x..x..
=Groove B;2;16
H   XoxoXoxoXoxoXo..|XoxoXoxoXoxoXoxo
HO  ..............x.|................
S   ....X..g.g..X..g|.g..X..g.g..X.g.
K   x.x.......x..x..|x..x..x...x.....
=Refrão;1;16
HO  ..x...x...x...x.
H   x...x...x...x...
S   ....X..g.g..X..g
K   x.x...x...x..x..
=Virada;1;16
H   XoxoXoxo........
S   ....X..g.xx.....
T1  ...........xx...
FT  .............xxX
K   x.x.....x.......

@Shuffle
=Shuffle A;1;12
H   x.xx.xx.xx.x
S   ...X.....X..
K   x.....x.....
=Shuffle B;1;12
R   x.xx.xx.xx.x
S   ...X..g..X.g
K   x.....x..x..
=Virada;1;12
H   x.xx.x......
S   ...X..xxx...
T1  .........x..
T2  ..........x.
FT  ...........X
K   x.....x.....

@Jazz
=Swing A;1;12
R   x..x.xx..x.x
HP  ...x.....x..
K   g..g..g..g..
=Swing B;2;12
R   x..x.xx..x.x|x..x.xx..x.x
HP  ...x.....x..|...x.....x..
S   .......g...o|.....g......
K   g..g..g..g..|g..g..g..x..
=Virada;1;12
R   x..x.x......
S   ......x.xo.X
K   g..g..g..x..

@Bossa Nova
=Bossa A;2;16
H   o.o.o.o.o.o.o.o.|o.o.o.o.o.o.o.o.
SS  x..x..x...x..x..|..x..x...x..x...
K   x..xx..xx..xx..x|x..xx..xx..xx..x
=Bossa B;2;16
R   o.o.o.o.o.o.o.o.|o.o.o.o.o.o.o.o.
SS  x..x..x...x..x..|..x..x...x..x...
K   x..xx..xx..xx..x|x..xx..xx..xx..x
=Virada;1;16
SS  x..x..x.........
T2  ........o.o.....
FT  ............o.x.
K   x..xx..x........

@Samba
=Samba A;1;16
H   xoxxxoxxxoxxxoxx
SS  ..x..x.x..x..x.x
K   x..xX..xx..xX..x
=Samba B;1;16
P2  xoxxxoxxxoxxxoxx
S   g.xg.gx.g.xg.gxg
K   x..xX..xx..xX..x
=Virada;1;16
S   x.xxx.xxx.xxX.XX
K   x..xX..xx..xX..x

@Baião
=Baião A;1;16
H   xoxxxoxxxoxxxoxx
SS  ....x.......x...
K   x..x....x..x....
=Baião B;1;16
P2  xoxxxoxxxoxxxoxx
SS  ....x.......x...
K   x..x..x.x..x..x.
=Virada;1;16
H   xoxxxoxx........
S   ........x.x.xxxX
K   x..x....x.......

@Reggae
=One Drop;1;16
H   x.x.x.x.x.x.x.x.
SS  ........x.......
K   ........x.......
=Steppers;1;16
H   x.x.x.x.x.x.x.x.
SS  ........x.......
K   x...x...x...x...
=Rockers;1;16
H   ..x...x...x...x.
S   ....x.......x...
K   x.......x.......
=Virada;1;16
H   x.x.x.x.........
S   ........x.xx.x.x
K   ........x.......

@Hip Hop
=Boom Bap A;1;16
H   x.x.x.x.x.x.x.x.
S   ....X.......X...
K   x......x..x.....
=Boom Bap B;2;16
H   x.x.x.x.x.x.x.x.|x.x.x.x.x.x.xoxo
S   ....X.......X...|....X.......X...
K   x......x..x.....|x.x....x..x..x..
=Lo-fi;1;16
H   x.o.x.o.x.o.xoxo
SS  ....x.......x...
K   x.....x...x.....

@Metal
=Double Bass;1;16
FX  x...x...x...x...
S   ....X.......X...
K   xxxxxxxxxxxxxxxx
=Thrash;1;16
R   x.x.x.x.x.x.x.x.
S   ..x...x...x...x.
K   x...x...x...x...
=Blast;1;16
C1  x.x.x.x.x.x.x.x.
S   .x.x.x.x.x.x.x.x
K   x.x.x.x.x.x.x.x.
=Virada;1;16
S   xxxxxxxx........
T1  ........xx......
T2  ..........xx....
FT  ............xxxx
K   x.x.x.x.x.x.x.x.

@Balada
=Verso;1;16
H   x.o.x.o.x.o.x.o.
SS  ........x.......
K   x.....x.........
=Refrão;2;16
C1  X...............|................
R   x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.
S   ........X.......|........X.......
K   x.....x...x.....|x.....x.........
=Virada;1;16
H   x.o.x.o.........
S   ........x..x.x..
T1  ............x...
FT  ..............xx
K   x.......x.......

@Disco
=Disco A;1;16
HO  ..x...x...x...x.
H   x...x...x...x...
S   ....X.......X...
K   x...x...x...x...
=Disco B;1;16
HO  ..x...x...x...x.
H   xo.oxo.oxo.oxo.o
S   ....X.......X...
P1  ....x.......x...
K   x...x...x...x...
=Virada;1;16
S   ....X...xxxxXXXX
K   x...x...x...x...

@Funk Carioca
=Tamborzão;1;16
P1  ....x.......x...
H   x.x.x.x.x.x.x.x.
K   x..x..x...x..x..
=Montagem;1;16
S   ...x..x....x..x.
K   x.....x...x.....
=Virada;1;16
S   ........x.x.xxXX
K   x..x..x.........
)";

int velocityFor (juce::juce_wchar c)
{
    switch (c)
    {
        case 'X': return 122;
        case 'x': return 100;
        case 'o': return 75;
        case 'g': return 42;
        default:  return 0;
    }
}

std::vector<Pattern> parseLibrary()
{
    std::vector<Pattern> patterns;
    juce::String genre;

    for (auto line : juce::StringArray::fromLines (juce::String::fromUTF8 (libraryText)))
    {
        line = line.trim();
        if (line.isEmpty())
            continue;

        if (line.startsWithChar ('@'))
        {
            genre = line.substring (1).trim();
            continue;
        }

        if (line.startsWithChar ('='))
        {
            const auto parts = juce::StringArray::fromTokens (line.substring (1), ";", "");
            Pattern p;
            p.genre = genre;
            p.name = parts[0].trim();
            p.bars = juce::jmax (1, parts[1].getIntValue());
            p.stepsPerBar = juce::jmax (4, parts[2].getIntValue());
            patterns.push_back (p);
            continue;
        }

        if (patterns.empty())
            continue;

        const auto code = line.upToFirstOccurrenceOf (" ", false, false);
        const auto* row = rowForCode (code);
        jassert (row != nullptr); // unknown row code in libraryText
        if (row == nullptr)
            continue;

        auto& p = patterns.back();
        const auto steps = line.fromFirstOccurrenceOf (" ", false, false).removeCharacters (" |");
        jassert (steps.length() == p.totalSteps()); // row length must match bars * steps

        for (int i = 0; i < juce::jmin (steps.length(), p.totalSteps()); ++i)
            if (const int velocity = velocityFor (steps[i]))
                p.hits.push_back ({ i, row->note, velocity });
    }

    return patterns;
}
} // namespace

const std::vector<Pattern>& builtInPatterns()
{
    static const auto patterns = parseLibrary();
    return patterns;
}

juce::StringArray builtInGenres()
{
    juce::StringArray genres;
    for (const auto& p : builtInPatterns())
        genres.addIfNotAlreadyThere (p.genre);
    return genres;
}
} // namespace woody
