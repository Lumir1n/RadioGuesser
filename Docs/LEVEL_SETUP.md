# Phase 3 — Level Setup Guide

Следуй этим шагам **после успешного билда** в Visual Studio.

---

## Шаг 1 — Regenerate + Build

1. Закрой Unreal Editor и Visual Studio
2. Правая кнопка на `RadioGuesser.uproject` → **Generate Visual Studio project files**
3. Открой `.sln` → **Build → Build Solution** (F7)
4. Дождись `Build succeeded`
5. Открой Unreal Editor из Visual Studio (кнопка Play в VS, или дважды кликни `.uproject`)

---

## Шаг 2 — Создать папки в Content Browser

В **Content Browser** (внизу экрана) создай папки:
- `Content/Maps`
- `Content/UI`
- `Content/Input`

Правая кнопка в пустом месте Content Browser → **New Folder**

---

## Шаг 3 — Создать уровень WorldMap

1. Меню: **File → New Level**
2. Выбери **Empty Level** (пустой — не OpenWorld)
3. Меню: **File → Save Current Level As...**
4. Сохрани в `Content/Maps/` с именем **WorldMap**

---

## Шаг 4 — Добавить Cesium в уровень

1. В Unreal Editor: меню **Cesium → Cesium** (или найди в меню сверху)
2. Нажми **Add Blank 3D Tiles Tileset** — появится `CesiumGeoreference` и `Cesium3DTileset`
3. Нажми **Connect to Cesium ion** — войди/создай бесплатный аккаунт на cesium.com
4. Нажми **Add Cesium World Terrain** — добавит глобус с рельефом
5. Нажми **Add Bing Maps Aerial imagery** — добавит спутниковые снимки (бесплатно для разработки)

После этого в уровне должен появиться земной шар.

---

## Шаг 5 — Настроить камеру

1. В **Place Actors** (левая панель) найди и перетащи **Cine Camera Actor** или обычный **Camera**
2. Расположи камеру так чтобы глобус был виден — примерно Z=+50 000 000 (50 000 км)
3. Укажи **Camera → Look At** на центр глобуса

Либо просто оставь дефолтную камеру — Blueprint в шаге 9 настроит её.

---

## Шаг 6 — Создать Blueprint PlayerController

1. **Content Browser** → правая кнопка → **Blueprint Class**
2. Ищи **RGPlayerController** → **Select** → назови **BP_RadioGuesserPC**
3. Сохрани в `Content/`
4. Открой BP_RadioGuesserPC двойным кликом

**В Details панели слева:**
- `Default Mapping Context` → пока оставь пустым (заполним ниже)
- `Map Click Action` → пока оставь пустым
- `Confirm Guess Action` → пока оставь пустым

---

## Шаг 7 — Создать Input Actions и Mapping Context

1. **Content/Input** → правая кнопка → **Input → Input Action** → назови **IA_MapClick**
   - Open **IA_MapClick** → Value Type = `Boolean`
2. Ещё одна **Input Action** → **IA_ConfirmGuess**
   - Value Type = `Boolean`
3. **Content/Input** → правая кнопка → **Input → Input Mapping Context** → назови **IMC_RadioGuesser**
4. Открой **IMC_RadioGuesser** двойным кликом:
   - Нажми **+** → выбери **IA_MapClick** → нажми **+** под ним → выбери **Left Mouse Button**
   - Нажми **+** → выбери **IA_ConfirmGuess** → нажми **+** → выбери **Enter**
5. Сохрани

Теперь вернись в **BP_RadioGuesserPC**:
- `Default Mapping Context` → **IMC_RadioGuesser**
- `Map Click Action` → **IA_MapClick**
- `Confirm Guess Action` → **IA_ConfirmGuess**

---

## Шаг 8 — Создать BP_RadioGuesserGameMode

1. **Content Browser** → **Blueprint Class** → ищи **RGGameMode** → назови **BP_RadioGuesserGM**
2. Открой его:
   - `Player Controller Class` → **BP_RadioGuesserPC**
3. Сохрани

---

## Шаг 9 — Настроить уровень WorldMap

