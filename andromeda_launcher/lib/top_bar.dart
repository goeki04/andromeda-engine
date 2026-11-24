import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';
import 'glass.dart';
import 'window_buttons.dart';

class TopBar extends StatelessWidget {
  const TopBar({super.key});

  @override
  Widget build(BuildContext context) {
    return GlassContainer(
      height: 48,
      blur: 30,
      color: const Color(0x66080808),
      border: Border(
        bottom:
            BorderSide(color: Colors.white.withValues(alpha: 0.06), width: 0.5),
      ),
      padding: EdgeInsets.zero,
      child: Stack(
        children: [
          Positioned.fill(
            child: DragToMoveArea(child: const SizedBox.expand()),
          ),
          Positioned.fill(
            child: Row(
              children: [
                const SizedBox(width: 24),
                const _LogoMark(size: 20),
                const Spacer(),
                const _TopIcon(Icons.notifications_none_rounded),
                const SizedBox(width: 14),
                const _TopIcon(Icons.settings_outlined),
                const SizedBox(width: 14),
                const _Avatar(),
                const SizedBox(width: 16),
                const WindowControls(),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _TopIcon extends StatefulWidget {
  final IconData icon;
  const _TopIcon(this.icon);

  @override
  State<_TopIcon> createState() => _TopIconState();
}

class _TopIconState extends State<_TopIcon> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: Icon(
        widget.icon,
        size: 20,
        color: Colors.white.withValues(alpha: _hover ? 0.90 : 0.55),
      ),
    );
  }
}

class _Avatar extends StatelessWidget {
  const _Avatar();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 26,
      height: 26,
      decoration: BoxDecoration(
        shape: BoxShape.circle,
        color: Colors.white.withValues(alpha: 0.05),
        border: Border.all(
          color: Colors.white.withValues(alpha: 0.12),
          width: 0.5,
        ),
      ),
      child: Icon(
        Icons.person_outline,
        size: 12,
        color: Colors.white.withValues(alpha: 0.60),
      ),
    );
  }
}

class _LogoMark extends StatelessWidget {
  final double size;
  const _LogoMark({required this.size});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: size,
      height: size,
      child: CustomPaint(painter: _LogoPainter()),
    );
  }
}

class _LogoPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final scale = size.width / 20.0;
    final c = Offset(size.width / 2, size.height / 2);

    Paint stroke(double o) => Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 0.5 * scale
      ..color = Colors.white.withValues(alpha: o);

    canvas.drawCircle(c, 8 * scale, stroke(0.40));
    canvas.drawCircle(c, 4 * scale, stroke(0.25));
    canvas.drawCircle(
      c,
      1.5 * scale,
      Paint()..color = const Color(0xFFE8A230),
    );

    final path = Path()
      ..moveTo(10 * scale, 2 * scale)
      ..quadraticBezierTo(14 * scale, 6 * scale, 14 * scale, 10 * scale)
      ..quadraticBezierTo(14 * scale, 14 * scale, 10 * scale, 18 * scale);
    canvas.drawPath(path, stroke(0.25));
  }

  @override
  bool shouldRepaint(_) => false;
}
