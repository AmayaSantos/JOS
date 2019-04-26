---
title: "Trabajos Prácticos con JOS"
author: |
  | Federico del Mazo - 100029
  | Rodrigo Souto - 97649
titlepage: true
geometry: margin=2.5cm, a4paper
---

# Trabajos Prácticos con JOS

Respuestas teóricas de los distintos trabajos prácticos/labs de Sistemas Operativos (75.08).

## TP1: Memoria virtual en JOS (26/04/2019)

### Memoria física: boot_alloc_pos

1. Un cálculo manual de la primera dirección de memoria que devolverá boot_alloc() tras el arranque. Se puede calcular a partir del binario compilado (obj/kern/kernel), usando los comandos readelf y/o nm y operaciones matemáticas.

2. Una sesión de GDB en la que, poniendo un breakpoint en la función boot_alloc(), se muestre el valor de end y nextfree al comienzo y fin de esa primera llamada a boot_alloc().

### Memoria física: page_alloc

1. Responder: ¿en qué se diferencia page2pa() de page2kva()?

### Large pages: map_region_large

1. Responder: ¿cuánta memoria se ahorró de este modo? ¿Es una cantidad fija, o depende de la memoria física de la computadora?