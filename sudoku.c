/*
 * Create and solve sudoku puzzles
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/random.h>

#define MAX_VALUES 64
#define ARRAY_SIZE_MAX ((MAX_VALUES)*(MAX_VALUES))

static unsigned int ROOT_SIZE = 3;
static unsigned int SIZE = 9;
static unsigned int ARRAY_SIZE = 81;
static unsigned int FONT_SIZE = 30;
static unsigned int FORCE_THRESHOLD = 8;


typedef int Index;
typedef unsigned char Value;
typedef unsigned long Mask;

typedef int SetIndex;
typedef int SetClass;
typedef struct {
    Value value;
    Mask mask;
} Cell;

typedef Cell Puzzle[ARRAY_SIZE_MAX];

typedef enum {
    RET_DONE = 0,
    RET_MORE,
    RET_INCONSISTENT
} SReturn;

typedef enum {
    False = 0,
    True = 1
} Boolean;


static Boolean verbose = False;

#define MAKE_INDEX(x,y) ((x)*SIZE +(y))
#define BOX_X_OFFSET(box) ((box) / ROOT_SIZE)
#define BOX_Y_OFFSET(box) ((box) % ROOT_SIZE)
#define MAKE_BOX_INDEX(box,i) \
     MAKE_INDEX(BOX_X_OFFSET(box)*ROOT_SIZE+BOX_X_OFFSET(i), \
                BOX_Y_OFFSET(box)*ROOT_SIZE+BOX_Y_OFFSET(i))
#define MAKE_MASK(val) (1 << ((val)-1))
#define ALL_MASK (MAKE_MASK(SIZE+1)-1)
#define HAS_VAL(mask,val) (((mask) & MAKE_MASK(val)) != 0)
#define SET_VAL(mask,val) ((mask) | MAKE_MASK(val))
#define CLR_VAL(mask,val) ((mask) & ~MAKE_MASK(val))

/* most basic function, define first */
void
zero_puzzle(Puzzle puzzle)
{
    Index i;
    for (i=0; i < ARRAY_SIZE; i++) {
        puzzle[i].value = 0;
        puzzle[i].mask = ALL_MASK;
    }
}

void
copy_puzzle(Puzzle src, Puzzle target)
{
    Index i;
    for (i=0; i < ARRAY_SIZE; i++) {
        target[i].value = src[i].value;
        target[i].mask = src[i].mask;
    }
}

/*
 * functions to output a puzzle
 */
char
value_to_char(Value val)
{
    if (val == 0) {
        return ' ';
    }
    if ((val > 0) && (val < 10)) {
        return '0'+val;
    }
    if ((val >= 10) && (val < 36)) {
        return 'a'+val-10;
    }
    if ((val >= 36) && (val < 62)) {
        return 'A'+val-36;
    }
    if (val == 62) { return '@'; }
    if (val == 63) { return '%'; }
    if (val == 64) { return '#'; }
    /* do you really want to do a 64 x 64 puzzle? */
    return '.';
}

void
print_puzzle_html(FILE *out, Puzzle puzzle)
{
    int i,j;
    int lps=0, tps=0;
    const char *bottom="";
    const char *right="";

    fprintf(out, "<table>\n<tbody style=\"font-size:%dpx\">\n", FONT_SIZE);
    for (i=0; i < SIZE; i++) {
        fprintf(out,"<tr>\n");
        tps=(i % ROOT_SIZE == 0) ? 2 : 1;
        bottom=(i == (SIZE-1)) ? "; border-bottom: 2.0pt solid": "";
        for (j=0; j < SIZE; j++) {
            lps=(j % ROOT_SIZE == 0) ? 2 : 1;
            right=(j == (SIZE-1)) ? "; border-right: 2.0pt solid" : "";
            fprintf(out, "<td style=\"border-top: %d.0pt solid;"
                    " border-left: %d.0pt solid%s%s\">%c</td>\n",
                    tps, lps, bottom, right,
                    value_to_char(puzzle[MAKE_INDEX(i,j)].value));
        }
        fprintf(out, "</tr>\n");
    }
    fprintf(out, "</tbody>\n</table>\n");
}

void
print_header(FILE *out, int len)
{
    int i, j;
    fprintf(out," ");
    for (i=0; i < ROOT_SIZE; i++) {
        fprintf(out, "+");
        for (j=0; j < ROOT_SIZE*len; j++) {
            fprintf(out, "-");
        }
        fprintf(out, "-");
    }
    fprintf(out, "+\n");
}

void
print_puzzle_ascii(FILE*out, Puzzle puzzle)
{
    int i,j;

    for (i=0; i < SIZE; i++) {
        fprintf(out, "    ");
        if ((i % ROOT_SIZE) == 0) {
            print_header(out, 2);
            fprintf(out, "    ");
        }
        for (j=0; j < SIZE; j++) {
            if ((j % ROOT_SIZE) == 0) {
                fprintf(out, " |");
            }
            fprintf(out, " %c", value_to_char(puzzle[MAKE_INDEX(i,j)].value));
        }
        fprintf(out, " |\n");
    }
    fprintf(out, "    ");
    print_header(out, 2);
}

