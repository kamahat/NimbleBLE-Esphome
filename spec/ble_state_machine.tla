---- MODULE ble_state_machine ----
(* GENERATED FILE -- do not edit by hand.
   Source of truth: spec/transitions.json, via tools/gen_state_machine/gen_tla.py.
   Sibling generator: tools/gen_state_machine/gen_cpp.py produces the C++ engine
   (components/nimble_ble/ble_connection_fsm.h/.cpp) from the same spec. *)
EXTENDS Naturals

CONSTANTS MaxClock, MaxBackoff, Deadline_CONNECT_REQUEST, Deadline_GAP_CONNECT_OK

VARIABLES state, clock, deadline, backoff_count, last_event

vars == <<state, clock, deadline, backoff_count, last_event>>

States == {"Idle", "Scanning", "Connecting", "Discovering", "Ready", "Disconnecting", "Backoff"}
Events == {"scan_start", "adv_matched", "connect_request", "gap_connect_ok", "gap_connect_fail", "connect_timeout", "discover_start", "discover_done", "discover_timeout", "disconnect_request", "gap_disconnect_evt", "backoff_elapsed", "drop_unsolicited_pairing"} \union {"none"}
DeadlineStates == {"Connecting", "Discovering"}

Init ==
  /\ state = "Idle"
  /\ clock = 0
  /\ deadline = 0
  /\ backoff_count = 0
  /\ last_event = "none"

ScanStartFromIdle ==
  /\ state = "Idle"
  /\ state' = "Scanning"
  /\ deadline' = 0
  /\ last_event' = "scan_start"
  /\ UNCHANGED <<clock, backoff_count>>

AdvMatchedFromScanning ==
  /\ state = "Scanning"
  /\ state' = "Idle"
  /\ deadline' = 0
  /\ last_event' = "adv_matched"
  /\ UNCHANGED <<clock, backoff_count>>

ConnectRequestFromIdle ==
  /\ state = "Idle"
  /\ state' = "Connecting"
  /\ deadline' = clock + Deadline_CONNECT_REQUEST
  /\ last_event' = "connect_request"
  /\ UNCHANGED <<clock, backoff_count>>

GapConnectOkFromConnecting ==
  /\ state = "Connecting"
  /\ state' = "Discovering"
  /\ deadline' = clock + Deadline_GAP_CONNECT_OK
  /\ last_event' = "gap_connect_ok"
  /\ UNCHANGED <<clock, backoff_count>>

GapConnectFailFromConnecting ==
  /\ state = "Connecting"
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "gap_connect_fail"
  /\ UNCHANGED <<clock>>

ConnectTimeoutFromConnecting ==
  /\ state = "Connecting"
  /\ clock >= deadline
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "connect_timeout"
  /\ UNCHANGED <<clock>>

DiscoverDoneFromDiscovering ==
  /\ state = "Discovering"
  /\ state' = "Ready"
  /\ deadline' = 0
  /\ backoff_count' = 0
  /\ last_event' = "discover_done"
  /\ UNCHANGED <<clock>>

DiscoverTimeoutFromDiscovering ==
  /\ state = "Discovering"
  /\ clock >= deadline
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "discover_timeout"
  /\ UNCHANGED <<clock>>

GapDisconnectEvtFromDiscovering ==
  /\ state = "Discovering"
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "gap_disconnect_evt"
  /\ UNCHANGED <<clock>>

DisconnectRequestFromReady ==
  /\ state = "Ready"
  /\ state' = "Disconnecting"
  /\ deadline' = 0
  /\ last_event' = "disconnect_request"
  /\ UNCHANGED <<clock, backoff_count>>

GapDisconnectEvtFromReady ==
  /\ state = "Ready"
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "gap_disconnect_evt"
  /\ UNCHANGED <<clock>>

GapDisconnectEvtFromDisconnecting ==
  /\ state = "Disconnecting"
  /\ state' = "Idle"
  /\ deadline' = 0
  /\ last_event' = "gap_disconnect_evt"
  /\ UNCHANGED <<clock, backoff_count>>

BackoffElapsedFromBackoff ==
  /\ state = "Backoff"
  /\ state' = "Idle"
  /\ deadline' = 0
  /\ last_event' = "backoff_elapsed"
  /\ UNCHANGED <<clock, backoff_count>>

DropUnsolicitedPairingFromConnecting ==
  /\ state = "Connecting"
  /\ backoff_count < MaxBackoff
  /\ state' = "Backoff"
  /\ deadline' = 0
  /\ backoff_count' = backoff_count + 1
  /\ last_event' = "drop_unsolicited_pairing"
  /\ UNCHANGED <<clock>>

Tick ==
  /\ clock < MaxClock
  /\ (state \notin DeadlineStates \/ clock < deadline)
  /\ clock' = clock + 1
  /\ UNCHANGED <<state, deadline, backoff_count, last_event>>

Next ==
  \/ ScanStartFromIdle
  \/ AdvMatchedFromScanning
  \/ ConnectRequestFromIdle
  \/ GapConnectOkFromConnecting
  \/ GapConnectFailFromConnecting
  \/ ConnectTimeoutFromConnecting
  \/ DiscoverDoneFromDiscovering
  \/ DiscoverTimeoutFromDiscovering
  \/ GapDisconnectEvtFromDiscovering
  \/ DisconnectRequestFromReady
  \/ GapDisconnectEvtFromReady
  \/ GapDisconnectEvtFromDisconnecting
  \/ BackoffElapsedFromBackoff
  \/ DropUnsolicitedPairingFromConnecting
  \/ Tick

Spec == Init /\ [][Next]_vars /\ WF_vars(Next)

TypeOK ==
  /\ state \in States
  /\ clock \in Nat
  /\ deadline \in Nat
  /\ backoff_count \in Nat
  /\ last_event \in Events

(* A deadline-carrying state's clock never exceeds its own deadline -- the
   direct, checkable form of the guarantee this project's engine exists to
   provide (Bluedroid's own GATT discovery has no such bound at all; see
   docs/HARDWARE_VALIDATION.md). *)
BoundedWait == state \in DeadlineStates => clock <= deadline

(* Weak fairness on Next (so on Tick and on every timeout action) is enough
   to guarantee every incursion into a deadline-carrying state eventually
   leaves it -- TLC checks this as a temporal property, not just a reachable-
   state invariant. *)
EventuallyExits == []<>(state \notin DeadlineStates)

(* Regression guard, not (yet) a full pairing-policy proof: M7 owns actually
   enforcing an allowlist before a peer ever reaches Connecting. What this FSM
   itself guarantees today is narrower but still real: a connection whose
   pairing was dropped as unsolicited can never reach Ready *as a direct
   result of that same event* -- i.e. no transition table row may ever map
   drop_unsolicited_pairing to Ready. Catches exactly that regression if
   transitions.json is ever hand-edited to violate it. *)
NoUnauthorizedPairing == state = "Ready" => last_event # "drop_unsolicited_pairing"

====
