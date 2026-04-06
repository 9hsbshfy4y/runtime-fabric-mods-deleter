# runtime-fabric-mods-deleter

Windows utility that finds and releases file handles held by `java.exe` / `javaw.exe`, allowing you to delete or replace Fabric mod `.jar` files without restarting Minecraft.

## How it works

1. You enter the file name (or part of the path) of the mod you want to delete
2. The tool scans all running Java processes for open file handles and loaded DLL modules matching your query
3. Found handles are automatically closed and modules unloaded
4. The file is now unlocked and can be freely deleted or replaced

## Usage

1. Run `jardeleter.exe` as Administator
2. Enter the mod file name, e.g. `sodium-0.6.jar`

```
@jniadmin

File path or name:
> sodium
[*] Target: "sodium" (java/javaw only)
[*] Found 1 java process(es)
[*] Scanning handles...
[*] Scanning modules...
[*] Found 2 handle(s), closing...

  PID=12340  javaw.exe      handle=0x03A4 -> OK
  PID=12340  javaw.exe      base=00007FFB1C3A0000 -> OK

[*] Closed 2 / 2
[+] File should now be deletable
```

## Requirements

- Windows 10 / 11
- Run as Administrator

---

# runtime-fabric-mods-deleter (RU)

Утилита для Windows, которая находит и освобождает файловые дескрипторы, удерживаемые процессами `java.exe` / `javaw.exe`, позволяя удалять или заменять `.jar` файлы модов Fabric без перезапуска Minecraft.

## Как это работает

1. Вы вводите имя файла (или часть пути) мода, который хотите удалить
2. Утилита сканирует все запущенные Java-процессы на наличие открытых файловых дескрипторов и загруженных DLL-модулей, совпадающих с запросом
3. Найденные дескрипторы автоматически закрываются, модули выгружаются
4. Файл разблокирован и может быть удалён или заменён

## Использование

1. Запустите `jardeleter.exe` от имени Администратора
2. Введите имя файла мода, например `sodium-0.6.jar`

```
@jniadmin

File path or name:
> sodium
[*] Target: "sodium" (java/javaw only)
[*] Found 1 java process(es)
[*] Scanning handles...
[*] Scanning modules...
[*] Found 2 handle(s), closing...

  PID=12340  javaw.exe      handle=0x03A4 -> OK
  PID=12340  javaw.exe      base=00007FFB1C3A0000 -> OK

[*] Closed 2 / 2
[+] File should now be deletable
```

## Требования

- Windows 10 / 11
- Запуск от имени Администратора
