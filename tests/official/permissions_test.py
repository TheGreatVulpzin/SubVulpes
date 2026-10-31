"""Run permission policy through the SDK runner and a native library consumer."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

BUILD = Path(sys.argv[1]).resolve()


def prepare(root):
    for plugin in ("sv.permissions", "sv.probe", "test.permissions"):
        shutil.copytree(BUILD / "plugins" / plugin, root / "plugins" / plugin)
    profile = "[plugins]\nsv.permissions=required\nsv.probe=required\ntest.permissions=required\n"
    (root / "plugins/plugins.ini").write_text(profile, encoding="ascii")
    approvals = ["[native]"]
    for plugin in ("sv.permissions", "sv.probe", "test.permissions"):
        binary = next((root / "plugins" / plugin).glob("*.dll"))
        approvals.append(plugin + "=" + hashlib.sha256(binary.read_bytes()).hexdigest())
    (root / "plugins/native-approvals.ini").write_text("\n".join(approvals) + "\n", encoding="ascii")


def run(root, name, steps):
    selected = os.environ.get("SV_PERMISSION_CASE")
    if selected and selected != name:
        return ""
    scenario = root / (name + ".svtest")
    scenario.write_text(steps, encoding="ascii")
    result = subprocess.run([str(BUILD / "subvulpes_plugin_test.exe"), str(scenario)], cwd=root,
                            capture_output=True, text=True, timeout=90)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    assert result.returncode == 0 and "PERMISSION_PROBE_FAILED" not in result.stdout, (name, result.returncode)
    return result.stdout


with tempfile.TemporaryDirectory(prefix="permissions-api-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    run(root, "api", """
spawn player Guest
tick 1
console permprobe list
expect log "PERMISSION_GROUPS 5"
console permprobe group Guest
expect log "PERMISSION_GROUP Guest guest 0"
expect no-failure
""")

with tempfile.TemporaryDirectory(prefix="permissions-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    run(root, "groups", """
spawn player Root
spawn player Mod
spawn player Guest
tick 1
console permprobe list
expect log "PERMISSION_GROUPS 5"
console permprobe group Guest
expect log "PERMISSION_GROUP Guest guest 0"
console perm set Root owner
console perm set Mod moderator
console permprobe can Mod core.kick
expect log "PERMISSION_CAN Mod core.kick 1"
console permprobe can Mod core.cash
expect log "PERMISSION_CAN Mod core.cash 0"
say Mod /cash
console permprobe state Mod
expect log "PERMISSION_STATE Mod money=50000 admin=0"
console group allow moderator core.cash
say Mod /cash
console permprobe state Mod
expect log "PERMISSION_STATE Mod money=150000 admin=0"
console plugins config sv.permissions gate_core_commands 0
say Mod /cash
console permprobe state Mod
expect count 2 "PERMISSION_STATE Mod money=150000 admin=0"
console plugins config sv.permissions gate_core_commands 1
console group create helper moderator
console group deny helper core.kick
console group allow helper admcmd.*
console group prefix helper [Helper]
console group level helper 40
console perm set Guest helper
console permprobe group Guest
expect log "PERMISSION_GROUP Guest helper 40"
expect log "PERMISSION_GROUP Guest helper 40 prefix=[Helper]"
console permprobe can Guest core.kick
expect log "PERMISSION_CAN Guest core.kick 0"
console permprobe can Guest core.list
expect log "PERMISSION_CAN Guest core.list 1"
console permprobe can Guest admcmd.heal
expect log "PERMISSION_CAN Guest admcmd.heal 1"
spawn player Low
tick 1
say Guest /perm set Low member
console permprobe group Low
expect log "PERMISSION_GROUP Low guest 0"
console permprobe assign Guest Low member
expect log "PERMISSION_ASSIGN Guest Low member 5"
console permprobe assign Root Low member
expect log "PERMISSION_ASSIGN Root Low member 0"
console permprobe assign Root Low member
expect log "PERMISSION_ASSIGN Root Low member 4"
tick 1
console permprobe group Low
expect log "PERMISSION_GROUP Low member 10"
say Guest /group prefix member [Forged]
console group show member
expect log "Group member level=10 parent=guest prefix=[Member]"
console group remove helper core.kick
console permprobe can Guest core.kick
expect log "PERMISSION_CAN Guest core.kick 1"
say Guest /perm set Guest owner
console permprobe group Guest
expect count 2 "PERMISSION_GROUP Guest helper 40"
console group allow helper permissions.assign
say Guest /perm set Mod helper
console permprobe group Mod
expect log "PERMISSION_GROUP Mod moderator 50"
console group allow helper permissions.group
say Guest /group create privileged admin
console group show privileged
expect log "Group not found."
say Guest /group level moderator 30
console group show moderator
expect log "Group moderator level=50"
say Guest /group prefix owner forged
console group show owner
expect log "Group owner level=100 parent=admin prefix=[Owner]"
say Root /group level owner 99
console group delete guest
console group delete owner
console permprobe group Root
expect log "PERMISSION_GROUP Root owner 100"
console group list
expect log "A default group cannot be deleted."
console group create bad.name
console group level helper 101
console group level helper -1
console group level helper 99999999999999999999999
console group level helper 1x
expect count 4 "Level must be 0 to 100 and within your authority."
console group allow helper bad..node
console group show helper
expect log "Group helper level=40"
console group prefix helper aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
expect log "A prefix holds at most 31 bytes."
console group create abcdefghijklmnopqrstuvwx
expect log "Invalid group name."
console perm set Nobody member
expect log "Player not found."
console group delete helper
console permprobe group Guest
expect log "PERMISSION_GROUP Guest guest 0"
console permprobe npc
tick 1
console permprobe group Bot
expect log "PERMISSION_GROUP Bot guest 0"
console perm set Bot owner
expect log "Group assignment refused: 5."
expect no-failure
""")
    # Account assignments and custom groups survive a new host, without a saved game file.
    run(root, "restart", """
