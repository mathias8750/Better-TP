/**	@file randomizer.cpp
 *  @brief Randomizer main class
 *
 *	@author AECX
 *	@bug No known bugs.
 */
#include "rando/randomizer.h"

#include <cstring>
#include <cstdio>
#include <cinttypes>

#include "data/items.h"
#include "data/stages.h"
#include "events.h"
#include "game_patch/game_patch.h"
#include "gc_wii/OSModule.h"
#include "gc_wii/card.h"
#include "main.h"
#include "patch.h"
#include "rando/data.h"
#include "rando/seed.h"
#include "rando/seedlist.h"
#include "tp/d_a_alink.h"
#include "tp/d_com_inf_game.h"
#include "tp/d_kankyo.h"
#include "tp/d_meter2_info.h"
#include "tp/d_resource.h"
#include "tp/dynamic_link.h"
#include "tp/dzx.h"
#include "user_patch/03_customCosmetics.h"

namespace mod::rando
{
    Randomizer::Randomizer( SeedInfo* seedInfo, uint8_t selectedSeed )
    {
        mod::console << "Rando loading...\n";
        loadSeed( seedInfo, selectedSeed );
    }

    Randomizer::~Randomizer( void )
    {
        mod::console << "Rando unloading...\n";

        // Clear Seed
        delete m_Seed;
    }

    void Randomizer::loadSeed( SeedInfo* seedInfo, uint8_t selectedSeed )
    {
        if ( seedInfo->fileIndex == 0xFF )
        {
            mod::console << "<Randomizer> Error: No such seed (0xFF)\n";
        }
        else
        {
            mod::console << "Seed: " << seedInfo->header.seed << "\n";
            // Load the seed
            m_SeedInfo = seedInfo;
            m_CurrentSeed = selectedSeed;
            m_Seed = new Seed( CARD_SLOT_A, seedInfo );
            // Load checks for first load
            onStageLoad();
        }
    }

    void Randomizer::changeSeed( SeedInfo* seedInfo, uint8_t newSeed )
    {
        mod::console << "Seed unloading...\n";
        delete m_Seed;
        m_SeedInfo = nullptr;
        m_Seed = nullptr;
        m_SeedInit = false;
        m_CurrentSeed = 0xFF;

        mod::console << "Seed Loading...\n";
        loadSeed( seedInfo, newSeed );
    }

    void Randomizer::initSave( void )
    {
        if ( m_Seed )
        {
            m_SeedInit = m_Seed->InitSeed();
        }
    }

    void Randomizer::onStageLoad( void )
    {
        const char* stage = libtp::tp::d_com_inf_game::dComIfG_gameInfo.play.mNextStage.stageValues.mStage;
        m_Seed->LoadChecks( stage );
    }

    void Randomizer::overrideREL()
    {
        // Local vars
        uint32_t numReplacements = m_Seed->m_numLoadedRELChecks;
        RELCheck* relReplacements = m_Seed->m_RELChecks;

        // If we don't have replacements just leave
        if ( !numReplacements )
            return;

        // Loop through all loaded OSModuleInfo entries and apply the specified values to the rels already loaded.
        libtp::gc_wii::os_module::OSModuleInfo* rel = libtp::gc_wii::os_module::osModuleList.first;
        for ( ; rel; rel = rel->next )
        {
            for ( uint32_t i = 0; i < numReplacements; i++ )
            {
                if ( rel->id == relReplacements[i].moduleID )
                {
                    *reinterpret_cast<uint32_t*>( reinterpret_cast<uint32_t>( rel ) + relReplacements[i].offset ) =
                        relReplacements[i].override;
                }
            }
        }
    }

    void Randomizer::overrideDZX( libtp::tp::dzx::ChunkTypeInfo* chunkTypeInfo )
    {
        // Local vars
        uint32_t numReplacements = m_Seed->m_numLoadedDZXChecks;
        dzxCheck* dzxReplacements = m_Seed->m_DZXChecks;

        uint32_t numChunks = chunkTypeInfo->numChunks;
        libtp::tp::dzx::ACTR* dzxData = reinterpret_cast<libtp::tp::dzx::ACTR*>( chunkTypeInfo->chunkDataPtr );

        // Check if we have DZX checks to work with
        if ( !numReplacements )
            return;
        // Loop through all chunks the game is currently loading/setting
        for ( uint32_t i = 0; i < numChunks; i++ )
        {
            // The hash in RAM right now
            uint16_t actorHash =
                libtp::tools::fletcher16( reinterpret_cast<uint8_t*>( &dzxData[i] ), sizeof( libtp::tp::dzx::ACTR ) );
            // Compare to all available replacements
            for ( uint32_t j = 0; j < numReplacements; j++ )
            {
                if ( dzxReplacements[j].hash == actorHash )
                {
                    // Bytearray of target ACTR struct
                    uint8_t* target = reinterpret_cast<uint8_t*>( &dzxData[i] );

                    // Replace target Actor with replacement values if != FF
                    for ( uint8_t b = 0; b < sizeof( libtp::tp::dzx::ACTR ); b++ )
                    {
                        // Fetch replacement byte
                        uint8_t newByte = dzxReplacements[j].data[b];

                        if ( newByte != dzxReplacements[j].magicByte )
                            target[b] = newByte;
                    }
                }
            }
        }
    }

