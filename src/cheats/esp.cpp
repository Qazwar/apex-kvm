#include "esp.hpp"
#include <thread>
#include <chrono>

#include "entities.hpp"
#include "utils/process.hpp"
#include "utils/utils.hpp"
#include "config/options.hpp"
#include <spdlog/spdlog.h>
#include "offsets.hpp"

int get_class_id_esp( std::uintptr_t ent )
{
    std::uintptr_t client_nttable = 0u;
    if ( apex::utils::process::get().read<std::uintptr_t>( ent + 24, client_nttable ); !client_nttable ) {
        return -1;
    }

    std::uintptr_t get_client_class_fn = 0u;
    if ( apex::utils::process::get().read<std::uintptr_t>( client_nttable + 24, get_client_class_fn ); !( get_client_class_fn ) ) {
        return -1;
    }

    std::uint32_t relative_table_offs = 0u;
    if ( apex::utils::process::get().read<std::uint32_t>( get_client_class_fn + 3, relative_table_offs ); !( relative_table_offs ) ) {
        return -1;
    }

    auto id = 0;
    apex::utils::process::get().read<std::int32_t>( get_client_class_fn + relative_table_offs + 7 + 0x28, id );

    return id;
}

void apex::cheats::esp::run()
{
    while ( true ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 3 ) );
        if ( !utils::process::get().good() || !this->should_iterate() ) {
            continue;
        }

        /*std::uint16_t local_player_index;
        if ( !utils::process::get().read( utils::process::get().base_address() + offsets_t::get().local_player_index, local_player_index ) || ( local_player_index == static_cast<uint16_t>( -1 ) ) ) {
            continue;
        }

        sdk::ent_info_t local_info;

        if ( auto read = utils::process::get().read(
                 utils::process::get().base_address() + ( offsets_t::get().entity_list + local_player_index * sizeof( sdk::ent_info_t ) ),
                 local_info );
             !read || !local_info.entity_ptr ) {
            continue;
        }

        sdk::player_t local_player { local_info.entity_ptr };
        local_player.update();
        if ( local_player.get_pos().z() > 11000.f ) {
            continue;
        }

        for ( int i = 0; i < sdk::MAXENTRIES; ++i ) {
            sdk::ent_info_t ei;
            utils::process::get().read( utils::process::get().base_address() + ( offsets_t::get().entity_list + i * sizeof( sdk::ent_info_t ) ), ei );

            if ( !ei.entity_ptr || ( i == local_player_index ) ) {
                continue;
            }

            auto cid = static_cast<sdk::class_id>( get_class_id_esp( ei.entity_ptr ) );
            if ( ( cid == sdk::class_id::CPlayer ) ) {
                sdk::player_t player { ei.entity_ptr };
                player.update();

                if ( !player.is_alive() ) {
                    continue;
                }
                static bool glow_enabled = true;
                static int glow_time = 1;
                static float glow_duration = std::numeric_limits<float>::max();
                static float glow_distance = 5000.f;
                static std::array<float, 6> max;
                if ( max[ 0 ] != std::numeric_limits<float>::max() ) {
                    max.fill( std::numeric_limits<float>::max() );
                }
                printf( "enabling glow for %d\n", i );
                auto color = options->visual.esp_color;
                std::array col {
                    static_cast<float>( color.r() ) * 0.1f,
                    static_cast<float>( color.g() ) * 0.1f,
                    static_cast<float>( color.b() ) * 0.1f,
                };
                utils::process::get().write( player.get_base() + 0x360, glow_enabled );
                utils::process::get().write( player.get_base() + 0x350, glow_time );
                utils::process::get().write_ptr( player.get_base() + 0x1D0, col.data(), sizeof( float ) * 3 );
                utils::process::get().write_ptr( player.get_base() + 0x310, max.data(), sizeof( float ) * max.size() );
                //utils::process::get().write( player.get_base() + 0x32c, glow_duration );
                utils::process::get().write( player.get_base() + 0x33c, glow_distance );
            }

            if ( ( cid == sdk::class_id::CPropSurvival ) ) {
                sdk::item_t item { ei.entity_ptr };
                item.update();

                if ( !item.is_glown() ) {
                    static int enable = 1363184265;
                    utils::process::get().write( item.get_base() + 0x2a8, enable, sizeof( int ) );
                }
            }
        }*/

        if ( !entity_list::get().get_local_player() || ( entity_list::get().get_local_player()->get_pos().z() > 11000.f ) ) {
            continue;
        }

        std::vector<utils::write_data_t> write_list;
        for ( auto &e : entity_list::get() ) {
            if ( !e ) {
                continue;
            }

            if ( ( options->visual.glow_esp ) && e->is_player() ) {
                apply_glow( e->as<sdk::player_t>(), write_list );
            }

            if ( ( options->visual.item_esp ) && e->is_item() ) {
                apply_glow( e->as<sdk::item_t>(), write_list );
            }
        }

        if ( !write_list.empty() ) {
            utils::process::get().write_list( write_list );
        }
    }
}
bool apex::cheats::esp::should_iterate() const noexcept
{
    // only run if the cheat needs to actually render something
    return ( options->visual.enabled && ( options->visual.glow_esp || options->visual.item_esp ) ) && utils::are_movement_keys_pressed();
}

