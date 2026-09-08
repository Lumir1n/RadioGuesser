# Настройка Supabase для RadioGuesser

## 1. Создание проекта

1. Зайдите на https://supabase.com
2. Нажмите **New Project**
3. Укажите название: `radioguesser`
4. Сохраните пароль от базы данных — он нужен для строки подключения

## 2. Применение миграций

Откройте **SQL Editor** в дашборде Supabase и выполните файлы по порядку:

```
Database/Migrations/001_enable_postgis.sql
Database/Migrations/002_create_radio_tables.sql
Database/Migrations/003_create_game_tables.sql
Database/Migrations/004_scoring_function.sql
```

Для каждого файла: скопируйте содержимое → вставьте в SQL Editor → нажмите **Run**.

## 3. Настройка строки подключения в бэкенде

Откройте файл:
```
Server/Radioguesser.Server/appsettings.json
```

Замените плейсхолдеры реальными значениями из Supabase дашборда
(Settings → Database → Connection string → .NET):

```json
"DefaultConnection": "Host=db.YOURPROJECT.supabase.co;Database=postgres;Username=postgres;Password=YOUR_PASSWORD;SSL Mode=Require;Trust Server Certificate=true"
```

## 4. Supabase URL и Anon Key

В том же файле замените:
```json
"Supabase": {
  "Url": "https://YOURPROJECT.supabase.co",
  "AnonKey": "YOUR_ANON_KEY"
}
```

Значения найдёте в: Settings → API → Project URL и anon public key.

## 5. Настройка в Unreal (DefaultGame.ini)

Откройте `Config/DefaultGame.ini` и добавьте:
```ini
[RadioGuesser]
BackendUrl=http://localhost:5000
```

Позже замените `localhost:5000` на реальный адрес продакшн-сервера.
