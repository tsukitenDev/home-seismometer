# home-seismometer

ESP32用のWeb UIを備えた地震計ファームウェア

<img src="https://raw.githubusercontent.com/tsukitenDev/home-seismometer/main/.github/assets/screenshot.png" width="50%" alt="screenshot">

## 機能
- [x] 計測震度の算出
- [x] 計測震度の表示 (ボード組み込み画面) 
- [x] 加速度波形・計測震度のリアルタイム表示 (Web UI)
- [ ] 地震履歴の保存、表示
- [x] 揺れを通知 (Webhook)
- [x] シリアル経由のWiFiプロビジョニング ([Improv Wi-Fi](https://www.improv-wifi.com/))

震度：

処理の制約のため、約40秒間の加速度データから計算します。


## 対応ボード・センサー
ボード
- EQIS-1 (Seeeduino XIAO ESP32S3)
- ESP32-1732S019 (ESP32-S3-WROOM-1-N16R8)


加速度センサー
- LSM6DSO
- ADXL355

> [!note]
> XIAO ESP32S3 と LSM6DSO の組み合わせで、センサーのノイズが通常より大きくなる現象を確認しています。
> このノイズは50Ω程度の抵抗を3.3Vラインに挿入することで対処できます。（センサー直近のパスコンよりホスト側）
> 対応を行わない限り、震度2～1以下の地震の検知は難しいです。


## インストール
https://tsukitendev.github.io/home-seismometer/

(Web Serial APIの許可が必要です)

## ビルド

ESP-IDF v5.4.0以上

```bash
git clone --recursive https://github.com/tsukitenDev/home-seismometer.git
cd home-seismometer
cp sdkconfig_cyd sdkconfig.defaults
# 以下、idf.pyが通る環境
idf.py set-target esp32s3
idf.py build
```


## 画面説明
### 状態表示画面

現在のステータスが表示されます。

一定時間経過後に待機画面に移行します。

```
+-----------------------+
1  eqis-1.local/monitor |
|　                     |
2　WiFi : Not Configured|
3　acc  : Connected     |
4　seis : Stabilizing   |
|　                     |
5　X      Y      Z  gal |
6　-000.0 -000.0 -000.0 |
+-----------------------+
```

**Web UI ( \*\*.local/monitor )**

加速度・震度をリアルタイムで表示するページに接続します。

**Wi-Fi**

|主な表示|説明|
|---|---|
|Not Configured|有効なSSIDが登録されていない|
|Connect Failed|APへの接続に失敗<br>SSIDまたはパスワードが間違っている、APまでの距離が遠い等|
|Connected|APに接続済み|

**acc**

|主な表示|説明|
|---|---|
|Connected|加速度センサーに接続済み|

**seis**

|主な表示|説明|
|---|---|
|stabilizing|地震計の静定待ち<br>加速度の直流成分を除去するためにHPFを使用しています。HPFからの出力が安定し、有効な震度が出力されるまで90秒程度かかります。|
|stabilized|震度測定中|


**加速度(cm/s2)**

センサーが出力する加速度を表示します。
デバイスが固定されていれば、震度は向きに関わらず計測できます。


### 待機画面

画面右下にスピナーが表示されます。
一定以上の揺れを観測すると震度表示画面に移行します。

**震度表示画面に移行するしきい値**

|センサー|計測震度|
|---|---|
|LSM6DSO（EQIS-1）|1.5以上|
|ADXL355|0.5以上|


### 震度表示画面

計測した最大の震度が表示されます。
揺れが収まると待機画面に移行します。