    int32_t Randomizer::getPoeItem( uint8_t flag )
    {
        for ( uint32_t i = 0; i < m_Seed->m_numLoadedPOEChecks; i++ )
        {
            if ( flag == m_Seed->m_POEChecks[i].flag )
            {
                // Return new item
                return static_cast<int32_t>( m_Seed->m_POEChecks[i].item );
            }
        }

        // Default
        return libtp::data::items::Poe_Soul;
    }

    uint8_t Randomizer::getSkyCharacter()
    {
        // Return the item id if we only have one item to pick from, otherwise, check the room to get the character we want.
        if ( m_Seed->m_numSkyBookChecks == 1 )
        {
            return m_Seed->m_SkyBookChecks[0].itemID;
        }
        else
        {
            for ( uint32_t i = 0; i < m_Seed->m_numSkyBookChecks; i++ )
            {
                if ( m_Seed->m_SkyBookChecks[i].roomID == libtp::tp::d_kankyo::env_light.currentRoom )
                {
                    return m_Seed->m_SkyBookChecks[i].itemID;
                }
            }
        }
        // default
        return libtp::data::items::Ancient_Sky_Book_Partly_Filled;
    }

    // There is (currently) never a situation where there are multiple boss checks on the same stage, so just return the item.
    uint8_t Randomizer::getBossItem() { return m_Seed->m_BossChecks[0].item; }

    void Randomizer::overrideARC( uint32_t fileAddr, FileDirectory fileDirectory, int roomNo )
    {
        m_Seed->LoadARCChecks( m_Seed->m_StageIDX, fileDirectory, roomNo );
        uint32_t numReplacements = m_Seed->m_numLoadedArcReplacements;
        // Loop through all ArcChecks and replace the item at an offset given the fileIndex.
        for ( uint32_t i = 0; i < numReplacements; i++ )
        {
            switch ( m_Seed->m_ArcReplacements[i].replacementType )
            {
                case rando::ArcReplacementType::Item:
                {
                    uint32_t replacementValue =
                        game_patch::_04_verifyProgressiveItem( mod::randomizer, m_Seed->m_ArcReplacements[i].replacementValue );
                    *reinterpret_cast<uint8_t*>( ( fileAddr + m_Seed->m_ArcReplacements[i].offset ) ) = replacementValue;
                    if ( replacementValue == libtp::data::items::Foolish_Item )
                    {
                        game_patch::_02_modifyFoolishFieldModel();
                    }
                    break;
                }
                case rando::ArcReplacementType::HiddenSkill:
                {
                    uint32_t adjustedFilePtr =
                        reinterpret_cast<uint32_t>( libtp::tp::d_meter2_info::g_meter2_info.mStageMsgResource );
                    uint8_t stageIDX;
                    for ( stageIDX = 0;
                          stageIDX < sizeof( libtp::data::stage::allStages ) / sizeof( libtp::data::stage::allStages[0] );
                          stageIDX++ )
                    {
                        if ( !strcmp( libtp::tp::d_com_inf_game::dComIfG_gameInfo.save.save_file.player.player_last_stay_info
                                          .player_last_stage,
                                      libtp::data::stage::allStages[stageIDX] ) )
                        {
                            break;
                        }
                    }

                    for ( uint32_t j = 0; j < m_Seed->m_numHiddenSkillChecks; j++ )
                    {
                        if ( ( m_Seed->m_HiddenSkillChecks[j].stageIDX == stageIDX ) &&
                             ( m_Seed->m_HiddenSkillChecks[j].roomID ==
                               libtp::tp::d_com_inf_game::dComIfG_gameInfo.save.save_file.player.player_return_place
                                   .link_room_id ) )
                        {
                            uint16_t msgID =
                                game_patch::_04_verifyProgressiveItem( mod::randomizer, m_Seed->m_HiddenSkillChecks[j].itemID );
                            *reinterpret_cast<uint16_t*>( adjustedFilePtr + m_Seed->m_ArcReplacements[i].offset ) =
                                msgID + 0x65;
                        }
                    }
                    break;
                }
                case rando::ArcReplacementType::ItemMessage:
                {
                    uint32_t replacementValue =
                        game_patch::_04_verifyProgressiveItem( mod::randomizer, m_Seed->m_ArcReplacements[i].replacementValue );
                    *reinterpret_cast<uint16_t*>( ( fileAddr + m_Seed->m_ArcReplacements[i].offset ) ) =
                        replacementValue + 0x65;
                }

                case rando::ArcReplacementType::AlwaysLoaded:
                {
                    uint32_t adjustedFilePtr =
                        reinterpret_cast<uint32_t>( libtp::tp::d_com_inf_game::dComIfG_gameInfo.play.mMsgDtArchive[0] );
                    uint32_t replacementValue =
                        game_patch::_04_verifyProgressiveItem( mod::randomizer, m_Seed->m_ArcReplacements[i].replacementValue );
                    *reinterpret_cast<uint16_t*>( ( adjustedFilePtr + m_Seed->m_ArcReplacements[i].offset ) ) =
                        replacementValue + 0x65;
                    break;
                }

                case rando::ArcReplacementType::MessageResource:
                {
                    uint32_t adjustedFilePtr =
                        reinterpret_cast<uint32_t>( libtp::tp::d_meter2_info::g_meter2_info.mStageMsgResource );

                    uint32_t replacementValue =
                        game_patch::_04_verifyProgressiveItem( mod::randomizer, m_Seed->m_ArcReplacements[i].replacementValue );
                    *reinterpret_cast<uint16_t*>( ( adjustedFilePtr + m_Seed->m_ArcReplacements[i].offset ) ) =
                        replacementValue + 0x65;
                }

                default:
                {
                    break;
                }
            }
        }
    }

