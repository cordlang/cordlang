# Flutter sketch — Counter (hand-written, not codegen)

Espejo manual de [`counter.cord`](./counter.cord). **No** es salida de un backend Cordlang;
sirve como contrato visual del spike nativo ([`docs/NATIVE.md`](../../docs/NATIVE.md)).

```dart
import 'package:flutter/material.dart';

void main() => runApp(const MaterialApp(home: Counter(label: 'Counter')));

class Counter extends StatefulWidget {
  const Counter({super.key, this.label = 'Counter'});
  final String label;

  @override
  State<Counter> createState() => _CounterState();
}

class _CounterState extends State<Counter> {
  int count = 0;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(widget.label, style: Theme.of(context).textTheme.headlineMedium),
            Text('$count', style: Theme.of(context).textTheme.displayMedium),
            Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                OutlinedButton(
                  onPressed: () => setState(() => count--),
                  child: const Text('-'),
                ),
                const SizedBox(width: 8),
                FilledButton(
                  onPressed: () => setState(() => count++),
                  child: const Text('+'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
```

| Cord | Flutter en este boceto |
|------|-------------------------|
| `state count=0` | `int count` + `setState` |
| `col` | `Column` |
| `row` | `Row` |
| `h1` / `span` | `Text` |
| `btn` + `@click=setCount(...)` | `OutlinedButton` / `FilledButton` + `onPressed` |
