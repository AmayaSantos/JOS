# TP3: Multitarea con desalojo[^src]

[^src]: Material original en inglés: [Lab 4: Preemptive Multitasking](https://pdos.csail.mit.edu/6.828/2017/labs/lab4/)

En este TP, se expande la implementación de procesos de usuario del [TP2](tp2.md) para:

1.  ejecutar de manera concurrente múltiples procesos de usuario; para ello será necesario implementar un **planificador con desalojo**.

2.   permitir que los procesos de usuario puedan crear, a su vez, nuevos procesos; para ello será necesario implementar la llamada al sistema **fork()**.[^nofork]

3.   ejecutar múltiples procesos en paralelo, añadiendo a JOS **soporte para multi-core**

4.   permitir que los procesos de usuario puedan comunicarse entre sí mediante un **mecanismo de IPC**.

5.  optimizar la implementación de _fork()_ usando **copy-on-write**.

[^nofork]: En la parte 1, sin _fork()_, los procesos se crean de manera estática solamente.

*[IPC]: Inter-process communication

## Índice
{:.no_toc}

* TOC
{:toc}


**Nota:** El orden de las tareas es algo distinto a la consigna original MIT,
para dar una continuidad al lab original [kern2](lab/kern2.md). El script de auto-corrección sigue esta nueva organización:

```
$ make grade
helloinit: OK (1.5s)
Part 0 score: 1/1

yield: OK (1.1s)
spin0: Timeout! OK (1.2s)
Part 1 score: 2/2

dumbfork: OK (0.7s)
forktree: OK (1.1s)
spin: OK (1.0s)
Part 2 score: 3/3

yield2: OK (1.1s)
stresssched: OK (1.1s)
Part 3 score: 2/2

sendpage: OK (0.9s)
pingpong: OK (1.0s)
primes: OK (3.6s)
Part 4 score: 3/3

faultread: OK (1.5s)
faultwrite: OK (0.9s)
faultdie: OK (1.9s)
faultregs: OK (2.0s)
faultalloc: OK (1.0s)
faultallocbad: OK (2.0s)
faultnostack: OK (2.2s)
faultbadhandler: OK (2.0s)
faultevilhandler: OK (0.9s)
Part 5 score: 9/9

Score: 20/20
```


## Parte 0: Múltiples CPUs

```
$ git show --stat tp3_parte0
 kern/mpentry.S |  4 +++
 kern/pmap.c    | 24 ++++++++++++++++--
 2 files changed, 26 insertions(+), 2 deletions(-)
```

Más adelante en este TP ([parte 3](#pt3-multicore)) se hará uso de cualquier CPU adicional en el sistema para correr múltiples procesos de usuario en paralelo.

El código base del TP ya incluye la detección e inicialización de todas las CPUs presentes. Ese código se estudiará y completará en tareas posteriores.

Por el momento, es necesario desde ya ajustar ciertas partes de JOS para esta nueva configuración (en particular el stack, y algunos flags de la CPU).

**Auto-corrección**

Una vez completadas las tareas de esta parte 0, se debe poder lanzar el programa _hello_ con más de una CPU:

```
$ make run-hello-nox
...
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
hello, world

$ make run-hello-nox CPUS=4
...
SMP: CPU 0 found 4 CPU(s)
enabled interrupts: 1 2
SMP: CPU 1 starting
SMP: CPU 2 starting
SMP: CPU 3 starting
[00000000] new env 00001000
hello, world
```

Para ello, el código base del curso incluye la siguiente modificación:

```diff
@@ -60,7 +60,11 @@ i386_init(void)
     // Touch all you want.
     ENV_CREATE(user_hello, ENV_TYPE_USER);
 #endif // TEST*

+    // Eliminar esta llamada una vez completada la parte 1
+    // e implementado sched_yield().
+    env_run(&envs[0]);
+
     // Schedule and run the first user environment!
     sched_yield();
 }
```


### Tarea: mem_init_mp
{: #mem_init_mp}

El _layout_ de memoria de JOS indica que en `KSTACKTOP` se acomoda el stack privado del kernel, y que se pueden emplazar en la misma región stacks adicionales para múltiples CPUs:

```
KERNBASE, ---->  +------------------------------+ 0xf0000000
KSTACKTOP        |     CPU0's Kernel Stack      | RW/--  KSTKSIZE
                 | - - - - - - - - - - - - - - -|
                 |      Invalid Memory          | --/--  KSTKGAP
                 +------------------------------+
                 |     CPU1's Kernel Stack      | RW/--  KSTKSIZE
                 | - - - - - - - - - - - - - - -|
                 |      Invalid Memory          | --/--  KSTKGAP
                 +------------------------------+
                 :              .               :
                 :              .               :
MMIOLIM ------>  +------------------------------+ 0xefc00000
```

El espacio físico para estos stacks adicionales se sigue reservando, como en TPs anteriores, en la sección BSS del binario ELF. No obstante, se define directamente desde C, en el archivo _mpconfig.c:_[^sys-clone]

```c
// La constante NCPU limita el número máximo de
// CPUs que puede manejar JOS; actualmente es 8.
unsigned char percpu_kstacks[NCPU][KSTKSIZE];
```

[^sys-clone]: Es el mismo mecanismo que se propone en la tarea [kern2-stack] del lab _kern2_.

[kern2-stack]: lab/kern2.md#kern2-stack

**Implementar la función `mem_init_mp()` en el archivo _pmap.c_.**
: Como parte de la inicialización del sistema de memoria, esta función registra en el _page directory_ el espacio asignado para el stack de cada CPU (desde 0 hasta `NCPU-1`, independiente de que estén presentes en el sistema, o no).


### Tarea: mpentry_addr
{: #mpentry_addr}

Como se estudia en la tarea [multicore_init](#multicore_init), el arranque de CPUs adicionales necesitará pre-reservar una página de memoria física con dirección conocida. Arbitrariamente, JOS asigna la página n.º 7:

```c
// memlayout.h
#define MPENTRY_PADDR	0x7000
```

Se debe actualizar la función [page_init](tp1.md#page_init) para no incluir esta página en la lista de páginas libres.

Bonus: se podría añadir un _assert_ exigiendo como pre-condición que `MPENTRY_PADDR` sea realmente la dirección de una página (alineada a 12 bits):

```c
assert(MPENTRY_PADDR % PGSIZE == 0);
```

No obstante, al ser `MPENTRY_PADDR` una constante (y no un parámetro), se puede realizar la comprobación en tiempo de compilación mediante un [static assert] de C11:

```c
_Static_assert(MPENTRY_PADDR % PGSIZE == 0,
               "MPENTRY_PADDR is not page-aligned");
```


### Tarea: static_assert
{: #static_assert}

Con anterioridad a la existencia de C11, a menudo se definía en C `static_assert` como una macro con funcionalidad equivalente.

Responder: ¿cómo y por qué funciona la macro `static_assert` que define JOS?

[static assert]: http://en.cppreference.com/w/c/language/_Static_assert


### Tarea: mmio_map_region
{: #mmio_map_region}

Implementar la función `mmio_map_region()` en el archivo _pmap.c_.


### Tarea: mpentry_cr4
{: #mpentry_cr4}

Debido al soporte para _large pages_ implementado en el TP1, se deberá ahora activar la extensión PSE para cada CPU adicional por separado—en el archivo _mpentry.S_.


## Parte 1: Planificador y múltiples procesos
{: #pt1-planificador}

```
$ git show --stat tp3_parte1
 kern/env.c       |  1 +
 kern/init.c      |  4 ----
 kern/sched.c     | 17 ++++++++++++++--
 kern/syscall.c   |  3 +++
 kern/trap.c      |  6 ++++++
 kern/trap.h      |  1 +
 kern/trapentry.S |  3 ++-
 7 files changed, 28 insertions(+), 7 deletions(-)
```

Por el momento, y mientras no exista una función `fork()` o similar, solo es posible crear nuevos procesos desde el kernel. Por ejemplo, en _init.c_:

```c
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);
```

El cambio entre distintos procesos se realiza en la función `sched_yield()`, aún por implementar.


### Tarea: env_return
{: #env_return}

La función `env_run()` de JOS está marcada con el atributo _noreturn_. Esto es, si escribiéramos:

```c
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);

env_run(&envs[0]);
env_run(&envs[1]);
```

se vería que solo el primer proceso entra en ejecución, ya que la segunda llamada a `env_run()` no se llegaría a realizar.

Responder:

  - al terminar un proceso su función `umain()` ¿dónde retoma la ejecución el kernel? Describir la secuencia de llamadas desde que termina `umain()` hasta que el kernel dispone del proceso.

  - ¿en qué cambia la función `env_destroy()` en este TP, respecto al TP anterior?


### Tarea: sched_yield
{: #sched_yield}

Implementar la función `sched_yield()`: un planificador round-robin. El cambio de proceso (y de privilegio) se realiza con `env_run()`.

Una vez implementado, se puede eliminar la llamada a `env_run(&envs[0])` en _init.c_, y se debería observar que se ejecutan secuencialmente todos los procesos _hello_ configurados:

```
$ make qemu-nox
[00000000] new env 00001000
[00000000] new env 00001001
[00000000] new env 00001002
hello, world
i am environment 00001000
[00001000] exiting gracefully
hello, world
i am environment 00001001
[00001001] exiting gracefully
hello, world
i am environment 00001002
[00001002] exiting gracefully
No runnable environments in the system!
```


### Tarea: sys_yield
{: #sys_yield}

La función `sched_yield()` pertenece al kernel y no es accesible para procesos de usuario. Sin embargo, es útil a veces que los procesos puedan desalojarse a sí mismos de la CPU.

1. Manejar, en _kern/syscall.c_, la llamada al sistema `SYS_yield`, que simplemente llama a `sched_yield()`. En la biblioteca de usuario de JOS ya se encuentra definida la función correspondiente `sys_yield()`.

2. Leer y estudiar el código del programa _user/yield.c_. Cambiar la función `i386_init()` para lanzar tres instancias de dicho programa, y mostrar y explicar la salida de `make qemu-nox`.


<!--
### Tarea: contador_env
{: #contador_env}

Crear un nuevo programa _user/contador.c_ con el código que se indica a continuación, y crear en `i386_init()` tres instancias del mismo. (Se deberá actualizar el archivo `kern/Makefrag` para que pueda ser ejecutado.)

El código es una versión no-cooperativa del contador visto en el lab _sched_, y se eligen unos u otros parámetros dependiendo del identificador del proceso. Por esto, y al no haber implementado desalojo aún, sólo se ejecutará el primero de los tres contadores.

El buffer VGA (`0xb8000`), no obstante, no es accesible desde procesos de usuario. Antes de poder lanzar los contadores en ejecución, se deberá elegir una dirección virtual donde alojarlo (se puede definir una constante `VGA_USER` en _memlayout.h_), y actualizar `env_setup_vm()` para configurar esa página.

Responder:

  - ¿qué ocurrirá con esa página en `env_free()` al destruir el proceso?

  - ¿qué código asegura que el buffer VGA físico no será nunca añadido a la lista de páginas libres?

```c
//
// Archivo: user/contador.c
//

#include <inc/lib.h>

static void contador(int linea, int color, int delay);

void
umain(int argc, char **argv) {
    int lineas[] = {0, 3, 7};
    int delays[] = {1, 4, 7};
    int colores[] = {0x2f, 0x6f, 0x4f};

    int i = (sys_getenvid() - 1) % 3;
    contador(lineas[i], colores[i], delays[i]);
}

static void
contador(int linea, int color, int delay) {
    char counter[40] = {'0'};  // Our ASCII digit counter (RTL).

    while (1) {
        char *buf = (void *) VGA_USER + linea * 160;
        char *c = &counter[40];

        unsigned p = 0;
        unsigned long long int i = 1ULL << (delay + 15);

        // Cambiar por una llamada a sleep_ticks() una vez
        // implementado.
        while (i--)
            ;

        while (counter[p] == '9') {
            counter[p++] = '0';
        }

        if (!counter[p]++) {
            counter[p] = '1';
        }

        while (c-- > counter) {
            *buf++ = *c;
            *buf++ = color;
        }
    }
}
```
-->


### Tarea: timer_irq
{: #timer_irq}

Para tener un planificador con desalojo, el kernel debe periódicamente retomar el control de la CPU y decidir si es necesario cambiar el proceso en ejecución.

Antes de habilitar ninguna interrupción hardware, se debe definir con precisión la disciplina de interrupciones, esto es: cuándo y dónde se deben habilitar e inhabilitar.

Por ejemplo, _xv6_ admite interrupciones en el ring 0 excepto si se va a adquirir un lock. Así, en _xv6_ las llamadas a `acquire()` van precedidas de una instrucción `cli` para inhabilitar interrupciones, y a `release()` le sigue un `sti` para restaurarlas.

En JOS se opta por una disciplina menos eficiente pero mucho más amena: en modo kernel no se acepta _ninguna_ interrupción. Dicho de otro modo: las interrupciones, incluyendo el _timer_, solo se reciben cuando hay un proceso de usuario en la CPU.

**Corolario:** una vez arrancado el sistema, el kernel no llama a `cli` como parte de su operación, pues es una pre-condición que en modo kernel están inhabilitadas.

La habilitación/inhabilitación de excepciones se hace de manera automática:

  - al ir a modo usuario, mediante el registro `%eflags`
  - al volver a modo kernel, mediante el flag [_istrap_](tp2.md#kern_idt) de `SETGATE`.

Tarea :

  1. Leer los primeros párrafos de la sección [Clock Interrupts and Preemption][partC] de la consigna original en inglés.

  2. Añadir una nueva entrada en el _IDT_ para `IRQ_TIMER`. (Por brevedad, no es necesario hacerlo para el resto de IRQ).

  3. Habilitar `FL_IF` durante la creación de nuevos procesos.

[partC]: https://pdos.csail.mit.edu/6.828/2017/labs/lab4/#Clock-Interrupts-and-Preemption


### Tarea: timer_preempt
{: #timer_preempt}

El _handler_ creado para `IRQ_TIMER` desemboca, como todos los demás, en la función `trap_dispatch()`. A mayor complejidad del sistema operativo, mayor será la lista de cosas para hacer—por ejemplo, verificar si es momento de despertar a algún proceso que previamente llamó a `sleep()`.

En JOS, el manejo de la interrupción del timer será, simplemente, invocar a `sched_yield()` (No olvidar hacer un _ACK_ de la interrupción del timer mediante la función `lapic_eoi()` antes de llamar al _scheduler_).

**Auto-corrección**: Una vez añadida la llamada a `sched_yield()` en `trap_dispatch()`, todas las pruebas de la parte 1 deberían dar OK (podría ser necesario aumentar el _timeout_ de ejecución del test de _spin0_ para que la prueba se pase correctamente; el _timeout_ se define en la función `test_spin0()` del archivo grade-lab4).


## Parte 2: Creación dinámica de procesos
{: #pt2-fork}

```
$ git show --stat tp3_parte2
 kern/syscall.c | 102 ++++++++++++++++-
 lib/fork.c     |  58 +++++++++-
 2 files changed, 154 insertions(+), 6 deletions(-)
```

Para crear procesos en tiempo de ejecución es necesario que el kernel proporcione alguna primitiva de creación de procesos tipo `fork()/exec()` en Unix, o [`spawn()`][spawn] en Windows.

En JOS todavía no contamos con sistema de archivos, por lo que se complica implementar una llamada como `exec()` a la que se le debe pasar la ruta de un archivo ejecutable. Por ello, nos limitaremos a implementar una llamada de tipo `fork()`, en su versión “exokernel”. Así, el código a ejecutar en distintos procesos queda contenido en un mismo binario.


### Tarea: sys_exofork
{: #sys_exofork}

En el modelo Unix, `fork()` crea un nuevo proceso (equivalente a `env_alloc()`) y a continuación le realiza una copia del espacio de direcciones del proceso padre. Así, el código sigue su ejecución en ambos procesos, pero cualquier cambio en la memoria del hijo no se ve reflejada en el padre.

En JOS y su modelo _exokernel_, la llamada equivalente `sys_exofork()` sólo realizará la primera parte (la llamada a `env_alloc()`), pero dejará al proceso con un espacio de direcciones sin inicializar.[^notrunnable]

A cambio, el kernel proporciona un número de llamadas al sistema adicionales para que, desde _userspace_, el proceso padre pueda “preparar” al nuevo proceso (aún vacío) con el código que debe ejecutar.

En esta tarea se debe implementar la llamada al sistema `sys_exofork()`, así como las llamadas adicionales:

  - `sys_env_set_status()`: una vez configurado el nuevo proceso, permite marcarlo como listo para ejecución.

  - `sys_page_alloc()`: reserva una página física para el proceso, y la mapea en la dirección especificada.

  - `sys_page_map()`: _comparte_ una página entre dos procesos, esto es: añade en el _page directory_ de B una página que ya está mapeada en A.

  - `sys_page_unmap()`: elimina una página del _page directory_ de un proceso.

Una vez implementada cada una de estas funciones, y como ya se hizo en la tarea
[sys_yield](#sys_yield), se debe manejar el enumerado correspondiente a la función `syscall()` (el síntoma de no hacerlo es el error _invalid parameter_). Aplica también al resto de tareas del TP: **para cada syscall implementada, se debe actualizar el `switch` de syscalls**.

**Modelo de permisos**: Todas estas funciones se pueden llamar bien para el proceso en curso, bien para un proceso hijo. Para comprender el modelo de permisos de JOS, leer y realizar en paralelo la tarea [envid2env](#envid2env).

**Auto-corrección**: Tras esta tarea, `make grade` debe reportar éxito en el test _dumbfork:_

```
$ make grade
...
Part 1 score: 2/2

dumbfork: OK (0.7s)
...
```

[^notrunnable]: Tampoco lo marcará como _runnable_, ya que se lanzaría inmediatamente una excepción. Mientras que en Unix  `fork()` siempre añade el nuevo proceso a la cola de _ready_,  `sys_exofork()` lo dejará en estado `ENV_NOT_RUNNABLE`.

[spawn]: https://stackoverflow.com/a/33249271/848301


### Tarea: envid2env
{: #envid2env}

La validación de permisos (esto es, si un proceso puede realizar cambios en otro) se centraliza en la función `envid2env()` que, al mismo tiempo, es la que permite obtener un `struct Env` a partir de un _envid_ (de un modo análogo a `pa2page()` para páginas).

En la llamada al sistema `sys_env_destroy()` (ya implementada) se puede encontrar un ejemplo de su uso:

```c
//
// envid2env() obtiene el struct Env asociado a un id.
// Devuelve 0 si el proceso existe y se tienen permisos,
// -E_BAD_ENV en caso contrario.
//
int envid2env(envid_t envid,
              struct Env **env_store, bool checkperm);

int sys_env_destroy(envid_t envid) {
    int r;
    struct Env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
        return r;    // No existe envid, o no tenemos permisos.

    env_destroy(e);  // Llamada al kernel directa, no verifica.
    return 0;
}
```

Responder qué ocurre:

  - en JOS, si un proceso llama a `sys_env_destroy(0)`
  - en Linux, si un proceso llama a `kill(0, 9)`

E ídem para:

  - JOS: `sys_env_destroy(-1)`
  - Linux: `kill(-1, 9)`


### Tarea: dumbfork
{: #dumbfork}

El programa _user/dumbfork.c_ incluye una implementación de `fork()` altamente ineficiente, pues copia físicamente (página a página) el espacio de memoria de padre a hijo. Como se verá en la [parte 5](#pt5-copy-on-write), la manera  eficiente de implementar `fork()` es usando [copy-on-write][cowwp].

Esta función `dumbfork()` muestra, no obstante, que es posible lanzar procesos dinámicamente desde modo usuario contando tan solo con las primitivas de la sección anterior.[^readelf]

Las ejecución de _dumbfork.c_ una vez lanzado el proceso hijo es similar a _yield.c_, visto con anterioridad: el programa cede el control de la CPU un cierto número de veces, y termina.

Tras leer con atención el código, se pide responder las siguientes preguntas:[^forkfirst]

  1. Si, antes de llamar a `dumbfork()`, el proceso se reserva a sí mismo una página con `sys_page_alloc()` ¿se propagará una copia al proceso hijo? ¿Por qué?

  2. ¿Se preserva el estado de solo-lectura en las páginas copiadas? Mostrar, con código en espacio de usuario, cómo saber si una dirección de memoria es modificable por el proceso, o no. (Ayuda: usar las variables globales `uvpd` y/o `uvpt`.)

  3. Describir el funcionamiento de la función `duppage()`.

  4. Supongamos que se añade a `duppage()` un argumento booleano que indica si la página debe quedar como solo-lectura en el proceso hijo:

      - indicar qué llamada adicional se debería hacer si el booleano es _true_

      - describir un algoritmo alternativo que no aumente el número de llamadas al sistema, que debe quedar en 3 (1 × _alloc_, 1 × _map_, 1 × _unmap_).

  5. ¿Por qué se usa `ROUNDDOWN(&addr)` para copiar el stack? ¿Qué es `addr` y por qué, si el stack crece hacia abajo, se usa `ROUNDDOWN` y no `ROUNDUP`?

[^readelf]: Con esas mismas primitivas, de hecho, ya se podría implementar en _userspace_ una versión de `exec()`, mediante una función similar a [`load_icode`](tp2.md#load_icode) del TP2.

[^forkfirst]: Se recomienda abordar estas preguntas en paralelo con la tarea siguiente [fork_v0](#fork_v0), ya que se retroalimentan. Dicho de otro  modo: enfrentarse a la implementación de `fork_v0()` puede ayudar a descubrir las respuestas.

[cowwp]: https://es.wikipedia.org/wiki/Copy-on-write


### Tarea: fork_v0
{: #fork_v0}

Se pide implementar la función `fork_v0()` en el archivo _lib/fork.c_, conforme a las características descritas a continuación. Esta versión actúa de paso intermedio entre `dumbfork()` y la versión final _copy-on-write_.

El comportamiento externo de `fork_v0()` es el mismo que el de `fork()`, devolviendo en el padre el _id_ de proceso creado, y 0 en el hijo. En caso de ocurrir cualquier error, se puede invocar a `panic()`.

El comienzo será parecido a `dumbfork()`:

  - llamar a `sys_exofork()`
  - en el hijo, actualizar la variable global `thisenv`

La copia del espacio de direcciones del padre al hijo difiere de `dumbfork()` de la siguiente manera:

  - se abandona el uso de `end`; en su lugar, se procesan página a página todas las direcciones desde 0 hasta `UTOP`.

  - si la página (dirección) está mapeada, se invoca a la función `dup_or_share()`

  - la función `dup_or_share()` que se ha de añadir tiene un prototipo similar al de `duppage()`:

        static void
        dup_or_share(envid_t dstenv, void *va, int perm) ...

    La principal diferencia con la funcion `duppage()` de _user/dumbfork.c_ es: si la página es de solo-lectura, se _comparte_ en lugar de crear una copia.

    La principal diferencia con la función `duppage()` que se implementará en la parte 6 (archivo _lib/fork.c_, ejercicio 12 de la consigna original) es: si la página es de escritura, simplemente se crea una copia; todavía no se marca como copy-on-write.

    **Nota:** Al recibir _perm_ como parámetro, solo `fork_v0()` necesita consultar _uvpd_ y _uvpt_.

    **Ayuda:** Con `pte & PTE_SYSCALL` se limitan los flags a aquellos que `sys_page_map()` acepta.

Finalmente, se marca el hijo como `ENV_RUNNABLE`. La función `fork()` se limitará, por el momento, a llamar `fork_v0()`:

```c
envid_t fork(void) {
    // LAB 4: Your code here.
    return fork_v0();
}
```

<!--
### Tarea: contador_fork
{: #contador_fork}

Modificar el código de _contador.c_ para que arranque los distintos contadores desde un mismo proceso, usando `fork()`:

```c
void umain(int argc, char **argv) {
    if (fork())
        contador(0, 0x2f, 1);  // Verde, rápido.
    else if (fork())
        contador(3, 0x6f, 4);  // Naranja, lento.
    else
        contador(7, 0x4f, 7);  // Rojo, muy lento.
}
```

  1. ¿Funciona? ¿Qué está ocurriendo con el mapping de `VGA_USER`? ¿Dónde hay que arreglarlo?

  2. Implementar una solución genérica haciendo uso de un nuevo flag en _mmu.h_, `PTE_MAPPED`:

         #define PTE_MAPPED 0x...  // Never to be be copied, eg. MMIO.

  3. ¿Podría `fork()` darse cuenta, en lugar de usando un flag propio, mirando los flags `PTE_PWT` y/o `PTE_PCD`? (Suponiendo que `env_setup_vm()` los añadiera para `VGA_USER`.)
-->


## Parte 3: Ejecución en paralelo (multi-core)
{: #pt3-multicore}

> **Una nota sobre terminología**: JOS usa el término _CPU_ cuando sería más correcto usar _core_, o “unidad de procesamiento”. Una CPU (circuito físico) puede tener más de una de estas unidades. En términos de equivalencia, tanto en JOS como en esta consigna el término “CPU” se puede interpretar indistintamente como “core”, o como “single-core CPU”. Para una descripción detallada de la relación entre estos términos, consultar artículos de ampliación en [teqlog.com][c1], [indiana.edu][c2] y/o [stackoverflow.com][c3].

```
$ git show --stat tp3_parte3
 kern/env.c   |  1 +
 kern/init.c  |  5 +++--
 kern/trap.c  | 23 ++++++++++++--------
 user/yield.c |  7 +++---
 4 files changed, 22 insertions(+), 14 deletions(-)
```

[c1]: http://www.teqlog.com/core-vs-cpu-socket-chip-processor-difference-comparison.html
[c2]: https://kb.indiana.edu/d/avfb
[c3]: https://stackoverflow.com/questions/19225859/difference-between-core-and-processor

Con lo implementado hasta ahora, si se pasa el parámetro `CPUS=n` a `make qemu-nox`, JOS inicializa correctamente cada _core_, pero todos menos uno entran en un bucle infinito en la función _mp_main():_

```c
void mp_main(void)
{
  // Cargar kernel page directory.
  lcr3(PADDR(kern_pgdir));

  /* Otras inicializaciones ... */

  // Ciclo infinito. En el futuro, habrá una llamada a
  // sched_yield() para empezar a ejecutar procesos de
  // usuario en CPUs adicionales.
  for (;;)
    ;
}
```

### Tarea: multicore_init
{: #multicore_init}

En este ejercicio se pide responder las siguientes preguntas sobre cómo se habilita el soporte multi-core en JOS. Para ello se recomienda leer con detenimiento, además del código, la _[Parte A]_ de la consigna original al completo.

Preguntas:

1.  ¿Qué código copia, y a dónde, la siguiente línea de la función _boot_aps()_?

    ```c
    memmove(code, mpentry_start, mpentry_end - mpentry_start);
    ```
2.  ¿Para qué se usa la variable global `mpentry_kstack`? ¿Qué ocurriría si el espacio para este stack se reservara en el archivo _kern/mpentry.S_, de manera similar a `bootstack` en el archivo _kern/entry.S_?

3.  Cuando QEMU corre con múltiples CPUs, éstas se muestran en GDB como hilos de ejecución separados. Mostrar una sesión de GDB en la que se muestre cómo va cambiando el valor de la variable global `mpentry_kstack`:

    ```
    $ make qemu-nox-gdb CPUS=4
    ...

    // En otra terminal:
    $ make gdb
    (gdb) watch mpentry_kstack
    ...
    (gdb) continue
    ...
    (gdb) bt
    ...
    (gdb) info threads
    ...
    (gdb) continue
    ...
    (gdb) info threads
    ...
    (gdb) thread 2
    ...
    (gdb) bt
    ...
    (gdb) p cpunum()
    ...
    (gdb) thread 1
    ...
    (gdb) p cpunum()
    ...
    (gdb) continue
    ```

3.  En el archivo _kern/mpentry.S_ se puede leer:

    ```nasm
    # We cannot use kern_pgdir yet because we are still
    # running at a low EIP.
    movl $(RELOC(entry_pgdir)), %eax
    ```

    - ¿Qué valor tendrá el registro _%eip_ cuando se ejecute esa línea?

      Responder con redondeo a 12 bits, justificando desde qué región de memoria se está ejecutando este código.

    - ¿Se detiene en algún momento la ejecución si se pone un breakpoint en _mpentry_start_? ¿Por qué?

4.  Con GDB, mostrar el valor exacto de _%eip_ y `mpentry_kstack` cuando se ejecuta la instrucción anterior en el _último_ AP. Se recomienda usar, por ejemplo:

    ```
    (gdb) b *0x7000 thread 4
    Breakpoint 1 at 0x7000
    (gdb) continue
    ...
    (gdb) disable 1

    // Saltar código 16 bits
    (gdb) si 10

    (gdb) x/10i $eip
    ...
    (gdb) watch $eax == 0x...
    ...
    (gdb) continue
    ...
    (gdb) p $eip
    ...
    (gdb) p mpentry_kstack
    ???
    (gdb) si ...
    ...
    ...
    (gdb) p mpentry_kstack
    ...
    ```

[Parte A]: https://pdos.csail.mit.edu/6.828/2017/labs/lab4/#Part-A--Multiprocessor-Support-and-Cooperative-Multitasking

### Tarea: trap_init_percpu
{: #trap_init_percpu}

Tal y como se implementó en el TP anterior, cuando se pasa de modo usuario a modo kernel, la CPU carga guarda el stack del proceso y, como medida de seguridad, carga en el registro _%esp_ el stack propio del kernel. Tal y como se vio, la instrucción `iret` de [_env_pop_tf()_](tp2.md#env_pop_tf) restaura el stack del proceso de usuario.

¿Cómo decide el procesador _qué_ nuevo stack cargar en _%esp_ antes de invocar al _interrupt handler_? Tal y como se explica en **IA32-3A, §6.12.1**, del “task state segment”:

> If the interrupt handler is going to be executed at higher privilege, a stack switch occurs. […] **The stack pointer for the stack to be used by the handler are obtained from the TSS** for the currently executing task.

El TSS se configura en la función `trap_init_percpu()`, cuya versión del TP2 era:

```c
static struct Taskstate ts;

void trap_init_percpu(void) {
  // Setup a TSS so that we get the right stack
  // when we return to the kernel from userspace.
  ts.ts_esp0 = KSTACKTOP;
  ts.ts_ss0 = GD_KD;

  uint16_t seg = GD_TSS0;
  uint16_t idx = seg >> 3;

  // Plug the the TSS into the global descriptor table.
  gdt[idx] = /* ... */;

  // Load the TSS selector into the task register.
  ltr(seg);
}
```

Si con multi-core no se modificara esta función, ocurriría que el _task register_ de cada core apuntaría al mismo segmento; y, por tanto, ante una interrupción todas las CPUs intentarían usar el mismo stack.

Dicho de otro modo: si en la tarea [mem_init_mp](#mem_init_mp) se reservó un espacio separado para el stack de cada CPU; y en [multicore_init](#multicore_init), `boot_ap()` nos aseguramos que cada CPU _arranque_ con el stack adecuado; ahora lo que falta es que ese stack único se siga usando durante el manejo de interrupciones propio a cada CPU.

Para ello, se deberá mejorar la implementación de `trap_init_percpu()` tal que:

  - en lugar de usar la variable global `ts`, se use el campo _cpu_ts_ del arreglo global `cpus`:

    ```c
    // mpconfig.c
    struct CpuInfo cpus[NCPU];

    // cpu.h
    struct CpuInfo {
      uint8_t cpu_id;
      volatile unsigned cpu_status;
      struct Env *cpu_env;
      struct Taskstate cpu_ts;
    };
    ```

    Para ello basta con eliminar la variable global `ts`, y definirla así dentro de la función:

    ```c
    int id = cpunum();
    struct CpuInfo *cpu = &cpus[id];
    struct Taskstate *ts = &cpu->cpu_ts;
    ```

  - `GD_TSS0` seguirá siendo el _task segment_ del primer core; para calcular el segmento e índice para cada core adicional, se pueden usar las definiciones:

    ```c
    uint16_t idx = (GD_TSS0 >> 3) + id;
    uint16_t seg = idx << 3;
    ```

  - el campo `ts->ts_ss0` seguirá apuntando a `GD_KD`; pero `ts->ts_esp0` se deberá inicializar, como en tareas anteriores, de manera dinámica según el valor de `cpunum()`.


### Tarea: kernel_lock
{: #kernel_lock}

El manejo de concurrencia y paralelismo _dentro_ del kernel es una de las fuentes más frecuentes de errores de programación.

Según lo explicado en [timer_irq](#timer_irq), JOS toma la opción fácil respecto a interrupciones y las deshabilita por completo mientras el kernel está en ejecución; ello garantiza que no habrá desalojo de las tareas propias del sistema operativo. Como contraste, se explicó que xv6 permite interrupciones excepto si el código adquirió un lock; y otros sistemas operativos las permiten en todo momento.

Ahora, al introducir soporte multi-core, el kernel debe tener en cuenta que su propio código podría correr en paralelo en más de una CPU. La solución pasa por introducir mecanismos de sincronización.

JOS toma, de nuevo, la opción fácil: además de deshabilitar interrupciones, todo código del kernel en ejecución debe adquirir un lock único: el [big kernel lock][BKL]. De esta manera, el kernel se garantiza que nunca habrá más de una tarea del sistema operativo corriendo en paralelo.[^forgotlock]

Como contraste, xv6 usa _fine-grained locking_ y tiene locks separados para el scheduler, el sub-sistemas de memoria, consola, etc. Linux y otros sistemas operativos usan, además, [lock-free data structures][lock-free].

En esta tarea se pide seguir las indicaciones de la consigna original (ejercicio 5) para:

  - en `i386_init()`, adquirir el lock antes de llamar a `boot_aps()`.
  - en `mp_main()`, adquirir el lock antes de llamar a `sched_yield()`.
  - en `trap()`, adquirir el lock **si** se llegó desde userspace.
  - en `env_run()`, liberar el lock justo antes de la llamada a `env_pop_tf()`.

También será necesario modificar el programa de usuario _user/yield.c_ para que imprima el número de _CPU_ en cada iteración. (La macro `thisenv` siempre apunta al proceso que está siendo ejecutado en ese momento, y por ende se puede acceder a los atributos del `struct Env`.)

[BKL]: https://en.wikipedia.org/wiki/Big_kernel_lock
[lock-free]: https://en.wikipedia.org/wiki/Concurrent_data_structure

[^forgotlock]: A menos que los autores del código olvidaran adquirir el lock en algún punto de entrada a kernel mode.

**Auto-corrección**: Una vez implementado el locking, todas las pruebas de la parte 3 (y anteriores) deberían dar OK:

```
$ make grade
Part 0 score: 1/1
Part 1 score: 2/2
Part 2 score: 3/3

yield2: OK (1.1s)
stresssched: OK (1.5s)
Part 3 score: 2/2
```


## Parte 4: Comunicación entre procesos
{: #pt4-ipc}

```
$ git show --stat tp3_parte4
 kern/syscall.c | 47 ++++++++++++++++++++++++++++++---
 lib/ipc.c      | 25 +++++++++++++++---
 2 files changed, 66 insertions(+), 6 deletions(-)
```

En esta parte se implementará un mecanismo comunicación entre procesos. Para evitar mensajes de longitud variable, lo único que se puede enviar es:

  - un entero de 32 bits; y, opcionalmente
  - una página de memoria

Para enviar, un proceso invoca a la función:

```c
void ipc_send(envid_t dst_env, uint32_t val, void *page, int perm);
```

Si el proceso destino existe, esta función sólo devuelve una vez se haya entregado el mensaje.

Los mensajes nunca se entregan de manera asíncrona. Para que se pueda entregar un mensaje, el proceso destino debe invocar a la función:

```c
int32_t ipc_recv(envid_t *src_env, void *page_dest, int *recv_perm);
```

Estas funciones forman parte de la biblioteca del sistema _(lib/ipc.c)_, e invocan a sendas llamadas al sistema que son las que realizan el trabajo (a implementar en _kern/syscall.c_):

  - `sys_ipc_recv()`
  - `sys_ipc_try_send()`


### Tarea: sys_ipc_recv
{: #sys_ipc_recv}

En este TP, se añadieron al `struct Env` los campos:

```c
struct Env {
  // ...
  bool env_ipc_recving;    // Env is blocked receiving
  void *env_ipc_dstva;     // VA at which to map received page
  uint32_t env_ipc_value;  // Data value sent to us
  envid_t env_ipc_from;    // envid of the sender
  int env_ipc_perm;        // Perm of page mapping received
};
```

El mecanismo de `sys_ipc_recv()` es simple: marcar el proceso como _ENV_NOT_RUNNABLE_, poniendo a _true_ el flag `thisenv->env_ipc_recving`.
De esta manera:

  - desde `sys_ipc_try_send()` será posible saber si el proceso realmente está esperando un mensaje

  - hasta que llegue dicho mensaje, el proceso no entrará en la CPU.


### Tarea: ipc_recv
{: #ipc_recv}

La función `ipc_recv()` es el wrapper en _user space_ de `sys_ipc_recv()`. Recibe dos punteros vía los que el proceso puede obtener qué proceso envió el mensaje y, si se envió una página de memoria, los permisos con que fue compartida.

Una vez implementada la función, resolver este ejercicio:

  - Un proceso podría intentar enviar el valor númerico `-E_INVAL` vía `ipc_send()`. ¿Cómo es posible distinguir si es un error, o no? En estos casos:

    ```c
    // Versión A
    envid_t src = -1;
    int r = ipc_recv(&src, 0, NULL);

    if (r < 0)
      if (/* ??? */)
        puts("Hubo error.");
      else
        puts("Valor negativo correcto.")
    ```

    y:

    ```c
    // Versión B
    int r = ipc_recv(NULL, 0, NULL);

    if (r < 0)
      if (/* ??? */)
        puts("Hubo error.");
      else
        puts("Valor negativo correcto.")
    ```

### Tarea: ipc_send
{: #ipc_send}

La llamada al sistema `sys_ipc_try_send()` que se implementará en la siguiente tarea es _no bloqueante_, esto es: devuelve inmediatamente con código `-E_IPC_NOT_RECV` si el proceso destino no estaba esperando un mensaje.

Por ello, el wrapper en _user space_ tendrá un ciclo que repetirá la llamada mientras no sea entregado el mensaje.


### Tarea: sys_ipc_try_send
{: #sys_ipc_try_send}

Implementar la llamada al sistema `sys_ipc_try_send()` siguiendo los comentarios en el código, y responder:

-  ¿Cómo se podría hacer bloqueante esta llamada? Esto es: qué estrategia de implementación se podría usar para que, si un proceso A intenta a enviar a B, pero B no está esperando un mensaje, el proceso A sea puesto en estado `ENV_NOT_RUNNABLE`, y sea despertado una vez B llame a `ipc_recv()`.

<!--
2.  Con esta nueva estrategia de implementación mejorada ¿podría ocurrir un _deadlock_? Poner un ejemplo de código de usuario que entre en deadlock.

3.  ¿Podría el kernel detectar el deadlock, e impedirlo devolviendo un nuevo error, `E_DEADLOCK`? ¿Qué función o funciones tendrían que modificarse para ello?
-->

```
$ make grade
...
sendpage: OK (0.6s)
pingpong: OK (1.0s)
primes: OK (4.1s)
Part 4 score: 3/3
```


## Parte 5: Manejo de _page faults_
{: #pt5-pgfault-upcall}

```
$ git show --stat tp3_parte5
 kern/syscall.c | 12 ++++-
 kern/trap.c    | 31 +++++++++++++
 lib/pfentry.S  | 12 +++
 lib/pgfault.c  |  6 +--
 4 files changed, 58 insertions(+), 4 deletions(-)
```

Una implementación eficiente de `fork()` no copia los contenidos de memoria del padre al hijo sino solamente _el espacio de direcciones_; esta segunda opción es mucho más eficiente pues sólo se han de copiar el _page directory_ y las _page tables_ en uso.

Para las regiones de solo lectura, compartir páginas físicas no supone un problema, pero sí lo hace para las regiones de escritura ya que —por la especificación de `fork()`— las escrituras no deben compartirse entre hijo y padre. Para solucionarlo, se utiliza la técnica de _copy-on-write_, que de forma resumida consiste en:

1.  mapear las páginas de escritura como solo lectura (en ambos procesos)

2.  detectar el momento en el que uno de los procesos intenta escribir a una de ellas

3.  en ese instante, remplazar —para ese proceso— la página compartida por una página nueva, marcada para escritura, con los contenidos de la antigua.

La detección de la escritura del segundo paso se hace mediante el mecanismo estándar de _page faults_. Si se detecta que una escritura fallida ocurrió en una página marcada como _copy-on-write_, el kernel realiza la copia de la página y la ejecución del programa se retoma en la instrucción que generó el error.

No obstante, el `fork()` de JOS se implementa en _userspace_, por lo que los _page faults_ que le sigan deberán ser manejados también desde allí; en otras palabras, será la biblioteca estándar —y no el kernel— quien realice la copia de páginas _copy-on-write_. Para ello, en esta parte del TP se implementará primero un mecanismo de propagación de excepciones de kernel a _userspace_, y después la versión final de `fork()` con _copy-on-write_.

**Nota:** Se recomienda leer al completo el enunciado de esta parte antes de comenzar con su implementación. Una vez hecho esto, el orden de implementación de las tareas se puede alterar más o menos libremente.


### Tarea: set_pgfault_upcall
{: #set_pgfault_upcall}

Desde el lado del kernel, la propagación de excepciones a _userspace_ comienza por saber a qué función de usuario debe saltar el kernel para terminar de manejar la excepción (esta función haría las veces de manejador). Para ello, se añade al struct _Env_ un campo:

```c
struct Env {
    // ...
    void *env_pgfault_upcall;  // Page fault entry point
    // ...
};
```

Para indicar al kernel cuál será el manejador de _page faults_ de un proceso, se introduce la llamada al sistema `sys_env_set_pgfault_upcall()`, que se debe implementar en _kern/syscall.c_ (no olvidar actualizar el _switch_ al final del archivo).

**Nota:** El término _upcall_ hace referencia a una llamada que se realiza desde un sistema de bajo nivel a un sistema más alto en la jerarquía. En este caso, una llamada desde el kernel a _userspace_. (Visto así, _upcall_ vendría a ser lo contrario de _syscall_.)


### Tarea: set_pgfault_handler
{: #set_pgfault_handler}

Cuando el kernel salta al manejador de _userspace_, lo hace en un _stack_ distinto al que estaba usando el programa.[^diffstack] Esto es necesario porque (como se verá) la página fallida por _copy-on-write_ podría ser el propio _stack_ del programa.

Esta “pila de excepciones” se ubica siempre en la dirección fija `UXSTACKTOP`. El kernel de JOS no reserva memoria para esta segunda pila, y los programas que quieran habilitarse un _page fault handler_ deberán obligatoriamente reservar memoria para esa pila antes de llamar a `sys_env_set_pgfault_upcall()`.

Para unificar este proceso, los programas de JOS no llaman directamente a la _syscall_ `sys_env_set_pgfault_upcall()` sino a la función de la biblioteca estándar `set_pgfault_handler()`, la cual reserva memoria para la pila de excepciones antes de instalar el manejador. Se pide implementar esta función en _lib/pgfault.c_.

**Nota:** Como se puede ver en la documentación y el esqueleto de la función, el manejador que se instala no es el que se recibe como parámetro sino que se instala siempre uno común llamado `_pgfault_upcall`. Los motivos se explican en detalle en la siguiente tarea, [pgfault_upcall](#pgfault_upcall).

[^diffstack]: O sea, no solo diferente a `KSTACKTOP` sino también diferente a `USTACKTOP`.


### Tarea: pgfault_upcall
{: #pgfault_upcall}

Un hecho importante del manejador en _userspace_ es que cuando el manejador termina, la ejecución _no_ vuelve al kernel sino que lo hace directamente a la instrucción que originalmente causó la falla. Esto quiere decir que, en el mecanismo de _upcalls_, es el propio código de usuario quien debe asegurarse que la ejecución del manejador es transparente, restaurando para ello los registros a sus valores originales, etc.

Así, y de manera similar a cómo la función `trap()` toma un struct _Trapframe_ como parámetro, el manejador de _page faults_ toma como parámetro un struct _UTrapframe_. En ambos casos, el struct contiene todos los valores
que se deberán restaurar:

```c
struct UTrapframe {
    // Descripción del page fault.
    uint32_t utf_fault_va;
    uint32_t utf_err;

    // Estado original a restaurar.
    struct PushRegs utf_regs;
    uintptr_t utf_eip;
    uint32_t utf_eflags;
    uintptr_t utf_esp;
};
```

En el kernel, la secuencia de llamadas para manejar una interrupción es la siguiente:

    _alltraps  CALL→  trap(tf)  CALL→  env_pop_tf(curenv->tf_eip)

Cabe notar que la secuencia no vuelve a `_alltraps` para restaurar el estado sino que el kernel decide salir por otra vía (porque _curenv_ podría haber cambiado). Por otra parte, tanto `_alltraps` como `env_pop_tf` están implementadas en asembler porque es difícil manejar el estado de la CPU (registros, etc.) desde C. Es esta última funcion, `env_pop_tf`, la que restaura el estado del proceso.

En cambio, el manejador de _page faults_ de _userspace_ sí retorna por un flujo de llamadas estándar (pues siempre se vuelve al lugar original):

    _pgfault_upcall  CALL→  _pgfault_handler(utf)  RET→  _pgfault_upcall

Como se vio en la tarea anterior, la función `_pgfault_upcall` es el manejador que siempre se instala en JOS; es una función escrita en asembler que actúa de punto de entrada y salida para cualquier manejador escrito en C. Su esqueleto es:

```nasm
_pgfault_upcall:
    //
    // (1) Preparar argumentos
    //
    ...

    //
    // (2) Llamar al manejador en C, cuya dirección está
    // en la variable global _pgfault_handler.
    //
    ...

    //
    // (3) Restaurar estado, volviendo al EIP y ESP originales.
    //
    ...
```

Se pide implementar esta función en _lib/pfentry.S_. El punto 1 y 2 son sencillos, y el 3 se asemeja en espíritu a la función `env_pop_tf()`; tanto para el punto 2 como el 3 debería haber en el tope de la pila un puntero a _UTrapframe_.

**Nota**: Como se explica en detalle en la siguiente tarea, nada más entrar al manejador ya hay en la pila un struct _UTrapframe_ completo, esto es, en `(%esp)` está el campo _fault_va_ del struct.


### Tarea: page_fault_handler
{: #page_fault_handler}

Del lado del kernel, la función que invoca al manejador de _page faults_ de _userspace_ es `page_fault_handler()` en _kern/trap.c_; a esta función se la llama desde `trap_dispatch()` —ya vista en TPs anteriores— cuando `tf->tf_trapno` es `T_PGFLT`.

Anteriormente, la implementación de `page_fault_handler()` simplemente abortaba el proceso en que se produjo la falla. Ahora, en cambio, deberá detectar si `env->env_pgfault_upcall` existe y, en ese caso, inicializar en la pila de excepciones del proceso un struct _UTrapframe_ para saltar a continuación allí con el manejador configurado:

```c
void page_fault_handler(struct Trapframe *tf) {
  //
  // ...
  //

  // LAB 4: Your code here.
  if (...) {
    struct UTrapframe *u;

    // Inicializar a la dirección correcta por abajo de UXSTACKTOP.
    // No olvidar llamadas a user_mem_assert().
    u = ...;

    // Completar el UTrapframe, copiando desde "tf".
    u->utf_fault_va = ...;
    ...

    // Cambiar a dónde se va a ejecutar el proceso.
    tf->tf_eip = ...
    tf->tf_esp = ...

    // Saltar.
    env_run(...);
  }

  //
  // ...
  //
}
```

Tras implementar las cuatro tareas anteriores, deberían pasar todas las pruebas de la parte 5:

```
$ make grade
...
faultread: OK (0.6s)
faultwrite: OK (0.6s)
faultdie: OK (0.8s)
faultregs: OK (1.0s)
faultalloc: OK (1.0s)
faultallocbad: OK (1.0s)
faultnostack: OK (1.0s)
faultbadhandler: OK (1.0s)
faultevilhandler: OK (1.0s)
Part 5 score: 9/9
```


## Parte 6: Copy-on-write fork
{: #pt6-cow-fork}

```
$ git show --stat tp3_parte6
 lib/fork.c | 78 +++++++++++++++++++++-
 1 file changed, 75 insertions(+), 3 deletions(-)
```

En esta parte se implementarán las funciones `fork()`, `duppage()` y `pgfault()` en el archivo _lib/fork.c_. Las dos primeras son análogas a `fork_v0()` y `dup_or_share()`; la tercera es el manejador de _page faults_ que se instalará en los procesos que hagan uso de `fork()`.

Se recomienda revisar la explicación de _copy-on-write_ ofrecida en la introducción de la [parte 5](#pt5-pgfault-upcall).

### Tarea: fork
{: #fork}

El cuerpo de la función `fork()` es muy parecido al de `dumbfork()` y `fork_v0()`, vistas anteriormente. Se pide implementar la función `fork()` teniendo en cuenta la siguiente secuencia de tareas:

1.  Instalar, en el padre, la función `pgfault` como manejador de _page faults_. Esto también reservará memoria para su pila de excepciones.

2.  Llamar a `sys_exofork()` y manejar el resultado. En el hijo, actualizar como de costumbre la variable global `thisenv`.

3.  Reservar memoria para la pila de excepciones del hijo, e instalar su manejador de excepciones.

    Pregunta: ¿Puede hacerse con la función `set_pgfault_handler()`? De no poderse, ¿cómo llega al hijo el valor correcto de la variable global `_pgfault_handler`?

4.  Iterar sobre el espacio de memoria del padre (desde 0 hasta `UTOP`) y, para cada página presente, invocar a la función `duppage()` para mapearla en el hijo. Observaciones:

      - no se debe mapear la región correspondiente a la pila de excepciones; esta región nunca debe ser marcada como _copy-on-write_ pues es la pila donde se manejan las excepciones _copy-on-write_: la región no podría ser manejada sobre sí misma.

      - se pide recorrer el número mínimo de páginas, esto es, no verificar ningún PTE cuyo PDE ya indicó que no hay _page table_ presente (y, desde luego, no volver a verificar ese PDE). Dicho de otra manera, minimizar el número de consultas a _uvpd_ y _uvpt_.

5.  Finalizar la ejecución de `fork()` marcando al proceso hijo como `ENV_RUNNABLE`.


### Tarea: duppage
{: #duppage}

La función `duppage()` conceptualmente similar a `dup_or_share()` pero jamás reserva memoria nueva (no llama a `sys_page_alloc`), ni tampoco la escribe. Así, para las páginas de escritura no creará una copia, sino que la mapeará como solo lectura.

Se pide implementar la función `duppage()` siguiendo la siguiente secuencia:

  - dado un número de página virtual _n_, se debe mapear en el hijo la página física correspondiente en la misma página virtual (usar `sys_page_map`).

  - los permisos en el hijo son los de la página original _menos_ `PTE_W`; si y solo si se sacó `PTE_W`, se añade el bit especial `PTE_COW` (ver abajo).

  - si los permisos resultantes en el hijo incluyen `PTE_COW`, se debe remapear la página en el padre con estos permisos.[^ptecoworig]

**Sobre el bit PTE_COW:** Ante un _page fault_, el manejador debe averiguar si el fallo ocurre en una región _copy-on-write_, o si por el contrario se debe a un acceso a memoria incorrecto. En la arquitectura x86, los bits 9-11 de un PTE son ignorados por la MMU, y el sistema operativo los puede usar para sus propósitos. El símbolo `PTE_COW`, con valor 0x800, toma uno de estos bits como mecanismo de discriminación entre errores por acceso incorrecto a memoria, y errores relacionados con _copy-on-write_.

[^ptecoworig]: Los permisos resultantes incluyen el bit `PTE_COW` bien si se añadió en esta función, bien si los permisos originales ya lo contenían.


### Tarea: pgfault
{: #pgfault}

El manejador en _userspace_ `pgfault()` se implementa en dos partes.

En primer lugar, se verifica que la falla recibida se debe a una escritura en una región _copy-on-write_. Así, el manejador llamará a `panic()` en cualquiera de los siguientes casos:

  - el error ocurrió en una dirección no mapeada
  - el error ocurrió por una lectura y no una escritura
  - la página afectada no está marcada como _copy-on-write_

En segundo lugar, se debe crear una copia de la página _copy-on-write_, y mapearla en la misma dirección. Este proceso es similar al realizado por la función `dup_or_share()` para páginas de escritura:

  - se reserva una nueva página en una dirección temporal
  - tras escribir en ella los contenidos apropiados, se mapea en la dirección destino
  - se elimina el mapeo usado en la dirección temporal

**Ayuda para la primera parte:** El error ocurrió por una lectura si el bit `FEC_WR` está a 0 en `utf->utf_err`; la dirección está mapeada si y solo sí el bit `FEC_PR` está a 1. Para verificar `PTE_COW` se debe usar _uvpt_.

-----

No hay tests específicos para la parte 6, pero se debe correr `make grade` verificando que todos los tests pasan con esta nueva versión de `fork()`.

{% include anchors.html %}
{% include footnotes.html %}