void apex::cheats::esp::apply_glow( sdk::player_t *entity, std::vector<utils::write_data_t> &write_list )
{
    if ( !this->validate_player( entity ) ) {
        return;
    }

    auto local_player = entity_list::get().get_local_player();

    auto color = [ & ]() -> sdk::color_t {
        auto is_friend = local_player->get_team() == entity->get_team();
        if ( is_friend ) {
            return options->visual.friend_color;
        }

        if ( ( options->aimbot.current_target == entity->get_base() ) && options->visual.aim_target_esp ) {
            return options->visual.aimbot_target_color;
        }

        if ( entity->get_bleedout_state() != sdk::bleedout_state_t::alive ) {
            return options->visual.downed_color;
        }

        if ( options->visual.color_health_based ) {
            const auto red = std::abs( 255.f - ( entity->get_health() * 2.55f ) );
            return sdk::color_t( red, 255.f - red, 0.f, options->visual.health_based_alpha );
        }
        return options->visual.esp_color;
    }();

    const auto brightness = std::clamp( 255.f / color.a(), 0.f, 1.f );

    std::array col {
        static_cast<float>( color.r() ) * brightness,
        static_cast<float>( color.g() ) * brightness,
        static_cast<float>( color.b() ) * brightness,
    };

    static bool glow_enabled = true;
    static int glow_time = 1;
    static float glow_distance = 5000.f;
    static std::array<float, 6> max;
    if ( max[ 0 ] != std::numeric_limits<float>::max() ) {
        max.fill( std::numeric_limits<float>::max() );
    }

    if ( entity_list::get().validate( entity ) ) {
        utils::process::get().write( entity->get_base() + 0x360, glow_enabled );
        utils::process::get().write( entity->get_base() + 0x350, glow_time );
        utils::process::get().write_ptr( entity->get_base() + 0x1D0, col.data(), sizeof( float ) * 3 );
        utils::process::get().write_ptr( entity->get_base() + 0x310, max.data(), sizeof( float ) * max.size() );
        utils::process::get().write( entity->get_base() + 0x33c, glow_distance );
    }

    /*write_list.emplace_back( static_cast<Address>( entity->get_base() + 0x32c ), reinterpret_cast<uint8_t *>( &glow_duration ), sizeof( glow_duration ) );
    write_list.emplace_back( static_cast<Address>( entity->get_base() + 0x33c ), reinterpret_cast<uint8_t *>( &glow_distance ), sizeof( glow_distance ) );
    write_list.emplace_back( static_cast<Address>( entity->get_base() + 0x310 ), reinterpret_cast<uint8_t *>( max.data() ), max.size() * sizeof( float ) );
    write_list.emplace_back( static_cast<Address>( entity->get_base() + 0x350 ), reinterpret_cast<uint8_t *>( &glow_time ), sizeof( glow_time ) );
    write_list.emplace_back( static_cast<Address>( entity->get_base() + 0x360 ), reinterpret_cast<uint8_t *>( &glow_enabled ), sizeof( glow_enabled ) );*/
}
void apex::cheats::esp::apply_glow( sdk::item_t *entity, std::vector<utils::write_data_t> &write_list )
{
    /*utils::write_data_t data;
        data.remote_address = static_cast<Address>( entity->get_base() + 0x2a8 );
        data.local_buffer = reinterpret_cast<uint8_t *>( &enable );
        data.size = sizeof( enable );
        write_list.emplace_back( data );*/

    if ( entity_list::get().validate( entity ) && !entity->is_glown() ) {
        static int enable = 1363184265;
        utils::process::get().write( entity->get_base() + 0x2a8, enable, sizeof( int ) );
    }
}
bool apex::cheats::esp::validate_player( sdk::player_t *player ) const noexcept
{
    auto local_player = entity_list::get().get_local_player();
    if ( player->get_base() == local_player->get_base() ) {
        return false;
    }
    if ( player->get_pos().z() > 11000.f /* in dropship */ ) {
        return false;
    }

    auto is_friend = local_player->get_team() == player->get_team();
    if ( is_friend && !options->visual.highlight_friends ) {
        return false;
    }

    return true;
}