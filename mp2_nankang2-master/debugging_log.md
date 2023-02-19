1. Algorithmic error: status bar doesn't show, all screen is black, even there was picture before adding status bar.
    Fix: Should static define the buffer that used to hold the image of the string
    Difficulty of finding the bug: 5h
    Difficulty of fixing the bug: within 5 lines

2. Algorithmic error: the status bar shows color like fence, some place doesn't have color
    Fix: Should split the buffer into 4 planes
    Difficulty of finding the bug: 2h
    Difficulty of fixing the bug: about 50 lines


3. Algorithmic error: cannot move the first picture after adding the status bar
    Fix: Change the IMAGE_Y_DIM = 182 in modex.h, this defines the vertical screen dimension
    Difficulty of finding the bug: 1h
    Difficulty of fixing the bug: 1 line

4. Semantic error: assign array value like this "palette[192][3]; p->palette[i] = {R_5, G_6, B_5};" induce "expected expression before token '{' in c" error
    Fix: Instead, initialize by p->palette[i][j] = 0.
    Difficulty of finding the bug: 1h
    Difficulty of fixing the bug: 1 line


5. Semantic error: declare the func in modex.c file, but use it in photo.c, result in "implicit declaration of function" error
    Fix: Instead, daclare the func in modex.h
    Difficulty of finding the bug: 1h
    Difficulty of fixing the bug: within 10 lines

6. Algorithmic errors: color of the picture look weird:
    Fix 1: forgot to shift R and B by 1 at palette, so the picture looks green
        Difficulty of finding the bug: 5h
        Difficulty of fixing the bug: within 5 lines
    Fix 2: wrongly use fseek: wrote fseek(in, 1, SEEK_SET) WRONG!, should be fseek(in, sizeof(p->hdr), SEEK_SET)
        Difficulty of finding the bug: 10h
        Difficulty of fixing the bug: 1 line
    Fix 3: some colors don't show: color count table overflow
        Difficulty of finding the bug: 2h
        Difficulty of fixing the bug: 1 line

7.  Semantic error: TUX doesn't work even check the code carefully
    Fix: Cannot connect to the port because type "/dev/ttys0" instead of "/dev/ttyS0"
    Difficulty of finding the bug: 10h
    Difficulty of fixing the bug: 1line

8. Algorithmic errors: LED display wrongly
    Fix: sequence of led arg is inversed
    Difficulty of finding the bug: 2h
    Difficulty of fixing the bug: within 10 lines

9. Specification ambiguity error: when enter the room, the photo shift right until it reach the right end
    Fix: add "sched_yield()" after mutex unlock in the tux_thread, this will allow the program to do single action per unit of time
    Difficulty of finding the bug: 2h
    Difficulty of fixing the bug: 1 line
