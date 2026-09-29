# miShell — Tarea 1, Sistemas Operativos 2026

Shell de texto simplificada para Linux, implementada en C (POSIX), que soporta
ciclo básico de comandos, comandos internos, redirección de E/S, pipes de
largo arbitrario, ejecución en background con manejo de señales, y un
comando de monitoreo de procesos (`pmon`) basado en `/proc`.

## Integrantes

- Jocabed López
- Danitza Ávila
- Ignacio Jara

## Requisitos
- Linux (o WSL en Windows) con `gcc` y `make`.
- Para la versión con historial de comandos (la que compila `make`): biblioteca
  **GNU Readline** con sus headers.
```bash
  sudo apt install build-essential libreadline-dev
```

## Compilación
El código soporta dos formas de compilar, controladas por la macro
`USE_READLINE`:
 
**1) Con `make` (recomendado, incluye el bonus de historial), desde la raíz de la tarea:**
 
```bash
make
```
 
Internamente corre:
 
```bash
gcc -Wall -Wextra -std=gnu11 -DUSE_READLINE -o mishell src/mishell.c -lreadline
```

**2) Sin Readline (sin historial con flechas, sin dependencias externas):**
 
```bash
gcc -Wall -Wextra -std=gnu11 -o mishell src/mishell.c
```
 
Ambas formas compilan sin advertencias. Para borrar el ejecutable:
 
```bash
make clean
```
 
## Ejecución
 
```bash
./mishell
```
 
o directamente:
 
```bash
make run
```
 
Esto abre el prompt de la shell (`mishell:/ruta/actual$`), donde se pueden
escribir comandos hasta terminar con `exit` o `Ctrl+D`. `Ctrl+C` y `Ctrl+\`
no cierran la shell.

### Comandos internos
 
| Comando | Descripción |
|---------|-------------|
| `cd [dir]` | Cambia el directorio de trabajo. Sin argumentos va a `$HOME`. |
| `exit [n]` | Termina la shell con código `n` (0 por defecto). |
| `jobs` | Lista los jobs en background activos (`[id] Ejecutando comando`). |
| `pmon [segundos]` | Monitor de procesos en background. |

## Comando `pmon`
 
```
pmon [segundos]
```
 
Si se omite `segundos`, se usa 2 por defecto. Muestra una tabla que se
refresca periódicamente con los jobs en background lanzados por la propia
shell
