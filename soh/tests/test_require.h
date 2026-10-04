#ifndef TEST_REQUIRE_H
#define TEST_REQUIRE_H

#include <stdio.h>
#include <stdlib.h>

/* Unlike assert, test checks are evaluated in Release/NDEBUG builds too.
 * Keep operations that change state outside the expression. */
#define REQUIRE(expression)                                                                  \
    do {                                                                                     \
        if (!(expression)) {                                                                 \
            fprintf(stderr, "%s:%d: REQUIRE(%s) failed\n", __FILE__, __LINE__, #expression); \
            abort();                                                                         \
        }                                                                                    \
    } while (0)

#endif
