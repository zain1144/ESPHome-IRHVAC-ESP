# ESPHome IRHVAC for ESP8266 and ESP32

[العربية](README.md) | [English](README_EN.md)

ملفات ESPHome مستقلة ترسل وتستقبل أوامر المكيف بالأشعة تحت الحمراء باستخدام مكتبة
[`IRremoteESP8266`](https://github.com/crankyoldgit/IRremoteESP8266) الإصدار
`2.9.0`. يضيف الفرع المستخدم جسراً صغيراً عاماً للتوقيت: تبقى المكتبة مسؤولة
عن إنشاء بروتوكول المكيف، بينما يرسل ESPHome النبضات الناتجة. تستخدم أوامر MQTT
ونتائج الاستقبال صيغة Tasmota المسماة `IRHVAC` و`IrReceived`.

> التوافق المقصود هنا يشمل أمر MQTT المسمى `IRHVAC` ونتيجة الاستقبال
> `IrReceived`. المشروع لا يحول ESPHome إلى Tasmota ولا ينفذ أوامر Tasmota
> الأخرى غير المتعلقة بهذه الوظيفة.

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
3. عدّل رقمي رجلي الإرسال والاستقبال في أعلى الملف:

   ```yaml
   tx_gpio: "4"
   rx_gpio: "14"
   ```

   اكتب رقم GPIO الفعلي فقط. في D1 mini مثلاً، الرجل `D2` هي `GPIO4` والرجل
   `D5` هي `GPIO14`. تستخدم ملفات ESP32-C3/S2/S3 الرجل `GPIO5` افتراضياً
   للاستقبال.

4. أضف إعداداتك إلى `secrets.yaml` في ESPHome:

   ```yaml
   wifi_ssid: "YOUR_WIFI_SSID"
   wifi_password: "YOUR_WIFI_PASSWORD"
   mqtt_broker: "192.168.1.10"
   mqtt_username: "YOUR_MQTT_USERNAME"
   mqtt_password: "YOUR_MQTT_PASSWORD"
   ```

5. ثبّت البرنامج على الشريحة.

لا تضف مرسلاً أو مستقبلاً آخر من ESPHome على هذه الأرجل؛ ملف YAML الكامل يحتوي
المرسل المطلوب. على ESP32 يستخدم ESPHome وحدة RMT العتادية لإخراج الإطار، ولذلك
لا تتلف نبضات الحامل بسبب مقاطعات Wi-Fi وإطار العمل. تستمر `IRremoteESP8266`
في التحكم برجل الاستقبال مباشرة. استخدم ترانزستوراً مناسباً لتشغيل LED الأشعة
تحت الحمراء، ووصل وحدة مستقبل IR تعمل بجهد 3.3 فولت إلى `rx_gpio`.

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

عندما يستقبل حساس IR أي كود مدعوم، ينشر الجهاز نتيجة مماثلة لـTasmota إلى:

```text
tele/ir-blaster/RESULT
```

إذا كانت الإشارة لمكيف وتم فكها، تتضمن النتيجة الحالة الكاملة:

```json
{"IrReceived":{"Protocol":"KELVINATOR","Bits":128,"Data":"0x...","Repeat":0,"IRHVAC":{"Vendor":"KELVINATOR","Model":-1,"Command":"Control","Mode":"Cool","Power":"On","Celsius":"On","Temp":24,"FanSpeed":"Min","SwingV":"Auto","SwingH":"Auto","Quiet":"Off","Turbo":"Off","Econo":"Off","Light":"Off","Filter":"Off","Clean":"On","Beep":"Off","Sleep":-1,"iFeel":"Off","SensorTemp":null}}}
```

تظهر بنية `IRHVAC` الكاملة نفسها في سجل ESPHome. كما تُنشر البروتوكولات
العادية المعروفة والإشارات غير المعروفة مع اسم البروتوكول وعدد البتات
والبيانات أو البصمة وحالة التكرار.

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

جميع ملفات YAML مستقلة ولا تستخدم `packages` أو `!include`. يعتمد فرع المكتبة
المستخدم على `IRremoteESP8266 2.9.0`، وإضافة التقاط التوقيت فيه لا تغير أي
مرمّز بروتوكول. تحول طبقة هذا المستودع MQTT والحالة المفكوكة إلى صيغة Tasmota،
ثم تمرر إطار التوقيت إلى مرسل ESPHome. هذا اختبار بناء برمجي؛ توافق كل الخصائص
فعلياً يبقى تابعاً للبروتوكول والموديل ودارة الإرسال ووحدة الاستقبال.
