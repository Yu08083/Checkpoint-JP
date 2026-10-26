import json
import os
from pathlib import Path
import urllib.error
import urllib.parse
import urllib.request

repository = os.environ['GITHUB_REPOSITORY']
tag = os.environ['GITHUB_REF_NAME']
headers = {'Authorization': 'Bearer ' + os.environ['GITHUB_TOKEN'], 'Accept': 'application/vnd.github+json', 'User-Agent': 'checkpoint-jp-release', 'X-GitHub-Api-Version': '2022-11-28'}


def request(method, url, data=None, content_type='application/json'):
    if data is not None and content_type == 'application/json':
        data = json.dumps(data).encode('utf-8')
    request_headers = dict(headers, **{'Content-Type': content_type})
    with urllib.request.urlopen(urllib.request.Request(url, data=data, headers=request_headers, method=method), timeout=90) as response:
        body = response.read()
        return json.loads(body) if body else None


api = f'https://api.github.com/repos/{repository}/releases'
try:
    release = request('GET', api + '/tags/' + urllib.parse.quote(tag, safe=''))
    if not release['draft']:
        raise SystemExit('Release already published')
except urllib.error.HTTPError as error:
    if error.code != 404:
        raise
    release = request('POST', api, {'tag_name': tag, 'target_commitish': os.environ['GITHUB_SHA'], 'name': 'Checkpoint 日本語版 ' + tag, 'body': 'FBIのQR読み取りでインストールできます。\n\n![インストール用QR](https://raw.githubusercontent.com/Yu08083/Checkpoint-JP/japanese-3ds/docs/install-qr.png)', 'draft': True, 'prerelease': False})

existing = {asset['name']: asset['id'] for asset in release['assets']}
for path in sorted(Path('release').iterdir()):
    if not path.is_file():
        continue
    if path.name in existing:
        request('DELETE', api + '/assets/' + str(existing[path.name]))
    upload = release['upload_url'].split('{')[0] + '?name=' + urllib.parse.quote(path.name)
    request('POST', upload, path.read_bytes(), 'application/octet-stream')
request('PATCH', api + '/' + str(release['id']), {'draft': False, 'make_latest': 'true'})
print(release['html_url'])
