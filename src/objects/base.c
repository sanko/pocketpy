#include "pocketpy/objects/base.h"

PyVar PY_NULL = {.type=0, .is_ptr=false, .extra=0, ._i64=0};
PyVar PY_OP_CALL = {.type=27, .is_ptr=false, .extra=0, ._i64=0};
PyVar PY_OP_YIELD = {.type=28, .is_ptr=false, .extra=0, ._i64=0};
