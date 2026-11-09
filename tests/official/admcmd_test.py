"""Check admin commands with real DLLs, public snapshots and the SDK runner."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

BUILD = Path(sys.argv[1]).resolve()


def run(name, steps, permissions=True, patchouli=True, soft=()):
    if os.environ.get("SV_ADMCMD_CASE", name) != name:
        return
    with tempfile.TemporaryDirectory(prefix="admcmd-", dir=BUILD / "tests") as folder:
        root = Path(folder)
        plugins = ["sv.patchouli", "sv.logging", "sv.admcmd", "sv.probe", "test.admcmd"]
        if not patchouli:
            plugins.remove("sv.patchouli")
        if permissions:
            plugins.append("sv.permissions")
        profile, approvals = ["[plugins]"], ["[native]"]
        for plugin in plugins:
            shutil.copytree(BUILD / "plugins" / plugin, root / "plugins" / plugin)
            profile.append(plugin + "=" + ("enabled" if plugin in ("sv.permissions", "sv.patchouli", "sv.logging", *soft) else "required"))
            binary = next((root / "plugins" / plugin).glob("*.dll"))
            approvals.append(plugin + "=" + hashlib.sha256(binary.read_bytes()).hexdigest())
        (root / "plugins/plugins.ini").write_text("\n".join(profile) + "\n", encoding="ascii")
        (root / "plugins/native-approvals.ini").write_text("\n".join(approvals) + "\n", encoding="ascii")
        scenario = root / (name + ".svtest")
        scenario.write_text(steps + "\nexpect no-failure\n", encoding="ascii")
        result = subprocess.run([str(BUILD / "subvulpes_plugin_test.exe"), str(scenario)], cwd=root,
                                capture_output=True, text=True, timeout=90)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        assert result.returncode == 0 and "ADM_PROBE_FAILED" not in result.stdout, (name, result.returncode)


weapons = """
spawn player Root
spawn player Guest
tick 1
console perm set Root owner
console admprobe api
expect log "ADM_OPTIONAL_API_OK"
"""
for selection, item_type in enumerate(range(0, 12, 2), 1):
    weapons += f"""
