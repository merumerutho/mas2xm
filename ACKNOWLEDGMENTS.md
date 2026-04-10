# Acknowledgments

mas2xm exists because the **MAS module format** exists, and that format
was designed and originally implemented by **Mukunda Johnson** in 2008
as part of [mmutil](https://github.com/blocksds/mmutil), the maxmod
utility. The [maxmod](https://github.com/blocksds/maxmod) runtime that
plays MAS modules on Nintendo DS and Game Boy Advance hardware was
built by the same author. Both projects are currently maintained as
part of the [BlocksDS SDK](https://blocksds.skylyrac.net/) by
**Antonio Niño Díaz** and other contributors. See each project's
upstream `COPYING` file for the full copyright list.

mas2xm goes the other way, turning a `.mas` file back into a
FastTracker II `.xm`. The format it consumes, the design choices it
preserves, and the whole reason it has anything to do exist because of
the work of Mukunda Johnson and the people who have maintained mmutil
and maxmod since.

## What mas2xm is, and what it is not

mas2xm is an **independent re-implementation** written from format
knowledge. It does not contain or redistribute any source code from
mmutil or maxmod. No function bodies, struct layouts, or comments
were copied. The code in `src/` was authored from scratch to consume
the byte layout that mmutil produces.

The format knowledge itself was reverse-engineered by reading mmutil's
open-source code and writing it down as a separate specification,
[mas_spec](https://github.com/merumerutho/mas_spec). mas2xm was then
implemented against that specification rather than against the mmutil
source directly. The constants you see in `src/types.h` such as
`MAS_FLAG_LINK_GXX`, `MM_SREPEAT_FORWARD`, `MAS_VERSION = 0x18`, the
IT-letter effect numbering, and the bit positions in the header flags
are facts about the on-disk format. They are not copyrightable
expression: any independent implementation of a MAS reader has to use
exactly the same values, because that is what is in the bytes.

## License relationship

| Project | License | Copyright holders | Relationship to mas2xm |
|---|---|---|---|
| **mmutil** | ISC | © 2008 Mukunda Johnson; current maintenance © 2023-2025 Antonio Niño Díaz, plus other BlocksDS contributors (see upstream `COPYING`) | Format origin. mas2xm reads the bytes mmutil writes. No code is copied. |
| **maxmod** | ISC | © 2008-2009 Mukunda Johnson; © 2021-2025 Antonio Niño Díaz; © 2023 Lorenzooone; plus other BlocksDS contributors (see upstream `COPYING`) | Runtime that plays the format. mas2xm does not link against maxmod or use any of its code. |
| **mas_spec** | CC-BY-4.0 (separately published) | © 2026 merumerutho | Human-readable documentation of the MAS format, derived from reading mmutil's source. mas2xm's reference, not its dependency. |
| **mas2xm** | MIT | © 2026 merumerutho | This project. Original code only. |

mas2xm does not redistribute any source code from mmutil or maxmod, so
the ISC license that covers those projects does not attach to mas2xm's
source files. mas2xm ships under the MIT license listed in
[`LICENSE`](LICENSE), in the same spirit of mmutil and maxmod's ISC licensing.

This acknowledgments file exists to credit prior art clearly. mas2xm
is **independent third-party work**. It is not endorsed by, affiliated
with, or sponsored by Mukunda Johnson, Antonio Niño Díaz, Lorenzooone,
the BlocksDS SDK project, or the original mmutil/maxmod projects. The
names "mmutil", "maxmod", "BlocksDS", and the names of individual
contributors are used here strictly for accurate technical attribution
and historical credit. They are not used to imply endorsement of
mas2xm.

## Upstream links

- **mmutil**: https://github.com/blocksds/mmutil (ISC; current canonical fork maintained by the BlocksDS SDK project)
- **maxmod**: https://github.com/blocksds/maxmod (ISC; current canonical fork maintained by the BlocksDS SDK project)
- **BlocksDS SDK**: https://blocksds.skylyrac.net/ (the umbrella project that maintains both mmutil and maxmod today)
- **devkitPro**: https://devkitpro.org/ (the historical home of mmutil and maxmod; the original 2008 mmutil release lived here)
- **mas_spec**: https://github.com/merumerutho/mas_spec (the format documentation mas2xm uses as its reference)

## Other credits

- The **FastTracker II `.xm` format** was created by Fredrik "Mr.H of
  Triton" Huss and Magnus "Vogue of Triton" Högdahl. mas2xm's
  `xm_write.c` reconstructs files according to the public `XM.TXT`
  documentation that ships with FT2 clones. The format itself is
  widely documented prior art.
- The **Impulse Tracker** effect numbering convention (A=1..Z=26 plus
  extensions 27-30) used in MAS effect bytes originates with Jeffrey
  Lim's Impulse Tracker. mmutil adopted it as its in-memory effect
  numbering scheme. mas2xm decodes it back to the XM effect range.
- The **mas2xm test corpus** is built against a small set of XM files
  shipped with the [MAXMXDS](https://github.com/merumerutho/MAXMXDS)
  reference project. None of those files are redistributed in this
  repository. The regression tests need a path to a local copy.

## Reporting credit problems

If you believe code or documentation in this repository fails to
credit you appropriately, please open an issue or contact the
maintainer. Credit problems are taken seriously and corrections are
welcome.
