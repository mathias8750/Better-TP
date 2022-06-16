#include <cstring>

#include "game_patch/game_patch.h"
#include "data/items.h"
#include "data/stages.h"
#include "gc_wii/bmgres.h"
#include "main.h"
#include "tp/control.h"
#include "tp/d_a_alink.h"
#include "tp/d_com_inf_game.h"
#include "tp/d_kankyo.h"
#include "tp/processor.h"
#include "tp/resource.h"
#include "tp/JKRArchivePub.h"

namespace mod::game_patch
{
    // Custom strings that are used for text search
    const char* talkToMidnaText = { "Talk to Midna" };
    const char* smallDonationText = { "30 Rupees" };

    /*
    int32_t getItemIdFromMsgId( const void* TProcessor, uint16_t unk3, uint32_t msgId )
    {
        void* unk = libtp::tp::processor::getResource_groupID( TProcessor, unk3 );
        if ( !unk )
        {
            return -1;
        }

        libtp::gc::bmgres::TextIndexTable* textIndexTable =
            *reinterpret_cast<libtp::gc::bmgres::TextIndexTable**>( reinterpret_cast<uint32_t>( unk ) + 0xC );

        libtp::gc::bmgres::MessageEntry* entry = &textIndexTable->entry[0];
        uint32_t loopCount = textIndexTable->numEntries;

        // Loop through the item IDs until msgId is found
        uint32_t itemId = libtp::data::items::Recovery_Heart;
        for ( uint32_t i = 0; i < loopCount; i++ )
        {
            uint16_t entryMessageId = entry->messageId;
            if ( entryMessageId == ( itemId + 0x64 ) )
            {
                if ( entryMessageId == msgId )
                {
                    return static_cast<int32_t>( itemId );
                }
                else
                {
                    itemId++;

                    // Make sure itemId is valid
                    if ( itemId > libtp::data::items::NullItem )
                    {
                        break;
                    }
                }
            }
            entry++;
        }

        // Didn't find the index
        return -1;
    }
    */

    // Most checks will use zel_00.bmg, so use a dedication function for it that specifies the archive, so less code runs per
    // check
    void* getZel00BmgInf()
    {
        uint32_t infPtrRaw = reinterpret_cast<uint32_t>( libtp::tp::JKRArchivePub::JKRArchivePub_getGlbResource(
            0x524F4F54,     // ROOT
            "zel_00.bmg",
            libtp::tp::d_com_inf_game::dComIfG_gameInfo.play.mMsgDtArchive[0] ) );

        // getGlbResource gets a pointer to MESGbmg1, but we need a pointer to INF1, which is just past MESGbmg1, and MESGbmg1
        // has a size of 0x20
        return reinterpret_cast<void*>( infPtrRaw + 0x20 );
    }

    void* getInf1Ptr( const char* file )
    {
        uint32_t infPtrRaw = reinterpret_cast<uint32_t>(
            libtp::tp::JKRArchivePub::JKRArchivePub_getGlbResource( /* ROOT */ 0x524F4F54, file, nullptr ) );

        // getGlbResource gets a pointer to MESGbmg1, but we need a pointer to INF1, which is just past MESGbmg1, and MESGbmg1
        // has a size of 0x20
        return reinterpret_cast<void*>( infPtrRaw + 0x20 );
    }

    void _05_setCustomItemMessage( libtp::tp::control::TControl* control,
                                   const void* TProcessor,
                                   uint16_t unk3,
                                   uint16_t msgId,
                                   rando::Randomizer* randomizer )
    {
        auto setMessageText = [&]( const char* text ) {
            // Only replace the message if a valid string was retrieved
            if ( text )
            {
                control->msg = text;
                control->wMsgRender = text;
            }
        };

        // Get the message id for the sign in front of Link's house
        constexpr uint16_t linkHouseMsgId = 0x658;

        // Get a pointer to the current BMG file being used
        // The pointer is to INF1
        const void* unk = libtp::tp::processor::getResource_groupID( TProcessor, unk3 );
        if ( !unk )
        {
            return;
        }
        const void* currentInf1 = *reinterpret_cast<void**>( reinterpret_cast<uint32_t>( unk ) + 0xC );

        // Most text replacements are for zel_00.bmg, so check that first
        if ( currentInf1 == getZel00BmgInf() )
        {
            const char* newMessage = _05_getMsgById( randomizer, msgId );
            setMessageText( newMessage );
        }
        else if ( ( msgId == linkHouseMsgId ) && ( libtp::tp::d_kankyo::env_light.currentRoom == 1 ) &&
                  libtp::tp::d_a_alink::checkStageName(
                      libtp::data::stage::allStages[libtp::data::stage::stageIDs::Ordon_Village] ) &&
                  ( currentInf1 == getInf1Ptr( "zel_01.bmg" ) ) )
        {
            // Set the new message for the sign in front of Link's house
            // Make sure the randomizer is loaded/enabled
            if ( randoIsEnabled( randomizer ) )
            {
                // Make sure a seed is loaded
                rando::Seed* seed = randomizer->m_Seed;
                if ( seed )
                {
                    setMessageText( seed->m_RequiredDungeons );
                }
            }
        }
    }

