# Sensitive-Touch-MIDI-Controller
Controlador MIDI XY capacitivo construido con hardware minimalista: 4 pines Dupont sobre PCB perforada y un Arduino Nano. Sin chips dedicados. Incluye calibración adaptativa, detección de toque sostenido y mapeo expresivo (X → notas, Y → filtro, presión → velocity).
# MIDI XY Controller — Capacitive, Minimal, Hackeable

Controlador MIDI XY capacitivo hecho con lo mínimo: **4 pines Dupont clavados en una PCB perforada** y un **Arduino Nano**. Sin grafito, sin chips dedicados, sin PCB custom.

No es un juguete. Es un instrumento expresivo:
- **Eje X** → notas (escala pentatónica de Do)
- **Eje Y** → filtro (CC74)
- **Presión / área de contacto** → expresión (CC11) y velocity
<img width="1088" height="1445" alt="WhatsApp Image 2026-11-23 at 8 29 20 PM" src="https://github.com/user-attachments/assets/b1dd8605-970b-4dd2-9959-d9d2b878d590" />

- 

El proyecto nace de un problema real: con pads de cobre tan pequeños, la señal es tan débil que el ruido de red la tapa. La solución no fue más código, fue **física**: cobre en los pads y una guarda a GND. Este repo documenta ese proceso.

![Estado](https://img.shields.io/badge/estado-funcional-brightgreen)
![Placa](https://img.shields.io/badge/placa-Arduino%20Nano-blue)
![Protocolo](https://img.shields.io/badge/protocolo-MIDI%20serial-orange)

---

## Características

- **Calibración adaptativa**: mide la línea base de cada pad, calcula la ganancia individual y fija los umbrales a partir del ruido real medido.
- **Detección de toque sostenido**: exige varias lecturas consecutivas por encima del umbral para evitar notas fantasma por un pico de ruido.
- **Filtro de mediana** sobre la suma de los 4 pads antes de comparar, para eliminar outliers.
- **Doble lectura con descarte** (`ADCTouch.read` dos veces) para limpiar el ADC entre canales.
- **Baseline adaptativo** en reposo, para compensar la deriva térmica y de humedad.
- **Mapeo directo a MIDI**: Note On/Off, CC74 (filtro) y CC11 (expresión).

---

## Hardware


https://github.com/user-attachments/assets/fffec69d-ad71-409a-acda-eca502ad2e2d



| Componente | Cantidad | Notas |
|---|---|---|
| Arduino Nano (clásico, ATmega328P) | 1 | Cualquier clon con FTDI sirve |
| PCB perforada | 1 | Tamaño libre |
| Pines Dupont macho | 4 | Los electrodos |
| Cinta de cobre adhesiva | ~10 cm | Para los pads y la guarda a GND |
| Cable Dupont o hilo fino | 4 + 1 | Señales + GND |

**Conexiones:**

| Pin Arduino | Función |
|---|---|
| `A0` | Pad superior izquierdo |
| `A1` | Pad superior derecho |
| `A2` | Pad inferior izquierdo |
| `A3` | Pad inferior derecho |
| `GND` | Guarda / barra de masa (tocar con la palma) |

> **Importante:** sin referencia a GND, los pads son antenas y el ruido de red domina la lectura. Ver la sección "El problema del GND".

---

## Software

### Dependencias

- [ADCTouch](https://github.com/martin2250/ADCTouch) — lectura capacitiva por ADC.
- [Hairless MIDI <-> Serial Bridge](https://projectgus.github.io/hairless-midiserial/) — puente serie-MIDI.
- [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) — puerto MIDI virtual (Windows).

### Instalación

1. Instalar `ADCTouch` desde el Library Manager de Arduino IDE.
2. Abrir `midi_xy_controller.ino` y subirlo al Nano.
3. Abrir **Hairless MIDI**, seleccionar el puerto del Nano a **115200**.
4. Activar **Serial ↔ MIDI Bridge On**.
5. En **loopMIDI**, crear un puerto virtual.
6. En Hairless, seleccionar ese puerto como **MIDI In** y **MIDI Out**.
7. En el DAW, habilitar el puerto de loopMIDI como entrada MIDI.

---

## Calibración

Al arrancar, el sketch entra en modo calibración por el **Monitor Serie (115200)**:
