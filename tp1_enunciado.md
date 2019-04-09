TP1: Memoria virtual en JOS[^1] {#tp1-memoria-virtual-en-jossrc}
===============================

El código de JOS para este TP
[añade](https://github.com/fisop/jos/commit/3bc74e4815902ec1a9551fdede1ad23fed07b54b#diff-7fd43f1f263e186ee9517c9003da0044L35)
en `i386_init()` una llamada para inicializar el sistema de memoria:

    // Memory management initialization function
    mem_init();

Este trabajo práctico consiste en implementar esa función y el resto de
la API del sistema de memoria.

    $ git diff --stat tp1..tp1_sol -- kern
     kern/pmap.c | 126 ++++++++++++++++----
     1 file changed, 106 insertions(+), 20 deletions(-)

**IMPORTANTE**: se debe leer antes la [información general sobre TPs con
JOS](tps.md) y el [software a instalar](kit.md#tools).

Resumen
-------

{:.no\_toc}

La implementación del sistema de memoria (en JOS o cualquier otro
kernel) consiste de, al menos, los siguientes componentes:

1.  la lista de páginas físicas libres, construida a partir de la
    cantidad de memoria de la computadora.

2.  funciones para manipular *page tables* y *page directories*.

3.  el espacio virtual de memoria del propio kernel, que persiste toda
    la ejecución una vez inicializado.

Índice
------

{:.no\_toc}

-   TOC {:toc}

Parte 1: Memoria física
-----------------------

{: \#pt1-memoria-fisica}

En JOS, la información sobre el estado de cada página física se mantiene
en un arreglo global definido en *pmap.c* (el tamaño del arreglo es,
precisamente, el número total de páginas físicas):

    struct PageInfo *pages;  // Arreglo de páginas físicas.

El arreglo se crea en tiempo de ejecución porque el número de páginas no
es una constante sino que depende de la cantidad real de memoria de la
máquina donde corre JOS.

La función `i386_detect_memory()` es quien detecta la cantidad de
memoria disponible. Esta función ya está implementada y no es necesario
cambiarla. El resultado se almacena en la variable global `npages`: el
número total de páginas físicas.

De cada página física interesa saber si está asignada o no. Se emplea un
entero en lugar de un booleano para, en el futuro, poder compartir una
misma página en varios procesos. Asimismo, para poder encontrar la
siguiente página libre en *O(1)*, el mismo struct *PageInfo* actúa como
lista enlazada de páginas libres.

``` {.c}
struct PageInfo {
    uint16_t pp_ref;
    struct PageInfo *pp_link;
};
```

<!--
### Tarea: page2pa
{: #page2pa}

El struct _PageInfo_ no guarda la dirección de memoria física donde empieza la página, pues se considera que se puede deducir. La función ya definida `page2pa()` toma un puntero a _PageInfo_ y devuelve la dirección física donde comienza la página.

Encontrar la definición de `page2pa()` y describir cómo funciona en uno o dos párrafos. En particular: ¿cómo se “deduce” la dirección física a partir del puntero a _PageInfo_?
-->
### Tarea: mem\_init\_pages

{: \#mem\_init\_pages}

Añadir a `mem_init()` código para crear el arreglo de páginas `pages`.
Se debe determinar cuánto espacio se necesita, e inicializar a 0 usando
`memset()`.

En esta fase temprana de la inicialización del sistema, la memoria se
reserva mediante la función auxiliar `boot_alloc()`. Una vez se termine
de inicializar el sistema de memoria, todas las reservas se realizarán
mediante la función `page_alloc()` a implementar en la parte 2.

### Tarea: boot\_alloc

{: \#boot\_alloc}

Completar la implementación de `boot_alloc()` en *pmap.c*; esta función
implementa una “reserva rudimentaria” de la siguiente manera:

-   la variable estática `nextfree` guarda la siguiente posición de
    memoria que se puede usar (alineada a 4096 bytes).
-   la reserva se realiza siempre en páginas físicas (múltiplos de 4096
    bytes).
-   se devuelve una dirección virtual, no física.

Si no hubiera suficiente memoria, la invocación debe resultar en
`panic()`.

### Tarea: boot\_alloc\_pos

Incluir en el archivo *TP1.md*:

{:.lower\_alpha} 1. Un cálculo manual de la primera dirección de memoria
que devolverá `boot_alloc()` tras el arranque. Se puede calcular a
partir del binario compilado (`obj/kern/kernel`), usando los comandos
`readelf` y/o `nm` y operaciones matemáticas.

2.  Una sesión de GDB en la que, poniendo un breakpoint en la función
    `boot_alloc()`, se muestre el valor de `end` y `nextfree` al
    comienzo y fin de esa primera llamada a `boot_alloc()`.

### Tarea: page\_init

{: \#page\_init}

El siguiente paso tras construir el arreglo `pages` es inicializar la
lista de páginas libres. Es una lista enlazada cuya cabeza se guarda en
la variable estática `page_free_list`.[^2] Para construirla, se debe
enlazar cada página a la siguiente *exceptuando* aquellas que ya estén
en uso o que nunca se deban usar.

Esta función no modifica el campo `pp_ref` de *PageInfo*. Simplemente
decide, para cada página física, si incluirla en la lista de páginas
libres inicial, o ignorarla. En particular, se deben ignorar las páginas
que quedaron en uso tras el arranque.

Implementar la función `page_init()`, cuya documentación describe
exactamente qué regiones de memoria *no* se deben incluir en la lista
(consultar también el archivo
[memlayout.h](https://github.com/fisop/jos/blob/tp1/inc/memlayout.h)):

-   la página 0
-   una sección de 384K para I/O
-   la región donde se encuentra el código del kernel
-   toda la memoria ya asignada por `boot_alloc()`

### Tarea: page\_alloc

{: \#page\_alloc}

Implementar la función `page_alloc()`, que saca una página de la lista
de páginas libres y devuelve su `struct PageInfo` asociado.

Si el argumento a la función incluye `ALLOC_ZERO`, se escriben ceros en
toda la longitud de la página.

Responder: ¿en qué se diferencia `page2pa()` de `page2kva()`?

### Tarea: page\_free

{: \#page\_free}

Implementar la función `page_free()` siguiendo los comentarios en el
código.

------------------------------------------------------------------------

Para esta primera parte, la función de corrección `check_page_alloc()`
debe terminar con éxito:

    $ make qemu-nox
    Physical memory: 131072K available, base = 640K ...
    check_page_alloc() succeeded!
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Parte 2: Memoria virtual
------------------------

{: \#pt2-memoria-virtual}

En esta parte no se realizarán cambios en `mem_init()`, solamente se
implementarán funciones para manejar las entradas de *page directory* y
*page tables*.

### Tarea: pgdir\_walk

{: \#pgdir\_walk}

La función:

    pte_t *pgdir_walk(pde_t *pgdir, const void *va) ...

se encarga de navegar la estructura de doble nivel *page directory* →
*page table entry*.

Recibe una dirección virtual más un puntero al comienzo del directorio,
y devuelve un puntero a la *page table **entry*** correspondiente. Por
ejemplo, para la dirección:

    0x4C3C00A  (binario: 0000010011 0000111100 000000001010)
                         ---------- ----------
                          PDE (19)   PTE (60)

devolvería un puntero a la 60.ª posición en la *page table* a que apunte
`pgdir[19]`. (En caso de `pgdir[19]` ser NULL, el parámetro booleano
*create* marcaría si se debe crear la page table correspondiente, o no.)

**Ayuda 1:** revisar todas las macros disponibles en la primera parte
del archivo [mmu.h](https://github.com/fisop/jos/blob/tp1/inc/mmu.h).

**Ayuda 2:** tanto *page directories* como *page tables* almacenan
direcciones *físicas* (ya que es la MMU quien las procesa). La función
debe devolver una dirección virtual.

### Tarea: page\_lookup

{: \#pgdir\_lookup}

La función:

    struct PageInfo *page_lookup(pde_t *pgdir, void *va) ...

devuelve la página física en la que se aloja una dirección virtual.
Emplea `pgdir_walk()` para encontrar en qué *page table entry* se
encuentra mapeada la dirección, y dereferencia el puntero devuelto para
acceder a su contenido.

De nuevo, revisar las macros en el archivo
[mmu.h](https://github.com/fisop/jos/blob/tp1/inc/mmu.h), en particular
`PTE_ADDR()`.

### Tarea: page\_insert

{: \#pgdir\_insert}

La función `page_insert()` configura la relación dirección virtual →
dirección física en un determinado directorio.

De manera similar a `page_lookup()`, obtiene con `pgdir_walk()` el PTE
donde se debe configurar, y escribe en esa posición el número de página
más los permisos apropiados.

**Ayuda**: el paso de “TLB invalidation” se puede realizar directamente
como parte de la función `page_remove()`.

### Tarea: page\_remove

{: \#pgdir\_remove}

Implementar la función `page_remove()` siguiendo los comentarios en el
código.

------------------------------------------------------------------------

Para esta segunda parte, la función de corrección `check_page()` debe
terminar con éxito:

    $ make qemu-nox
    Physical memory: 131072K available, base = 640K ...
    check_page_alloc() succeeded!
    check_page() succeeded!
    ^^^^^^^^^^^^^^^^^^^^^^^

Parte 3: Page directory del kernel
----------------------------------

{: \#pt3-kernel-pgdir}

Al comienzo de `mem_init()` se reserva una página para el *page
directory* del kernel, y se guarda en la variable global `kern_pgdir`.
Al final de la función, se carga en `%cr3` en sustitución del usado en
el proceso de arranque.

Antes de poder usarlo, se le debe añadir las entradas correspondientes a
la configuración expresada en
[memlayout.h](https://github.com/fisop/jos/blob/tp1/inc/memlayout.h)
para direcciones mayores que `UTOP`.[^3]

### Tarea: boot\_map\_region

{: \#boot\_map\_region}

Implementar la función encargada de configurar estas regiones:

    static void boot_map_region(pgdir, va, size, pa, int perm) ...

Es similar a `page_insert()` en tanto que escribe en el PTE
correspondiente, pero:

1.  no incrementa el contador de referencias de las páginas
2.  puede ser llamada con regiones mucho mayores de 4KiB

### Tarea: kernel\_pgdir\_setup

{: \#kernel\_pgdir\_setup}

Añadir en `mem_init()` las tres llamadas a `boot_map_region()`
necesarias para configurar:

-   el stack del kernel en `KSTACKTOP`
-   el arreglo `pages` en `UPAGES`
-   los primeros 256 MiB de memoria física en `KERNBASE`

------------------------------------------------------------------------

Para esta tercera parte parte, deben terminar con éxito las funciones de
corrección `check_kern_pgdir()` y `check_page_installed_pgdir()`:

    $ make qemu-nox
    Physical memory: 131072K available, base = 640K ...
    check_page_alloc() succeeded!
    check_page() succeeded!
    check_kern_pgdir() succeeded!
    check_page_installed_pgdir() succeeded!

    $ make grade
    running JOS: (1.0s)
      Physical page allocator: OK
      Page management: OK
      Kernel page directory: OK
      Page management 2: OK
    Score: 4/4

Parte 4: Large pages
--------------------

{: \#pt4-large-pages}

Usando *large pages*, se puede mapear 4 MiB de un espacio de direcciones
usando un solo PDE —una sola entrada en el *page directory*— sin
necesidad de una *page table* intermedia. Se debe activar soporte para
*large pages* mediante el registro `%cr4` de la CPU, y poner a 1 el bit
`PTE_PS` en el PDE correspondiente.

    $ git diff --stat tp1_sol..tp1_sol_pt4
     kern/entry.S      |  4 +++
     kern/entrypgdir.c | 10 +++---
     kern/pmap.c       | 28 ++++++++++++---
     3 files changed, 32 insertions(+), 10 deletions(-)

### Tarea: entry\_pgdir\_large

{: \#entry\_pgdir\_large}

Durante el arranque de JOS, antes de llamar a `i386_init()`, se mapean
los primeros 4 MiB de memoria física en las direcciones 0x0 y
0xF0000000. Para ello, se crea un *page directory* inicial con dos
entradas
([entry\_pgdir](https://github.com/fisop/jos/blob/tp1/kern/entrypgdir.c#L6))
y un *page table* asociado que lista cada página individual en esos 4
MiB
([entry\_pgtable](https://github.com/fisop/jos/blob/tp1/kern/entrypgdir.c#L30)).
Al tratarse de 4 MiB exactos, no obstante, se puede conseguir el mismo
efecto usando tan solo `entry_pgdir`.

Se pide:

1.  añadir en *entry.S* el código necesario para activar soporte de
    large pages en la CPU

2.  modificar `entry_pgdir` para que haga uso de “large pages”

3.  eliminar el arreglo estático `entry_pgtable`, ya que no debería
    necesitarse más.

    Para no generar un diff innecesariamente grande, se puede omitir de
    la compilación mediante una instrucción al pre-procesador:

         #if 0  // entry_pgtable no longer needed.
         pte_t entry_pgtable[NPTENTRIES] = {
             0x000000 | PTE_P | PTE_W,
             ...
             0x3ff000 | PTE_P | PTE_W,
         };
         #endif

### Tarea: map\_region\_large

{: \#map\_region\_large}

Modificar la función `boot_map_region()` para que use page directory
entries de 4 MiB cuando sea apropiado. (En particular, sólo se pueden
usar en direcciones alineadas a 22 bits.)

Guardar la implementación con un flag `TP1_PSE`:

``` {.c}
static void boot_map_region(...)
{
#ifndef TP1_PSE
    // Código original.
#else
    // Nueva implementación.
#endif
}
```

Responder: ¿cuánta memoria se ahorró de este modo? ¿Es una cantidad
fija, o depende de la memoria física de la computadora?

{% include anchors.html %} {% include footnotes.html %}

[^1]: Material original en inglés: [Lab 2: Memory
    Management](https://pdos.csail.mit.edu/6.828/2017/labs/lab2/)

[^2]: El uso de una lista enlazada permite a `page_alloc()` encontrar
    una página libre en *O(1)*, en lugar de recorrer el arreglo de
    páginas al completo.

[^3]: Las direcciones por debajo de `UTOP` corresponden a procesos de
    usuario, y se configurarán en futuros TPs.
