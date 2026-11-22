#include "banjo_pose_table.h"

#include <stddef.h>
#include <string.h>

#include "banjo_animation_ids.h"

#define POSE(link, banjo, playback) { #link, BANJO_ANIM_##banjo, BANJO_MODEL_PLAYER, BANJO_POSE_##playback, 0, 0 }
#define POSE_RANGE(link, banjo, playback, first, last) \
    { #link, BANJO_ANIM_##banjo, BANJO_MODEL_PLAYER, BANJO_POSE_##playback, first, last }
#define POSE_MODEL(link, banjo, playback, first, last, model) \
    { #link, BANJO_ANIM_##banjo, BANJO_MODEL_##model, BANJO_POSE_##playback, first, last }

// Each Link animation the game plays while it drives the player, paired with the closest Banjo-Kazooie one.
// Banjo-Kazooie has no door clips, so doors are walked through.
static const BanjoPose kPoses[] = {
    // Doors.
    POSE(link_demo_doorA_link, WALK, STRIDE),
    POSE(link_demo_doorA_link_free, WALK, STRIDE),
    POSE(clink_demo_doorA_link, WALK, STRIDE),
    POSE(link_demo_doorB_link, WALK, STRIDE),
    POSE(link_demo_doorB_link_free, WALK, STRIDE),
    POSE(clink_demo_doorB_link, WALK, STRIDE),

    // Chests and receiving items: the item rises with the arm Banjo raises for a Jiggy.
    POSE(link_demo_Tbox_open, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(clink_demo_Tbox_open, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_normal_box_kick, PUNCH, FOLLOW_LINK),
    POSE_RANGE(link_demo_get_itemA, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(link_demo_get_itemB, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(clink_demo_get1, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(clink_demo_get2, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(clink_demo_get3, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(o_get_mae, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(o_get_ato, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(om_get_mae, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(om_get, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(L_okarina_get, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(okarinatori, JIG_JIGGY, ONCE, 100, 131),

    // Ocarina (Banjo's banjo), items shown or handed over.
    POSE_MODEL(link_normal_okarina_start, CONCERT, FOLLOW_LINK, 420, 519, CONCERT),
    POSE_MODEL(link_normal_okarina_swing, CONCERT, LOOP, 519, 684, CONCERT),
    POSE_MODEL(link_normal_okarina_end, CONCERT, FOLLOW_LINK, 1410, 1478, CONCERT),
    POSE(link_normal_take_out, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_normal_give_other, CROUCH_ENTER, FOLLOW_LINK),
    POSE(demo_link_tewatashi, CROUCH_ENTER, FOLLOW_LINK),
    POSE(demo_link_orosuu, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_okarina_warp_goal, STAND_IDLE, LOOP),
    POSE(link_demo_warp, TWIRL, LOOP),

    // Talking, reading, looking.
    POSE(link_normal_talk_free, STAND_IDLE, LOOP),
    POSE(link_normal_talk_free_wait, STAND_IDLE, LOOP),
    POSE(link_normal_check, REST_LISTEN, LOOP),
    POSE(link_normal_check_free, REST_LISTEN, LOOP),
    POSE(link_normal_check_wait, REST_LISTEN, LOOP),
    POSE(link_normal_check_wait_free, REST_LISTEN, LOOP),
    POSE(link_normal_check_end, STAND_IDLE, LOOP),
    POSE(link_normal_check_end_free, STAND_IDLE, LOOP),
    POSE(link_bottle_read, REST_LISTEN, LOOP),
    POSE(link_bottle_read_end, STAND_IDLE, LOOP),
    POSE(clink_demo_nozoki, REST_LISTEN, LOOP),
    POSE(link_demo_nozokikomi, REST_LISTEN, LOOP),
    POSE(link_demo_nozokikomi_wait, REST_LISTEN, LOOP),
    POSE(clink_demo_mimawasi, STAND_IDLE, LOOP),
    POSE(clink_demo_mimawasi_wait, STAND_IDLE, LOOP),
    POSE(link_demo_kakeyori_mimawasi, STAND_IDLE, LOOP),
    POSE(alink_kyoro, STAND_IDLE, LOOP),
    POSE(alink_kyoro_loop, STAND_IDLE, LOOP),
    POSE(L_1kyoro, STAND_IDLE, LOOP),
    POSE(L_2kyoro, STAND_IDLE, LOOP),
    POSE(d_link_imanodare, STAND_IDLE, LOOP),
    POSE(lost_horse, REST_LISTEN, LOOP),
    POSE(lost_horse2, REST_LISTEN, LOOP),
    POSE(lost_horse_wait, REST_LISTEN, LOOP),
    POSE(link_demo_ue, STAND_IDLE, LOOP),
    POSE(link_demo_ue_wait, STAND_IDLE, LOOP),
    POSE(link_demo_kaoage, STAND_IDLE, LOOP),
    POSE(link_demo_kaoage_wait, STAND_IDLE, LOOP),
    POSE(Link_miageru, STAND_IDLE, LOOP),
    POSE(Link_ue_wait, STAND_IDLE, LOOP),
    POSE(cl_umamiage, STAND_IDLE, LOOP),
    POSE(cl_umamiage_loop, STAND_IDLE, LOOP),
    POSE(link_demo_kenmiru1, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_kenmiru1_wait, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_kenmiru2, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_kenmiru2_wait, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_kenmiru2_modori, STAND_IDLE, LOOP),
    POSE(L_ken_miru, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_look_hand, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_look_hand_wait, STAND_KAZOOIE_PECK, LOOP),
    POSE(link_demo_jibunmiru, TIMEOUT, LOOP),
    POSE(cl_jibun_miru, TIMEOUT, LOOP),
    POSE(dl_jibunmiru, TIMEOUT, LOOP),
    POSE(dl_jibunmiru_wait, TIMEOUT, LOOP),
    POSE(urusai, TIMEOUT, LOOP),

    // Surprise, fear and stepping back.
    POSE_RANGE(link_demo_bikkuri, OW, ONCE, 18, 36),
    POSE_RANGE(link_hatto_demo, OW, ONCE, 18, 36),
    POSE_RANGE(kolink_odoroki_demo, OW, ONCE, 18, 36),
    POSE_RANGE(link_demo_goma_furimuki, OW, ONCE, 18, 36),
    POSE_RANGE(Link_ha, OW, ONCE, 18, 36),
    POSE_RANGE(alink_ee, OW, ONCE, 18, 36),
    POSE(alink_ee_loop, STAND_IDLE, LOOP),
    POSE_RANGE(al_elf_tobidasi, OW, ONCE, 18, 36),
    POSE_RANGE(clink_demo_atozusari, OW, ONCE, 18, 36),
    POSE_RANGE(clink_demo_koutai, OW, ONCE, 18, 36),
    POSE_RANGE(L_sagaru, OW, ONCE, 18, 36),
    POSE(cl_ononoki, TIMEOUT, LOOP),
    POSE(dl_yusaburu, TIMEOUT, LOOP),
    POSE(demo_pikupiku, TIMEOUT, LOOP),
    POSE(alink_yurayura, TIMEOUT, LOOP),
    POSE(d_link_orooro, TIMEOUT, LOOP),
    POSE(d_link_orowait, TIMEOUT, LOOP),
    POSE(link_normal_re_dead_attack, TIMEOUT, LOOP),
    POSE(link_normal_re_dead_attack_wait, TIMEOUT, LOOP),
    POSE_RANGE(cl_kubisime, OW, LOOP, 18, 36),
    POSE(link_demo_gurad, CROUCH_IDLE, LOOP),
    POSE(link_demo_gurad_wait, CROUCH_IDLE, LOOP),
    POSE_RANGE(link_demo_kousan, JIG_JIGGY, ONCE, 100, 131),

    // Hits, falls and getting up.
    POSE(link_normal_front_hit, OW, FOLLOW_LINK),
    POSE_RANGE(link_normal_back_hit, OW, ONCE, 18, 36),
    POSE_RANGE(link_normal_front_downA, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(link_normal_front_downB, SPLAT, FOLLOW_LINK, 0, 43),
    POSE(link_normal_back_downA, REBOUND, FOLLOW_LINK),
    POSE(link_normal_back_downB, REBOUND, FOLLOW_LINK),
    POSE_RANGE(link_normal_front_down_wake, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(link_normal_back_down_wake, SPLAT, FOLLOW_LINK, 86, 195),
    POSE(link_normal_ice_down, REBOUND, FOLLOW_LINK),
    POSE(link_normal_electric_shock, SHOCK_CHARGE, LOOP),
    POSE_RANGE(link_normal_electric_shock_end, SPLAT, FOLLOW_LINK, 86, 195),
    POSE(clink_demo_futtobi, REBOUND, FOLLOW_LINK),
    POSE(alink_fukitobu, REBOUND, FOLLOW_LINK),
    POSE(L_hajikareru, REBOUND, FOLLOW_LINK),
    POSE_RANGE(dl_kokeru, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(rakuba, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(link_normal_hip_down, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(link_normal_hip_down_free, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(link_normal_hip_down_long, SPLAT, FOLLOW_LINK, 0, 43),
    POSE_RANGE(sirimochi, SPLAT, FOLLOW_LINK, 0, 43),
    POSE(sirimochi_wait, CROUCH_IDLE, LOOP),
    POSE(demo_rakka, JUMP_FALL, LOOP),
    POSE(rakka, JUMP_FALL, LOOP),
    POSE(alink_rakkatyu, JUMP_FALL, LOOP),
    POSE_RANGE(link_derth_rebirth, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(clink_demo_standup, SPLAT, FOLLOW_LINK, 86, 195),
    POSE(clink_demo_standup_wait, STAND_IDLE, LOOP),
    POSE_RANGE(okiagaru, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(okiagaru_tatu, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(okiagaru_wait, SPLAT, ONCE, 0, 20),
    POSE_RANGE(link_okiru_demo, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(clink_op3_negaeri, SPLAT, ONCE, 0, 20),
    POSE_RANGE(clink_op3_wait1, SPLAT, ONCE, 0, 20),
    POSE_RANGE(clink_op3_wait2, SPLAT, ONCE, 0, 20),
    POSE_RANGE(clink_op3_wait3, SPLAT, ONCE, 0, 20),
    POSE_RANGE(clink_op3_okiagari, SPLAT, FOLLOW_LINK, 86, 195),
    POSE_RANGE(clink_op3_tatiagari, SPLAT, FOLLOW_LINK, 86, 195),
    POSE(demo_suwari1, CROUCH_ENTER, FOLLOW_LINK),
    POSE(demo_suwari2, CROUCH_IDLE, LOOP),
    POSE(demo_suwari3, CROUCH_IDLE, LOOP),
    POSE(link_shagamu_demo, CROUCH_IDLE, LOOP),

    // Carrying, pushing and pulling.
    POSE(link_normal_carryB, CARRY_IDLE, FOLLOW_LINK),
    POSE(link_normal_carryB_free, CARRY_IDLE, FOLLOW_LINK),
    POSE(link_normal_carryB_wait, CARRY_IDLE, LOOP),
    POSE(link_normal_put, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_normal_put_free, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_normal_throw, THROW, FOLLOW_LINK),
    POSE(link_normal_throw_free, THROW, FOLLOW_LINK),
    POSE(link_normal_nocarry_free, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_normal_nocarry_free_wait, CARRY_IDLE, LOOP),
    POSE(link_normal_nocarry_free_end, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_normal_heavy_carry, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_normal_heavy_carry_end, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_normal_heavy_run, CARRY_WALK, STRIDE),
    POSE(link_normal_heavy_run_free, CARRY_WALK, STRIDE),
    POSE(link_silver_carry, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_silver_wait, CARRY_IDLE, LOOP),
    POSE(link_silver_throw, THROW, FOLLOW_LINK),
    POSE(cl_dakisime, CARRY_IDLE, LOOP),
    POSE(cl_dakisime_loop, CARRY_IDLE, LOOP),
    POSE(link_normal_push_start, WALK_CREEP, FOLLOW_LINK),
    POSE(link_normal_pushing, WALK_CREEP, STRIDE),
    POSE(link_normal_push_wait, STAND_IDLE, LOOP),
    POSE(link_normal_push_wait_end, STAND_IDLE, LOOP),
    POSE(link_normal_push_end, STAND_IDLE, LOOP),
    POSE(link_normal_pull_start, WALK_CREEP, FOLLOW_LINK),
    POSE(link_normal_pull_start_free, WALK_CREEP, FOLLOW_LINK),
    POSE(link_normal_pulling, WALK_CREEP, STRIDE),
    POSE(link_normal_pulling_free, WALK_CREEP, STRIDE),
    POSE(link_normal_pull_end, STAND_IDLE, LOOP),
    POSE(link_normal_pull_end_free, STAND_IDLE, LOOP),

    // Ladders and vines, crawlspaces.
    POSE(link_normal_Fclimb_startA, CLIMB_IDLE, FOLLOW_LINK),
    POSE(link_normal_Fclimb_startB, CLIMB_IDLE, FOLLOW_LINK),
    POSE(link_normal_Fclimb_upL, CLIMB_MOVE, FOLLOW_LINK),
    POSE(link_normal_Fclimb_upR, CLIMB_MOVE, FOLLOW_LINK),
    POSE(link_normal_Fclimb_hold2upL, CLIMB_IDLE, LOOP),
    POSE(link_normal_Fclimb_sideL, CLIMB_MOVE, FOLLOW_LINK),
    POSE(link_normal_Fclimb_sideR, CLIMB_MOVE, FOLLOW_LINK),
    POSE(link_child_tunnel_start, CROUCH_ENTER, FOLLOW_LINK),
    POSE(link_child_tunnel_end, CROUCH_ENTER, FOLLOW_LINK),

    // Bottles.
    POSE(link_bottle_drink_demo_start, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(link_bottle_drink_demo_wait, REST_LISTEN, LOOP),
    POSE(link_bottle_drink_demo_end, STAND_IDLE, LOOP),
    POSE(link_bottle_bug_in, PUNCH, FOLLOW_LINK),
    POSE(link_bottle_fish_in, PUNCH, FOLLOW_LINK),
    POSE(link_bottle_bug_out, THROW, FOLLOW_LINK),
    POSE(link_bottle_fish_out, THROW, FOLLOW_LINK),

    // Story moments of each game.
    POSE_RANGE(link_demo_back_to_past, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(link_demo_return_to_past, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(clink_demo_goto_future, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(clink_demo_return_to_future, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(alink_powerup, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(alink_powerup_loop, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(al_hensin, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(al_hensin_loop, JIG_JIGGY, ONCE, 100, 131),
    POSE_RANGE(link_keirei, JIG_JIGGY, ONCE, 100, 131),
    POSE(link_last_hit_motion1, PUNCH, FOLLOW_LINK),
    POSE(link_last_hit_motion2, PUNCH, FOLLOW_LINK),
    POSE(cl_setmask, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(cl_maskoff, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(cl_tewofuru, REST_LISTEN, LOOP),
    POSE(cl_wakare, REST_LISTEN, LOOP),
    POSE(cl_wakare_loop, REST_LISTEN, LOOP),
    POSE(alink_dance_loop, REST_LISTEN, LOOP),
    POSE(cl_nigeru, WALK_RUN, STRIDE),
    POSE(link_demo_kakeyori, WALK_RUN, STRIDE),
    POSE(bajyo_walk, WALK, STRIDE),
    POSE(cl_tobikakaru, JUMP, FOLLOW_LINK),
    POSE(cl_furafura, TIMEOUT, LOOP),
    POSE(cl_umanoru, CROUCH_ENTER, FOLLOW_LINK),
    POSE(cl_umanoru_loop, CROUCH_IDLE, LOOP),
    POSE(alink_ozigi, STAND_PULL_KAZOOIE, FOLLOW_LINK),
    POSE(alink_ozigi_loop, REST_LISTEN, LOOP),
    POSE(alink_ozigi_modori, STAND_IDLE, LOOP),
    POSE(alink_somukeru, STAND_IDLE, LOOP),
    POSE(alink_somukeru_loop, STAND_IDLE, LOOP),
    POSE(al_no, STAND_IDLE, LOOP),
};

// Indexed by Link's eye texture (open, half, closed, left, right, wide, down, wincing). Banjo's eyes only blink:
// his geo selectors run from 1 (open) to 8 (shut), so looking aside keeps them open.
static const uint8_t kBanjoEyesForLink[] = { 1, 4, 8, 1, 1, 1, 1, 8 };

uint8_t BanjoPoseTable_GetEyeSelector(int32_t linkEyes) {
    if (linkEyes < 0 || linkEyes >= (int32_t)(sizeof(kBanjoEyesForLink) / sizeof(kBanjoEyesForLink[0]))) {
        return 0;
    }
    return kBanjoEyesForLink[linkEyes];
}

const BanjoPose* BanjoPoseTable_Find(const char* linkAnimation) {
    for (size_t index = 0; index < sizeof(kPoses) / sizeof(kPoses[0]); index++) {
        if (strcmp(kPoses[index].linkAnimation, linkAnimation) == 0) {
            return &kPoses[index];
        }
    }
    return NULL;
}
