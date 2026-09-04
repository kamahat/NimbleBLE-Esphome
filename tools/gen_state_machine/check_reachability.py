"""Post-processes a TLC state-graph dump (spec/states.dot, generated with
`-dump dot,actionlabels`) to independently confirm that no reachable cycle is
confined entirely to a deadline-carrying state (Connecting/Discovering).

This is deliberately redundant with the EventuallyExits temporal property
TLC itself already checks (weak fairness on Next + []<>(state \\notin
DeadlineStates)) -- TLC's own liveness checking is the stronger, more general
technique. This script exists as a second, structurally different check
(plain graph reachability, no fairness assumptions) on the same question, in
the spirit of this project's "prove it, don't just believe it" approach to
verification. If this script and TLC ever disagreed, that would itself be a
bug worth finding.

Usage: python3 check_reachability.py spec/states.dot
"""
import re
import sys

NODE_RE = re.compile(r'^(-?\d+)\s*\[label="((?:[^"\\]|\\.)*)"')
EDGE_RE = re.compile(r'^(-?\d+)\s*->\s*(-?\d+)\s*\[label="([^"]*)"')
DASHED_EDGE_RE = re.compile(r'^(-?\d+)\s*->\s*(-?\d+)\s*\[style="dashed"\]')
STATE_VALUE_RE = re.compile(r'state = "([^"]+)"')

DEADLINE_STATES = {"Connecting", "Discovering"}


def unescape_dot_label(s):
    """DOT quoted-string labels escape embedded quotes as \\" (a literal
    backslash followed by a literal quote character, not an actual quote) --
    NODE_RE's job is only to find where the label ends, not to resolve these,
    so state_value_re must run against the unescaped text or "state = \\"Idle\\""
    never matches a bare state = "([^"]+)"."""
    out = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            out.append("\n" if nxt == "n" else nxt)
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def parse_dot(path):
    node_state = {}
    edges = []  # (src, dst)
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if DASHED_EDGE_RE.match(line):
                continue  # TLC's own stuttering/terminal marker, not a real transition
            m = EDGE_RE.match(line)
            if m:
                edges.append((m.group(1), m.group(2)))
                continue
            m = NODE_RE.match(line)
            if m:
                node_id, label = m.group(1), unescape_dot_label(m.group(2))
                state_m = STATE_VALUE_RE.search(label)
                if state_m:
                    node_state[node_id] = state_m.group(1)
    return node_state, edges


def tarjan_sccs(nodes, edges):
    """Standard Tarjan's algorithm, iterative to avoid recursion-depth limits
    on a state graph that can have thousands of nodes."""
    adj = {n: [] for n in nodes}
    for src, dst in edges:
        if src in adj and dst in adj:
            adj[src].append(dst)

    index_counter = [0]
    index = {}
    lowlink = {}
    on_stack = {}
    stack = []
    sccs = []

    for start in nodes:
        if start in index:
            continue
        work = [(start, iter(adj[start]))]
        index[start] = lowlink[start] = index_counter[0]
        index_counter[0] += 1
        stack.append(start)
        on_stack[start] = True

        while work:
            node, it = work[-1]
            advanced = False
            for succ in it:
                if succ not in index:
                    index[succ] = lowlink[succ] = index_counter[0]
                    index_counter[0] += 1
                    stack.append(succ)
                    on_stack[succ] = True
                    work.append((succ, iter(adj[succ])))
                    advanced = True
                    break
                elif on_stack.get(succ):
                    lowlink[node] = min(lowlink[node], index[succ])
            if advanced:
                continue
            work.pop()
            if work:
                parent = work[-1][0]
                lowlink[parent] = min(lowlink[parent], lowlink[node])
            if lowlink[node] == index[node]:
                scc = []
                while True:
                    w = stack.pop()
                    on_stack[w] = False
                    scc.append(w)
                    if w == node:
                        break
                sccs.append(scc)
    return sccs


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <states.dot>", file=sys.stderr)
        return 2
    node_state, edges = parse_dot(sys.argv[1])
    sccs = tarjan_sccs(list(node_state.keys()), edges)

    confined = []
    for scc in sccs:
        # A single node with no self-loop (already excluded the dashed
        # bookkeeping ones) is not a cycle at all.
        if len(scc) == 1:
            node = scc[0]
            has_self_edge = any(s == node and d == node for s, d in edges)
            if not has_self_edge:
                continue
        states_in_scc = {node_state[n] for n in scc}
        if states_in_scc and states_in_scc.issubset(DEADLINE_STATES):
            confined.append((scc, states_in_scc))

    if confined:
        print(f"FAIL: {len(confined)} cycle(s) confined entirely to {DEADLINE_STATES}:", file=sys.stderr)
        for scc, states_in_scc in confined:
            print(f"  states {states_in_scc}, {len(scc)} node(s)", file=sys.stderr)
        return 1

    print(f"OK: {len(node_state)} states, {len(sccs)} SCC(s) checked, "
          f"no cycle confined to {DEADLINE_STATES}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
