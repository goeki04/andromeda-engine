import 'dart:convert';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

class Project {
  final String name;
  final String path;
  final DateTime lastOpened;

  Project({
    required this.name,
    required this.path,
    required this.lastOpened,
  });

  factory Project.fromJson(Map<String, dynamic> json) => Project(
        name: json['name'] as String,
        path: json['path'] as String,
        lastOpened: DateTime.parse(json['lastOpened'] as String),
      );

  Map<String, dynamic> toJson() => {
        'name': name,
        'path': path,
        'lastOpened': lastOpened.toIso8601String(),
      };

  Project copyWith({DateTime? lastOpened}) => Project(
        name: name,
        path: path,
        lastOpened: lastOpened ?? this.lastOpened,
      );

  IconData get icon => Icons.view_in_ar_outlined;
}

class ProjectService extends ChangeNotifier {
  static final ProjectService instance = ProjectService._();
  ProjectService._();

  List<Project> _projects = [];
  List<Project> get projects => List.unmodifiable(_projects);

  List<Project> get recent {
    final sorted = [..._projects]
      ..sort((a, b) => b.lastOpened.compareTo(a.lastOpened));
    return sorted.take(3).toList();
  }

  Future<File> _file() async {
    final dir = await getApplicationSupportDirectory();
    return File('${dir.path}/projects.json');
  }

  Future<void> load() async {
    try {
      final f = await _file();
      if (!await f.exists()) return;
      final data = jsonDecode(await f.readAsString()) as List;
      _projects =
          data.map((e) => Project.fromJson(e as Map<String, dynamic>)).toList();
      notifyListeners();
    } catch (_) {}
  }

  Future<void> _save() async {
    final f = await _file();
    await f
        .writeAsString(jsonEncode(_projects.map((p) => p.toJson()).toList()));
  }

  Future<void> add(String path) async {
    final name = path.split(Platform.pathSeparator).last;
    if (_projects.any((p) => p.path == path)) return;
    _projects.add(Project(name: name, path: path, lastOpened: DateTime.now()));
    notifyListeners();
    await _save();
  }

  Future<void> create(String name, String parentPath) async {
    final projectPath = '$parentPath${Platform.pathSeparator}$name';
    final dir = Directory(projectPath);
    if (!await dir.exists()) await dir.create(recursive: true);
    final projectFile =
        File('$projectPath${Platform.pathSeparator}project.json');
    await projectFile.writeAsString(jsonEncode({'name': name}));
    if (_projects.any((p) => p.path == projectPath)) return;
    _projects.add(
        Project(name: name, path: projectPath, lastOpened: DateTime.now()));
    notifyListeners();
    await _save();
  }

  Future<void> open(Project project) async {
    final idx = _projects.indexWhere((p) => p.path == project.path);
    if (idx == -1) return;
    _projects[idx] = project.copyWith(lastOpened: DateTime.now());
    notifyListeners();
    await _save();
  }

  Future<void> remove(Project project) async {
    _projects.removeWhere((p) => p.path == project.path);
    notifyListeners();
    await _save();
  }

  /// Deletes a project. Always removes it from the launcher list; when
  /// [deleteFiles] is true the entire project folder is also removed from disk.
  Future<void> delete(Project project, {bool deleteFiles = false}) async {
    if (deleteFiles) {
      try {
        final dir = Directory(project.path);
        if (await dir.exists()) {
          await dir.delete(recursive: true);
        }
      } catch (_) {}
    }
    _projects.removeWhere((p) => p.path == project.path);
    notifyListeners();
    await _save();
  }
}
