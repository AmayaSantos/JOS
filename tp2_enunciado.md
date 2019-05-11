# TP2: Procesos de usuario[^src]

[^src]: Material original en inglés: [Lab 3: User Environments](https://pdos.csail.mit.edu/6.828/2017/labs/lab3/)

Este TP guía la implementación de procesos de usuario. En particular, cubre la ejecución de _un_ solo proceso, esto es: una vez inicializado el sistema, el kernel lanzará el programa indicado por línea de comandos y, una vez finalizado este, volverá al monitor de JOS. En futuros TPs se abordará la ejecución de múltiples programas simultáneamente.

Los programas de usuario se encuentran en el directorio _user_, y la biblioteca estándar en _lib_. Por ejemplo, el programa _user/hello.c:_

```c
#include <inc/lib.h>

void umain(int argc, char **argv) {
    cprintf("hello, world\n");
    cprintf("i am environment %08x\n", thisenv->env_id);
}
```

una vez finalizada la [parte 4](tp2.md#tp2-interrupts-y-syscalls) se va a poder ejecutar mediante `make run-hello-nox`:

```
$ make run-hello-nox
[00000000] new env 00001000
hello, world
i am environment 00001000
[00001000] exiting gracefully
[00001000] free env 00001000
Destroyed the only environment - nothing more to do!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K>
```

**Nota:** JOS usa el término _environment_ para referirse a proceso, porque la semántica de los procesos en JOS diverge de la semántica típica en Unix. En esta consigna, se usa “proceso” directamente para _environment_.


## Índice
{:.no_toc}

* TOC
{:toc}


## Código

El código base para el TP se encuentra en la rama _tp2_ del [repositorio](tps.md#jos-repo). Esta nueva base añade un número de archivos:

Dir      | File          | Description
:-       | :-            | :-
`inc/ `  | `env.h`       | Public definitions for user-mode environments
         | `trap.h`      | Public definitions for trap handling
         | `syscall.h`   | Public definitions for system calls from user environments to the kernel
         | `lib.h`       | Public definitions for the user-mode support library
-|-
`kern/ ` | `env.h`       | Kernel-private definitions for user-mode environments
         | `env.c`       | Kernel code implementing user-mode environments     |
         | `trap.h`      | Kernel-private trap handling definitions            |
         | `trap.c`      | Trap handling code                                  |
         | `trapentry.S` | Assembly-language trap handler entry-points         |
         | `syscall.h`   | Kernel-private definitions for system call handling |
         | `syscall.c`   | System call implementation code                     |
-|-
`lib/ `  | `Makefrag`    | Makefile fragment to build std library, `obj/lib/libjos.a`
         | `entry.S`     | Assembly-language entry-point for user environments
         | `libmain.c`   | User-mode library setup code called from _entry.S_
         | `syscall.c`   | User-mode system call stub functions
         | `console.c`   | User-mode implementations of `putchar()` and `getchar()`
         | `exit.c`      | User-mode implementation of `exit()`                                    |
         | `panic.c`     | User-mode implementation of `panic()`


## Parte 1: Inicializaciones

```
$ git show --stat tp2_parte1
 kern/env.c  | 10 ++++++++++
 kern/pmap.c |  5 +++++
 2 files changed, 15 insertions(+)

$ wc --words < TP2.md
253
```

En JOS, toda la información de un proceso de usuario se guarda en un **`struct Env`**, el cual se define en el archivo [inc/env.h][envstruct]. _Env_ contiene, notablemente, los siguientes campos:

  - `env_id:` identificador numérico del proceso[^env_id]
  - `env_parent_id:` identificador numérico del proceso padre
  - `env_status:` estado del proceso (en ejecución, listo para ejecución, bloqueado…)

[^env_id]: El identificador de proceso es único a lo largo de la ejecución del sistema, esto es: una vez termina un proceso con identificador _N_, jamás se vuelve a usar ese valor como identificador. De esto se encarga la función `env_alloc()`, ya implementada.

Así como:

  - `env_pgdir:` el _page directory_ del proceso
  - `env_tf:` un **`struct Trapframe`** (definido en [inc/trap.h][tfstruct]) donde guardar el estado de la CPU (registros, etc.) cuando se interrumpe la ejecución del proceso. De esta manera, al reanudar el proceso es posible restaurar con exactitud su estado anterior.

[tfstruct]: https://github.com/fisop/jos/blob/4f94e0629fe/inc/trap.h#L58
[envstruct]: https://github.com/fisop/jos/blob/4f94e0629fe/inc/env.h#L46

La constante **`NENV`**, por su parte, limita la cantidad máxima de procesos concurrentes en el sistema; el límite actual es 1024. Este límite facilita la creación de procesos de la siguiente manera:

  - al arrancar el sistema, se _pre-reserva_ un arreglo de `NENV` elementos `struct Env` (de manera similar al arreglo [pages](tp1.md#pt1-memoria-fisica) del TP1)

  - al crear procesos, no será necesario reservar memoria de manera dinámica, sino que se usan los `struct Env` del arreglo

  - el arreglo se configura en una lista enlazada `env_free_list` de la que se puede obtener el siguiente `Env` libre en _O(1)_. Cuando se destruye un proceso, se reinserta su _struct_ en la lista.

Tanto el arreglo como la lista de procesos libres se definen en `kern/env.c`:

```c
// Arreglo de procesos (variable global, de longitud NENV).
struct Env *envs = NULL;

// Lista enlazada de `struct Env` libres.
static struct Env *env_free_list;

// Proceso actualmente en ejecución (inicialmente NULL).
struct Env *curenv = NULL;
```


### Tarea: mem_init_envs
{: #mem_init_envs}

1.  Añadir a `mem_init()` código para crear el arreglo de procesos `envs`. Se debe determinar cuánto espacio se necesita, e inicializar a 0 usando `memset()`.

2.  Mapear `envs`, con permiso de sólo lectura para usuarios, en `UENVS` del page directory del kernel.[^uenvsro]

[^uenvsro]: Así, cuando el kernel necesite modificar el arreglo, lo hará mediante la variable global `envs`; pero los procesos de usuario podrán _consultar_ la información en la dirección `UENVS`.

Tras esta tarea, la función `check_kern_pgdir()` debe reportar éxito:

```
$ make qemu-nox
Physical memory: 131072K available, base = 640K ...
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
check_page_installed_pgdir() succeeded!
```


### Tarea: env_init
{: #env_init}

Inicializar, en la función `env_init()` de _env.c_, la lista de `struct Env` libres. Para facilitar la corrección automática, se pide que la lista enlazada siga el orden del arreglo (esto es, que `env_free_list` apunte a `&envs[0]`).


### Tarea: env_alloc
{: #env_alloc}

La función `env_alloc()`, ya [implementada][env_alloc], encuentra un `struct Env` libre y lo inicializa para su uso. Entre otras cosas:

  - le asigna un identificador numérico único
  - lo marca como listo para ejecutar (`ENV_RUNNABLE`)
  - inicializa sus segmentos de datos y código con permisos adecuados

Se pide leer la función `env_alloc()` en su totalidad y responder las siguientes preguntas:

  1. ¿Qué identificadores se asignan a los primeros 5 procesos creados? (Usar base hexadecimal.)

  2. Supongamos que al arrancar el kernel se lanzan `NENV` procesos a ejecución. A continuación se destruye el proceso asociado a `envs[630]` y se lanza un proceso que cada segundo muere y se vuelve a lanzar. ¿Qué identificadores tendrá este proceso en sus sus primeras cinco ejecuciones?

<!-- 3. TODO EFLAGS?? -->

[env_alloc]: https://github.com/fisop/jos/blob/4f94e0629fe/kern/env.c#L191


### Tarea: env_setup_vm

Desde `env_alloc()` se llama a `env_setup_vm()` (no implementada) para configurar el _page directory_ correspondiente al nuevo proceso. Implementar esta función siguiendo las instrucciones en el código fuente.

**Ayuda:** es una función _muy_ corta, y apenas le faltan 3 líneas de código por añadir. Se permite usar `memcpy()`.


### Tarea: env_init_percpu
{: #env_init_percpu}

La función `env_init()` hace una llamada a `env_init_percpu()` para configurar sus segmentos. Antes de ello, se invoca a la instrucción `lgdt`. Responder:

  - ¿Cuántos bytes escribe la función `lgdt`, y dónde?
  - ¿Qué representan esos bytes?

Bibliografía relevante:

  - **\[IA32-3A]:** §2.1.1 — §2.4 — §3.2.2 — §3.5.1 — §3.4
  - documentación sobre [inline assembly](kit.md#x86-assembly) de GCC


## Parte 2: Carga de ELF
{: #pt2-elf}

```
$ git show --stat tp2_parte2
 kern/env.c | 32 ++++++++++++++++++++++
 1 file changed, 32 insertions(+)
```

El segundo paso para lanzar un proceso, tras inicializar su `struct Env`, es copiar el código del programa a memoria para que pueda ser ejecutado. Normalmente, el código se carga del sistema de archivos, pero en JOS no tenemos soporte para discos todavía.

Por el momento, para ejecutar un programa en JOS, el linker _empotra_ el código máquina del programa al final de la imagen del kernel. La posición y tamaño de cada programa disponible se marca con símbolos en el binario. Por ejemplo, el código para ejecutar _user/hello.c_ se puede encontrar así:

```
$ grep user_hello obj/kern/kernel.sym
00008948 A _binary_obj_user_hello_size
f01217f4 D _binary_obj_user_hello_start
f012a13c D _binary_obj_user_hello_end
```

Es decir, 549 KiB a partir de la dirección de enlazado `0xf01217f4`. Ahí en realidad se encuentra un archivo ELF (ver `readelf -a obj/user/hello`).

En la parte 3, se lanzará el programa mediante:

```
// Este símbolo marca el comienzo del ELF user/hello.c.
extern uint8_t _binary_obj_user_hello_start[];

// No es necesario indicar el tamaño; env_create()  lo
// encuentra vía las cabeceras ELF.
env_create(_binary_obj_user_hello_start, ENV_TYPE_USER);
```

O, de manera más sencilla usando la macro `ENV_CREATE`:

    ENV_CREATE(user_hello, ENV_TYPE_USER);


### Tarea: region_alloc
{: #region_alloc}

Se puede usar `page_insert()` para reservar 4 KiB de memoria en el espacio de memoria de un proceso. Para facilitar la carga del código en memoria, la función auxiliar `region_alloc()` reserva una cantidad arbitraria de memoria.

Se pide la función `region_alloc()` siguiendo las instrucciones en su documentación. Atención a los alineamientos.

**Ayuda:** usar las funciones `page_alloc()` y `page_insert()`.


### Tarea: load_icode
{: #load_icode}

La función `load_icode()` recibe un puntero a un binario ELF, y lo carga en el espacio de memoria de un proceso en las direcciones que corresponda. En particular, para cada uno de los _e_phnum_ segmentos o _program headers_ de tipo PT_LOAD:

  - reserva _memsz_ bytes de memoria con `region_alloc()` en la dirección _va_ del segmento
  - copia _filesz_ bytes desde _binary + offset_ a _va_
  - escribe a 0 el resto de bytes desde _va + filesz_ hasta _va + memsz_

Se debe, además, configurar el _entry point_ del proceso.

**Ayuda:** usar las funciones `memcpy()` y `memset()` en el espacio de direcciones del proceso, y la documentación de la función.

Bibliografía:

  - la sección [Program headers][phwp] de la entrada en Wikipedia sobre ELF
  - `readelf -l obj/user/hello`

[phwp]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#Program_header


## Parte 3: Lanzar procesos

```
$ git show --stat tp2_parte3
 kern/env.c | 17 ++++++++++++++++-
 1 file changed, 16 insertions(+), 1 deletion(-)

$ wc --words < TP2.md
687
```

Una vez llamado a `env_init()`, el kernel llama a `env_create()` y `env_run()`:

  - `env_create()` combina todas las funciones de partes anteriores para dejar el proceso listo para ejecución

  - `env_run()` se invoca cada vez que se desea pasar a ejecución un proceso que está listo


### Tarea: env_create
{: #env_create}

Implementar la función `env_create()` siguiendo la documentación en el código.

Si, por ejemplo, `env_alloc()` devuelve un código de error, se puede usar el modificador `"%e"` de la función `panic()` para formatear el error:

    if (err < 0)
        panic("env_create: %e", err);


### Tarea: env_pop_tf
{: #env_pop_tf}

La función `env_pop_tf()` ya implementada es en JOS el último paso de un _context switch_ a modo usuario. Antes de implementar `env_run()`, responder a las siguientes preguntas:

 1. ¿Qué hay en `(%esp)` tras el primer `movl` de la función?
 2. ¿Qué hay en `(%esp)` justo antes de la instrucción `iret`? ¿Y en `8(%esp)`?
 3. En la documentación de `iret` en **\[IA32-2A]** se dice:

    > If the return is to another privilege level, the IRET instruction also pops the stack pointer and SS from the stack, before resuming program execution.

    ¿Cómo puede determinar la CPU si hay un cambio de ring (nivel de privilegio)?


### Tarea: env_run
{: #env_run}

Implementar la función `env_run()` siguiendo las instrucciones en el código fuente. Tras arrancar el kernel, esta función lanza en ejecución el proceso configurado en `i386_init()`.

**Nota:** El programa por omisión, _user/hello.c_, imprime una cadena en pantalla mediante la llamada al sistema `sys_cputs()`. Al no haber implementado aún soporte para llamadas al sistema, se observará una triple falla en QEMU (o un loop de reboots, según la versión de QEMU). En la tarea a continuación se guía el uso de GDB para averiguar cuándo aborta exactamente el programa.


### Tarea: gdb_hello
{: #gdb_run}

Arrancar el programa _hello.c_ bajo GDB. Se puede usar, en lugar de `make qemu-gdb-nox`:

    $ make run-hello-nox-gdb
    $ make gdb

Se pide mostrar una sesión de GDB con los siguientes pasos:

  1. Poner un breakpoint en `env_pop_tf()` y continuar la ejecución hasta allí.

  2. En QEMU, entrar en modo monitor (`Ctrl-a c`), y mostrar las cinco primeras líneas del comando `info registers`.

  3. De vuelta a GDB, imprimir el valor del argumento _tf_:

         (gdb) p tf
         $1 = ...

  4. Imprimir, con `x/Nx tf` tantos enteros como haya en el struct _Trapframe_ donde `N = sizeof(Trapframe) / sizeof(int)`. 

      (Se puede calcular fuera de _GDB_, o mediante el comando: `print sizeof(struct Trapframe) / sizeof(int)` utilizando ese resultado en `x/Nx tf`)

  5. Avanzar hasta justo después del `movl ...,%esp`, usando `si M` para ejecutar tantas instrucciones como sea necesario **en un solo paso**:

         (gdb) disas
         (gdb) si M

  6. Comprobar, con `x/Nx $sp` que los contenidos son los mismos que _tf_ (donde `N` es el tamaño de _tf_).

  7. Explicar con el mayor detalle posible cada uno de los valores. Para los valores no nulos, se debe indicar dónde se configuró inicialmente el valor, y qué representa.

  8. Continuar hasta la instrucción `iret`, sin llegar a ejecutarla. Mostrar en este punto, de nuevo, las cinco primeras líneas de `info registers` en el monitor de QEMU. Explicar los cambios producidos.

  9. Ejecutar la instrucción `iret`. En ese momento se ha realizado el cambio de contexto y los símbolos del kernel ya no son válidos.

      - imprimir el valor del contador de programa con `p $pc` o `p $eip`
      - cargar los símbolos de _hello_ con `symbol-file obj/user/hello`
      - volver a imprimir el valor del contador de programa

     Mostrar una última vez la salida de `info registers` en QEMU, y explicar los cambios producidos.

  10. Poner un breakpoint temporal (`tbreak`, se aplica una sola vez) en la función `syscall()` y explicar qué ocurre justo tras ejecutar la instrucción `int $0x30`. Usar, de ser necesario, el monitor de QEMU.

**Ayuda:** muy posiblemente GDB no encuentre la variable _tf_ definida. En ese caso, se recomienda aumentar el nivel de debug en el Makefile, usando `-ggdb3` en lugar de `-gstabs`.


## Parte 4: Interrupts y syscalls

Una vez lanzado un proceso, este debe poder interaccionar con el sistema operativo para realizar tareas como imprimir por pantalla o leer archivos. Asimismo, el sistema operativo debe estar preparado para manejar excepciones que deriven de la ejecución de un proceso (por ejemplo, si realiza una división por cero o dereferencia un puntero nulo).

```
$ git show --stat tp2_parte4
 kern/syscall.c   | 18 ++++-
 kern/trap.c      | 50 ++++++++++---
 kern/trapentry.S | 59 ++++++++++++++++
 3 files changed, 117 insertions(+), 10 deletions(-)

$ wc --words < TP2.md
960
```

Bibliografía para esta parte:

<!-- TODO: ¿encontrar mejores explicaciones? -->

  - Sección [Handling Interrupts and Exceptions][lab3ie] y siguientes de la versión MIT de este TP.

  - Capítulo 6 de **\[IA32-3A]**: _Interrupts and Exception Handling_ (secciones 6.1 a 6.6 y 6.10 a 6.12).

  - Recordatorio de excepciones: capítulo 8 de **\[BRY2]** (introducción y sección 8.1).

  - Recordatorio de cambio de privilegio, capítulo 5 de **\[IA32-3A]**: _Protection_ (en especial secciones 5.2, 5.5 y 5.8).

[lab3ie]: https://pdos.csail.mit.edu/6.828/2017/labs/lab3/#Handling-Interrupts-and-Exceptions


### Tarea: kern_idt
{: #kern_idt}

En JOS, todas las excepciones, interrupciones y traps se derivan a la función `trap()`, definida en _trap.c_. Esta función recibe un puntero a un  _struct Trapframe_ como parámetro, por lo que cada interrupt handler debe, en cooperación con la CPU, dejar uno en el stack antes de llamar a `trap()`.

Se debe definir ahora en JOS interrupt handlers para todas las interrupciones de la arquitectura x86 (ver Tabla 6-1 en **\[IA32-3A]**: _Protected-Mode Exceptions and Interrupts_). Esto se realiza en dos partes:

  1. en `trap_init()`, se usará la macro `SETGATE` para configurar la tabla de descriptores de interrupciones (IDT), alojada en el arreglo global `idt[]`.

  2. previamente, se debe definir cada interrupt handler en _trapentry.S_. Para no repetir demasiado código, se proporcionan las macros `TRAPHANDLER` y `TRAPHANDLER_NOEC` (leer cuidadosamente su documentación). Los nombres de los interrupt handlers tienen que ser distintos a cualquier función definida ya en JOS, por ejemplo el handler para designar a la interrupción _breakpoint_ no se debería llamar `breakpoint` pues ya existe una función `breakpoint()` definida en el kernel. Un patrón posible puede ser `trap_N`, siendo N el número de la interrupción según la tabla de Intel.

     Ambas macros comparten código común en una función `_alltraps`, que se debe implementar también en assembler.

     **Ayuda**: cargar GD_KD en `%ds` y `%es` mediante un registro intermedio de 16 bits (por ejemplo, `%ax`). Considerar, además, que `GD_KD` es una constante numérica, no una dirección de memoria (`‘mov $GD_KD’` vs `‘mov GD_KD’`).

Tras esta tarea, deben pasar las siguientes pruebas:

```
$ make grade
divzero: OK (0.7s)
softint: OK (0.9s)
badsegment: OK (0.9s)
Part A score: 3/3
```

Responder:

  - ¿Cómo decidir si usar `TRAPHANDLER` o `TRAPHANDLER_NOEC`? ¿Qué pasaría si se usara solamente la primera?

  - ¿Qué cambia, en la invocación de handlers, el segundo parámetro _(istrap)_ de la macro `SETGATE`? ¿Por qué se elegiría un comportamiento u otro durante un syscall?

  - Leer _user/softint.c_ y ejecutarlo con `make run-softint-nox`. ¿Qué excepción se genera? Si es diferente a la que invoca el programa… ¿cuál es el mecanismo por el que ocurrió esto, y por qué motivos?


### Tarea: kern_interrupts
{: #kern_interrupts}


Para este TP, se manejan las siguientes dos excepciones: breakpoint (n.º 3) y page fault (n.º 14). El manejo de excepciones se centraliza en la función `trap_dispatch()`, que decide a qué otra función de C invocar según el valor de _tf->tf_trapno_:

  - para `T_BRKPT` se invoca a `monitor()` con el _Trapframe_ adecuado.
  - para `T_PGFLT` se invoca a `page_fault_handler()` (ya implementado).

Además, la excepción de breakpoint se debe poder lanzar desde programas de usuario. En general, esta excepción se usa para implementar el depurado de código.[^int3]

Tras esta tarea, los siguientes tests pasan también:

```
$ make grade
...
faultread: OK (1.0s)
faultreadkernel: OK (1.0s)
faultwrite: OK (1.9s)
faultwritekernel: OK (1.1s)
breakpoint: OK (1.9s)
```

[^int3]: Para una lectura opcional sobre el tema ver, por ejemplo, [How debuggers work][debuggers], en particular la sección _The magic behind INT 3_.

[debuggers]: http://www.alexonlinux.com/how-debugger-works


### Tarea: kern_syscalls
{: #kern_syscalls}

Hoy en día, la mayoría de sistemas operativos implementan sus _syscalls_ en x86 mediante las instrucciones SYSCALL/SYSRET (64-bits) o SYSENTER/SYSEXIT (32-bits). Tradicionalmente, no obstante, siempre se implementaron mediante una interrupción por software de tipo _trap_.

En JOS, se elige la interrupción n.º 48 (0x30) como slot para `T_SYSCALL`. Tras la implementación de syscalls, el programa _user/hello_ podrá imprimir su mensaje en pantalla.

Pasos a seguir:

  1. Definir un interrupt handler adicional en _trapentry.S_ y configurarlo adecuadamente en `trap_init()`.

  2. Invocar desde `trap_dispatch()` a la función `syscall()` definida en _kern/syscall.c_. A la hora de especificar los parámetros de la función, se debe respetar la convención de llamada de JOS para syscalls (leer y estudiar el archivo _lib/syscall.c_).

  3. Implementar en `syscall()` soporte para cada tipo de syscall definido en _inc/syscall.h_. Se debe devolver `-E_INVAL` para números de syscall desconocidos.

     Nota: solo hace falta despachar, desde `syscall()`, cada tipo a las funciones estáticas ya implementadas: `SYS_cputs` a `sys_cputs()`, `SYS_getenvid` a `sys_getenvid()`, etc.

```
$ make grade
...
testbss: OK (1.0s)
hello: OK (1.0s)
```

**IMPORTANTE**: Aplicar el siguiente cambio a _user/hello.c:_

```diff
--- user/hello.c
+++ user/hello.c
@@ -5,5 +5,5 @@ void
 umain(int argc, char **argv)
 {
        cprintf("hello, world\n");
-       cprintf("i am environment %08x\n", thisenv->env_id);
+       cprintf("i am environment %08x\n", sys_getenvid());
 }
```

## Parte 5: protección de memoria

```
$ git show --stat tp2_parte5
 kern/pmap.c    | 22 ++++++++++++++++++++++
 kern/syscall.c |  1 +
 kern/trap.c    |  5 +++++
 3 files changed, 28 insertions(+)
```

En la implementación actual de algunas syscalls ¡no se realiza suficiente validación! Por ejemplo, con el código actual es posible que cualquier proceso de usuario acceda (imprima) cualquier dato de la memoria del kernel mediante `sys_cputs()`. Ver, por ejemplo, el programa _user/evilhello.c:_

    // Imprime el primer byte del entry point como caracter.
    sys_cputs(0xf010000c, 1);

### Tarea: user_evilhello

Ejecutar el siguiente programa y describir qué ocurre:

```c
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    char *entry = (char *) 0xf010000c;
    char first = *entry;
    sys_cputs(&first, 1);
}
```

Responder las siguientes preguntas:

  - ¿En qué se diferencia el código de la versión en _evilhello.c_ mostrada arriba?
  - ¿En qué cambia el comportamiento durante la ejecución?
    - ¿Por qué?
    - ¿Cuál es el mecanismo?

### Tarea: user_mem_check

Leer la sección [Page faults and memory protection][lab3mp] de la consigna original de 6.828 y completar el ejercicio 9:

  1. Llamar a `panic()` en _trap.c_ si un page fault ocurre en el ring 0.

  2. Implementar `user_mem_check()`, previa lectura de `user_mem_assert()` en _kern/pmap.c_.

  3. Para cada syscall que lo necesite, invocar a `user_mem_assert()` para verificar las ubicaciones de memoria.

```
$ make grade
...
buggyhello: OK (1.1s)
buggyhello2: OK (2.1s)
evilhello: OK (0.9s)
Part B score: 10/10
```

[lab3mp]: https://pdos.csail.mit.edu/6.828/2017/labs/lab3/#Page-faults-and-memory-protection

{% include anchors.html %}
{% include footnotes.html %}
