#include "data/items.h"
#include "game_patch/game_patch.h"
#include "tp/d_com_inf_game.h"
#include "tp/d_item.h"

namespace mod::game_patch
{
    void _00_poe( void ) { libtp::tp::d_item::item_func_ptr[libtp::data::items::Poe_Soul] = _00_handle_poeItem; }

    void _00_handle_poeItem() { libtp::tp::d_com_inf_game::dComIfG_gameInfo.save.save_file.player.player_collect.poe_count++; }
}     // namespace mod::game_patch