# Changelog / История изменений

Every change gets its own version. The number is stored in `VERSION`; the build, the installer,
the app header and the GitHub release all take it from there.

Каждая правка получает свой номер версии. Он хранится в файле `VERSION`; оттуда его берут сборка,
установщик, шапка программы и релиз на GitHub.

## 1.4.1
- Fixed: in the live preview, lyrics could stay a blurry streak of particles when the picture was drawn at a low frame rate (slow GPU, big window). Text, 3D particles and attractors now move by real time, not by the number of drawn frames.
- New README with screenshots, an animated preview and a preset gallery.
- Исправлено: в живом превью текст мог оставаться размытой полосой частиц, если картинка рисовалась с низкой частотой кадров (слабая видеокарта, большое окно). Текст, 3D-частицы и аттракторы теперь движутся по реальному времени, а не по числу нарисованных кадров.
- Новый README со скриншотами, анимированным превью и галереей пресетов.

## 1.4.0
- "About Auralis" window: version, release date, copyright DJ Maksy and a clickable link to djmaksy.com. Opens with the "i" button in the header or by clicking the logo.
- Окно «О программе Auralis»: версия, дата выпуска, авторские права DJ Maksy и кликабельная ссылка djmaksy.com. Открывается кнопкой «i» в шапке или щелчком по логотипу.

## 1.3.0
- Version number shown in the app header (next to the logo) and in the installer name.
- Every version is published as its own GitHub release (`v1.3.0`, …) with these notes.
- Номер версии виден в шапке программы (рядом с логотипом) и в имени установщика.
- Каждая версия публикуется отдельным релизом на GitHub (`v1.3.0`, …) с этим описанием.

## 1.2.1
- README translated to English; Russian version moved to `README.ru.md`.
- README переведён на английский; русская версия — в `README.ru.md`.

## 1.2.0
- "AURALIS by DJ Maksy" logo in the header; DJ Maksy as the plug-in vendor and installer publisher.
- Логотип «AURALIS by DJ Maksy» в шапке; DJ Maksy — производитель плагина и издатель установщика.

## 1.1.0
- Text appearance option: particles fly in / fade in / instant.
- Selectable video bitrate on export: auto or 4–120 Mbps.
- Опция появления текста: частицы слетаются / плавно / сразу.
- Выбор битрейта видео при экспорте: авто или 4–120 Мбит/с.

## 1.0.1
- Windows build fix (Media Foundation encoder).
- Исправлена сборка под Windows (кодировщик Media Foundation).

## 1.0.0
- First release: VST3 plug-in and standalone Windows app, background/spectrum/3D/text/ornament/optics/video
  layers, 62 presets, lyrics with auto-sync, frame-accurate MP4 export up to 4K, auto-render on DAW audio export.
- Первый выпуск: VST3-плагин и приложение для Windows, слои фона/спектра/3D/текста/орнамента/оптики/видео,
  62 пресета, текст песни с автосинхронизацией, MP4-экспорт кадр в кадр до 4K, автоматический рендер при экспорте звука из DAW.
