import 'package:flutter/material.dart';
import 'glass.dart';
import 'package:url_launcher/url_launcher.dart';

class _NavItem {
  final IconData icon;
  final String tip;
  final bool active;
  final String? url;
  const _NavItem(this.icon, this.tip, {this.active = false, this.url});
}

const List<_NavItem?> _items = [
  _NavItem(Icons.grid_view_rounded, 'Projekte', active: true),
  _NavItem(Icons.schedule, 'Zuletzt'),
  _NavItem(Icons.dashboard_outlined, 'Vorlagen'),
  null, // divider
  _NavItem(Icons.download_rounded, 'Updates'),
  _NavItem(Icons.tune, 'Einstellungen'),
  null, // spacer + divider
  _NavItem(Icons.menu_book_outlined, 'Dokumentation'),
  _NavItem(Icons.code_rounded, 'GitHub',
      url: 'https://github.com/goeki04/andromeda-engine'),
];

class IconBar extends StatelessWidget {
  const IconBar({super.key});

  @override
  Widget build(BuildContext context) {
    // Split into top group and bottom group at the `spacer` marker (index 6)
    final children = <Widget>[];
    for (var i = 0; i < _items.length; i++) {
      final item = _items[i];
      if (item == null) {
        if (i == 6) {
          children.add(const Spacer());
        }
        children.add(const _Divider());
      } else {
        children.add(_IconButton(item: item));
      }
    }

    return GlassContainer(
      width: 64,
      blur: 30,
      color: const Color(0x73060606), // rgba(6,6,6,0.45)
      border: Border(
        right:
            BorderSide(color: Colors.white.withValues(alpha: 0.05), width: 0.5),
      ),
      padding: const EdgeInsets.symmetric(vertical: 16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: children,
      ),
    );
  }
}

class _Divider extends StatelessWidget {
  const _Divider();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 24,
      height: 0.5,
      margin: const EdgeInsets.symmetric(vertical: 6),
      color: Colors.white.withValues(alpha: 0.07),
    );
  }
}

class _IconButton extends StatefulWidget {
  final _NavItem item;
  const _IconButton({required this.item});

  @override
  State<_IconButton> createState() => _IconButtonState();
}

class _IconButtonState extends State<_IconButton> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    const amber = Color(0xFFE8A230);
    final active = widget.item.active;
    final bg = active
        ? amber.withValues(alpha: .12)
        : (_hover ? Colors.white.withValues(alpha: 0.06) : Colors.transparent);
    final fg = active
        ? amber.withValues(alpha: 0.90)
        : Colors.white.withValues(alpha: _hover ? 0.85 : 0.50);

    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: MouseRegion(
        cursor: SystemMouseCursors.click,
        onEnter: (_) => setState(() => _hover = true),
        onExit: (_) => setState(() => _hover = false),
        child: Tooltip(
          message: widget.item.tip,
          waitDuration: const Duration(milliseconds: 400),
          child: GestureDetector(
            onTap: widget.item.url != null
                ? () => launchUrl(Uri.parse(widget.item.url!))
                : null,
            child: Stack(
              alignment: Alignment.centerLeft,
              children: [
                AnimatedContainer(
                  duration: const Duration(milliseconds: 150),
                  width: 38,
                  height: 38,
                  decoration: BoxDecoration(
                    color: bg,
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Icon(widget.item.icon, size: 18, color: fg),
                ),
                if (active)
                  Container(
                    width: 2,
                    height: 20,
                    decoration: BoxDecoration(
                      color: const Color(0xFFE8A230).withValues(alpha: 0.85),
                      borderRadius: const BorderRadius.only(
                        topRight: Radius.circular(2),
                        bottomRight: Radius.circular(2),
                      ),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