    void Randomizer::overrideObjectARC()
    {
        m_Seed->LoadObjectARCChecks();
        uint32_t numReplacements = m_Seed->m_numLoadedObjectArcReplacements;
        // Loop through all ArcChecks and replace the item at an offset given the fileIndex.
        for ( uint32_t i = 0; i < numReplacements; i++ )
        {
            libtp::tp::d_resource::dRes_info_c* resourcePtr =
                events::getObjectResInfo( m_Seed->m_ObjectArcReplacements[i].fileName );
            if ( resourcePtr )
            {
                uint32_t replacementValue =
                    game_patch::_04_verifyProgressiveItem( mod::randomizer,
                                                           m_Seed->m_ObjectArcReplacements[i].replacementValue );

                uint32_t archiveData =
                    *reinterpret_cast<uint32_t*>( reinterpret_cast<uint32_t>( resourcePtr->mArchive ) + 0x28 );
                *reinterpret_cast<uint8_t*>( ( archiveData + m_Seed->m_ObjectArcReplacements[i].offset ) ) = replacementValue;
            }
        }
    }

    void Randomizer::overrideEventARC()
    {
        uint32_t bmgHeaderLocation = reinterpret_cast<uint32_t>( libtp::tp::d_meter2_info::g_meter2_info.mStageMsgResource );
        uint32_t messageFlowOffset = bmgHeaderLocation + *reinterpret_cast<uint32_t*>( bmgHeaderLocation + 0x8 );

        this->overrideARC( messageFlowOffset, rando::FileDirectory::Message, 0xFF );
    }

    uint8_t Randomizer::overrideBugReward( uint8_t bugID )
    {
        for ( uint32_t i = 0; i < m_Seed->m_numBugRewardChecks; i++ )
        {
            if ( bugID == m_Seed->m_BugRewardChecks[i].bugID )
            {
                // Return new item
                return m_Seed->m_BugRewardChecks[i].itemID;
            }
        }
        // Default
        return bugID;
    }

    uint8_t Randomizer::getHiddenSkillItem( uint16_t eventIndex )
    {
        for ( uint32_t i = 0; i < m_Seed->m_numHiddenSkillChecks; i++ )
        {
            if ( eventIndex == m_Seed->m_HiddenSkillChecks[i].indexNumber )
            {
                // Return new item
                return m_Seed->m_HiddenSkillChecks[i].itemID;
            }
        }
        // Default
        return libtp::data::items::Recovery_Heart;
    }
}     // namespace mod::rando