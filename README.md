# EuloLab: استودیوی بومی موشک مدل برای PS4

EuloLab یک برنامهٔ C++ با رابط workbench است: درخت مونتاژ، inspector قابل‌ویرایش، نمای سه‌بعدی، نمودار پرواز و صفحهٔ جداگانهٔ آزمون سروو. همان هسته و رابط، روی میزبان Linux نیز اجرا می‌شوند؛ مرورگرِ پیش‌نمایش فقط تصویر framebuffer و ورودی را منتقل می‌کند. **خروجی Windows در این مخزن نیست.**

> **وضعیت تأیید:** آزمون‌های هسته، رابط، parser دادهٔ واقعی‌نما در `tests/` (فقط دادهٔ مصنوعیِ آزمون) و شبیه‌سازی USB روی میزبان می‌گذرند. CI یک بستهٔ خانگی PS4 را با OpenOrbis می‌سازد و ساختار آن را بررسی می‌کند؛ firmware مرجع ESP32-S3 را کامپایل می‌کند و job دیگری برای **کامپایلِ بدون flash** sketch Arduino `.ino` در دو خانوادهٔ ESP32/STM32 اضافه شده است (نتیجهٔ آن را در run جاری بررسی کنید). **نصب/اجرای کنسول، تبادل USB روی PS4، سنسور و LoRa روی برد واقعی، کالیبراسیونِ ابزاری و حرکت سروو هنوز آزموده نشده‌اند**. هیچ‌یک از مقادیر پرواز، تحلیل پایداری یا مجوز پرتاب نیستند. برای USB بدون شناسهٔ بردِ بررسی‌شده هیچ دستگاهی باز نمی‌شود.

## شروع سریع

```bash
make -j2 all test
make preview          # پیش‌نمایش میزبان، پورت 8000
# یا: ./out/eulolab-host --screenshot out/workbench.ppm 22
```

پیش‌نمایش میزبان framebuffer خود برنامه را نشان می‌دهد، نه یک پیاده‌سازی مجدد رابط در JavaScript. در محیط محلی `http://localhost:8000` را باز کنید. روی workbench کلیک کنید تا صفحه‌کلید به برنامه برسد؛ `F2` فوکوس را به دکمهٔ مرورگر برمی‌گرداند. این مسیر **USB واقعی را باز نمی‌کند**. تصویر [مونتاژ](docs/assembly-host.png)، [پرواز آموزشی](docs/descent-host.png)، [میز USB بدون اتصال](docs/bench-host.png) و [ویرایش پروفایل USB/پین در خود برنامه](docs/bench-setup-host.png) همگی خروجی میزبان‌اند، نه PS4 یا دوربین فیزیکی.

### تیزر پروژه

[تماشای تیزر ۳۰ ثانیه‌ای (MP4)](video/eulolab-teaser.mp4) · [پوستر](docs/teaser-poster.png) · [زیرنویس/توصیف صحنه‌ها](video/eulolab-teaser.vtt)

ویدیو از فریم‌های **خود رابط C++ روی میزبان** و دو تصویر ثبت‌شدهٔ همان رابط در Bench ساخته شده است؛ بخش پرواز، بازپخش زمان‌فشردهٔ محاسبهٔ مدل است. نمای نزدیکِ پرواز با برچسب مشخص شده؛ صدای زمینه نیز موسیقی مصنوعیِ ساخته‌شده برای تدوین است، نه صدای موتور یا سروو. **هیچ نما یا صدایی از اجرای PS4، پرتاب، سنسور زنده یا حرکت عملگر واقعی وجود ندارد.** برای بازسازی ویدیو از `make teaser` استفاده کنید؛ Python با `Pillow` و `numpy` و یکی از `ffmpeg` یا `imageio-ffmpeg` لازم است. [ارزیابی جزءبه‌جزء و میانگین امتیاز پروژه](docs/project-review.md) مرزهای فعلی را جدا از جنبهٔ تبلیغاتی ثبت می‌کند.