1. Открой уровень **WorldMap** (если не открыт)
2. **World Settings** (меню Window → World Settings):
   - `Game Mode Override` → **BP_RadioGuesserGM**
3. Перетащи в уровень **BP_RadioGuesserPC** как spawner... нет — он назначается через GameMode.

---

## Шаг 10 — Добавить ARGCesiumMapManager в уровень

1. В **Place Actors** или **Content Browser** найди `RGCesiumMapManager`
2. Перетащи в уровень — появится актор
3. В **Details** этого актора:
   - `Cesium Georeference` → выбери **CesiumGeoreference** из уровня (пипетка)

---

## Шаг 11 — Создать и подключить MediaSoundComponent для радио

1. В уровне создай новый Blueprint Actor: **Content Browser** → BP Class → Actor → **BP_RadioPlayer**
2. В BP_RadioPlayer добавь компонент **MediaSoundComponent**
3. В BeginPlay Blueprint:
   - Получи `GameInstance` → `Get Subsystem (RGRadioSubsystem)`
   - Соедини `Get Media Player` → MediaSoundComponent → `Set Media Player`
4. Сохрани и размести в уровне

---

## Шаг 12 — Создать простейший HUD Blueprint

1. **Content/UI** → **Blueprint Class** → ищи **RGGameHUDWidget** → назови **WBP_GameHUD**
2. Открой WBP_GameHUD
3. Добавь в Designer панели:
   - **Text Block** (верхний левый) → имя переменной `Txt_RoundInfo`, текст по умолчанию "ROUND 1 / 5"
   - **Text Block** (верхний правый) → `Txt_Timer`, "02:00"
   - **Text Block** (по центру снизу) → `Txt_RadioStatus`, "🔊 LIVE RADIO"
   - **Button** (снизу по центру) → `Btn_ConfirmGuess`, текст "CONFIRM GUESS"
4. В **Graph** переопределяй Blueprint-события:
   - `Event OnRoundDataUpdated`: установи Txt_RoundInfo в `"ROUND " + RoundNumber + " / " + TotalRounds`
   - `Event OnTimerUpdated`: установи Txt_Timer в `FormatTime(SecondsRemaining)`
   - `Event OnRadioStateUpdated`: установи Txt_RadioStatus текст по статусу
   - `Event OnGuessPinUpdated`: включай/выключай Btn_ConfirmGuess
5. Btn_ConfirmGuess → On Clicked → вызови `OnConfirmGuessClicked`

---

## Шаг 13 — Показать HUD в GameMode

В **BP_RadioGuesserGM** в BeginPlay:
```
Create Widget (WBP_GameHUD) → Add to Viewport
```

---

## Шаг 14 — Запустить бэкенд

Открой PowerShell в папке `Server/Radioguesser.Server/`:
```powershell
dotnet run
```
Сервер стартует на `http://localhost:5000`.

Затем нажми **Play** в Unreal Editor и вызови в Blueprint или в Console:
```
RGMatchSubsystem.StartSoloMatch
```

---

## Шаг 15 — Тест полного цикла

1. Нажать Play в Unreal Editor
2. Должен загрузиться Cesium глобус
3. В Output Log должно появиться: `RGMatchSubsystem initialised`
4. Вызови Start Solo Match из BP или кнопки
5. Должен прийти ответ от бэкенда с roundToken + streamUrl
6. Радио должно начать играть
7. Кликни на глобус — появится маркер
8. Нажми Confirm Guess
9. В Output Log: `Solo guess — distance=Xm score=Y`
10. Результат должен отобразиться в HUD

---

## Возможные проблемы

**Cesium не видит земной шар:** убедись что подключён к Cesium ion аккаунту (бесплатно).

**Радио не играет:** некоторые потоки используют нестандартные форматы. Электра Media Player
поддерживает HLS, MP3, AAC. Если поток не работает — импортируй другую станцию.

**Backend 503:** нет станций с координатами. Запусти geo-sweep импорт:
```powershell
Invoke-WebRequest -Uri "https://dmwnegtvotnrajzpyfad.supabase.co/functions/v1/import-radio-stations" -Method POST -Body '{"geoOnly":true}' -ContentType "application/json" -UseBasicParsing
```
