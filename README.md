# ESPHome IRHVAC for ESP8266 and ESP32

[العربية](README.md) | [English](README_EN.md)

ملفات ESPHome مستقلة تستقبل أمر المكيف `IRHVAC` عبر MQTT بصيغة متوافقة مع
أمر Tasmota، ثم ترسل الإشارة باستخدام مكتبة
[`IRremoteESP8266`](https://github.com/crankyoldgit/IRremoteESP8266) الرسمية
غير المعدلة، الإصدار `2.9.0`.

> التوافق المقصود هنا هو استقبال أمر MQTT `IRHVAC` وحقول الـJSON وإرجاع
> `RESULT`. المشروع لا يحول ESPHome إلى Tasmota ولا ينفذ بقية أوامر Tasmota.

## اختر ملفاً واحداً

كل ملف YAML مستقل بالكامل ويمكن نسخه وحده إلى واجهة ESPHome:

| الشريحة | الملف | اللوحة الافتراضية |
|---|---|---|
| ESP8266 | `ir-blaster-esp8266.yaml` | D1 mini |
| ESP32 | `ir-blaster-esp32.yaml` | ESP32 DevKit |
| ESP32-C3 | `ir-blaster-esp32-c3.yaml` | ESP32-C3 DevKitM-1 |
| ESP32-S2 | `ir-blaster-esp32-s2.yaml` | ESP32-S2 Saola-1 |
| ESP32-S3 | `ir-blaster-esp32-s3.yaml` | ESP32-S3 DevKitC-1 |

تحتاج نسخ ESP32 إلى إطار Arduino لأن المكتبة المكتوبة لهذا الإطار. لا يشمل
هذا الحل ESP32-C2/C5/C6/C61/H2/P4، لأن ESPHome لا يوفر لها إطار Arduino.

## الإعداد

1. أنشئ جهازاً جديداً في لوحة ESPHome.
2. استبدل محتوى YAML بمحتوى الملف المناسب لشريحتك.
3. عدّل رقم رجل الإرسال في أعلى الملف:

   ```yaml
   tx_gpio: "4"
   ```

   اكتب رقم GPIO الفعلي فقط. في D1 mini مثلاً، الرجل `D2` هي `GPIO4`.

4. أضف إعداداتك إلى `secrets.yaml` في ESPHome:

   ```yaml
   wifi_ssid: "YOUR_WIFI_SSID"
   wifi_password: "YOUR_WIFI_PASSWORD"
   mqtt_broker: "192.168.1.10"
   mqtt_username: "YOUR_MQTT_USERNAME"
   mqtt_password: "YOUR_MQTT_PASSWORD"
   ```

5. ثبّت البرنامج على الشريحة.

لا تضف `remote_transmitter` على GPIO نفسه. مكتبة `IRremoteESP8266` تتحكم
بالرجل وتولد الحامل مباشرة. استخدم ترانزستوراً مناسباً لتشغيل LED الأشعة تحت
الحمراء؛ لا توصل LED عالي التيار مباشرة إلى GPIO.

## MQTT

يراقب الجهاز الموضوعين التاليين:

```text
cmnd/ir-blaster/IRHVAC
cmnd/ir-blaster/irhvac
```

الـpayload الأساسي مطابق لمدخل أمر Tasmota:

```json
{
  "Vendor": "KELVINATOR",
  "Model": -1,
  "Command": "Control",
  "Mode": "Cool",
  "Power": "Off",
  "Celsius": "On",
  "Temp": 25,
  "FanSpeed": "Min",
  "SwingV": "Off",
  "SwingH": "Off",
  "Quiet": "Off",
  "Turbo": "Off",
  "Econo": "Off",
  "Light": "Off",
  "Filter": "Off",
  "Clean": "On",
  "Beep": "Off",
  "Sleep": -1,
  "iFeel": "Off",
  "SensorTemp": null
}
```

تُقبل أيضاً الصيغة المغلفة:

```json
{"IRHVAC":{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":25}}
```

بعد الإرسال ينشر الجهاز الحالة التي استخدمها إلى:

```text
stat/ir-blaster/RESULT
```

مثال باستخدام Mosquitto:

```bash
mosquitto_pub -h 192.168.1.10 \
  -t cmnd/ir-blaster/IRHVAC \
  -m '{"Vendor":"KELVINATOR","Power":"On","Mode":"Cool","Temp":25}'
```

إذا غيّرت اسم الجهاز أو Topic في أعلى الملف، اضبط إضافة Home Assistant على
الموضوع نفسه.

## حقول الأمر

يدعم المحول الحقول التالية:

`Vendor`, `Model`, `Command`, `Mode`, `Power`, `Celsius`, `Temp`,
`FanSpeed`, `SwingV`, `SwingH`, `Quiet`, `Turbo`, `Econo`, `Light`,
`Filter`, `Clean`, `Beep`, `Sleep`, `iFeel`, و`SensorTemp`.

توفر كل خاصية يعتمد على بروتوكول المكيف نفسه. قبول المكتبة لحقل لا يعني أن
كل جهاز يدعم تلك الخاصية. أسماء الشركات والبروتوكولات المتاحة هي التي يدعمها
`IRac` في `IRremoteESP8266 2.9.0`.

## التحقق

تم التحقق باستخدام ESPHome `2026.8.2`، ونجح التحقق من الإعداد ثم البناء
الكامل وتوليد firmware لكل من:

- ESP8266
- ESP32
- ESP32-C3
- ESP32-S2
- ESP32-S3

جميع ملفات YAML مستقلة ولا تستخدم `packages` أو `!include`، والمكتبة
المشار إليها هي الإصدار الرسمي الثابت `IRremoteESP8266 2.9.0`. هذا اختبار
بناء برمجي؛ توافق كل خصائص المكيف فعلياً يبقى تابعاً للبروتوكول والموديل
ودارة الإرسال المستخدمة.