void
print_puzzle(FILE*out, Puzzle puzzle, Boolean use_html)
{
    if (use_html) {
        print_puzzle_html(out, puzzle);
    } else {
        print_puzzle_ascii(out, puzzle);
    }
}

void
print_mask(FILE *out, unsigned int mask)
{
    Value i;
    char cval[MAX_VALUES+1];
    int count= 0;
    fprintf(out, " ");
    for (i=1; i <= SIZE; i++) {
        if (HAS_VAL(mask,i)) {
            cval[count++] = value_to_char(i);
        } 
    }
    cval[count] = 0; /* turn it into a string */
    for (i=0; i < SIZE-count; i++) {
        fprintf(out, ".");
    }
    fprintf(out,"%s", cval);
}

void
print_puzzle_mask(FILE *out, Puzzle puzzle, Boolean use_html)
{
    int i,j;
    for (i=0; i < SIZE; i++) {
        fprintf(out, "    ");
        if ((i % ROOT_SIZE) == 0) {
            print_header(out, SIZE+1);
            fprintf(out, "    ");
        }
        for (j=0; j < SIZE; j++) {
            if ((j % ROOT_SIZE) == 0) {
                fprintf(out, " |");
            }
            print_mask(out, puzzle[MAKE_INDEX(i,j)].mask);
        }
        fprintf(out, " |\n");
    }
    fprintf(out, "    ");
    print_header(out, SIZE+1);
}

/*
 * functions to read a puzzle
 */
Value
char_to_value(char c)
{
    if ((c >= '1') && (c <= '9')) {
        return c - '0';
    }
    if ((c >= 'a') && (c <= 'z')) {
        return (c - 'a') + 10;
    }
    if ((c >= 'A') && (c <= 'Z')) {
        return (c - 'A') + 36;
    }
    if (c == '@') { return 62; }
    if (c == '%') { return 63; }
    if (c == '#') { return 64; }
    return 0;
}

int
read_skip_to_header(FILE *in) 
{
    int c;
    while ( (c = fgetc(in)) != EOF) {
        if (c == '+') {
            ungetc(c, in);
            return 0;
        }
        if (verbose) {
            fputc(c, stderr);
        }
    }
    if (verbose) {
        fprintf(stderr, "EOF\n");
    }
    return -1;
}

typedef enum {
    ReadHeaderInit,
    ReadHeaderStart,
    ReadHeaderRootDone,
    ReadHeaderDone,
    ReadEHeaderrror
} ReadHeaderState;

/* returns the root size from the header */
int
read_header(FILE *in)
{
    int c;
    int root_size = -1;
    int count, cell_count=0;
    ReadHeaderState state = ReadHeaderInit;

    while ( (c = fgetc(in)) != EOF) {
        /* found the end of the line, time to return */
        if (verbose) {
            fputc(c, stderr);
        }
        if (c == '\n') {
            return state == ReadHeaderDone ? root_size : -1;
        }
        /* once we are done, ignore the rest of the line */
        if (state == ReadHeaderDone) {
            continue;
        }
        switch (c) {
            /* skip any initial blanks. once we start, blanks in the headers
             * are errors */
            case ' ':
                if (state != ReadHeaderInit) {
                    fprintf(stderr, "spurious space in header\n");
                    return -1;
                }
                break;
            /* the start of each cell is a '+', use the first cell to determine
             * the root_size, then verify that each remaining cell matches.
             * Also make sure we read enough cells */
            case '+':
                switch (state) {
                    case ReadHeaderInit:
                        state = ReadHeaderStart;
                        count = 0;
                        break;
                    case ReadHeaderStart:
                        root_size = (count-1)/2;
                        state = ReadHeaderRootDone;
                        count = 0;
                        cell_count++;
                        break;
                    case ReadHeaderRootDone:
                        if ((root_size*2)+1 != count) {
                            return -1;
                        }
                        cell_count++;
                        if (cell_count == root_size) {
                            state = ReadHeaderDone;
                        }
                        count=0;
                        break;
                }
                break;
            case '-':
                /* need a '+' cell start before we process '-' */
                if (state == ReadHeaderInit) {
                    fprintf(stderr, "spurious '-' in header\n");
                    return -1;
                }
                count++;
                break;
            default:
                fprintf(stderr, "spurious character in header (%c)\n", c);
                return -1;
        }
    }
    /* we hit the EOF before we could process the line */
    fprintf(stderr, "Unexpected EOF in header\n");
    return -1;
}

typedef enum {
    ReadCellInit = 0,
    ReadCellSpace,
    ReadCellDigit,
    ReadCellBoundarySpace,
    ReadCellBoundary,
    ReadCellDone,
} ReadCellState;

