import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:window_manager/window_manager.dart';
import 'top_bar.dart';
import 'icon_bar.dart';
import 'project_list.dart';
import 'project_service.dart';
import 'side_panel.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await windowManager.ensureInitialized();
  await ProjectService.instance.load();
  const WindowOptions options = WindowOptions(
      size: Size(1200, 800),
      minimumSize: Size(600, 400),
      center: true,
      titleBarStyle: TitleBarStyle.hidden);
  await windowManager.waitUntilReadyToShow(
    options,
    () async {
      await windowManager.show();
      await windowManager.focus();
    },
  );

  runApp(const AndromedaApp());
}

class AndromedaApp extends StatelessWidget {
  const AndromedaApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Andromeda Launcher',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF242424),
        textTheme: GoogleFonts.interTextTheme(ThemeData.dark().textTheme),
      ),
      home: const LauncherHome(),
    );
  }
}

class LauncherHome extends StatefulWidget {
  const LauncherHome({super.key});

  @override
  State<LauncherHome> createState() => _LauncherHomeState();
}

class _LauncherHomeState extends State<LauncherHome> {
  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      body: _LauncherUI(),
    );
  }
}

class _LauncherUI extends StatelessWidget {
  const _LauncherUI();

  @override
  Widget build(BuildContext context) {
    return const Column(
      children: [
        TopBar(),
        Expanded(
          child: Row(
            // stretch so the side columns fill the full height
            // (otherwise the Row centers them to content height and the
            //  Spacer in IconBar collapses)
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              IconBar(),
              Expanded(child: ProjectList()),
              SidePanel(),
            ],
          ),
        ),
      ],
    );
  }
}
