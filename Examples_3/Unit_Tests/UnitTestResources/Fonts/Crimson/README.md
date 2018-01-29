The *Crimson Text* Typeface
===========================

Crimson Text ("Crimson") is a free and open-source text type family.

<img src="https://raw.githubusercontent.com/skosch/Crimson/master/specimen1.png" width="500" >

Directories and Releases
------------------------
The `Desktop Fonts` and `Web Fonts` directories above always contain the most up-to-date binaries, respectively.

`Desktop Fonts` contains both OTF and TTF versions of the font. While OTF is generally regarded as the more modern and powerful format, some Windows users may prefer the rendered appearance of the TTF files, at least for screen use.

`Web Fonts` contains subsetted (!) TTF, EOF and WOFF files. If the provided files do not meet the requirements of your website, which may well be the case, you will need to generate the webfonts yourself – using either a font editor like Fontforge or an online service such as fontsquirrel.com.

Background
----------
The font is designed in the tradition of beautiful oldstyle type, and inspired particularly by the fantastic work of people like Jan Tschichold (Sabon), Robert Slimbach (Arno, Minion) and Jonathan Hoefler (Hoefler Text). It features

* six cuts (regular, semibold and bold; with a Roman and Italic each)
* characters for a wide range of European languages – though some are still better supported than others between different cuts
* spacing/kerning done by Igino Marini's spectacular [iKern](http://www.ikern.com)
* an unbeatable price of zero!

Naming confusion
----------------
The full name of the font is *Crimson Text*, since I had originally planned to follow up with a *Crimson Display* as well. That never came to pass, and laziness on my part and that of most users led to the *Text* casually falling by the wayside. To add to the confusion, two distinct Crimsons exist, since the original font was completely redrawn two years after its release. While you are looking at the new, improved version, Google Fonts continues to offer the clumsy-looking original via and has not heeded the repeated pleas for an upgrade.

Credits
-------
This project owes its success to (in no particular order)
* Google's generous funding,
* a handful of anonymous donors,
* Adrien Tétar, for tirelessly fixing bugs,
* Rainer Schuhsler, for correcting the vertical metrics,
* Hector Haralambolous from Athens, who contributed many of the Coptic and Cyrillic glyphs,
* Georg Duffner of EB-Garamond fame, who helped with OpenType wizardry,
* Khaled Hosny, font guru, for fixing things I never knew were broken,
* George Williams, author of FontForge,
* Kate F., for hours and hours of overtime put into all of the font's rough edges
* the many talented and generous people I forgot to mention, including those submitting bug reports

Contributing
------------
Contributions to the project in any form are very much welcome, and indeed encouraged! We always need help with:
* Improving/tidying up glyph outlines
* Correcting wrongly placed accents, messed up encodings, or making other language-specific fixes
* Writing comprehensive OpenType features
* Adding new glyphs to expand the coverage of languages, symbol sets, typographic niceties, etc.
* Ironing out spacing and kerning wrinkles
* Streamlining the build process from SFD (or UFO) to binary files.

I'm looking forward to your pull requests!
