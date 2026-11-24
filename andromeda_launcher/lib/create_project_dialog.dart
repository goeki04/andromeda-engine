import 'dart:ui' show ImageFilter;
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';

class CreateProjectDialog extends StatefulWidget {
  const CreateProjectDialog({super.key});

  @override
  State<CreateProjectDialog> createState() => _CreateProjectDialogState();
}

class _CreateProjectDialogState extends State<CreateProjectDialog> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _pathController = TextEditingController();
  bool _nameError = false;
  bool _pathError = false;

  @override
  void dispose() {
    _nameController.dispose();
    _pathController.dispose();
    super.dispose();
  }

  Future<void> _selectFolder() async {
    final String? path = await FilePicker.platform.getDirectoryPath(
      dialogTitle: 'Projektordner auswählen',
    );
    if (path != null) {
      setState(() {
        _pathController.text = path;
        _pathError = false;
      });
    }
  }

  void _submit() {
    final name = _nameController.text.trim();
    final path = _pathController.text.trim();
    setState(() {
      _nameError = name.isEmpty;
      _pathError = path.isEmpty;
    });
    if (name.isNotEmpty && path.isNotEmpty) {
      Navigator.of(context).pop({'name': name, 'path': path});
    }
  }

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: Colors.transparent,
      elevation: 0,
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 24, sigmaY: 24),
          child: Container(
            width: 420,
            padding: const EdgeInsets.all(28),
            decoration: BoxDecoration(
              color: Colors.white.withValues(alpha: 0.05),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(
                color: Colors.white.withValues(alpha: 0.10),
                width: 0.5,
              ),
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Header
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'NEUES PROJEKT',
                      style: TextStyle(
                        fontSize: 9.5,
                        letterSpacing: 1.8,
                        color: Colors.white.withValues(alpha: 0.60),
                      ),
                    ),
                    const SizedBox(height: 4),
                    Text(
                      'Projekt erstellen',
                      style: TextStyle(
                        fontSize: 15,
                        fontWeight: FontWeight.w300,
                        color: Colors.white.withValues(alpha: 0.85),
                      ),
                    ),
                  ],
                ),

                const SizedBox(height: 24),

                // Divider
                Container(
                  height: 0.5,
                  color: Colors.white.withValues(alpha: 0.08),
                ),

                const SizedBox(height: 24),

                // Name field
                _FieldLabel('Projektname'),
                const SizedBox(height: 6),
                _GlassTextField(
                  controller: _nameController,
                  hint: 'Mein neues Projekt',
                  hasError: _nameError,
                  onChanged: (_) => setState(() => _nameError = false),
                ),

                const SizedBox(height: 16),

                // Path field
                _FieldLabel('Projektpfad'),
                const SizedBox(height: 6),
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  children: [
                    Expanded(
                      child: _GlassTextField(
                        controller: _pathController,
                        hint: r'C:\Users\...',
                        hasError: _pathError,
                        onChanged: (_) => setState(() => _pathError = false),
                      ),
                    ),
                    const SizedBox(width: 8),
                    _FolderButton(onTap: _selectFolder),
                  ],
                ),

                const SizedBox(height: 28),

                // Actions
                Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    _DialogButton(
                      label: 'Abbrechen',
                      filled: false,
                      onTap: () => Navigator.of(context).pop(),
                    ),
                    const SizedBox(width: 8),
                    _DialogButton(
                      label: 'Erstellen',
                      filled: true,
                      onTap: _submit,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _FieldLabel extends StatelessWidget {
  final String text;
  const _FieldLabel(this.text);

  @override
  Widget build(BuildContext context) {
    return Text(
      text,
      style: TextStyle(
        fontSize: 10,
        letterSpacing: 0.6,
        color: Colors.white.withValues(alpha: 0.65),
      ),
    );
  }
}

class _GlassTextField extends StatefulWidget {
  final TextEditingController controller;
  final String hint;
  final bool hasError;
  final ValueChanged<String>? onChanged;

  const _GlassTextField({
    required this.controller,
    required this.hint,
    this.hasError = false,
    this.onChanged,
  });

  @override
  State<_GlassTextField> createState() => _GlassTextFieldState();
}

class _GlassTextFieldState extends State<_GlassTextField> {
  bool _focused = false;

  @override
  Widget build(BuildContext context) {
    final borderColor = widget.hasError
        ? Colors.redAccent.withValues(alpha: 0.60)
        : _focused
            ? const Color(0xFFE8A230).withValues(alpha: 0.50)
            : Colors.white.withValues(alpha: 0.10);

    return AnimatedContainer(
      duration: const Duration(milliseconds: 150),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: _focused ? 0.07 : 0.04),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: borderColor, width: 0.5),
      ),
      child: Focus(
        onFocusChange: (v) => setState(() => _focused = v),
        child: TextField(
          controller: widget.controller,
          onChanged: widget.onChanged,
          style: TextStyle(
            fontSize: 12.5,
            color: Colors.white.withValues(alpha: 0.95),
          ),
          decoration: InputDecoration(
            hintText: widget.hint,
            hintStyle: TextStyle(
              fontSize: 12.5,
              color: Colors.white.withValues(alpha: 0.45),
            ),
            contentPadding:
                const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
            border: InputBorder.none,
            isDense: true,
          ),
        ),
      ),
    );
  }
}

class _FolderButton extends StatefulWidget {
  final VoidCallback onTap;
  const _FolderButton({required this.onTap});

  @override
  State<_FolderButton> createState() => _FolderButtonState();
}

class _FolderButtonState extends State<_FolderButton> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: GestureDetector(
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 150),
          width: 38,
          height: 38,
          decoration: BoxDecoration(
            color: Colors.white.withValues(alpha: _hover ? 0.10 : 0.05),
            borderRadius: BorderRadius.circular(10),
            border: Border.all(
              color: Colors.white.withValues(alpha: _hover ? 0.18 : 0.10),
              width: 0.5,
            ),
          ),
          child: Icon(
            Icons.folder_open_rounded,
            size: 16,
            color: Colors.white.withValues(alpha: _hover ? 0.95 : 0.72),
          ),
        ),
      ),
    );
  }
}

class _DialogButton extends StatefulWidget {
  final String label;
  final bool filled;
  final VoidCallback onTap;
  const _DialogButton(
      {required this.label, required this.filled, required this.onTap});

  @override
  State<_DialogButton> createState() => _DialogButtonState();
}

class _DialogButtonState extends State<_DialogButton> {
  bool _hover = false;

  @override
  Widget build(BuildContext context) {
    const amber = Color(0xFFE8A230);
    final filled = widget.filled;
    final bg = filled
        ? amber.withValues(alpha: _hover ? 0.18 : 0.11)
        : Colors.transparent;
    final fgColor = filled
        ? amber.withValues(alpha: _hover ? 1.0 : 0.88)
        : Colors.white.withValues(alpha: _hover ? 0.85 : 0.68);
    final borderColor = filled
        ? amber.withValues(alpha: _hover ? 0.45 : 0.30)
        : Colors.white.withValues(alpha: _hover ? 0.14 : 0.08);

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hover = true),
      onExit: (_) => setState(() => _hover = false),
      child: GestureDetector(
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 150),
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 8),
          decoration: BoxDecoration(
            color: bg,
            borderRadius: BorderRadius.circular(99),
            border: Border.all(color: borderColor, width: 0.5),
          ),
          child: Text(
            widget.label,
            style: TextStyle(fontSize: 11.5, color: fgColor),
          ),
        ),
      ),
    );
  }
}
