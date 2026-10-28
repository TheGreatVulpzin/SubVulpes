# Core seams of the system areas

The seven system areas of the second editor (AI, economy, missions, settings, storage, engine and shop) came with hooks that the core must call. This page records which hooks the server calls and which ones wait.
A hook that the server does not call leaves its public methods in the state `SV_STATE`, or leaves the default behavior of the game. Nothing here changes the behavior of S0 until a plugin uses a method.
Each area has its own page with the design: [AI](plugin-ai.md), [economy](plugin-economy.md), [missions](plugin-mission.md), [settings](plugin-settings.md), [storage](plugin-storage.md), [engine](plugin-engine.md) and [shop](plugin-shop.md).

## Installed

| Area | Hook | Where the server calls it |
| --- | --- | --- |
| All | `sv_systems_install` | `main_loop.c`, once the world is ready. It installs AI, missions, settings, engine and shop. |
| AI | `sv_ai_install` | `sv_systems_install` |
| AI | `sv_ai_before_player`, `sv_ai_after_player` | The bot loop in `main_loop.c`. A bot under plugin control skips `playerai` for that tick. |
| AI | `sv_ai_after_threats` | `playerai`, after each `ai_assessthreats`. |
| AI | `sv_ai_perception_part` | `ai_assessthreats`. A result of 1 or 2 replaces the body part that the scan looks at. |
| AI | `sv_ai_shot` | The two shot decisions of `playerai`. Without a policy the draw is the same as before. |
| AI | `sv_ai_route_next` | `ai_car_nextwaypoint`. The game function is now `nextwaypoint_default`, and the new function calls it when no route is set. |
| AI | `sv_ai_release_owner` | The release hook, when a plugin stops. |
| Missions | `sv_mission_install` | `sv_systems_install` |
| Missions | `sv_mission_before_complete` | The menu action that completes a mission (`logic.c`). A zero result leaves the mission as it is. |
| Missions | `sv_mission_before_tick` | The start of each tick, the world mission pass (`logic_sim.c`), and each item retirement (`itemtimetolive`). |
| Missions | `sv_mission_release_owner` | The release hook. |
| Settings | `sv_settings_install` | `sv_systems_install`, with the event bit `SV_EVENT_SETTING` (131072). The notice joins the shared queue. |
| Settings | `sv_settings_native` | The money of a new account (`account_db.c`) and the two traffic counts (`logic_sim.c`). A setting that nobody changed keeps the old constant. |
| Storage | `sv_storage_open` | The host, before the enable callback. A plugin that cannot open `user/plugins/ID` does not start. |
| Storage | `sv_storage_release_owner` | The release hook. |
| Storage | `sv_storage_leave` | The retirement of a player (`sv_entity_deleted`), so the session values of that player go. |
| Engine | `sv_engine_install` | `sv_systems_install`, with `SV_ENGINE_OWNER_CLEANUP` only. |
| Engine | `sv_engine_release_owner` | The release hook. |
| Shop | `sv_shop_install` | `sv_systems_install`, with the shared decision and write-context functions, the bits 256 and 512, and no safe feature flag. It also registers the rows `item.phone_state` and `item.phone_number`. |
| Shop | `sv_shop_phone_retire` | `itemtimetolive`, for a phone (item type 23), before the game retires the reference. |

The release hook (`plugin_game.c`) runs when a plugin stops for any reason, after its disable callback. It calls the five `release_owner` functions, the property restore and the cleanup of made things.

## Open

| Area | Missing | Why it waits |
| --- | --- | --- |
| Engine | The consumers of `SV_ENGINE_DEATH_START` and `SV_ENGINE_RAGDOLL_RESET`, and the flag `SV_ENGINE_GLOBAL_CONSUMERS`. | The death threshold touches about 15 branches in `humans.c`, `human_pose.c`, `human_arms.c`, `bonds.c` and `bullet_sim.c`, and the packet camera in `netpack.c`. The area page asks for all of them together. The original client may show a positive-health collapse in a way nobody tested. Until then the global rows keep their fallbacks. |
| Shop | The safe points for the phone digit and the phone call (`SV_SHOP_SAFE_DECISIONS`). | The native digit code runs in `logic_playeractions` and the call lookup in `itemlogic`, inside the simulation. A plugin must not run there. The core needs a deferred decision. |
| Shop | The fix of the native purchase allocation (`SV_SHOP_SAFE_PURCHASES`). | `logic_gunstore_buy` and `logic_cardealer_buy` can charge after an allocation fails. A catalog write stays closed until they check first. |
| Missions | The notice and decision rows in the event queue. | The area has no event payload type yet. |
| Settings and engine | Rows in the property table. | The areas keep their own typed tables. They have an owner and a change event, and a plain row has neither. Keep the typed tables, unless a plugin needs the rows. |
| Economy | None. | The area needs no hook. |
