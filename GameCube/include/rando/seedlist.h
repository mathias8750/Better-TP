/**	@file seedselector.h
 *  @brief Selection tool to select the desired data-gci
 *
 *	@author AECX
 *	@bug No known bugs.
 */
#ifndef RANDO_SEEDLIST_H
#define RANDO_SEEDLIST_H

#include "rando/data.h"
#include "rando/seed.h"
#include "rando/seedlist.h"

#define SEED_MAX_ENTRIES 10

namespace mod::rando
{
    class SeedList
    {
       public:
        SeedList( void );
        ~SeedList( void );

        SeedInfo FindSeed( uint64_t seed );

        uint8_t m_numSeeds;
        uint8_t m_selectedSeed;

        SeedInfo* m_seedInfo = nullptr;
    };
}     // namespace mod::rando
#endif