| صفحه | آنچه امروز کار می‌کند | مرز مهم |
| --- | --- | --- |
| Design | ۹ بخش و **۵۲ ویژگی** قابل‌ویرایش برای بدنه، دماغه، بالک، جرم/محل اویونیک، محفظهٔ موتور، بازیابی، insert استوانه‌ای سفارشی، تنظیمات پرواز و مرجع بصری؛ undo، اعتبارسنجی هندسی، جرم/CG، نماهای side/top/iso و چرخش/zoom | insert قطعهٔ CAD آزاد نیست؛ جنس و هندسه فرض‌های آموزشی‌اند |
| File | پروژهٔ متنی نسخه‌دار `.eul`، ذخیرهٔ اتمیک، خواندن نسخهٔ ۱ و قالب‌های قدیمی؛ واردکردن OBJ، STL متنی/باینری و خروجی OBJ نمایشی | مش واردشده **فقط مرجع بصری** است؛ جرم، آیرودینامیک یا حل‌گر را تغییر نمی‌دهد و solid قابل‌ساخت نیست. `.eul` به فایل `reference.obj` یا `reference.stl` کنار خود پیوند می‌دهد و آن را در فایل پروژه جاسازی نمی‌کند |
| Flight | مدل باز آموزشی سه‌بعدی با مکان/سرعت افقی، جهت‌گیری/نرخ زاویه‌ای، باد، زاویهٔ ریل، زاویهٔ ثابت gimbal، نماهای مختلف، مسیر، ماخ و نمودار قابل‌جابجایی | ضرایب بالک/فشار تخمینی، چتر ایده‌آل، بدون کنترل بازخوردی و بدون اعتبارسنجی پروازی؛ فرمانی به USB نمی‌فرستد |
| Ground | واردکردن CSV قدیمیِ ارتفاع/ژیروسکوپ یا CSV جدیدِ **فشار خام، دما، ژیروسکوپ و شتاب**؛ بررسی محدوده/زمان، مرجع فشارِ ثبت‌شده در ۲ ثانیهٔ ثابت، ارتفاع نسبی بارومتری، نمایش محورهای سنسور و گسست داده بدون جعل نقطه | فایل واردشده از نظر **منشأ سخت‌افزاری تأیید نمی‌شود**؛ اپ هیچ دادهٔ سنسور زنده یا ارتباط LoRa/USB سنسور ندارد، ارتفاع مطلق/GPS محاسبه نمی‌کند و انتگرال‌گیری ژیروسکوپ drift دارد |
| Bench | صفحهٔ تنظیم پروفایل USB و چهار نگاشت پین **در خود برنامه**: VID:PID، شمارهٔ interface، آدرس endpointهای bulk و نوع برد؛ ذخیرهٔ جداگانه، دریافت فهرست پین‌های مجاز از firmware، تأیید نگاشت با gate باز، سپس فرمان دستی ±۲°/۱۵۰ ms با تأیید مستقل | فقط بردی با firmware پروتکل `EuloBench/2`، USB سازگار و پین‌های PWM ازپیش‌تأییدشده پاسخ می‌دهد. پیش‌نمایش میزبان USB ندارد؛ برد/کنسول و حرکت واقعی هنوز آزموده نشده‌اند |

منحنی‌های موتور Klima C6/D9 در [`assets/motors`](assets/motors/README.md) دادهٔ منتشرشده‌اند؛ انتخابشان دادهٔ رانش مدل را تغییر می‌دهد، نه موتور واقعی. شبیه‌سازی نمونه‌های تقریباً ۵۰ ms را از گام ۵ ms تولید می‌کند و ضریب‌های آیرودینامیکی اندازه‌گیری‌شده ندارد. **مقادیر آغازین طرح، ابعاد نمونهٔ فرضی هستند؛ جرم/CG و مسیرِ نمایش‌داده‌شده محاسبه‌اند، نه اندازه‌گیری.** نتیجه برای طراحی/بازبینی مفهومی است، نه مهندسی پرواز. [برنامهٔ آزمون واقعیِ بالک، سنسور و میز](docs/ground-calibration.md) ثبت شده و هنوز فاقد اندازه‌گیری کاربر است.

### دادهٔ واقعی، فقط پس از شناسایی سخت‌افزار

[`firmware/arduino/EuloRecorder/EuloRecorder.ino`](firmware/arduino/EuloRecorder/EuloRecorder.ino) یک sketch مشترک Arduino برای **هر دو خانوادهٔ ESP32 و STM32duino** است: ماژول‌های مشخص‌شدهٔ BMP388/390 و LSM6DSOX را از I²C می‌خواند، دادهٔ خام را روی Serial ثبت می‌کند و با **انتخاب صریح، آنتن/فرکانس/پین‌های بررسی‌شده و محدودیت رادیویی** می‌تواند دادهٔ کم‌نرخ را از SX127x LoRa به sketch گیرنده بفرستد. پیکربندیِ داخل مخزن عمداً خاموش است؛ **مدل برد، سنسورها و سیم‌کشی کاربر ناشناخته‌اند**. هیچ دادهٔ سنسور یا گزارش آزمون RF واقعی در مخزن نیست؛ هیچ GPS اضافه نشده است. برنامهٔ [`tools/capture_serial.py`](tools/capture_serial.py) فقط بایت‌های دریافت‌شده از پورت واقعی را با header و محدوده‌های سختگیرانه به CSV و پروندهٔ منشأ/هش ثبت می‌کند؛ سپس کاربر CSV را در Ground وارد می‌کند. این مسیر **آفلاین** است و مجوز/بازخورد servo/پرواز نمی‌دهد. [راهنمای شناسایی، سیم‌کشی، کتابخانه‌ها، اجرا و محدودیت LoRa](firmware/arduino/EuloRecorder/README.md).

