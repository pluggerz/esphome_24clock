#include "handles.h"

using handles::ClockCharacter;
using handles::ClockHandleRow;

const ClockCharacter handles::ZERO =
    ClockCharacter(ClockHandleRow(3, 6, 6, 9), ClockHandleRow(0, 6, 0, 6),
                   ClockHandleRow(0, 3, 0, 9));

const ClockCharacter handles::ONE =
    ClockCharacter(ClockHandleRow(-1, -1, 6, 6), ClockHandleRow(-1, -1, 0, 6),
                   ClockHandleRow(-1, -1, 0, 0));

const ClockCharacter handles::TWO =
    ClockCharacter(ClockHandleRow(3, 3, 9, 6), ClockHandleRow(3, 6, 0, 9),
                   ClockHandleRow(0, 3, 9, 9));

const ClockCharacter handles::THREE =
    ClockCharacter(ClockHandleRow(3, 3, 6, 9), ClockHandleRow(3, 3, 0, 9),
                   ClockHandleRow(3, 3, 0, 9));

const ClockCharacter handles::FOUR =
    ClockCharacter(ClockHandleRow(6, 6, 6, 6), ClockHandleRow(0, 3, 0, 6),
                   ClockHandleRow(-1, -1, 0, 0));

const ClockCharacter handles::FIVE =
    ClockCharacter(ClockHandleRow(3, 6, 9, 9), ClockHandleRow(0, 3, 6, 9),
                   ClockHandleRow(3, 3, 0, 9));

const ClockCharacter handles::SIX =
    ClockCharacter(ClockHandleRow(3, 6, 9, 9), ClockHandleRow(0, 6, 6, 9),
                   ClockHandleRow(0, 3, 0, 9));

const ClockCharacter handles::SEVEN =
    ClockCharacter(ClockHandleRow(3, 3, 6, 9), ClockHandleRow(-1, -1, 0, 6),
                   ClockHandleRow(-1, -1, 0, 0));

const ClockCharacter handles::EIGHT =
    ClockCharacter(ClockHandleRow(3, 6, 6, 9), ClockHandleRow(0, 3, 6, 9),
                   ClockHandleRow(0, 3, 0, 9));

const ClockCharacter handles::NINE =
    ClockCharacter(ClockHandleRow(3, 6, 6, 9), ClockHandleRow(0, 3, 0, 6),
                   ClockHandleRow(3, 3, 0, 9));

const ClockCharacter handles::FORCED_ZERO =
    ClockCharacter(ClockHandleRow(0, 1, 0, 1), ClockHandleRow(0, 1, 0, 1),
                   ClockHandleRow(0, 1, 0, 1));

const ClockCharacter handles::FORCED_ONE =
    ClockCharacter(ClockHandleRow(1, 2, 1, 2), ClockHandleRow(1, 2, 1, 2),
                   ClockHandleRow(1, 2, 1, 2));

const ClockCharacter handles::EMPTY = ClockCharacter(
    ClockHandleRow(-1, -1, -1, -1), ClockHandleRow(-1, -1, -1, -1),
    ClockHandleRow(-1, -1, -1, -1));

const ClockCharacter *handles::NMBRS[] = {
    &handles::ZERO,  &handles::ONE,  &handles::TWO, &handles::THREE,
    &handles::FOUR,  &handles::FIVE, &handles::SIX, &handles::SEVEN,
    &handles::EIGHT, &handles::NINE};