int
read_puzzle_line(FILE *in, Puzzle puzzle, Index index)
{
    int c;
    int root_size = -1;
    int count = 0, cell_count=0;
    Value val;
    ReadCellState state = ReadCellInit;

    while ( (c = fgetc(in)) != EOF) {
        /* found the end of the line, time to return */
        if (verbose) {
            fputc(c, stderr);
        }
        if (c == '\n') {
            return state == ReadCellDone ? 0 : -1;
        }
        /* once we are done, ignore the rest of the line */
        if (state == ReadCellDone) {
            continue;
        }
        switch (c) {
            /* the start of each cell is a '|' */
            case '|':
                if ((state != ReadCellInit) && (state != ReadCellBoundary)) {
                    fprintf(stderr, "Spurious '|' in puzzleline\n");
                    return -1;
                }
                if (cell_count == ROOT_SIZE) {
                    state = ReadCellDone;
                    break;
                }
                count = 0;
                state = ReadCellSpace;
                cell_count ++;
                break;
            case ' ':
                switch (state) {
                    case ReadCellInit:
                        break; /* ignore leading spaces */
                    case ReadCellSpace:
                        state = ReadCellDigit;
                        break;
                    case ReadCellBoundarySpace:
                        state = ReadCellBoundary;
                        break;
                    case ReadCellDigit:
                        count++;
                        index++;
                        state = (count == ROOT_SIZE) ?
                            ReadCellBoundarySpace : ReadCellSpace;
                        break;
                    default:
                        fprintf(stderr, "Spurious space in puzzle line\n");
                        return -1;
                }
                break;
            default:
                if (state != ReadCellDigit) {
                    fprintf(stderr, "Spurious char (%c) in puzzle line\n",c);
                    return -1;
                }
                val = char_to_value(c);
                if ((val == 0) || (val > SIZE)) {
                    fprintf(stderr,
                            "Character(%c) not in range in puzzle line\n", c);
                    return -1;
                }
                /* fprintf(stderr,"<%d>->p[%d]", val,index); */
                puzzle[index++].value = val;
                count++;
                state = (count == ROOT_SIZE) ?
                    ReadCellBoundarySpace : ReadCellSpace;
        }
    }
    fprintf(stderr, "Unexpected EOF in puzzle line.\n");
    return -1;
}

Boolean
read_puzzle(FILE *in, Puzzle puzzle)
{
    int root_size;
    int ret;
    int i,j;

    /* get to the first header */
    ret = read_skip_to_header(in);
    if (ret == -1) {
        return False;
    }
    /* read the header in and return the root_size */
    root_size = read_header(in);
    if (root_size == -1) {
        return False;
    }
    /* initialize the globs based on root_size */
    ROOT_SIZE = root_size;
    SIZE = ROOT_SIZE*ROOT_SIZE;
    ARRAY_SIZE = SIZE*SIZE;

    /* clear out puzzle out */
    zero_puzzle(puzzle);
    /* read it in */
    for (i=0; i < ROOT_SIZE; i++) {
        /* read root_size rows, then find a new header */
        /* we do this root_size times for the full puzzle */
        for (j=0; j < ROOT_SIZE; j++) {
            ret = read_puzzle_line(in, puzzle, i*(SIZE*ROOT_SIZE)+j*SIZE);
            if (ret == -1) {
                return False;
            }
        }
        root_size = read_header(in);
        if (root_size != ROOT_SIZE) {
            return False;
        }
    }
    if (verbose) {
        fprintf(stderr, "read_done\n");
        print_puzzle(stderr, puzzle, False);
    }
    return True;
}

/*
 * functions to help us create and solve puzzles 
 */

/* we have 3 types of sets, which we need to run solutions over, the only
 * difference between the sets is how we address the members, so we create
 * and Index, which we tell it which set and which member of the set we are
 * interested in and we produce an index function which returns the puzzle
 * index for that cell. */
typedef Index (*IndexFunction)(SetClass setClass, SetIndex index);
Index row_index_function (SetClass row, SetIndex i) { return MAKE_INDEX(row,i); }
Index column_index_function (SetClass col, SetIndex i) { return MAKE_INDEX(i,col); }
Index box_index_function (SetClass box, SetIndex i) { return MAKE_BOX_INDEX(box,i); }
typedef void (*OffsetFunction)(Index index, SetClass *box, SetIndex *boxIndex);

/* advanced solutions require us to match a cell to a specific alternate set.
 * We use the global index to find the row and row index, or column and column
 * index, or box and box index. */
void
getRowOffset(Index index, SetClass *row, SetIndex *rowIndex)
{
    *row = index/SIZE;
    *rowIndex = index % SIZE;
}

void
getColumnOffset(Index index, SetClass *col, SetIndex *colIndex)
{
    *colIndex = index/SIZE;
    *col = index % SIZE;
}

/* finding the box is more complicated, we find the row and column
 * then find the row and colum index for the box and the offset in the box.
 * then we convert that into a box and box index */
void
getBoxOffset(Index index, SetClass *box, SetIndex *boxIndex)
{
    SetClass row;
    SetIndex col;
    SetClass box_x;
    SetClass box_y;
    SetIndex box_i_x;
    SetIndex box_i_y;
    getRowOffset(index, &row, &col);
    box_x = row/ROOT_SIZE;
    box_y = col/ROOT_SIZE;
    box_i_x = row % ROOT_SIZE;
    box_i_y = col % ROOT_SIZE;
    *box = box_x * ROOT_SIZE + box_y;
    *boxIndex = box_i_x * ROOT_SIZE + box_i_y;
}


/* for any known value in a set, reduce the masks of all other members of the
 * set by that value */
