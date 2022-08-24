# brr2aif

I wrote this because I wanted to see how SNES samples could sound in GBA games, and the sample
converter I used for GBA (aif2pcm by huderlem) took AIFF files tuned to middle C as input.

Features:

* Correctly handles strange samples that cause integer overflow in the SNES's DSP.
* Has no known undefined behavior, at least on the happy path. UBSan does not complain about
  anything.
* Pure ISO C, few assumptions about architecture. I think this software ought to work correctly on
  all the current major PC architectures (x86/x64/AArch64), even though AIFF relies on 80-bit
  floats.
* Tells you when it runs out of memory or can't write to a file. At least, it should. As of writing,
  only the happy path has been tested.

See the LICENSE file in the root of the repository for licensing information. To paraphrase it, this
software is just a personal project, and I don't want any companies that want to make money off of
old SNES game music to use it, at least for the next 20 years. After that point, all the parts I
have copyright over will be effectively relicensed under the CC0 1.0, so that anybody can use the
software. Nobody will probably care about this software by then, anyway.

Note that the copyright of this software is unrelated to the copyright of the samples it can
convert. Make sure you have appropriate rights to the samples you feed into the program, and all
that.

This software uses information from the following sources:

* [fullsnes](https://problemkaputt.de/fullsnes.htm#snesapudspbrrsamples) by Martin Korth
* `preproc` by YamaArashi, for a function that reads a whole file
  [without undefined behavior](https://wiki.sei.cmu.edu/confluence/display/c/FIO19-C.+Do+not+use+fseek%28%29+and+ftell%28%29+to+compute+the+size+of+a+regular+file)
* The [EarthBound Music Editor](https://github.com/PKHackers/ebmused), for the bulk of the
  BRR sample conversion code's control flow.

## Build instructions

Just run a C compiler on the code: `c99 brr2aif.c -o brr2aif`