### کنترل‌ها و فایل‌ها

- موس و صفحه‌کلید میزبان: انتخاب جزء/مقدار، تایپ عدد و `Enter` برای اعمال یا `Esc` برای لغو، `Tab` برای فوکوس، +/- برای گام‌بندی، کشیدن viewport برای چرخش، wheel برای zoom و کلیک نمودار برای زمان. `Ctrl+S/O/N/Z` ذخیره/بازکردن/پروژهٔ جدید/undo؛ `Ctrl+M` مرجع OBJ، `Ctrl+Shift+M` مرجع STL، `Ctrl+E` خروجی OBJ و `Ctrl+L` ضبط CSV را بارگذاری می‌کند.
- دستهٔ PS4: `L1/R1` جزء، `L2/R2` ناحیهٔ فوکوس، D-pad انتخاب/گام‌بندی، `×` تأیید، `○` بازگشت، `□` ذخیره (در Bench درخواست خنثی)، `Options` منو، `△` Ground و `R3` تغییر نما. **در صفحهٔ تنظیم USB** با `×` ویرایش hex باز می‌شود؛ D-pad رقم را انتخاب و مقدارش را تغییر می‌دهد و `○` لغو می‌کند. تایپ عدد برای inspector مدل با دسته هنوز پیاده‌سازی نشده و مقادیر مدل با گام تغییر می‌کنند. تغییرات ذخیره‌نشدهٔ پروژه پیش از تعویض آن تأیید می‌خواهند.
- روی میزبان فایل‌های `out/project.eul`، `out/bench-profile.cfg`، `out/reference.obj`/`.stl`، `out/recording.csv` و `out/assembly.obj` به‌کار می‌روند. در مرورگر می‌توان CSV/OBJ/STL را از file picker بارگذاری کرد؛ فایل پیش از جایگزینی با parser بومی بررسی می‌شود. پروفایل سخت‌افزار از خود رابط ویرایش/ذخیره می‌شود، نه از فایل پروژه. روی PS4 مسیر پروژه `/data/eulolab/project.eul` و مسیر جداگانهٔ پروفایل `/data/eulolab/bench-profile.cfg` است؛ فایل‌های مرجع و ضبط باید **کنار پروژه** باشند. دسترسی ذخیره/ورودی کنسول هنوز تأیید نشده است.

## USB و آزمون سرووی روی میز

[پروتکل EuloBench/2، سیم‌کشی احتیاطی و روش آزمون](docs/bench-usb.md) را **پیش از اتصال عملگر** بخوانید. از منوی **View → Servo bench (USB)** یا دکمهٔ Bench وارد صفحهٔ دستی شوید؛ سپس **Configure USB and pin map** را بزنید. VID:PID مجازِ همان برد، interface، endpointهای bulk (یا `00` برای تشخیص از descriptor)، خانوادهٔ برد و نگاشت چهار پین در خود برنامه ویرایش و در فایل مستقل ذخیره می‌شوند. ابتدا شناسهٔ USB را بدون حدس‌زدن پین‌ها ذخیره و برد را شناسایی کنید؛ فهرست پین‌های **مجاز اعلام‌شده توسط firmware** را ببینید، چهار پین را انتخاب/ذخیره کنید و دوباره با gate باز شناسایی کنید. اعمال نگاشت پنجرهٔ پیش‌فرض-لغو دارد. پس از بستن gate باید دوباره شناسایی کنید. پین gate را نمی‌توان از راه USB جابه‌جا کرد. فرمان حرکت نیز تأیید مستقل می‌خواهد؛ `□` روی دسته یا `N` روی صفحه‌کلید درخواست خنثی می‌دهد. قطع USB یا بازشدن gate باید توسط **خود برد** به خنثی برگردد؛ قطع فیزیکیِ مستقلِ برق سروو همچنان الزامی است.

