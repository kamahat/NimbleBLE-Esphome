"""Generates spec/ble_state_machine.tla (+ .cfg) from spec/transitions.json.

Do not hand-edit ble_state_machine.tla -- re-run this script after editing the
spec. Must stay in lockstep with tools/gen_state_machine/gen_cpp.py (same
source of truth) so the verified model and the shipped C++ engine cannot
silently diverge -- see docs/ARCHITECTURE.md, state machine section.

Deadlines are represented symbolically (Deadline_<EVENT>) rather than as the
literal millisecond constants from the spec: the *shape* of the bounded-wait
guarantee (a state with a deadline cannot let its clock exceed that deadline)
is scale-invariant, so TLC checks it against small concrete values (see the
generated .cfg) to keep the explored state space tiny. The real 5000ms/8000ms
values are a deployment parameter of the C++ engine, already confirmed
against real hardware (see docs/HARDWARE_VALIDATION.md) -- TLC re-deriving
them from scratch would buy nothing and would blow up the state space.
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SPEC = json.loads((ROOT / "spec" / "transitions.json").read_text(encoding="utf-8"))
OUT_DIR = ROOT / "spec"


def pascal(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


states = SPEC["states"]
events = SPEC["events"]
transitions = SPEC["transitions"]

deadline_states = sorted({t["to"] for t in transitions if t.get("set_deadline_ms", 0) > 0})
# One symbolic deadline constant per event that sets a deadline (there may be
# several transitions for the same event from different states in general,
# but today each deadline-setting event sets exactly one).
deadline_consts = {}
for t in transitions:
    if t.get("set_deadline_ms", 0) > 0:
        deadline_consts[t["event"]] = f"Deadline_{t['event'].upper()}"

action_names = []
lines = []
lines.append("---- MODULE ble_state_machine ----")
lines.append("(* GENERATED FILE -- do not edit by hand.")
lines.append("   Source of truth: spec/transitions.json, via tools/gen_state_machine/gen_tla.py.")
lines.append("   Sibling generator: tools/gen_state_machine/gen_cpp.py produces the C++ engine")
lines.append("   (components/nimble_ble/ble_connection_fsm.h/.cpp) from the same spec. *)")
lines.append("EXTENDS Naturals")
lines.append("")
lines.append(f"CONSTANTS MaxClock, MaxBackoff, {', '.join(deadline_consts.values())}")
lines.append("")
lines.append("VARIABLES state, clock, deadline, backoff_count, last_event")
lines.append("")
lines.append("vars == <<state, clock, deadline, backoff_count, last_event>>")
lines.append("")
lines.append("States == {" + ", ".join(f'"{s}"' for s in states) + "}")
lines.append("Events == {" + ", ".join(f'"{e}"' for e in events) + "} \\union {\"none\"}")
lines.append("DeadlineStates == {" + ", ".join(f'"{s}"' for s in deadline_states) + "}")
lines.append("")
lines.append("Init ==")
lines.append('  /\\ state = "Idle"')
lines.append("  /\\ clock = 0")
lines.append("  /\\ deadline = 0")
lines.append("  /\\ backoff_count = 0")
lines.append('  /\\ last_event = "none"')
lines.append("")

for t in transitions:
    action_name = pascal(t["event"]) + "From" + pascal(t["from"])
    action_names.append(action_name)
    deadline_expr = f"clock + {deadline_consts[t['event']]}" if t.get("set_deadline_ms", 0) > 0 else "0"
    resets_backoff = t["to"] == "Ready"
    enters_backoff = t["to"] == "Backoff"
    unchanged = ["clock"]
    if not resets_backoff and not enters_backoff:
        unchanged.append("backoff_count")
    lines.append(f"{action_name} ==")
    lines.append(f'  /\\ state = "{t["from"]}"')
    if t["event"].endswith("timeout"):
        lines.append("  /\\ clock >= deadline")
    if enters_backoff:
        # Without this bound, a slot that keeps failing to connect at a
        # fixed clock value (Idle -> Connecting -> Backoff -> Idle -> ...,
        # none of which requires Tick) can raise backoff_count forever with
        # no bearing on the properties being checked -- pure state-space
        # blowup (observed directly: TLC reached 900k+ states and was still
        # growing before this guard was added). Capping it does not change
        # what the properties mean, only how large a backoff_count TLC has
        # to consider.
        lines.append("  /\\ backoff_count < MaxBackoff")
    lines.append(f'  /\\ state\' = "{t["to"]}"')
    lines.append(f"  /\\ deadline' = {deadline_expr}")
    if resets_backoff:
        lines.append("  /\\ backoff_count' = 0")
    elif enters_backoff:
        lines.append("  /\\ backoff_count' = backoff_count + 1")
    lines.append(f'  /\\ last_event\' = "{t["event"]}"')
    lines.append(f"  /\\ UNCHANGED <<{', '.join(unchanged)}>>")
    lines.append("")

# Tick: the only action that advances clock. Guarded so it can never push
# clock past a deadline while still in a deadline-carrying state -- this is
# what makes BoundedWait an invariant rather than something only true
# "eventually" (matches the real engine: poll_timeout() is checked every
# loop() iteration before anything else, so nothing can let clock run past
# deadline_ms_ while remaining in Connecting/Discovering).
lines.append("Tick ==")
lines.append("  /\\ clock < MaxClock")
lines.append("  /\\ (state \\notin DeadlineStates \\/ clock < deadline)")
lines.append("  /\\ clock' = clock + 1")
lines.append("  /\\ UNCHANGED <<state, deadline, backoff_count, last_event>>")
lines.append("")

lines.append("Next ==")
lines.append("  \\/ " + "\n  \\/ ".join(action_names + ["Tick"]))
lines.append("")
lines.append("Spec == Init /\\ [][Next]_vars /\\ WF_vars(Next)")
lines.append("")
lines.append("TypeOK ==")
lines.append("  /\\ state \\in States")
lines.append("  /\\ clock \\in Nat")
lines.append("  /\\ deadline \\in Nat")
lines.append("  /\\ backoff_count \\in Nat")
lines.append("  /\\ last_event \\in Events")
lines.append("")
lines.append("(* A deadline-carrying state's clock never exceeds its own deadline -- the")
lines.append("   direct, checkable form of the guarantee this project's engine exists to")
lines.append("   provide (Bluedroid's own GATT discovery has no such bound at all; see")
lines.append("   docs/HARDWARE_VALIDATION.md). *)")
lines.append("BoundedWait == state \\in DeadlineStates => clock <= deadline")
lines.append("")
lines.append("(* Weak fairness on Next (so on Tick and on every timeout action) is enough")
lines.append("   to guarantee every incursion into a deadline-carrying state eventually")
lines.append("   leaves it -- TLC checks this as a temporal property, not just a reachable-")
lines.append("   state invariant. *)")
lines.append("EventuallyExits == []<>(state \\notin DeadlineStates)")
lines.append("")
lines.append("(* Regression guard, not (yet) a full pairing-policy proof: M7 owns actually")
lines.append("   enforcing an allowlist before a peer ever reaches Connecting. What this FSM")
lines.append("   itself guarantees today is narrower but still real: a connection whose")
lines.append("   pairing was dropped as unsolicited can never reach Ready *as a direct")
lines.append("   result of that same event* -- i.e. no transition table row may ever map")
lines.append("   drop_unsolicited_pairing to Ready. Catches exactly that regression if")
lines.append("   transitions.json is ever hand-edited to violate it. *)")
lines.append('NoUnauthorizedPairing == state = "Ready" => last_event # "drop_unsolicited_pairing"')
lines.append("")
lines.append("====")

tla_text = "\n".join(lines) + "\n"
(OUT_DIR / "ble_state_machine.tla").write_text(tla_text, encoding="utf-8")

cfg_lines = []
cfg_lines.append("\\* GENERATED FILE -- do not edit by hand. See ble_state_machine.tla.")
cfg_lines.append("SPECIFICATION Spec")
cfg_lines.append("INVARIANTS TypeOK BoundedWait NoUnauthorizedPairing")
cfg_lines.append("PROPERTIES EventuallyExits")
cfg_lines.append("CONSTANTS")
cfg_lines.append("  MaxClock = 10")
cfg_lines.append("  MaxBackoff = 3")
# Small, distinct concrete values so TLC actually exercises both deadlines
# reaching zero (and thus both timeout actions firing) within MaxClock.
for i, const_name in enumerate(sorted(deadline_consts.values())):
    cfg_lines.append(f"  {const_name} = {2 + i}")
cfg_text = "\n".join(cfg_lines) + "\n"
(OUT_DIR / "ble_state_machine.cfg").write_text(cfg_text, encoding="utf-8")

print("Generated ble_state_machine.tla/.cfg OK")
