# tools/tla/

Place `tla2tools.jar` here to run TLC locally (not committed -- third-party binary,
downloaded fresh by CI too):

```bash
curl -sL -o tla2tools.jar https://github.com/tlaplus/tlaplus/releases/latest/download/tla2tools.jar
```

Then, from `spec/`:

```bash
java -XX:+UseParallelGC -jar ../tools/tla/tla2tools.jar -config ble_state_machine.cfg ble_state_machine.tla
```
