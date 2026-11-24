import 'dart:math' as math;
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';

/// Single nebula orb definition.
class Orb {
  final double ox, oy, r, speed, pull;
  final int c; // grayscale brightness 0..255
  const Orb(this.ox, this.oy, this.r, this.c, this.speed, this.pull);
}

const List<Orb> _orbs = [
  Orb(0.15, 0.20, 0.45, 80, 0.6, 0.35),
  Orb(0.75, 0.55, 0.38, 55, 0.5, -0.28),
  Orb(0.45, 0.85, 0.32, 100, 0.7, 0.22),
  Orb(0.90, 0.15, 0.28, 40, 0.4, -0.18),
];

/// Mouse-reactive animated background. Runs its own ticker so only the
/// CustomPaint repaints each frame — the UI on top is never rebuilt.
class AnimatedOrbBackground extends StatefulWidget {
  final ValueListenable<Offset> target;
  const AnimatedOrbBackground({super.key, required this.target});

  @override
  State<AnimatedOrbBackground> createState() => _AnimatedOrbBackgroundState();
}

class _AnimatedOrbBackgroundState extends State<AnimatedOrbBackground>
    with SingleTickerProviderStateMixin {
  late final Ticker _ticker;
  final ValueNotifier<_Frame> _frame =
      ValueNotifier(const _Frame(0, Offset(0.5, 0.4)));

  Offset _smooth = const Offset(0.5, 0.4);
  double _t = 0;

  @override
  void initState() {
    super.initState();
    _ticker = createTicker(_onTick)..start();
  }

  void _onTick(Duration _) {
    final target = widget.target.value;
    // Ease the smoothed position toward the target (matches the JS 0.04 factor)
    _smooth = Offset(
      _smooth.dx + (target.dx - _smooth.dx) * 0.04,
      _smooth.dy + (target.dy - _smooth.dy) * 0.04,
    );
    _t += 0.008;
    _frame.value = _Frame(_t, _smooth);
  }

  @override
  void dispose() {
    _ticker.dispose();
    _frame.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ValueListenableBuilder<_Frame>(
      valueListenable: _frame,
      builder: (context, frame, _) => CustomPaint(
        painter: _OrbPainter(frame.t, frame.smooth),
        size: Size.infinite,
      ),
    );
  }
}

class _Frame {
  final double t;
  final Offset smooth;
  const _Frame(this.t, this.smooth);
}

class _OrbPainter extends CustomPainter {
  final double t;
  final Offset smooth;
  _OrbPainter(this.t, this.smooth);

  @override
  void paint(Canvas canvas, Size size) {
    final w = size.width;
    final h = size.height;

    // Dark base
    canvas.drawRect(
      Offset.zero & size,
      Paint()..color = const Color(0xFF242424),
    );

    for (final o in _orbs) {
      final px = (o.ox +
              math.sin(t * o.speed + o.ox * 5) * 0.08 +
              (smooth.dx - 0.5) * o.pull) *
          w;
      final py = (o.oy +
              math.cos(t * o.speed + o.oy * 4) * 0.07 +
              (smooth.dy - 0.5) * o.pull) *
          h;
      final rr = o.r * math.min(w, h);
      final rect = Rect.fromCircle(center: Offset(px, py), radius: rr);

      final paint = Paint()
        ..shader = RadialGradient(
          colors: [
            Color.fromRGBO(o.c, o.c, o.c, 0.55),
            Color.fromRGBO(o.c, o.c, o.c, 0.18),
            Color.fromRGBO(o.c, o.c, o.c, 0.0),
          ],
          stops: const [0.0, 0.4, 1.0],
        ).createShader(rect);

      canvas.drawCircle(Offset(px, py), rr, paint);
    }
  }

  @override
  bool shouldRepaint(_OrbPainter old) => old.t != t || old.smooth != smooth;
}