spawn player Root
spawn player Mod
spawn player Guest
tick 1
console permprobe group Root
expect log "PERMISSION_GROUP Root owner 100"
console permprobe group Mod
expect log "PERMISSION_GROUP Mod moderator 50"
console permprobe group Guest
expect log "PERMISSION_GROUP Guest guest 0"
console perm set Root guest
expect log "Group assignment refused: 5."
console permprobe group Root
expect log "PERMISSION_GROUP Root owner 100"
expect no-failure
""")

with tempfile.TemporaryDirectory(prefix="permissions-login-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    run(root, "login", """
spawn player Root
tick 1
console plugins config sv.permissions password_login 0
say Root /admin svtest
tick 1
expect prop player.admin Root 0
console permprobe group Root
expect log "PERMISSION_GROUP Root guest 0"
console plugins config sv.permissions password_login 1
say Root /admin wrong
tick 1
expect prop player.admin Root 0
say Root /admin svtest
tick 1
expect prop player.admin Root 1
console permprobe group Root
expect log "PERMISSION_GROUP Root admin 90"
leave Root
tick 1
spawn player Root
tick 1
expect prop player.admin Root 0
console permprobe group Root
expect count 2 "PERMISSION_GROUP Root guest 0"
console plugins config sv.permissions save_login_group 1
say Root /admin svtest
tick 1
leave Root
tick 1
spawn player Root
tick 1
expect prop player.admin Root 1
console permprobe group Root
expect count 2 "PERMISSION_GROUP Root admin 90"
expect no-failure
""")

with tempfile.TemporaryDirectory(prefix="permissions-recovery-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    run(root, "recovery", """
spawn player Root
tick 1
console group deny admin permissions.assign
say Root /admin svtest
tick 1
say Root /perm set Root owner
console permprobe group Root
expect log "PERMISSION_GROUP Root owner 100"
spawn player Other
tick 1
say Other /admin svtest
tick 1
say Other /perm set Other owner
console permprobe group Other
expect log "PERMISSION_GROUP Other admin 90"
say Root /perm set Root guest
console permprobe group Root
expect count 2 "PERMISSION_GROUP Root owner 100"
expect no-failure
""")

with tempfile.TemporaryDirectory(prefix="permissions-authority-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    run(root, "authority", """
spawn player Root
spawn player Peer
tick 1
console perm set Root owner
console group create peer
console group level peer 100
console group allow peer permissions.group
console perm set Peer peer
say Peer /group prefix owner Forged
console group show owner
expect log "Group owner level=100 parent=admin prefix=[Owner]"
console plugins config sv.permissions default_group member
spawn player New
tick 1
console permprobe group New
expect log "PERMISSION_GROUP New member 10"
console plugins config sv.permissions default_group missing
console permprobe group New
expect log "PERMISSION_GROUP New guest 0"
expect no-failure
""")

with tempfile.TemporaryDirectory(prefix="permissions-bounds-", dir=BUILD / "tests") as folder:
    root = Path(folder)
    prepare(root)
    steps = ["spawn player Root", "tick 1", "console perm set Root owner", "console group create flags"]
    steps += [f"console group allow flags test.node{i}" for i in range(20)]
    steps += ["console group allow flags test.overflow", 'expect log "The node list or node text is full."']
    steps += [f"console group create extra{i}" for i in range(18)]
    steps += ["console group create overflow", 'expect log "Group exists, the group limit is full, or the parent is invalid."']
    steps += ["console permprobe list", 'expect log "PERMISSION_GROUPS 24"', "expect no-failure"]
    run(root, "bounds", "\n".join(steps) + "\n")

print("Permission SDK scenarios passed.")
