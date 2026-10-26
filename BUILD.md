# ビルド

サブモジュール込みで取得したソースを、次のコンテナーの `/src` にマウントして実行します。

```sh
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5 bash tools/localization/build-3ds.sh
```

出力先は `release/` です。配布用の対応ソースにはサブモジュールも含めています。
