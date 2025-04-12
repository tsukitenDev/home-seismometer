# home-seismometer

ESP32用のWeb UIを備えた地震計ファームウェア

## 機能
- [x] 計測震度の算出
- [x] 計測震度の表示 (ボード組み込み画面) 
- [x] 加速度波形・計測震度のリアルタイム表示 (Web UI)
- [ ] 地震履歴の保存、表示
- [x] シリアル経由のWiFiプロビジョニング ([Improv Wi-Fi](https://www.improv-wifi.com/))

## 対応ボード・センサー
ボード
- EQIS-1 (Seeeduino XIAO ESP32S3)
- ESP32-1732S019 (ESP32-S3-WROOM-1-N16R8)


加速度センサー
- LSM6DSO
- ADXL355


## インストール
https://tsukitendev.github.io/home-seismometer/

(Web Serial APIの許可が必要です)