[`firmware/esp32s3/`](firmware/esp32s3/) هدف مرجع قابل‌کامپایل برای ESP32-S3 دارای native USB OTG و چهار تا هشت خروجی PWM ازپیش‌بررسی‌شده است. پین‌هایی که در app انتخاب می‌شوند باید **در pool همان firmware موجود باشند**؛ بدون VID/PID قانونی و GPIOهای واقعی در `idf.py menuconfig` firmware برای برد کاربر آماده نمی‌شود. [`firmware/stm32/`](firmware/stm32/README.md) هستهٔ USB/نگاشت/PWMِ **کامپایل‌شده و آزموده‌شده با HAL جعلی** را برای خانوادهٔ STM32 می‌دهد، اما بدون مدل دقیق و لایهٔ USB/تایمر آن برد، image قابل‌فلش نیست. هیچ نرم‌افزاری نمی‌تواند یک تراشهٔ دلخواه، مبدل USB-serial یا firmware ناسازگار را بدون درایور/پروتکل مخصوص خود به GPIO قابل‌کنترل تبدیل کند.

برای PS4، شناسه یا پین در PKG کامپایل نمی‌شود و **نیازی به بازسازی بسته پس از ویرایش پروفایل نیست**. پروفایل اولیه VID:PID صفر دارد و USB را باز نمی‌کند. ساخت هدف PS4:

```bash
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis-PS4-Toolchain
make ps4
```

مسیر ساخت به [OpenOrbis v0.5.4](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4)، Clang/LLD 18 و ابزارهای بسته‌بندی آن وابسته است؛ ابزار بسته‌بندی legacy در میزبان Linux به OpenSSL 1.1 ایزوله نیاز دارد که CI جداگانه می‌سازد. بستهٔ `out/ps4/stage/IV0000-BREW00242_00-EULOROCKETLAB001.pkg` **نامزد خانگیِ آزمایش‌نشده روی کنسول** است، نه بستهٔ تأییدشده یا firmware آمادهٔ پرواز. جزئیات build در [`ps4/Makefile`](ps4/Makefile) و [گردش CI](.github/workflows/build.yml) است.

## کد، آزمون و مجوز

`src/core` فایل/جرم/شبیه‌سازی/مش/پروتکل را نگه می‌دارد؛ `src/ui` رابط و rasterizer مشترک را؛ `src/platform/ps4.cpp` و `ps4_bench_usb.cpp` ورودی، تصویر و USB بومی Orbis را؛ `src/platform/host.cpp` و `tools/host_preview.py` پیش‌نمایش و فایل‌های میزبان را. `tests/bench_tests.cpp` پروتکل/پروفایل/descriptor را دو سرِ حافظه‌ای و `tests/stm32_port_tests.cpp` آداپتور STM32 را با HAL جعلی، بدون سخت‌افزار می‌آزمایند. [`DESIGN.md`](DESIGN.md) تصمیم‌های UX و [`docs/workbench-audit.md`](docs/workbench-audit.md) شواهد آزمون و مرزها را ثبت می‌کنند. سبک توسعه بر اساس [anti-slop](https://github.com/miqdadbadjuber/anti-slop) حین ساخت و بازبینی اعمال شده است.

کد تحت [MIT](LICENSE)، فونت Space Grotesk تحت [SIL OFL](assets/OFL.txt) و دادهٔ منحنی موتور با مجوز توضیح‌داده‌شده در [`assets/motors/README.md`](assets/motors/README.md) عرضه می‌شوند. کتابخانه و SDK شخص ثالث در مخزن کپی نشده‌اند.

**English:** Native C++ PS4-targeted workbench with a Linux framebuffer preview. Design defaults are unmeasured assumptions; motor curves are published sources; Flight values are *predictions*. Ground now accepts raw BMP3xx/LSM6DSOX pressure, temperature, gyro and accelerometer CSV, derives only **relative** barometric height after a stationary baseline, and shows sensor axes without a fictitious measured rocket mesh. A fail-closed Arduino `.ino` recorder targets **both ESP32 and STM32duino**, with optional opt-in SX127x point-to-point LoRa and a serial CSV capture tool; it requires identified hardware and has **not been run on a board**. The separate EuloBench/2 manual servo USB protocol still lacks an identified, instrumented Arduino vendor-bulk port; ESP32-S3 has an ESP-IDF reference and STM32 has a portable policy adapter, neither proving live hardware. Host tests, compile-only CI and a console-untested PKG do **not** verify attached sensors, RF, PWM/servo travel, PS4 runtime or flight safety. See [hardware setup](firmware/arduino/EuloRecorder/README.md) and [ground evidence](docs/ground-calibration.md).
