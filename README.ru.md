# Krita Baron Edition

Нативный Android-порт [Krita AI Diffusion от Acly](https://github.com/Acly/krita-ai-diffusion)
и настольный форк. Интерфейс работает на C++/Qt внутри Krita; генерация выполняется
на удалённом сервере. Python на планшете не нужен.

## Скачать

- **[APK 0.1.36 · Android 7+, ARM64](https://github.com/Darxarz/krita-baron-android/releases/download/v0.1.36/krita-baron-android-arm64-v0.1.36.apk)**
- **[Плагин 1.53.0-baron.15 · Windows, Krita 5.x/Qt5](https://github.com/Darxarz/krita-baron-android/releases/download/v0.1.36/krita_ai_diffusion-1.53.0-baron.15.zip)**
- [Полные исходники Android-сборки](https://github.com/Darxarz/krita-baron-android/releases/download/v0.1.36/krita-baron-edition-v0.1.36-source.zip)

Основа Android-сборки — стабильная Krita 5.3.4 / Qt5. Сам порт Baron остаётся
предварительной версией; официальная Android-платформа Krita имеет статус beta.
Она устанавливается отдельно от официальной Krita. В публичной версии — 96 семейств
свободных шрифтов с лицензиями; личные Windows-шрифты в неё не входят.

Уже есть генерация и редактирование выделения, пользовательский инпейнт,
ControlNet/референсы, препроцессоры, регионы, диффузионный апскейл, отделение фона,
предпросмотр результатов на холсте, история в документах, стили, галерея моделей,
кэш миниатюр, подсказки тегов, очередь, автообновления и раскладки при повороте.

[Что ещё не перенесено](docs/CURRENT_PARITY.md) · [Установка и сборка](docs/BUILD.md)
· [English](README.md).

На Android установи APK и выбери удалённый backend в настройках соединения.
Для прямого ComfyUI нужен [серверный обработчик](server/comfyui-baron-native/README.md).
На Windows импортируй ZIP через «Сервис → Скрипты → Импортировать плагин Python из файла»,
перезапусти Krita и включи плагин в менеджере Python-плагинов. Он использует имя
`ai_diffusion` и заменяет установленную версию исходного плагина.

Оригинальный плагин: **Acly и участники проекта**. Krita: **команда Krita и участники**.
**Darxarz / Baron Edition** — портирование и модификации; это независимая сборка.
[Лицензия](LICENSE) · [Авторство и уведомления](THIRD_PARTY_NOTICES.md).
