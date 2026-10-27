/* The bond solver. See bonds.c. */
#ifndef SR_BONDS_H
#define SR_BONDS_H

#include "sr_runtime.h"

static __inline int bonds_available(int needed)
{
    return needed >= 0 && sr_numofbonds >= 0 && sr_numofbonds <= SR_MAX_BOND &&
        needed <= SR_MAX_BOND - sr_numofbonds;
}

void bondsimulation2(void);

#endif /* SR_BONDS_H */
