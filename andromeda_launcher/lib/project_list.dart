import 'dart:io';
import 'dart:ui' show ImageFilter;
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'project_service.dart';
import 'create_project_dialog.dart';

class ProjectList extends StatefulWidget {
  const ProjectList({super.key});

  @override
  State<ProjectList> createState() => _ProjectListState();
}

class _ProjectListState extends State<ProjectList> {
  final ScrollController _controller = ScrollController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  Future<void> _import() async {
    final result = await FilePicker.platform.getDirectoryPath(
      dialogTitle: 'Projektordner auswählen',
    );
    if (result != null) {
      await ProjectService.instance.add(result);
    }
  }

  Future<void> _create() async {
    final result = await showDialog<Map<String, String>>(
      context: context,
      builder: (context) => const CreateProjectDialog(),
    );
    if (result != null) {
      await ProjectService.instance.create(result['name']!, result['path']!);
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: ProjectService.instance,
      builder: (context, _) {
        final recent = ProjectService.instance.recent;
        final all = ProjectService.instance.projects;

        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _Hero(
              onImport: _import,
              onCreate: _create,
            ),
            Container(
              height: 0.5,
              margin: const EdgeInsets.symmetric(horizontal: 22),
              color: Colors.white.withValues(alpha: 0.18),
            ),
            Expanded(
              child: all.isEmpty
                  ? _EmptyState(
                      onImport: _import,
                      onCreate: _create,
                    )
                  : RawScrollbar(
                      controller: _controller,
                      thumbColor: Colors.white.withValues(alpha: 0.10),
                      thickness: 3,
                      radius: const Radius.circular(99),
                      child: ListView(
                        controller: _controller,
                        padding: const EdgeInsets.fromLTRB(28, 14, 28, 20),
                        children: [
                          if (recent.isNotEmpty) ...[
                            const _SectionLabel('Zuletzt geöffnet'),
                            ...recent.map((p) => _ProjectRow(project: p)),
                            const SizedBox(height: 20),
                          ],
                          const _SectionLabel('Alle Projekte'),
                          ...all.map((p) => _ProjectRow(project: p)),
                        ],
                      ),
                    ),
            ),
          ],
        );
      },
    );
  }
}

class _Hero extends StatelessWidget {
  final VoidCallback onImport;
  final VoidCallback onCreate;
  const _Hero({required this.onImport, required this.onCreate});

