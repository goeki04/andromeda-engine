import 'dart:convert';
import 'dart:developer' as dev;
import 'package:http/http.dart' as http;
import 'config.dart';

class GithubService {
  static final GithubService _instance = GithubService._();
  static GithubService get instance => _instance;
  GithubService._();

  List? _commits;
  int? _commitCount;

  Map<String, String> get _headers => {
        if (kGithubToken.isNotEmpty) 'Authorization': 'Bearer $kGithubToken',
        'Accept': 'application/vnd.github+json',
      };

  Future<List> fetchCommits() async {
    if (_commits != null) return _commits!;
    try {
      final res = await http.get(
        Uri.parse(
            'https://api.github.com/repos/goeki04/andromeda-engine/commits?per_page=5'),
        headers: _headers,
      );
      if (res.statusCode == 200) {
        _commits = jsonDecode(res.body);
      } else {
        dev.log('fetchCommits: ${res.statusCode} – ${res.body}',
            name: 'GithubService');
      }
    } catch (e) {
      dev.log('fetchCommits error: $e', name: 'GithubService');
    }
    return _commits ?? [];
  }

  Future<int> fetchCommitCount() async {
    if (_commitCount != null) return _commitCount!;
    try {
      final res = await http.get(
        Uri.parse(
            'https://api.github.com/repos/goeki04/andromeda-engine/commits?per_page=1'),
        headers: _headers,
      );
      if (res.statusCode == 200) {
        final link = res.headers['link'] ?? '';
        final match = RegExp(r'page=(\d+)>; rel="last"').firstMatch(link);
        if (match != null) _commitCount = int.parse(match.group(1)!);
      } else {
        dev.log('fetchCommitCount: ${res.statusCode} – ${res.body}',
            name: 'GithubService');
      }
    } catch (e) {
      dev.log('fetchCommitCount error: $e', name: 'GithubService');
    }
    return _commitCount ?? 0;
  }
}
