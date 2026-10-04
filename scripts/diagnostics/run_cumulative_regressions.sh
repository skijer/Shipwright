#!/usr/bin/env bash
# Required source-level checks before distributing a cumulative working build.
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 -B scripts/diagnostics/check_feature_baselines.py
python3 -B -m unittest discover -s scripts/diagnostics -p test_feature_baselines.py
python3 -B scripts/diagnostics/run_hint_item_name_tests.py
python3 -B scripts/diagnostics/run_nei_gi_tests.py --held
python3 -B tests/nei_held/run_hand_fit_tests.py
python3 -B tests/nei_held/run_articulated_tests.py
python3 -B tests/nei_used_fx/run_tests.py
python3 -B tests/nei_leaf/run_tests.py
python3 -B tests/nei_whip/run_tests.py
python3 -B tests/nei_lantern_grip/run_tests.py
python3 -B scripts/diagnostics/run_switch_hook_instant_tests.py
python3 -B scripts/diagnostics/run_time_gate_visibility_tests.py
python3 -B tests/nei_item_stow/run_ballchain_tests.py
python3 -B tests/nei_item_stow/run_lantern_rod_tests.py
python3 -B tests/nei_item_stow/run_stow_tests.py
python3 -B tools/nei_held/verify_assets.py
python3 -B scripts/diagnostics/test_young_epona_assets.py
python3 -B scripts/diagnostics/run_young_epona_tests.py
python3 -B scripts/diagnostics/run_young_epona_actor_tests.py
python3 -B scripts/diagnostics/run_young_epona_player_tests.py
python3 -B scripts/diagnostics/run_young_epona_save_tests.py
python3 -B scripts/diagnostics/run_midna_navi_draw_test.py
python3 -B scripts/diagnostics/run_midna_audio_test.py
python3 -B scripts/diagnostics/run_epona_cosmetics_tests.py
python3 -B scripts/diagnostics/run_alt_segment_binding_tests.py
python3 -B scripts/diagnostics/run_house_rocs_feather_tests.py
python3 -B scripts/diagnostics/run_zora_barrier_cosmetics_tests.py
python3 -B scripts/diagnostics/run_din_fire_shield_tests.py
python3 -B scripts/diagnostics/run_din_fire_sword_tests.py
python3 -B scripts/diagnostics/run_din_fire_sword_damage_tests.py
python3 -B scripts/diagnostics/run_ice_arrow_snowflake_tests.py
python3 -B scripts/diagnostics/run_medallion_arrow_textures_tests.py
python3 -B scripts/diagnostics/run_medallion_cast_textures_tests.py
python3 -B scripts/diagnostics/run_spin_effect_texture_tests.py
python3 -B scripts/diagnostics/run_demise_lightning_tests.py
python3 -B scripts/diagnostics/run_oot_custom_cosmetics_tests.py
python3 -B scripts/diagnostics/run_stat_upgrade_tests.py
python3 -B scripts/diagnostics/run_chest_size_tests.py
python3 -B scripts/diagnostics/run_time_pedestal_tests.py
python3 -B scripts/diagnostics/run_pedestal_sword_selection_tests.py
python3 -B scripts/diagnostics/check_time_pedestal_syntax.py
bash scripts/diagnostics/run_stabilization_tests.sh
