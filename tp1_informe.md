---
title: "TP1: Memoria virtual en JOS"
author: |
  | Federico del Mazo - 100029
  | Rodrigo Souto - 97649
date: |
  | Facultad de Ingeniería, Universidad de Buenos Aires
  | [75.08] Sistemas Operativos
  | 
  | 26 de abril de 2019
titlepage: true
geometry: margin=2.5cm, a4paper
header-includes: |
   \usepackage{framed,fancyhdr,xcolor}
   \pagestyle{fancy}
   \fancyhead[L]{[75.08] Sistemas Operativos \\ x86: Assembler y Call Conventions}
   \fancyhead[R]{Federico del Mazo \\ 2019c1}
   \let\OldTexttt\texttt
   \newenvironment{leftbar_mod}{
       \def\FrameCommand{{\color{magenta}\vrule width 1pt} \hspace{10pt}}
       \MakeFramed {\advance\hsize-\width \FrameRestore}}
   {\endMakeFramed}
   \let\oldshaded\Shaded
   \renewenvironment{Shaded}{\begin{leftbar_mod}\begin{oldshaded}}{\end{oldshaded}\end{leftbar_mod}}
      \let\oldverbatim\verbatim
   \renewenvironment{verbatim}{\begin{leftbar_mod}\begin{oldverbatim}}{\end{oldverbatim}\end{leftbar_mod}}
include-before: |
   \renewcommand{\texttt}[1]{\OldTexttt{\color{magenta}{#1}}}    
---

# Parte 1: Memoria física

## mem_init_pages

## boot_alloc

## boot_alloc_pos

## page_init

## page_alloc

## page_free

# Parte 2: Memoria virtual

## pgdir_walk

## page_lookup

## page_insert

## page_remove

# Parte 3: Page directory del kernel

## boot_map_region

## kernel_pgdir_setup

# Parte 4: Large pages

## entry_pgdir_large

## map_region_large

```zsh
Prueba de código
```
