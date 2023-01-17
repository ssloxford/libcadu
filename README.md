# libcadu

This is a header-only C++ library for en/decoding [CADU](https://directreadout.sci.gsfc.nasa.gov/links/rsd_eosdb/PDF/ICD_Space_Ground_Aqua.pdf) frames, as used in the Terra/Aqua satellite missions.
These files are sometimes called "Level 0" data, and can be found at the NASA archives such as [LAADS DAAC](https://ladsweb.modaps.eosdis.nasa.gov/#level0-level1).

Also contained are the following command-line tools:

* `caduinfo` - Displays the header contents of a CADU stream from stdin.
* `cadupack` - Pack bytes from stdin into a CADU stream on stdout.
* `caduunpack` - Unpack a CADU stream from stdin to stdout.
* `caduhead` - Output the first part of a CADU stream from stdin, in whole CADUs, up to a given index.
* `cadutail` - Output the last part of a CADU stream from stdin, in whole CADUs, from a given index.

## Building

Dependencies:

* [libfec](https://github.com/quiet/libfec/)
* [libgetsetproxy](https://github.com/ssloxford/libgetsetproxy)

Building:

```
make
make install
```

## Further ideas

Detection of whether CADU is a "fill" CADU (as used in gov/nasa/gsfc/drl/rtstps/core/ccsds/CaduService.java) l202

Packet routing system to replace RT-STPS

## Related work/documents

NASA have decoders for Level 0 data already implemented in the IPOPP MODISL1DB algorithm available at the [Direct Readout Laboratory website](https://directreadout.sci.gsfc.nasa.gov/).

This wraps the [ocssw tools](https://oceandata.sci.gsfc.nasa.gov/ocssw) from the OceanColor Data software repositories.

A description of the protocol in found in [this document](https://directreadout.sci.gsfc.nasa.gov/links/rsd_eosdb/PDF/ICD_Space_Ground_Aqua.pdf).
Note that the checksum, although documented as an XOR, is actually implemented as addition in the official NASA data decoders - we therefore implemented addition as well.

## Thanks

Many thanks to [Jonathan Tanner](https://github.com/aDifferentJT) for his help in writing this library.
