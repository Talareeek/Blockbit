# Blockbit Chunk Inspector

Konsolowy inspektor plików chunku gry **Blockbit**.  
Wczytuje binarny plik chunku i rysuje jego przekrój jako kolorową mapę ASCII — tak jak wyglądałby świat oglądany **z boku**.

## Budowanie

```bash
cd inspector
./build.sh
```

Wymaga `g++` z obsługą C++17.

## Użycie

```bash
./inspector <plik_chunku> [min_y] [max_y]
```

| Argument     | Opis                                              |
|--------------|---------------------------------------------------|
| `plik_chunku`| Ścieżka do pliku chunku, np. `../saves/Swiat/chunk_8` |
| `min_y`      | (opcjonalny) dolna granica zakresu Y (domyślnie `0`) |
| `max_y`      | (opcjonalny) górna granica zakresu Y (domyślnie `255`) |

### Przykłady

```bash
# Cały chunk (256 wierszy)
./inspector ../saves/Swiat/chunk_8

# Tylko okolice powierzchni (poziomy 60–90)
./inspector ../saves/Swiat/chunk_8 60 90

# Sam dół świata (bedrock)
./inspector ../saves/Swiat/chunk_8 0 5
```

## Format wyjścia

```
=== LEGENDA ===
  S  Stone (id=1)
  T  Grass (id=2)
  ...

Plik: ../saves/Swiat/chunk_8
Zakres Y: 60 - 90

    0123456789012345
    ----------------
 90|                
 89|                
...
 75|   TZZZZZZSSSSSS
 74|WWTZZZZZZSSSSSSS
 73|ZZZZZZZZSSSSSSSS
...
 60|SSSSSSSSSSSSSSSS
```

### Orientacja

Wyświetlanie odwzorowuje **rzeczywisty układ świata**:

```
góra ekranu  = wysokie Y = niebo / powietrze
dół ekranu   = niskie Y  = ziemia / bedrock
```

- Oś pozioma (`x`) → 0 do 15 (szerokość chunku = 16 bloków)  
- Oś pionowa (`y`) → wyświetlana od `max_y` (góra) do `min_y` (dół)

### Legenda symboli

| Symbol | Kolor         | Blok         | ID |
|--------|---------------|--------------|----|
| ` `    | —             | Air (powietrze) | 0 |
| `S`    | szary         | Stone (kamień) | 1 |
| `T`    | zielony jasny | Grass (trawa)  | 2 |
| `Z`    | żółty         | Dirt (ziemia)  | 3 |
| `C`    | biały         | Cobblestone    | 4 |
| `O`    | fioletowy     | Obsidian       | 5 |
| `B`    | szary         | Bedrock        | 6 |
| `W`    | niebieski jasny | Water (woda) | 7 |
| `I`    | biały         | Iron Ore (ruda żelaza) | 8 |
| `G`    | żółty jasny   | Gold Ore (ruda złota)  | 9 |
| `D`    | cyan          | Diamond Ore (diament)  | 10 |
| `L`    | żółty         | Oak Log (pień dębu)    | 11 |
| `F`    | zielony       | Oak Leaves (liście)    | 12 |
| `?`    | czerwony      | Nieznany blok          | — |

## Format pliku chunku

Plik chunku to surowe dane binarne — **brak nagłówka**.

| Pole      | Typ        | Rozmiar | Opis                        |
|-----------|------------|---------|-----------------------------|
| `id`      | `uint32_t` | 4 bajty | ID bloku (little-endian)    |
| `metadata`| `uint8_t`  | 1 bajt  | Metadane bloku              |

Bloki zapisane są w pętli `y` zewnętrznej, `x` wewnętrznej:

```
for y in 0..255:
    for x in 0..15:
        write uint32_t id
        write uint8_t  metadata
```

Rozmiar pliku: `16 × 256 × 5 = 20 480 bajtów`.

Adres bajtu dla bloku `(x, y)`:

```
offset = (y * 16 + x) * 5
```