SReturn
reduce_set(Puzzle puzzle, SetClass setClass, IndexFunction f)
{
    Boolean more = False;
    SetIndex i, j;
    for (i=0; i < SIZE; i++) {
        Value val = puzzle[(*f)(setClass,i)].value;
        if (val) {
            for (j=0; j < SIZE; j++) {
                if (j != i) {
                    Cell *p = &puzzle[(*f)(setClass,j)];
                    Mask mask = p->mask;
                    p->mask = CLR_VAL(mask, val);
                    if (p->mask == 0) {
                        return RET_INCONSISTENT;
                    }
                    if (p->mask != mask) {
                        more = True;
                    }
                }
            }
        }
    }
    return more ? RET_MORE : RET_DONE;
}

/* reduce the masks of all the sets */
SReturn
reduce(Puzzle puzzle)
{
    Boolean more = False;
    SetClass row, col, box;
    SReturn ret;

    for (row = 0; row < SIZE; row++) {
        ret = reduce_set(puzzle, row, row_index_function);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    for (col = 0; col < SIZE; col++) {
        ret = reduce_set(puzzle, col, column_index_function);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    for (box= 0; box < SIZE; box ++) {
        ret = reduce_set(puzzle, box, box_index_function);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    return more ? RET_MORE : RET_DONE;
}

/* recalculate the masks for a given puzzle (clobbers the existing masks) */
SReturn
setup_masks(Puzzle puzzle, Boolean reset)
{
    SReturn ret;
    Index i;
    for (i=0; i < ARRAY_SIZE; i++) {
        Mask default_mask = reset ? ALL_MASK : puzzle[i].mask;
        puzzle[i].mask = puzzle[i].value ?
            MAKE_MASK(puzzle[i].value) : default_mask;
    }
    while ((ret = reduce(puzzle)) == RET_MORE) ;
    return ret;
}

/* return an index to the first unsolved puzzle entry. If the puzzle is solved
 * return -1 */
Index
first_unsolved(Puzzle puzzle)
{
    Index i;
    for (i=0; i < ARRAY_SIZE; i++) {
        if (puzzle[i].value == 0) {
            return i;
        }
    }
    return -1;
}

int
count_mask(Mask mask)
{
}

Index get_smallest_mask(Puzzle puzzle, Mask *smallest_mask)
{
    Index i;
    Index ret_index = -1;
    Mask ret_mask = ALL_MASK;
    int mask_count =  count_mask(ALL_MASK);
    for (i=0; i < ARRAY_SIZE; i++) {
        if (puzzle[i].value == 0) {
            int mask_size = count_mask(puzzle[i].mask);
            if (mask_count > mask_size) {
                mask_count = mask_size;
                ret_index = i;
                ret_mask = puzzle[i].mask;
            }
        }
    }
    *smallest_mask = ret_mask;
    return ret_index;
}

/* fill in the array of values that are still possible based on the mask */
int
set_values(Value *values, Mask mask)
{
    int next =0;
    Value i;
    for (i=1; i <= SIZE; i++) {
        if (HAS_VAL(mask, i)) {
            if (values) {
                values[next++] = i;
            } else next++;
        }
    }
    return next;
}

/* if only a single value is in the mask, get it, otherwise return 0 */
Value
get_single(unsigned int mask)
{
    Value i;
    for (i=1; i <= SIZE; i++) {
        if (mask == MAKE_MASK(i)) {
            return i;
        }
    }
    return 0;
}

/* find places where our mask has been reduced to a single
 * value. In those cases the value of the cell must be that value) */
Boolean
reap_singles(Puzzle puzzle)
{
    Boolean found = False;
    Index i;
    for (i=0; i < ARRAY_SIZE; i++) {
        if (puzzle[i].value == 0) {
            puzzle[i].value = get_single(puzzle[i].mask);
            if (puzzle[i].value != 0) {
                found = True;
            }
        }
    }
    return found;
}

/* if there is only one possible place for a value in a set, it must go
 * here, set that value int it's place */
Boolean
reap_set_uniques(Puzzle puzzle, SetClass setClass, IndexFunction f)
{
    Value val;
    SetIndex i;
    Boolean found = False;
    Cell *possible_cell = NULL;

    for (val=1; val <= SIZE; val++) {
        possible_cell = NULL;
        for (i=0; i < SIZE; i++) {
            /* if we have it already skip to the next value */
            Cell *p = &puzzle[(*f)(setClass,i)];
            if (val == p->value) {
                possible_cell = NULL; /* our mask may not be uptodate */
                break;
            }
            if (HAS_VAL(p->mask, val)) {
                if (possible_cell) {
                    /* second possibility found, not unique */
                    possible_cell = NULL;
                    break;
                }
                possible_cell = p;
            }
        }
        if (possible_cell) {
            found = True;
            possible_cell->value = val;
        }
    }
    return found;
}

/* find places where there is only one possible Cell for a given value.
 * The value must be in that spot. We do this form all rows, columns,
 * and boxes. */
Boolean
reap_uniques(Puzzle puzzle)
{
    SetClass row, col, box;
    Boolean found = False;

    for (row = 0; row < SIZE; row++) {
        found |= reap_set_uniques(puzzle, row, row_index_function);
    }
    for (col = 0; col < SIZE; col++) {
        found |= reap_set_uniques(puzzle, col, column_index_function);
    }
    for (box= 0; box < SIZE; box ++) {
        found |= reap_set_uniques(puzzle, box, box_index_function);
    }
    return found;

}

/*
 * for now we pick the biggest remaining mask. If there are more than 1,
 * randomly choose amongst the candidates */
Index
find_best_unknown_index(Puzzle puzzle)
{
    Index candidates[ARRAY_SIZE_MAX];
    Index i;
    int big_mask_count = 0;
    int selected;
    int count = 0;
    for (i=0; i < ARRAY_SIZE; i++) {
        int mask_count;
        if (puzzle[i].value) {
           continue;
        }
        mask_count = set_values(NULL, puzzle[i].mask);

        if (mask_count < big_mask_count) {
            continue;
        }
        if (mask_count > big_mask_count) {
            big_mask_count = mask_count;
            count = 0; /* clear the old candidates list */
        }
        candidates[count++] = i; /* this index is a candidate */
    }
    if (count == 1) {
        return candidates[0];
    }
    /* multiple are possible, select one at random */
    selected = rand() % count;
    return candidates[selected];
}


/*
 * This is a medium advanced feature. If we have n identical masks of n mask
 * values, then those values cannot be anywhere else in the set. Make that
 * reduction here we limit how big these masks are as a matter of difficulty
 * level, in practice n == 2 or n == 3 for a root_size = 3 puzzle. */
SReturn
reduce_n_set(Puzzle puzzle, SetClass setClass, IndexFunction f, int n)
{
    Boolean more = False;
    SetIndex i, j;
    for (i=0; i < SIZE; i++) {
        Mask mask = puzzle[(*f)(setClass,i)].mask;
        int maskCount = set_values(NULL, mask);
        /* maskCount must be 2-n inclusive */
        if (maskCount <= 1 ||  maskCount > n) {
            continue;
        }
        /* find how many masks meet the criteria */
        int count = 0;
        for (j=0; j < SIZE; j++) {
            if (j != i) {
                Cell *p = &puzzle[(*f)(setClass,j)];
                if (p->mask == mask) {
                    count++;
                    /* count has to exactly equal maskCount for this 
                     * reduction. If we've gone over, look for the next mask */
                    if (count > maskCount) {
                        break;
                    }
                }
            }
        }
        /* if the number of identical masks don't equal mask count,
         * no reduction */
        if (count != maskCount) {
            continue;
        }
        /* now reduce remaining mask */
        for (j=0; j < SIZE; j++) {
            if (j == i) {
                Cell *p = &puzzle[(*f)(setClass,j)];
                Mask thisMask = p->mask;
                if (thisMask ==  mask) {
                    continue;
                }
                p->mask =  thisMask & ~mask;
                if (p->mask == 0) {
                    return RET_INCONSISTENT;
                }
                if (p->mask != thisMask) {
                    more = True;
                }
            }
        }
    }
    return more ? RET_MORE : RET_DONE;
}

/* we do an 'n' reduction on all our sets (rows, columns, boxes) */
SReturn
reduce_n(Puzzle puzzle, int n)
{
    Boolean more = False;
    SetClass row, col, box;
    SReturn ret;

    for (row = 0; row < SIZE; row++) {
        ret = reduce_n_set(puzzle, row, row_index_function, n);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    for (col = 0; col < SIZE; col++) {
        ret = reduce_n_set(puzzle, col, column_index_function, n);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    for (box= 0; box < SIZE; box ++) {
        ret = reduce_n_set(puzzle, box, box_index_function, n);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) more = True;
    }
    return more ? RET_MORE : RET_DONE;
}

/*
 * This is a medium advanced feature. If we have all the possible cells
 * for a value in a box line up in the same row or column, then that value
 * can't be anywhere else in that row or column
 */
typedef enum {
    Box_State_None = 0, /* value not found in the box */
    Box_State_Found,    /* one value found in the box */
    Box_State_Good,     /* more than one value found and they are all in the
                           same row or column */
    Box_State_Bad,      /* multiple values found but they are not colinear */
} BoxState;

/* clear all the bits from mask in a set */
SReturn
Clear_Mask_Set_Skip_Box(Puzzle puzzle, SetClass setClass, SetClass box, 
        Mask mask, IndexFunction f)
{
    SetClass box_t;
    SetIndex i, box_i;
    Boolean more = False;
    for (i=0; i < SIZE; i++) {
        Index index= (*f)(setClass,i);
        getBoxOffset(index, &box_t, &box_i);
        if (box != box_t) {
            Cell *p = &puzzle[index];
            Mask pmask = p->mask;
            p->mask &= ~mask;
            if (p->mask == 0) {
                return RET_INCONSISTENT;
            }
            if (p->mask != pmask) {
                more = True;
            }
        }
    }
    return more ? RET_MORE : RET_DONE;
}

/* process the current state for the current Cell */
BoxState
update_box_set_state(Index index, BoxState state, SetClass *setClass,
        OffsetFunction of)
{
    SetClass newSetClass;
    SetIndex setIndex;
    switch (state) {
        case Box_State_Bad:
            break;
        case Box_State_None:
            (*of)(index, setClass, &setIndex);
            return Box_State_Found;
        case Box_State_Found:
        case Box_State_Good:
            (*of)(index, &newSetClass, &setIndex);
            if (*setClass == newSetClass) {
                return Box_State_Good;
            }
            break;
    }
    return Box_State_Bad;
}

SReturn
reduce_box(Puzzle puzzle)
{
    Boolean more = False;
    SetClass box;
    SetIndex i, j;
    Value val;
    SReturn ret;
    for (box=0; box < SIZE; box++) {
        for (val=1; val <= SIZE; val++) {
            Mask mask = MAKE_MASK(val);
            SetClass row = -1, col = -1;
            BoxState row_state = Box_State_None, col_state = Box_State_None;
            for (i=0; i < SIZE; i++) {
                Index index = MAKE_BOX_INDEX(box,i);
                if ((puzzle[index].mask & mask) == mask) {
                    row_state = update_box_set_state(index, row_state, &row,
                            getRowOffset);
                    col_state = update_box_set_state(index, col_state, &col,
                            getColumnOffset);
                    /* if ((row_state == Box_State_Bad) && 
                            (col == Box_State_Bad)) {
                        break;
                    } */
                }
            }
            if (row_state == Box_State_Good) {
                ret = Clear_Mask_Set_Skip_Box(puzzle, row, box, mask,
                        row_index_function);
                if (ret == RET_INCONSISTENT) {
                    return ret;
                }
                if (ret == RET_MORE) more = True;
            }
            if (col_state == Box_State_Good) {
                ret = Clear_Mask_Set_Skip_Box(puzzle, col, box, mask,
                        column_index_function);
                if (ret == RET_INCONSISTENT) {
                    return ret;
                }
                if (ret == RET_MORE) more = True;
            }
        }
    }
    return more ? RET_MORE : RET_DONE;
}


SReturn basic_solve_puzzle(Puzzle puzzle,  int level);
/* just brute force a solution */
SReturn
brute_force(Puzzle puzzle)
{
   Mask smallest_mask=0;
   Index index = get_smallest_mask(puzzle, &smallest_mask);
   Puzzle new_puzzle;
   Value val;
   SReturn global_ret = RET_INCONSISTENT;

   if (index == -1) {
       return RET_DONE;
   }

   if (count_mask(smallest_mask) > FORCE_THRESHOLD) {
       return RET_MORE;
   }

   for (val=1; val <= SIZE; val++) {
       Mask mask = MAKE_MASK(val);
       if ((smallest_mask & mask) == mask) {
           copy_puzzle(puzzle, new_puzzle);
           new_puzzle[index].value = val;
           SReturn ret = basic_solve_puzzle(new_puzzle, 100);
           /* if the lower level returned 'More' it means
            * the puzzle has multiple solutions, don't
            * look for more solutions */
           if (ret == RET_MORE) {
               return RET_MORE;
           }
           /* if we got 'RET_DONE', we have a solution,
            * we continue to look for more */
           if (ret == RET_DONE) {
               /* if we have found a second, that means
                * there are multiple solutions */
               if (global_ret == RET_DONE) {
                   return RET_MORE;
               }
               global_ret == RET_DONE;
           }
       }
   }
   /* if we didn't find any solutions, then gobal_ret
    * will be RET_INCONSISTENT. If we found exactly
    * one solution, we return RET_DONE */
   return global_ret;
}


/* use our basic tools to solve a puzzle. 'level' sets which tools we plan
 * on using */
SReturn
basic_solve_puzzle(Puzzle puzzle,  int level)
{
    SReturn ret;
    Boolean found;
    Index index;
    ret = setup_masks(puzzle, True);
    if (ret == RET_INCONSISTENT) {
        return ret;
    }
    do {
        found = False;
        ret = setup_masks(puzzle, False);
        if (ret == RET_INCONSISTENT) {
            return ret;
        }
        if (ret == RET_MORE) found = True;
        switch (level) {
            default:
            case 7:
            case 6:
            case 5:
                ret = reduce_box(puzzle);
                if (ret == RET_INCONSISTENT) {
                    return ret;
                }
                if (ret == RET_MORE) found = True;
            case 4:
            case 3:
                ret = reduce_n(puzzle, level ==3 ? 2: SIZE-ROOT_SIZE -1);
                if (ret == RET_INCONSISTENT) {
                    return ret;
                }
                if (ret == RET_MORE) found = True;
            case 2:
                found |= reap_singles(puzzle);
            case 1:
                found |= reap_uniques(puzzle);
        }
    } while(found);
    if (level >=  100) {
         ret = brute_force(puzzle);
         printf("brute_force, ret=%d, first_unsolved=%d\n", ret, first_unsolved(puzzle));
         if (ret == RET_INCONSISTENT) {
             return ret;
         }
         if (ret == RET_DONE) {
             printf("solved by brute_force first_unsolved=%d\n", first_unsolved(puzzle));
         }
    }
    return (first_unsolved(puzzle) == -1)  ? RET_DONE : RET_MORE;
}

/* make a puzzle of 'level' difficulty from an existing puzzle solution */
void
make_puzzle(Puzzle solution, Puzzle puzzle, Puzzle working, int n, int level)
{
    int i;
    zero_puzzle(puzzle);
    zero_puzzle(working);
    /* pick n random starting positions to reveal */
    for (i=0; i < n; i++) {
        Index i = rand() % ARRAY_SIZE;
        puzzle[i].value = solution[i].value;
        working[i].value = solution[i].value;
    }
    do {
        /* now let's see how hard it is to solve */
        Boolean found;
        Index index;
        SReturn ret;
        ret = basic_solve_puzzle(working, level);
        if (ret == RET_DONE) {
            break; /* we have an easy puzzle */
        }
        if (ret == RET_INCONSISTENT) {
            fprintf(stderr, "Inconsistent Puzzle, programming error\n");
            return;
        }
        index = find_best_unknown_index(working);
        working[index].value = solution[index].value;
        puzzle[index].value = solution[index].value;
    } while(True);
}

/* use random r to decide which masked values to return.
 * This function should return the same shuffled result for the
 * same 'r' value */
void
shuffle_values(Value *values, int count, unsigned long r)
{
    int i;
    unsigned long  x = r % count;
    for (i=0; i < count; i++) {
        int index1, index2, tmp;
        index1 = (r) % count;
        index2 = (r >> 1) % count;
        r = r >> 2;
        tmp = values[index1];
        values[index1] = values[index2];
        values[index2] = tmp;
    }
}

/* pick a value represented by the mask for a given r.
 * i allows us to iterate through all the possible results, so
 * that on each call we'll get the next remaining value. We
 * should never return the same value for the same mask and r, but different
 * 'i' */
Value
pick_random(Mask mask, int i, unsigned long r)
{
    Value values[MAX_VALUES];
    int count;
    count = set_values(values, mask);
    if (i >= count) {
        return 0;
    }
    shuffle_values(values, count, r);
    return values[i];
}

/* Make a basic Sudoku solution */
Boolean
new_puzzle(Puzzle puzzle)
{
    Index index = first_unsolved(puzzle);
    Mask mask;
    Value val;
    unsigned long r;
    int i;


    if (setup_masks(puzzle, True) == RET_INCONSISTENT) {
        return False;
    }
    if (index == -1) {
        return True;
    }
    r = rand();
    mask = puzzle[index].mask;
    for (i=0; i < SIZE; i++) {
        val = pick_random(mask,i,r);
        if (val == 0) {
            puzzle[index].value = 0;
            puzzle[index].mask = ALL_MASK;
            return False; /* inconsistent */
        }
        puzzle[index].value = val;
        if (new_puzzle(puzzle)) {
            return True;
        }
        /* that value didn't produce a consistent puzzle, try the next one */
    }
    puzzle[index].value = 0;
    puzzle[index].mask = ALL_MASK;
    return False;
}

#define DO_HTML(command, pzFile, solFile, use_html) \
    if (use_html) {\
        command(pzFile); \
        if (solFile) { \
            command(solFile); \
        } \
    }

/* html common usages */
void
html_header(FILE *out) {
    fprintf(out, "<html>\n<head>\n<style>\n"
           "@media print{\n  .page-break {\n    clear: both;"
           "\n    break-after: page;\n"
           "    page-break-after: always;\n  }\n"
           "  .no-split{\n    break-inside: avoid;\n"
           "    page-break-inside:  avoid;\n  }\n}\n"
           "table {\n  width: %dpx;\n  table-layout: fixed;\n"
           "  aspect-ratio: 1 / 1;\n}\ntd {\n"
           "  text-align: center;\n}\n"
           "</style></head>\n<body style=\"font-family:monospace\">\n",
           FONT_SIZE*10); 
}

void
html_page_init(FILE *out) {
    fprintf(out,"<div class=\"selection no-split\">\n");
}

void
html_page_finish(FILE *out) {
    fprintf(out,"</div>\n");
}

void
html_page_break(FILE *out) {
    html_page_finish(out);
    fprintf(out,"<div class-\"page-break\"> </div><br>\n");
    html_page_init(out);
}


void
html_trailer(FILE *out) {
    fprintf(out, "</body>\n</html>\n");
}

void
header(FILE *out,unsigned char *format, int number, Boolean use_html) {
    if (use_html) {
        fprintf(out, "<h3>");
    }
    fprintf(out, format, number);
    if (use_html) {
        fprintf(out, "</h3>");
    }
    fprintf(out, "\n");
}
  


/*
 * main program and argument parsing
 */
void usage(char *prog)
{
    fprintf(stderr,"usage: %d [arguments]\n");
    fprintf(stderr,"Arguments:\n");
    fprintf(stderr,"-p       print the solutions\n");
    fprintf(stderr,"-n       don't print the actual puzzle\n");
    fprintf(stderr,"-v       verbose\n");
    fprintf(stderr,"-r size  change the default puzzle size. This value is the sqare root of the\n");
    fprintf(stderr,"         length and heigh of the puzzle\n");
    fprintf(stderr,"-h       output the puzzle in html\n");
    fprintf(stderr,"-i count set the number of initial values to reveal initially. This value will\n");
    fprintf(stderr,"         increase until the puzzle hits the requested difficulty\n");
    fprintf(stderr,"-l val   set the difficulty level of the puzzle. Higher numbers or more difficult\n");
    fprintf(stderr,"-s val   use this seed rather than a random seed\n");
    fprintf(stderr,"-c count set how many puzzles to print\n");
    fprintf(stderr,"-F file  write puzzles to file\n");
    fprintf(stderr,"-S file  write solutions to file\n");
    fprintf(stderr,"-I file  read puzzle from file and solve it.\n");
    exit(-1);
}

int
main(int argc, char **argv)
{
    Puzzle puzzle, solution, working;
    int i;
    char *prog = *argv++;
    Boolean print_solution = False;
    Boolean no_puzzle = False;
    Boolean solve = False;
    Boolean use_html = False;
    Boolean ret;
    FILE *puzFile = stdout;
    FILE *inPuzFile = stdout;
    FILE *solFile = stdout;
    char *puzFileName = NULL;
    char *inPuzFileName = NULL;
    char *solFileName = NULL;
    char *seed = NULL;
    char *arg,c;
    int count=1;
    int initial=1;
    int level=2;

    while ((arg = *argv++) != NULL) {
        if (*arg == '-') {
            while ((c = *++arg) != '\0') {
                switch (c) {
                    case 'h':
                        use_html = True;
                        break;
                    case 'p':
                        print_solution = True;
                        break;
                     case 'n':
                        no_puzzle = True;
                        break;
                     case 's':
                        seed = *argv++;
                        break;
                     case 'c':
                        count = atoi(*argv++);
                        break;
                     case 'i':
                        initial = atoi(*argv++);
                        break;
                     case 'l':
                        level = atoi(*argv++);
                        break;
                     case 'r':
                        ROOT_SIZE = atoi(*argv++);
                        break;
                     case 'F':
                        puzFileName = *argv++;
                        break;
                     case 'S':
                        solFileName = *argv++;
                        break;
                     case 'I':
                        inPuzFileName = *argv++;
                        solve = True;
                        break;
                     case 'v':
                        verbose = True;
                        break;
                     default:
                        fprintf(stderr,"unknown switch %c\n", c);
                        usage(prog);
                }
            }
        } else {
            fprintf(stderr,"unknown argument %s\n", arg);
            usage(prog);
        }
    }
    if (seed) {
        srand(atoi(seed));
    } else {
        unsigned int iseed;
        getrandom(&iseed, sizeof(unsigned int), 0);
        srand(iseed);
    }
    SIZE = ROOT_SIZE * ROOT_SIZE;
    ARRAY_SIZE = SIZE * SIZE;
    if (solFileName) {
        solFile = fopen(solFileName,"w+");
        if (solFile == NULL) {
            perror(solFileName);
            exit(-2);
        }
        print_solution = True; /* if we specified a solutions file, print to it */
    }
    if (puzFileName) {
        puzFile = fopen(puzFileName,"w+");
        if (puzFile == NULL) {
            perror(puzFileName);
            exit(-2);
        }
    }
    if (inPuzFileName) {
        inPuzFile = fopen(inPuzFileName,"r");
        if (puzFile == NULL) {
            perror(puzFileName);
            exit(-2);
        }
    }

    DO_HTML(html_header, puzFile, solFile, use_html);
    if (solve) {
        int ret;
        i=0;
        while (read_puzzle(inPuzFile, solution)){
            SReturn ret = basic_solve_puzzle(solution, level);
            header(solFile, "Puzzle $d", i++, use_html);
            switch (ret) {
                case RET_INCONSISTENT:
                    fprintf(solFile," puzzle inconsistent");
                    ret = 2;
                    break;
                case RET_MORE:
                    fprintf(solFile," solution incomplete");
                    ret = 1;
                    break;
                case RET_DONE:
                    fprintf(solFile," solved");
                    break;
            }
            fprintf(solFile, "\n");
            if (ret != RET_DONE) {
                print_puzzle_mask(solFile, solution, use_html);
            }
            print_puzzle(solFile, solution, use_html);
        }
        return ret;
    }

    DO_HTML(html_page_init, puzFile, solFile, use_html);
    for (i=0; i < count; i++) {
            printf("Puzzle %d:\n", i);
        zero_puzzle(solution);
        ret = new_puzzle(solution);
        if (!ret) {
            fprintf(stderr,"Make Puzzle %d failed!\n", i);
            print_puzzle_mask(stderr,solution, use_html);
            continue;
        }
        if (!no_puzzle) {
            make_puzzle(solution, puzzle, working, initial, level);
            header(puzFile, "Puzzle %d", i, use_html);
            print_puzzle(puzFile, puzzle, use_html);
        }
        if (print_solution) {
            header(solFile, " Soution %d", i, use_html);
            print_puzzle(solFile, solution, use_html);
        }
        if (use_html && (i & 1) == 1 ) {
            DO_HTML(html_page_break, puzFile, solFile, use_html);
        }
    }

    DO_HTML(html_page_finish, puzFile, solFile, use_html);
    DO_HTML(html_trailer, puzFile, solFile, use_html);
    return 0;
}
