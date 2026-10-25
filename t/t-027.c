/* Run the socket-address suite with IPv6 support compiled out. Load the
 * configuration first so subsequent includes cannot restore the setting. */
#include "hawk-prv.h"

#undef HAWK_SIZEOF_STRUCT_SOCKADDR_IN6
#define HAWK_SIZEOF_STRUCT_SOCKADDR_IN6 0

#include "../lib/skad.c"
#include "t-026.c"
