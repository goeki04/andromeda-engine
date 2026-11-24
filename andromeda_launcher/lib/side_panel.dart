import 'package:flutter/material.dart';
import 'glass.dart';
import 'services.dart';

class SidePanel extends StatelessWidget {
  const SidePanel({super.key});

  @override
  Widget build(BuildContext context) {
    return GlassContainer(
      width: 280,
      blur: 30,
      color: const Color(0x66060606),
      border: Border(
        left:
            BorderSide(color: Colors.white.withValues(alpha: 0.05), width: 0.5),
      ),
      child: const Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _Block(
            label: 'Engine',
            child: _EngineCard(),
          ),
          _PanelDivider(),
          _Block(
            label: 'Übersicht',
            child: _StatsGrid(),
          ),
          _PanelDivider(),
          Expanded(
            child: _Block(
              label: 'Neuigkeiten',
              child: _NewsFeed(),
            ),
          ),
        ],
      ),
    );
  }
}

class _PanelDivider extends StatelessWidget {
  const _PanelDivider();

  @override
  Widget build(BuildContext context) {
    return Container(height: 0.5, color: Colors.white.withValues(alpha: 0.05));
  }
}

class _Block extends StatelessWidget {
  final String label;
  final Widget child;
  const _Block({required this.label, required this.child});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        mainAxisSize: MainAxisSize.min,
        children: [
          Text(
            label.toUpperCase(),
            style: TextStyle(
              fontSize: 9.5,
              letterSpacing: 1.4,
              color: Colors.white.withValues(alpha: 0.38),
            ),
          ),
          const SizedBox(height: 12),
          child,
        ],
      ),
    );
  }
}

class _EngineCard extends StatelessWidget {
  const _EngineCard();

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.03),
        borderRadius: BorderRadius.circular(10),
        border:
            Border.all(color: Colors.white.withValues(alpha: 0.08), width: 0.5),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                'Andromeda v0.4.2',
                style: TextStyle(
                  fontSize: 12,
                  fontWeight: FontWeight.w500,
                  color: Colors.white.withValues(alpha: 0.70),
                ),
              ),
              Row(
                children: [
                  Container(
                    width: 5,
                    height: 5,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: const Color(0xFFB4FFB4).withValues(alpha: 0.70),
                    ),
                  ),
                  const SizedBox(width: 5),
                  Text(
                    'aktuell',
                    style: TextStyle(
                        fontSize: 10,
                        color: Colors.white.withValues(alpha: 0.52)),
                  ),
                ],
              ),
            ],
          ),
          const SizedBox(height: 8),
          ClipRRect(
            borderRadius: BorderRadius.circular(99),
            child: Stack(
              children: [
                Container(
                    height: 2, color: Colors.white.withValues(alpha: 0.07)),
                FractionallySizedBox(
                  widthFactor: 0.72,
                  child: Container(
                      height: 2,
                      color: const Color(0xFFE8A230).withValues(alpha: 0.70)),
                ),
              ],
            ),
          ),
          const SizedBox(height: 6),
          Text(
            'v0.5.0 Beta verfügbar',
            style: TextStyle(
                fontSize: 10, color: Colors.white.withValues(alpha: 0.40)),
          ),
        ],
      ),
    );
  }
}

class _StatsGrid extends StatelessWidget {
  const _StatsGrid();

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Row(children: [
          const Expanded(child: _StatCell('5', 'Projekte')),
          const SizedBox(width: 8),
          const Expanded(child: _StatCell('2h', 'Heute')),
        ]),
        const SizedBox(height: 8),
        Row(children: [
          Expanded(
            child: FutureBuilder<int>(
              future: GithubService.instance.fetchCommitCount(),
              builder: (context, snap) => _StatCell(
                snap.hasData ? '${snap.data}' : '—',
                'Commits',
              ),
            ),
          ),
          const SizedBox(width: 8),
          const Expanded(child: _StatCell('1.0', 'Engine')),
        ]),
      ],
    );
  }
}

class _StatCell extends StatelessWidget {
  final String value;
  final String label;
  const _StatCell(this.value, this.label);

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
      decoration: BoxDecoration(
        color: Colors.white.withValues(alpha: 0.03),
        borderRadius: BorderRadius.circular(8),
        border:
            Border.all(color: Colors.white.withValues(alpha: 0.07), width: 0.5),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            value,
            style: const TextStyle(
              fontSize: 18,
              fontWeight: FontWeight.w300,
              color: Color(0xFFF5BE68),
            ),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            style: TextStyle(
                fontSize: 9.5, color: Colors.white.withValues(alpha: 0.48)),
          ),
        ],
      ),
    );
  }
}

class _NewsFeed extends StatefulWidget {
  const _NewsFeed();
  @override
  State<_NewsFeed> createState() => _NewsFeedState();
}

class _NewsFeedState extends State<_NewsFeed> {
  List _commits = [];

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final commits = await GithubService.instance.fetchCommits();
    setState(() => _commits = commits);
  }

  @override
  Widget build(BuildContext context) {
    if (_commits.isEmpty) return const SizedBox();

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (var i = 0; i < _commits.length; i++)
          _NewsItem(
              title: (_commits[i]['sha'] as String).substring(0, 7),
              body: (_commits[i]['commit']['message'] as String)
                  .split('\n')
                  .first,
              showDivider: i > 0)
      ],
    );
  }
}

class _NewsItem extends StatelessWidget {
  final String title;
  final String body;
  final bool showDivider;
  const _NewsItem({
    required this.title,
    required this.body,
    required this.showDivider,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (showDivider)
          Container(height: 0.5, color: Colors.white.withValues(alpha: .05)),
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 9),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Padding(
                padding: const EdgeInsets.only(top: 5, right: 10),
                child: Container(
                  width: 4,
                  height: 4,
                  decoration: const BoxDecoration(
                    shape: BoxShape.circle,
                    color: Color(0xFFE8A230),
                  ),
                ),
              ),
              Expanded(
                child: RichText(
                  text: TextSpan(
                    style: const TextStyle(fontSize: 11, height: 1.5),
                    children: [
                      TextSpan(
                        text: '$title ',
                        style: const TextStyle(
                          color: Color(0xFFF5BE68),
                          fontWeight: FontWeight.w500,
                        ),
                      ),
                      TextSpan(
                        text: '— $body',
                        style: TextStyle(
                            color: Colors.white.withValues(alpha: 0.58)),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
