# Intel 8080 Space Invaders Emulator

A complete **Intel 8080** CPU emulator built from scratch in C, capable of running the original **Space Invaders (1978)** arcade machine at full speed with real-time **SDL2** graphics, hardware shift register logic, and full keyboard input mapping.

---

## Controls

| Key | Action |
| :--- | :--- |
| **`C`** | Insert Coin (Credit) |
| **`1`** | Start (1 Player) |
| **`A`** / **`←`** | Move Left |
| **`D`** / **`→`** | Move Right |
| **`Space`** | Shoot |

---

## Building and Running

### Prerequisites
- **GCC Compiler** (via MinGW / MSYS2 on Windows or native GCC on Linux/macOS).
- **SDL2 Library** installed.

### ROM Files
Place the original *Space Invaders* ROM files in the root directory alongside your source code:
- `invaders.h` (Address: `0x0000`)
- `invaders.g` (Address: `0x0800`)
- `invaders.f` (Address: `0x1000`)
- `invaders.e` (Address: `0x1800`)

### Compilation (Windows / MSYS2)
Run the following command in PowerShell / Terminal:

```powershell
gcc main.c -I C:\msys64\ucrt64\include -o main.exe -lSDL2 -lSDL2main
```

### AI Use
Most opcodes in the Emulate8080p function follow similar implementation patterns, differing primarily in the variables involved. As a result, I used AI assistance to implement approximately 50% of the function.
