# Метаданные каталога для Krita 0.1.33

Браузерный токен Krita ограничен `/api/krita/` и `/krita/connection/`.
Новый маршрут `/api/krita/model-metadata?name=...&kind=lora|checkpoint`
возвращает описание только модели, видимой на сервере пользователя.
Общий доступ к веб-API и обход ограничений не нужны.

Патч сохраняет имена файлов, авторские описания, теги, триггеры и параметры
примеров из существующих метаданных сайта. Он не загружает и не хеширует
веса моделей и не меняет обработку генерации или цены.

Подготовка патча (читает исходники сайта, пишет отдельный файл):

```powershell
python server/orchestrion-catalog/prepare-patch.py Z:/orchestrator artifacts/orchestrion-catalog-v0133.patch --review-directory build/site-catalog-review
```

Перед установкой проверить текущие изменения сайта и создать отдельную ветку,
сохранив работу других агентов. Не сбрасывать файлы и не заменять их полными
старыми копиями. Проверить и применить именно подготовленный патч:

```powershell
git apply --check D:/krita-baron-android-native/artifacts/orchestrion-catalog-v0133.patch
git apply D:/krita-baron-android-native/artifacts/orchestrion-catalog-v0133.patch
```

Потребуется перезапуск Node.js сервера сайта. Пересборка фронтенда, миграция
БД и переустановка native-компилятора не требуются. До установки серверного
патча кеш миниатюр и каталог работают; API подробной информации вернёт 404.
