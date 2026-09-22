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

- Linux (o WSL en Windows) con `gcc` y `make` instalados.
- No requiere librerías externas.

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

## Ejecución

```bash
./mishell
```

o directamente:

```bash
make run
```

Esto abre el prompt de la shell (`mishell:/ruta/actual$`), donde se pueden
escribir comandos hasta terminar con `exit`, `Ctrl+D`, o cerrando la terminal.

## Comando `pmon`
 
```
pmon [segundos]
```
 
Si se omite `segundos`, se usa 2 por defecto. Muestra una tabla que se
refresca periódicamente con los jobs en background lanzados por la propia
shell
