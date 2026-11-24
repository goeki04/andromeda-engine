import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';

class WindowControls extends StatelessWidget {
  const WindowControls({super.key});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: const [
        _WindowButton(type: 'minimize'),
        _WindowButton(type: 'maximize'),
        _WindowButton(type: 'close'),
      ],
    );
  }
}

class _WindowButton extends StatefulWidget {
  final String type;
  const _WindowButton({required this.type});

  @override
  State<_WindowButton> createState() => _WindowButtonState();
}

class _WindowButtonState extends State<_WindowButton> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    final isClose = widget.type == 'close';

    return MouseRegion(
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: GestureDetector(
        onTap: _onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 100),
          width: 46,
          height: 32,
          color: _hover
              ? (isClose ? const Color(0xFFC42B1C) : const Color(0x1AFFFFFF))
              : Colors.transparent,
          child: Center(
            child: CustomPaint(
              size: const Size(10, 10),
              painter: _WinIconPainter(
                type: widget.type,
                color:
                    _hover && isClose ? Colors.white : const Color(0xCCFFFFFF),
              ),
            ),
          ),
        ),
      ),
    );
  }

  void _onTap() async {
    switch (widget.type) {
      case 'close':
        await windowManager.close();
        break;
      case 'minimize':
        await windowManager.minimize();
        break;
      case 'maximize':
        final isMaximized = await windowManager.isMaximized();
        if (isMaximized) {
          await windowManager.unmaximize();
        } else {
          await windowManager.maximize();
        }
        break;
    }
  }
}

class _WinIconPainter extends CustomPainter {
  final String type;
  final Color color;

  const _WinIconPainter({required this.type, required this.color});

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = 1.0
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.square;

    final cy = size.height / 2;

    switch (type) {
      case 'minimize':
        // Horizontal line centered
        canvas.drawLine(
          Offset(0, cy),
          Offset(size.width, cy),
          paint,
        );
        break;
      case 'maximize':
        // Square (restore icon — simple square outline)
        canvas.drawRect(Rect.fromLTWH(0, 0, size.width, size.height), paint);
        break;
      case 'close':
        // X
        canvas.drawLine(Offset(0, 0), Offset(size.width, size.height), paint);
        canvas.drawLine(Offset(size.width, 0), Offset(0, size.height), paint);
        break;
    }
  }

  @override
  bool shouldRepaint(_WinIconPainter old) =>
      old.type != type || old.color != color;
}