  @override
  Widget build(BuildContext context) {
    final count = ProjectService.instance.projects.length;
    return Padding(
      padding: const EdgeInsets.fromLTRB(28, 28, 28, 20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'WILLKOMMEN ZURÜCK',
            style: TextStyle(
              fontSize: 10,
              letterSpacing: 1.8,
              color: Colors.white.withValues(alpha: 0.42),
            ),
          ),
          const SizedBox(height: 8),
          RichText(
            text: TextSpan(
              style: const TextStyle(
                  fontSize: 26, fontWeight: FontWeight.w200, height: 1.2),
              children: [
                TextSpan(
                  text: 'Deine Projekte\n',
                  style: TextStyle(color: Colors.white.withValues(alpha: 0.82)),
                ),
                TextSpan(
                  text: count == 0
                      ? 'Noch keine Projekte'
                      : '$count ${count == 1 ? 'Projekt' : 'Projekte'}',
                  style: TextStyle(
                    color: Colors.white.withValues(alpha: 0.55),
                    fontSize: 14,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          Row(
            children: [
              _HeroButton(
                label: 'Neues Projekt',
                icon: Icons.add,
                filled: true,
                onTap: onCreate,
              ),
              const SizedBox(width: 8),
              _HeroButton(
                label: 'Importieren',
                filled: false,
                onTap: onImport,
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _EmptyState extends StatelessWidget {
  final VoidCallback onImport;
  final VoidCallback onCreate;
  const _EmptyState({required this.onImport, required this.onCreate});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Icon(Icons.folder_open_outlined,
              size: 40, color: Colors.white.withValues(alpha: 0.20)),
          const SizedBox(height: 14),
          Text(
            'Noch keine Projekte',
            style: TextStyle(
                fontSize: 14, color: Colors.white.withValues(alpha: 0.40)),
          ),
          const SizedBox(height: 6),
          Text(
            'Importiere einen vorhandenen Projektordner',
            style: TextStyle(
                fontSize: 11, color: Colors.white.withValues(alpha: 0.28)),
          ),
          const SizedBox(height: 20),
          _HeroButton(label: 'Importieren', filled: true, onTap: onImport),
        ],
      ),
    );
  }
}

class _HeroButton extends StatefulWidget {
  final String label;
  final IconData? icon;
  final bool filled;
  final VoidCallback onTap;
  const _HeroButton(
      {required this.label,
      required this.filled,
      required this.onTap,
      this.icon});

  @override
  State<_HeroButton> createState() => _HeroButtonState();
}

class _HeroButtonState extends State<_HeroButton> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    const amber = Color(0xFFE8A230);
    final filled = widget.filled;
    final bg = filled
        ? amber.withValues(alpha: _hover ? 0.18 : 0.11)
        : Colors.transparent;
    final borderColor = filled
        ? amber.withValues(alpha: _hover ? 0.45 : 0.30)
        : Colors.white.withValues(alpha: _hover ? 0.14 : 0.08);
    final fgColor = filled
        ? amber.withValues(alpha: _hover ? 1.0 : 0.88)
        : Colors.white.withValues(alpha: _hover ? 0.72 : 0.55);

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: GestureDetector(
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 150),
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 7),
          decoration: BoxDecoration(
            color: bg,
            borderRadius: BorderRadius.circular(99),
            border: Border.all(color: borderColor, width: 0.5),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (widget.icon != null) ...[
                Icon(widget.icon, size: 12, color: fgColor),
                const SizedBox(width: 5),
              ],
              Text(
                widget.label,
                style: TextStyle(fontSize: 11.5, color: fgColor),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _SectionLabel extends StatelessWidget {
  final String text;
  const _SectionLabel(this.text);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 10),
      child: Text(
        text.toUpperCase(),
        style: TextStyle(
          fontSize: 9.5,
          letterSpacing: 1.4,
          color: Colors.white.withValues(alpha: 0.38),
        ),
      ),
    );
  }
}

class _ProjectRow extends StatefulWidget {
  final Project project;
  const _ProjectRow({required this.project});

  @override
  State<_ProjectRow> createState() => _ProjectRowState();
}

class _ProjectRowState extends State<_ProjectRow> {
  bool _hover = false;

  String get _meta {
    final diff = DateTime.now().difference(widget.project.lastOpened);
    if (diff.inMinutes < 60) return 'vor ${diff.inMinutes} Min.';
    if (diff.inHours < 24) return 'vor ${diff.inHours} Std.';
    if (diff.inDays == 1) return 'gestern';
    return 'vor ${diff.inDays} Tagen';
  }

  Future<void> _launch() async {
    await ProjectService.instance.open(widget.project);
    try {
      await Process.start(
        r'..\Andromeda\conan\build\Release\Andromeda.exe',
        ['-project', widget.project.path],
      );
    } catch (_) {}
  }

  Future<void> _showContextMenu(Offset globalPos) async {
    final overlay = Overlay.of(context).context.findRenderObject() as RenderBox;
    final selected = await showMenu<String>(
      context: context,
      position: RelativeRect.fromRect(
        Rect.fromLTWH(globalPos.dx, globalPos.dy, 0, 0),
        Offset.zero & overlay.size,
      ),
      items: const [
        PopupMenuItem<String>(
          value: 'remove',
          child: Row(
            children: [
              Icon(Icons.playlist_remove, size: 16, color: Colors.white70),
              SizedBox(width: 8),
              Text('Aus Liste entfernen'),
            ],
          ),
        ),
        PopupMenuItem<String>(
          value: 'delete',
          child: Row(
            children: [
              Icon(Icons.delete_outline, size: 16, color: Colors.redAccent),
              SizedBox(width: 8),
              Text('Komplett löschen'),
            ],
          ),
        ),
      ],
    );
    if (selected == 'remove') {
      await ProjectService.instance.remove(widget.project);
    } else if (selected == 'delete') {
      await _confirmDelete();
    }
  }

  Future<void> _confirmDelete() async {
    if (!mounted) return;
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Projekt löschen?'),
        content: Text(
          '„${widget.project.name}" wird endgültig gelöscht. Der gesamte '
          'Projektordner inklusive aller Dateien wird vom Datenträger '
          'entfernt. Das kann nicht rückgängig gemacht werden.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Abbrechen'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Löschen',
                style: TextStyle(color: Colors.redAccent)),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      await ProjectService.instance.delete(widget.project, deleteFiles: true);
    }
  }

  @override
  Widget build(BuildContext context) {
    final p = widget.project;
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: MouseRegion(
        cursor: SystemMouseCursors.click,
        onEnter: (_) => setState(() => _hover = true),
        onExit: (_) => setState(() => _hover = false),
        child: GestureDetector(
          onTap: _launch,
          onSecondaryTapDown: (d) => _showContextMenu(d.globalPosition),
          onLongPressStart: (d) => _showContextMenu(d.globalPosition),
          child: AnimatedContainer(
            duration: const Duration(milliseconds: 150),
            transform: Matrix4.translationValues(_hover ? 2 : 0, 0, 0),
            child: ClipRRect(
              borderRadius: BorderRadius.circular(10),
              child: BackdropFilter(
                filter: ImageFilter.blur(sigmaX: 8, sigmaY: 8),
                child: Container(
                  padding:
                      const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
                  decoration: BoxDecoration(
                    color: Colors.white.withValues(alpha: _hover ? 0.08 : 0.04),
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(
                      color:
                          Colors.white.withValues(alpha: _hover ? 0.15 : 0.07),
                      width: 0.5,
                    ),
                  ),
                  child: Row(
                    children: [
                      Container(
                        width: 34,
                        height: 34,
                        decoration: BoxDecoration(
                          color: Colors.white.withValues(alpha: 0.05),
                          borderRadius: BorderRadius.circular(8),
                          border: Border.all(
                              color: Colors.white.withValues(alpha: 0.08),
                              width: 0.5),
                        ),
                        child: Icon(p.icon,
                            size: 16,
                            color: Colors.white.withValues(alpha: 0.62)),
                      ),
                      const SizedBox(width: 14),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            Text(
                              p.name,
                              overflow: TextOverflow.ellipsis,
                              style: TextStyle(
                                fontSize: 12.5,
                                color: Colors.white.withValues(alpha: 0.88),
                              ),
                            ),
                            const SizedBox(height: 2),
                            Text(
                              _meta,
                              style: TextStyle(
                                fontSize: 10.5,
                                color: Colors.white.withValues(alpha: 0.48),
                              ),
                            ),
                          ],
                        ),
                      ),
                      Icon(
                        Icons.chevron_right,
                        size: 16,
                        color: Colors.white
                            .withValues(alpha: _hover ? 0.65 : 0.32),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