say Root /arm {selection} Guest
console admprobe inventory Guest
expect log "ADM_ITEM Guest slot=0 type={item_type} children=1"
console admprobe empty Guest
"""
weapons += """
console arm 2 Guest
expect log "Gave M-16 to Guest with 1 loaded and 3 spare magazines."
console admprobe inventory Guest
expect log "ADM_ITEM Guest slot=0 type=2 children=1"
expect log "ADM_ITEM Guest slot=3 type=3 children=0"
expect log "ADM_ITEM Guest slot=4 type=3 children=0"
console admprobe empty Guest
console item 12 Guest
expect log "Gave bandage to Guest."
console admprobe inventory Guest
expect log "ADM_ITEM Guest slot=0 type=12 children=0"
console arm 2 Guest
console admprobe inventory Guest
expect log "ADM_ITEM Guest slot=1 type=2 children=1"
console arm 3 Guest
expect log "Gave Magnum to Guest in the inventory"
console admprobe pockets Guest
console arm 4 Guest
expect log "Nothing was given: both hands and all pockets of Guest are full."
say Root /arm 1
console admprobe inventory Root
expect log "ADM_ITEM Root slot=0 type=0 children=1"
console admprobe empty Root
say Root /arm
console admprobe inventory Root
expect log "ADM_ITEM Root slot=0 type=2 children=1"
console arm
expect log "Use: /arm [N|NAME] [PLAYER]"
"""
run("weapons", weapons)

run("players", """
spawn player Root
spawn player Guest
spawn player GuestTwo
tick 1
console perm set Root owner
set human.health Guest 25
set human.bleeding Guest 1
set human.stun Guest 15
say Root /heal Guest
expect prop human.health Guest 100
expect prop human.bleeding Guest 0
expect prop human.stun Guest 0
set human.health Root 20
say Guest /heal Root
expect prop human.health Root 20
console group allow guest admcmd.heal
say Guest /heal Root
expect prop human.health Root 100
say Root /imortal Guest
set human.health Guest 25
tick 1
expect prop human.health Guest 100
expect prop player.godmode Guest 0
say Root /god Guest
set human.health Guest 25
tick 1
expect prop human.health Guest 25
console heal all
expect prop human.health Guest 100
console team 5 Guest
expect prop player.team Guest 5
console herobrine Guest
console admprobe state Guest
expect log "ADM_STATE Guest health=100 team=5"
console players
expect log "acc 0  id:0  Root [owner]"
expect log "acc 1  id:1  Guest [guest]"
console who
expect log "acc 2  id:2  GuestTwo"
console id GuestT
expect log "acc 2  id:2  GuestTwo [guest]"
console id estTwo
console id 2
console id Guest
console give money Guest 15
expect prop human.health Guest 100
console admprobe state Guest
expect log "money=50015"
console give admin Guest
tick 1
expect prop player.admin Guest 1
console admprobe bot
console players
expect no-log "TestBot"
expect log "bots hidden. Use /players all."
console players all
expect log "id:3  TestBot [bot]"
console heal TestBot
expect log "Healed TestBot."
console team 3 TestBot
expect log "Set the team of TestBot to nexaco."
console give money TestBot 10
expect log "Use: /give money|admin PLAYER [N]"
console give admin TestBot
expect count 2 "Use: /give money|admin PLAYER [N]"
console kill GuestTwo
expect prop human.health GuestTwo -5
expect prop human.bleeding GuestTwo 1
console heal GuestTwo
expect prop human.health GuestTwo 100
console turn 0 Guest
expect log "Changed the look and name of Guest to account 0."
console help
expect log "/arm [N|NAME] [PLAYER] - Weapon with 4 magazines"
expect log "/tp [PLAYER] PLAYER|X Y Z"
say Root /clear
say Root /clear Guest
console clear Guest
console say A public server message.
expect log "Sent your message to everybody."
""")

run("motion", """
spawn player Root
spawn player Target
tick 1
console perm set Root owner
console where Target
expect log "Target: 13.0 20.0 30.0"
console bring Target Root
console admprobe state Target
expect log "ADM_POSITION Target 12.00 20.00 30.00"
console tp Root Target
console admprobe state Root
expect log "ADM_POSITION Root 14.00 20.00 30.00"
console goto Target Root
console admprobe state Target
expect log "ADM_POSITION Target 16.00 20.00 30.00"
console freeze Target
console admprobe push Target
tick 1
console admprobe state Target
expect log "ADM_POSITION Target 16.00 20.00 30.00 velocity=0.00,0.00,0.00"
console unfreeze Target
console admprobe push Target
tick 1
console admprobe state Target
expect log "velocity=4.00,5.00,6.00"
console tpall Root
expect log "Moved 1 players to Root."
console npc Root
console npc Target
expect log "Another bot is already waiting. Try again in a moment."
tick 1
expect entities players 3
console vehicle 0 Root
expect log "Made a towncar (color 0) in front of Root."
expect entities objects 1
console sound 0 Root
expect log "Played the sound magnum at the aim point of Root."
say Root /tp
console car 11 Target
expect log "Made a heli (color 0) in front of Target."
expect entities objects 2
console round reset
expect log "Queued the round reset."
""")

# The thing that moves comes first and the place second: /tp [PLAYER] PLAYER, and /tp [PLAYER] X Y Z.
run("teleport", """
spawn player Root
spawn player Target
tick 1
console perm set Root owner
console tp Root 100 50 -20.5
console admprobe state Root
expect log "ADM_POSITION Root 100.00 50.00 -20.50"
console tp Root ~ ~5 ~-10
console admprobe state Root
expect log "ADM_POSITION Root 100.00 55.00 -30.50"
console tp Target 1 2 3
console admprobe state Target
expect log "ADM_POSITION Target 1.00 2.00 3.00"
say Root /tp Target
console admprobe state Root
expect log "ADM_POSITION Root 3.00 2.00 3.00"
say Root /tp 10 20 30
console admprobe state Root
expect log "ADM_POSITION Root 10.00 20.00 30.00"
console tp Root 40000 0 0
expect log "That place is outside the world."
console tp Root 1 2 x
expect log "Use: /tp [PLAYER] X Y Z"
console admprobe state Root
expect log "ADM_POSITION Root 10.00 20.00 30.00"
console tp 1 2 3
expect log "Use: /tp [PLAYER] PLAYER|X Y Z"
""")

# A plain number is an account number. Only id:N is a slot. A number never falls back to a slot.
run("targets", """
spawn player Root
spawn player Guest
tick 1
leave Root
spawn player Late
tick 1
set human.health Late 20
console heal 0
expect log "Account 0 is not online."
expect prop human.health Late 20
console heal 2
expect prop human.health Late 100
set human.health Late 20
console heal id:0
expect prop human.health Late 100
set human.health Guest 20
console heal 1
expect prop human.health Guest 100
console heal id:9
expect log "No player has ID 9."
console heal id:x
expect log "Bad player ID."
console heal 99999
console tp 7 Late
expect log "Account 7 is not online."
console players
expect log "acc 2  id:0  Late [guest]"
expect log "acc 1  id:1  Guest [guest]"
""")

# /delh ends the body of a real player. The player stays, with no body, and can respawn.
run("delh", """
spawn player Root
spawn player Target
tick 1
console perm set Root owner
expect entities humans 2
console delh Target
expect log "Ended the body of Target."
expect entities humans 1
expect entities players 2
console heal Target
expect log "Target has no living body. Wait until the player spawns."
console delh Target
expect log "Target has no living body. Wait until the player spawns."
say Root /delh Root
expect entities humans 1
console admprobe bot
console delh TestBot
expect log "Ended the body of TestBot."
""")

heavy = """
spawn player Root
tick 1
"""
for slot in range(15):
    heavy += f"console probe set human.bone_mass Root 2 {slot}\n"
heavy += """
console heavydude Root
expect log "Made Root very heavy."
console probe get human.bone_mass Root 0
expect log "PROBE get human.bone_mass Root = 200000.000"
console probe get human.bone_mass Root 14
expect count 2 "= 200000.000"
console heavy Root
expect log "Root is already heavy."
"""
run("heavy", heavy)

bounds = """
spawn player Root
spawn player Aster
spawn player Asteroid
tick 1
console perm set Root owner
console heal Ast
expect log "Several players match."
console heal Missing
expect log "Use /players to see the names."
"""
for command, kind, lower, upper in [("arm", "weapon", 1, 6), ("item", "item", 0, 25), ("team", "team", 0, 5),
                                    ("sound", "sound", 0, 46), ("turn", "account number", 0, 65535)]:
    for value in (lower - 1, upper + 1, "99999999999999999999999999999", "1x"):
        bounds += f"console {command} {value} Root\n"
    bounds += f'expect count 4 "is not a valid {kind}. Use a number from {lower} to {upper}"\n'
for value in (-1, 12, "99999999999999999999999999999", "1x"):
    bounds += f"console vehicle {value} Root\n"
bounds += 'expect count 4 "is not a valid vehicle. Use a number from 0 to 11"\n'
bounds += """
console give money Root 0
console give money Root 100000001
expect count 2 "Use: /give money|admin PLAYER [N]"
console probe set human.bone_mass Root 501 14
console heavy Root
expect log "Use: /heavy [PLAYER]"
console heal Root extra
expect count 1 "Use: /heal [PLAYER|all]"
console arm 1 Root extra extra extra
expect count 1 "Use: /arm [N|NAME] [PLAYER]"
console heal AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
expect count 2 "Use: /heal [PLAYER|all]"
console arm
expect log "Use: /arm [N|NAME] [PLAYER]"
"""
run("bounds", bounds)

run("fallback", """
spawn player Root admin=1
spawn player Guest
tick 1
set human.health Guest 20
say Guest /heal
expect prop human.health Guest 20
say Root /heal Guest
expect prop human.health Guest 100
say Root /arm 1 Guest
console admprobe inventory Guest
expect log "ADM_ITEM Guest slot=0 type=0 children=1"
console give admin Guest
expect log "This needs the Permission System plugin, and it is not running."
""", permissions=False)

# A full pocket skips only its magazines. The weapon and the loaded magazine stay.
run("arm-partial", """
spawn player Root
tick 1
console admprobe magpockets Root
console arm 1 Root
expect log "Gave AK-47 to Root with 1 loaded and 0 spare magazines."
expect log "Skipped 3 magazines: no room."
console admprobe inventory Root
expect log "ADM_ITEM Root slot=0 type=0 children=1"
""")

run("optional", """
spawn player Root
spawn player Guest
tick 1
console perm set Root owner
console admprobe api
console plugins disable sv.permissions
set human.health Guest 20
say Root /heal Guest
expect prop human.health Guest 100
set human.health Guest 20
say Guest /heal
expect prop human.health Guest 20
console plugins enable sv.permissions
tick 1
console admprobe api
say Root /heal Guest
expect prop human.health Guest 100
""")

# Patchouli owns /help. The console sees every topic. Topics leave when the plugin stops and return when it starts.
run("help", """
spawn player Root
spawn player Guest
tick 1
console perm set Root owner
console help
expect log "/heal [PLAYER|all] - Set health 100"
expect log "/delh [PLAYER] - End the body"
expect log "/logs [COUNT] - Show the last lines"
expect count 1 "/kill [PLAYER] - Set health -5"
console help tp
expect log "/tp [PLAYER] PLAYER|X Y Z"
expect log "Permission: admcmd.tp"
console help nothing
expect log "No help for nothing."
console plugins disable sv.admcmd
console help
expect count 1 "/kill [PLAYER] - Set health -5"
expect count 2 "/logs [COUNT] - Show the last lines"
console plugins enable sv.admcmd
console help
expect count 2 "/kill [PLAYER] - Set health -5"
say Guest /help
say Root /help 2
say Root /page
""", soft=("sv.admcmd",))

# Without Patchouli the plugin keeps its own short /help.
run("help-fallback", """
spawn player Root
tick 1
console help
expect log "arm 1=AK47"
""", patchouli=False)

# A name stands in for a number. The number still works.
run("names", """
spawn player Root
spawn player Guest
tick 1
console perm set Root owner
console arm uzi Guest
expect log "Gave Uzi to Guest with 1 loaded and 3 spare magazines."
console admprobe empty Guest
console arm AK Guest
expect log "Gave AK-47 to Guest"
console admprobe empty Guest
console arm m Guest
expect log "matches several: m16, magnum, mp5"
console item bandage Guest
expect log "Gave bandage to Guest."
console item disk Guest
expect log "matches several:"
console team pentacom Guest
expect log "Set the team of Guest to pentacom."
expect prop player.team Guest 4
# Nothing is on the aim line here, so the sound fails the same way by name and by number.
console sound ping Root
console sound 39 Root
expect count 2 "sound by console target Root (acc 0) result FAIL SV_NOT_FOUND(6)"
expect no-log "is not a valid sound"
console vehicle nosuch Root
expect log "is not a valid vehicle"
console names weapon
expect log "1 ak47, ak-47, ak"
console names vehicles
expect log "5 turbos, turbo-s"
console names team
expect log "0 goldmen, goldmen-inc"
console names boats
expect log "Use: /names weapon|item|vehicle|team|sound"
""")

# /car takes modifiers. The test world has no seats, so the driver seat fails here and the log says why.
# A seat that works needs a check in the game.
run("car", """
spawn player Root
spawn player Target
tick 1
console perm set Root owner
console car van color=3 Target
expect log "Made a van (color 3) in front of Target."
expect entities objects 1
console car limo dist=10 yaw=90 enter Target
expect log "Made a limo (color 0) in front of Target. The seat failed: Target did not get in."
expect log "vehicle by console target Target (acc 1) result FAIL SV_INVALID(1) vehicle_enter failed"
expect entities objects 2
console car van color=9 Target
expect log "color must be a number from 0 to 5."
console car van dist=99 Target
expect log "dist must be a number from 2 to 30."
console car van yaw=x Target
expect log "yaw must be a number from -180 to 180."
console car van size=2 Target
expect log "size=2 is not a modifier."
expect entities objects 2
""")

# The log service writes one fixed line for each failure and for each dangerous action.
run("logging", """
spawn player Root
spawn player Guest
tick 1
console perm set Root owner
console kill Guest
expect log "kill by console target Guest (acc 1) result OK"
console heal Missing
expect log "heal by console result FAIL SV_NOT_FOUND(6)"
console arm 1 Guest extra extra
expect log "arm by console result FAIL SV_INVALID(1)"
console heal Guest
expect no-log "heal by console target Guest (acc 1) result OK"
console give money Guest 5
expect log "give by console target Guest (acc 1) result OK"
say Root /give money Guest 5
expect count 1 "give by Root (acc 0) target Guest (acc 1) result OK"
console logs 3
expect count 2 "give by Root (acc 0) target Guest (acc 1) result OK"
""")