    uint32_t _05_getCustomMsgColor( uint8_t colorId )
    {
        uint32_t newColorCode;     // RGBA
        switch ( colorId )
        {
            case CUSTOM_MSG_COLOR_DARK_GREEN_HEX:
            {
                newColorCode = 0x4BBE4BFF;
                break;
            }
            case CUSTOM_MSG_COLOR_BLUE_HEX:
            {
                newColorCode = 0x4B96D7FF;
                break;
            }
            case CUSTOM_MSG_COLOR_SILVER_HEX:
            {
                newColorCode = 0xBFBFBFFF;
                break;
            }
            default:
            {
                // Return the color white if there is not a match
                newColorCode = 0xFFFFFFFF;
                break;
            }
        }
        return newColorCode;
    }

    const char** _05_replaceMessageString( const char** text )
    {
        const char* replacementText;
        if ( strncmp( *text, talkToMidnaText, strlen( talkToMidnaText ) ) == 0 )
        {
            // If it is day/night set the text to reflect the current time.

            if ( ( libtp::tp::d_com_inf_game::dComIfG_gameInfo.save.save_file.player.player_status_b.skyAngle >= 284 ) ||
                 ( libtp::tp::d_com_inf_game::dComIfG_gameInfo.save.save_file.player.player_status_b.skyAngle <= 104 ) )
            {
                replacementText = { "Wait until day" };
            }
            else
            {
                replacementText = { "Wait until night" };
            }
            *text = replacementText;
        }
        else if ( strncmp( *text, smallDonationText, strlen( smallDonationText ) ) == 0 )
        {
            if ( libtp::tp::d_a_alink::checkStageName(
                     libtp::data::stage::allStages[libtp::data::stage::stageIDs::Castle_Town] ) &&
                 libtp::tp::d_kankyo::env_light.currentRoom == 2 )
            {
                replacementText = {
                    "100 Rupees"
                    "\x0A\x1A\x06\x00\x00\x09\x02"
                    "50 Rupees"
                    "\x0A\x1A\x06\x00\x00\x09\x03"
                    "Sorry" };
                *text = replacementText;
            }
        }
        return text;
    }

    const char* _05_getMsgById( rando::Randomizer* randomizer, uint32_t msgId )
    {
        // Make sure the randomizer is loaded/enabled
        if ( !randoIsEnabled( randomizer ) )
        {
            return nullptr;
        }

        // Make sure the custom text is loaded
        uint32_t msgTableInfoRaw = reinterpret_cast<uint32_t>( m_MsgTableInfo );
        if ( !msgTableInfoRaw )
        {
            return nullptr;
        }

        // Get a pointer to the message ids to search for
        uint16_t* msgIds = reinterpret_cast<uint16_t*>( msgTableInfoRaw );

        // Get the total size of the message ids
        uint32_t totalMessages = m_TotalMsgEntries;
        uint32_t msgIdTableSize = totalMessages * sizeof( uint16_t );

        // Round msgIdTableSize up to the size of the offset type to make sure the offsets are properly aligned
        msgIdTableSize = ( msgIdTableSize + sizeof( uint32_t ) - 1 ) & ~( sizeof( uint32_t ) - 1 );

        // Get a pointer to the message offsets
        uint32_t* msgOffsets = reinterpret_cast<uint32_t*>( msgTableInfoRaw + msgIdTableSize );

        // Get the total size of the message offsets
        uint32_t msgOffsetTableSize = totalMessages * sizeof( uint32_t );

        // Get a pointer to the messages
        const char* messages = reinterpret_cast<const char*>( msgTableInfoRaw + msgIdTableSize + msgOffsetTableSize );

        // Get the custom message
        for ( uint32_t i = 0; i < totalMessages; i++ )
        {
            if ( msgIds[i] == msgId )
            {
                return &messages[msgOffsets[i]];
            }
        }

        // Didn't find msgId
        return nullptr;
    }
}     // namespace mod::game_patch