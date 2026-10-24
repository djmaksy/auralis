#include "Strings.h"
#include <map>
#include <string>

namespace au
{
juce::String tr (const char* key, bool ru)
{
    static const std::map<std::string, std::pair<const char*, const char*>> m =
    {
        { "save",        { "Save", "Сохранить" } },
        { "delete",      { "Delete", "Удалить" } },
        { "export",      { "Export", "Экспорт" } },
        { "render",      { "Render video", "Рендер видео" } },
        { "openTrack",   { "Open track", "Открыть трек" } },
        { "play",        { "Play", "Играть" } },
        { "pause",       { "Pause", "Пауза" } },
        { "lyrics",      { "Lyrics…", "Текст…" } },
        { "loadLyrics",  { "Load .lrc / .txt", "Загрузить .lrc / .txt" } },
        { "saveLrc",     { "Save .lrc", "Сохранить .lrc" } },
        { "recognise",   { "Sync to voice", "Синхр. с голосом" } },
        { "cancel",      { "Cancel", "Отмена" } },
        { "title",       { "Title", "Надпись" } },
        { "offset",      { "Offset", "Сдвиг" } },
        { "factory",     { "Factory", "Заводские" } },
        { "user",        { "My presets", "Мои пресеты" } },
        { "presetName",  { "Preset name", "Имя пресета" } },
        { "videoFile",   { "File…", "Файл…" } },
        { "clear",       { "Clear", "Убрать" } },
        { "noVideo",     { "no video / image", "нет видео / картинки" } },
        { "dropHint",    { "Drop a track, video, image, .txt or .lrc here", "Бросьте сюда трек, видео, картинку, .txt или .lrc" } },
        { "renderTrack", { "Render loaded track to MP4…", "Рендер загруженного трека в MP4…" } },
        { "autoBounce",  { "Render video automatically on DAW audio export", "Делать видео автоматически при экспорте звука в DAW" } },
        { "outFolder",   { "Output folder…", "Папка для видео…" } },
        { "openFolder",  { "Open output folder", "Открыть папку с видео" } },
        { "fileName",    { "File name for DAW export…", "Имя файла при экспорте из DAW…" } },
        { "ffmpeg",      { "Encode with ffmpeg when available", "Кодировать через ffmpeg, если есть" } },
        { "rendering",   { "Rendering", "Рендер" } },
        { "done",        { "Done", "Готово" } },
        { "failed",      { "Failed", "Ошибка" } },
        { "close",       { "Close", "Закрыть" } },
        { "showFile",    { "Show file", "Показать файл" } },
        { "needTrack",   { "Load a track first (or drop one onto the window).", "Сначала загрузите трек (или бросьте его в окно)." } },
        { "noAudio",     { "No audio yet: play the song in the DAW once, or drop the track file onto the window.",
                           "Звука пока нет: проиграйте песню в DAW или бросьте файл трека в окно." } },
        { "modelAsk",    { "Speech recognition needs the Whisper model (about 470 MB). Download it now?",
                           "Для распознавания нужна модель Whisper (около 470 МБ). Скачать сейчас?" } },
        { "downloading", { "Downloading model", "Загрузка модели" } },
        { "recognising", { "Recognising vocal", "Распознаю вокал" } },
        { "recognised",  { "Lyrics synced", "Текст синхронизирован" } },
        { "editLyrics",  { "Paste the song text, one line per row. Then press \"Sync to voice\" — or apply as is.",
                           "Вставьте текст песни, по строке на ряд. Затем «Синхр. с голосом» — или примените как есть." } },
        { "apply",       { "Apply", "Применить" } },
        { "applySync",   { "Apply and sync to voice", "Применить и синхронизировать" } },
        { "modNone",     { "No modulation", "Без модуляции" } },
        { "modSource",   { "Modulate by", "Модуляция от" } },
        { "modDepth",    { "Depth", "Глубина" } },
        { "resetValue",  { "Reset to default", "Сбросить" } },
        { "bass",        { "Bass", "Бас" } },
        { "mid",         { "Mids", "Середина" } },
        { "treble",      { "Highs", "Верх" } },
        { "beat",        { "Hits", "Удары" } },
        { "level",       { "Loudness", "Громкость" } },
        { "beatLfo",     { "Beat LFO", "LFO по доле" } },
        { "barLfo",      { "Bar LFO", "LFO по такту" } },
        { "fullscreen",  { "Full screen (D)", "Во весь экран (D)" } },
        { "audioSettings", { "Audio device / ASIO", "Аудиоустройство / ASIO" } },
        { "glError",     { "OpenGL 3.2 is not available on this graphics driver.", "Драйвер видеокарты не поддерживает OpenGL 3.2." } },
    };
    const auto it = m.find (key);
    if (it == m.end()) return key;
    return juce::String::fromUTF8 (ru ? it->second.second : it->second.first);
}
}
