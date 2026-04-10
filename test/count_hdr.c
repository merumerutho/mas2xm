#include <stdio.h>
// XM instrument header fields (from XM.TXT):
// 4  Instrument size (includes all the below)
// 22 Instrument name
// 1  Instrument type (always 0)
// 2  Number of samples
// -- if nsamp > 0: --
// 4  Sample header size
// 96 Sample number for all notes
// 48 Points for volume envelope (12 points * 4 bytes)
// 48 Points for panning envelope
// 1  Number of volume points
// 1  Number of panning points
// 1  Volume sustain point
// 1  Volume loop start point
// 1  Volume loop end point
// 1  Panning sustain point
// 1  Panning loop start point
// 1  Panning loop end point
// 1  Volume type
// 1  Panning type
// 1  Vibrato type
// 1  Vibrato sweep
// 1  Vibrato depth
// 1  Vibrato rate
// 2  Volume fadeout
// 2  Reserved
int main() {
    int size = 4 + 22 + 1 + 2  // basic fields
             + 4               // sample header size
             + 96              // sample map
             + 48 + 48         // vol/pan env points
             + 1 + 1           // vol/pan point counts
             + 1 + 1 + 1      // vol sus, loop start, loop end
             + 1 + 1 + 1      // pan sus, loop start, loop end
             + 1 + 1           // vol type, pan type
             + 1 + 1 + 1 + 1  // vibrato
             + 2               // fadeout
             + 2;              // reserved
    printf("XM instrument header size (with samples): %d\n", size);
    printf("Without samples: %d\n", 4 + 22 + 1 + 2);
    return 0;
}
